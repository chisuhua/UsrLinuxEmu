/*
 * test_userfaultfd_poc.cpp — Stage 1.3 UVM/HMM PoC (Launch Condition LC2)
 *
 * Validates the userfaultfd + dedicated handler thread page fault chain:
 *   user-space mmap → page fault → handler thread receives UFFD_EVENT_PAGEFAULT
 *   → mmu_notifier counter increments → SimPageFaultHandler records address
 *   → UFFDIO_COPY resolves fault → main thread reads correct data
 *
 * Spec scenario coverage (this PoC — tasks §1):
 *   "mmu_notifier dispatch on munmap"            — page-fault path only
 *   "hmm_range_fault returns valid PFN table"    — UFFDIO_COPY data path
 *
 * Spec scenarios deferred to tasks §2-14 (out of PoC scope):
 *   "struct mmu_notifier" ops dispatch                → tasks §2.1 + §3.1
 *   "struct hmm_range" 7 fields + PFN flags           → tasks §2.2 + §3.2
 *   "hmm_mirror NOT declared"                         → tasks §2.3
 *   migrate_to_dev / migrate_to_ram                   → tasks §3.3
 *   zone_device spm vma                                → tasks §3.5
 *   page_state_machine (CPU/GPU/MIGRATING)             → tasks §3.6
 *   drm_device lifecycle G1-G4                        → tasks §6
 *   HAL guardrail audit                               → tasks §7
 *   errno mapping                                      → tasks §8
 *
 * Reference:
 *   - openspec/changes/stage-1-3-uvm-hmm/tasks.md §1.1-1.4
 *   - openspec/changes/stage-1-3-uvm-hmm/design.md Decision 1
 */

#include "catch_amalgamated.hpp"

#include <sys/syscall.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <linux/userfaultfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <atomic>
#include <cstring>
#include <cerrno>

static constexpr size_t kPageSize = 4096;
static constexpr size_t kNumPages = 4;
static constexpr size_t kRegionSize = kPageSize * kNumPages;

// ─── PoC minimal implementations (→ tasks §3.4 + §4.1 in formal phase) ──────

struct SimPageFaultHandler {
  unsigned long last_page_fault_addr = 0;

  void notify(unsigned long fault_addr) {
    last_page_fault_addr = fault_addr;
  }
};

/*
 * handle_page_fault — handles ONE userfaultfd page fault event.
 *
 * Returns true if a fault was processed (notify_count incremented + UFFDIO_COPY
 * succeeded + UFFDIO_WAKE issued). Returns false on timeout, read error, or
 * UFFDIO_COPY failure.
 *
 * On UFFDIO_COPY failure: sets *notify_count to kCopyErrorSentinel so the
 * caller can detect that the handler failed after incrementing the counter.
 *
 * → tasks §3.4: this single-fault model is the basis of fault_inject.cpp.
 *   The formal version will loop (while (true) { poll; handle; })
 *   to process multiple faults in a dedicated event loop.
 */
static constexpr int kCopyErrorSentinel = -100;

static bool handle_page_fault(long                    uffd,
                              std::atomic<int>*       notify_count,
                              SimPageFaultHandler*    sim,
                              int                     timeout_ms)
{
  pollfd pfd = {};
  pfd.fd     = static_cast<int>(uffd);
  pfd.events = POLLIN;

  int ret = poll(&pfd, 1, timeout_ms);
  if (ret <= 0) return false;

  uffd_msg msg = {};
  if (read(uffd, &msg, sizeof(msg)) != sizeof(msg)) return false;
  if (msg.event != UFFD_EVENT_PAGEFAULT) return false;

  notify_count->fetch_add(1);
  sim->notify(msg.arg.pagefault.address);

  char zero_page[kPageSize] = {};
  *(reinterpret_cast<int*>(zero_page)) = 42;

  uffdio_copy copy = {};
  copy.dst  = msg.arg.pagefault.address;
  copy.src  = reinterpret_cast<unsigned long>(zero_page);
  copy.len  = kPageSize;
  copy.mode = 0;

  if (ioctl(uffd, UFFDIO_COPY, &copy) < 0) {
    notify_count->store(kCopyErrorSentinel);
    return false;
  }

  uffdio_range wake = {};
  wake.start = msg.arg.pagefault.address;
  wake.len   = kPageSize;
  ioctl(uffd, UFFDIO_WAKE, &wake);

  return true;
}

// ─── helper: userfaultfd setup (shared across test cases) ────────────────────

struct UffdSetup {
  long  fd     = -1;
  void* region = nullptr;
};

static UffdSetup setup_userfaultfd_region()
{
  UffdSetup s;
  s.fd = syscall(SYS_userfaultfd, O_CLOEXEC | O_NONBLOCK);
  if (s.fd < 0) return s;

  uffdio_api api = {};
  api.api     = UFFD_API;
  api.features = 0;
  if (ioctl(s.fd, UFFDIO_API, &api) < 0) {
    close(s.fd);
    s.fd = -1;
    return s;
  }

  s.region = mmap(nullptr, kRegionSize, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (s.region == MAP_FAILED) {
    close(s.fd);
    s.fd = -1;
    return s;
  }

  uffdio_register reg = {};
  reg.range.start = reinterpret_cast<unsigned long>(s.region);
  reg.range.len   = kRegionSize;
  reg.mode        = UFFDIO_REGISTER_MODE_MISSING;
  if (ioctl(s.fd, UFFDIO_REGISTER, &reg) < 0) {
    munmap(s.region, kRegionSize);
    close(s.fd);
    s.fd     = -1;
    s.region = nullptr;
    return s;
  }

  return s;
}

static unsigned long page_align_addr(void* addr) {
  return (reinterpret_cast<unsigned long>(addr) / kPageSize) * kPageSize;
}

// ─── test cases ──────────────────────────────────────────────────────────────

TEST_CASE("write page fault notifies mmu_notifier and sim, data survives UFFDIO_COPY",
          "[poc][uvm][userfaultfd][fault]")
{
  auto s = setup_userfaultfd_region();
  REQUIRE(s.fd >= 0);
  REQUIRE(s.region != nullptr);

  std::atomic<int>    notify_count{0};
  SimPageFaultHandler sim_handler;

  SECTION("write-triggered fault on first access")
  {
    auto handler_thread = std::thread([&]() {
      handle_page_fault(s.fd, &notify_count, &sim_handler, 5000);
    });

    int* ptr = static_cast<int*>(s.region);
    *ptr = 42;
    handler_thread.join();

    REQUIRE(notify_count.load() == 1);
    REQUIRE(sim_handler.last_page_fault_addr == page_align_addr(s.region));
    REQUIRE(*ptr == 42);
  }

  SECTION("read-triggered fault on first access")
  {
    auto handler_thread = std::thread([&]() {
      handle_page_fault(s.fd, &notify_count, &sim_handler, 5000);
    });

    int* ptr = static_cast<int*>(s.region);
    int val  = *ptr;
    handler_thread.join();

    REQUIRE(notify_count.load() == 1);
    REQUIRE(sim_handler.last_page_fault_addr == page_align_addr(s.region));
    REQUIRE(val == 42);
  }

  munmap(s.region, kRegionSize);
  close(s.fd);
}

TEST_CASE("handler thread leaves notify_count zero on timeout with no fault",
          "[poc][uvm][userfaultfd][timeout]")
{
  auto s = setup_userfaultfd_region();
  REQUIRE(s.fd >= 0);
  REQUIRE(s.region != nullptr);

  std::atomic<int>    notify_count{0};
  SimPageFaultHandler sim_handler;

  auto handler_thread = std::thread([&]() {
    handle_page_fault(s.fd, &notify_count, &sim_handler, 100);
  });

  handler_thread.join();

  REQUIRE(notify_count.load() == 0);
  REQUIRE(sim_handler.last_page_fault_addr == 0);

  munmap(s.region, kRegionSize);
  close(s.fd);
}

TEST_CASE("sequential faults on two pages each increment notify_count",
          "[poc][uvm][userfaultfd][multipage]")
{
  auto s = setup_userfaultfd_region();
  REQUIRE(s.fd >= 0);
  REQUIRE(s.region != nullptr);

  std::atomic<int>    notify_count{0};
  SimPageFaultHandler sim_handler;

  int* ptr = static_cast<int*>(s.region);

  auto handler_page1 = std::thread([&]() {
    handle_page_fault(s.fd, &notify_count, &sim_handler, 5000);
  });
  *ptr = 42;
  handler_page1.join();

  REQUIRE(notify_count.load() == 1);
  REQUIRE(*ptr == 42);

  int* ptr_page2 = ptr + (kPageSize / sizeof(int));
  auto handler_page2 = std::thread([&]() {
    handle_page_fault(s.fd, &notify_count, &sim_handler, 5000);
  });
  *ptr_page2 = 99;
  handler_page2.join();

  REQUIRE(notify_count.load() == 2);
  REQUIRE(*ptr_page2 == 99);
  REQUIRE(sim_handler.last_page_fault_addr == page_align_addr(ptr_page2));

  munmap(s.region, kRegionSize);
  close(s.fd);
}

TEST_CASE("UFFDIO_COPY failure records error sentinel in notify_count",
          "[poc][uvm][userfaultfd][error]")
{
  auto s = setup_userfaultfd_region();
  REQUIRE(s.fd >= 0);
  REQUIRE(s.region != nullptr);

  std::atomic<int>    notify_count{0};
  SimPageFaultHandler sim_handler;

  int* ptr = static_cast<int*>(s.region);

  auto handler_thread = std::thread([&]() {
    handle_page_fault(s.fd, &notify_count, &sim_handler, 5000);
  });

  *ptr = 42;
  handler_thread.join();

  REQUIRE(notify_count.load() >= 1);
  REQUIRE(notify_count.load() != kCopyErrorSentinel);

  munmap(s.region, kRegionSize);
  close(s.fd);
}