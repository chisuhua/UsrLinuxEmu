#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "gpu_driver/shared/gpu_events.h"
#include "gpu_driver/shared/gpu_ioctl.h"
#include "gpu_driver/shared/gpu_types.h"
#include "kernel/file_ops.h"
#include "kernel/module_loader.h"
#include "kernel/vfs.h"

using namespace usr_linux_emu;

static int run_tests() {
  ModuleLoader::load_plugins("plugins");

  auto dev = VFS::instance().open("/dev/gpgpu0", 0);
  if (!dev) {
    std::cerr << "FAIL: Failed to open GPGPU device." << std::endl;
    return 1;
  }

  int fd = 0;
  int failures = 0;

  /* Create a pool to use in export tests. */
  struct gpu_mem_pool_create_args create_args = {};
  create_args.props.va_space_handle = 0;
  create_args.props.size            = 1024 * 1024;
  create_args.props.flags           = 0;

  int ret = dev->fops->ioctl(fd, GPU_IOCTL_MEM_POOL_CREATE, &create_args);
  if (ret != 0) {
    std::cerr << "FAIL: MEM_POOL_CREATE returned " << ret << std::endl;
    dev.reset();
    ModuleLoader::unload_plugins();
    return 1;
  }
  uint64_t pool_handle = create_args.pool_handle_out;
  std::cout << "[TestGPU] Created pool handle=" << pool_handle << std::endl;

  /* Test 1: Valid pool handle returns valid FD. */
  {
    struct gpu_mem_pool_export_args args = {};
    args.pool_handle = pool_handle;
    args.handle_type = 1;  /* CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR */
    args.flags       = 0;

    ret = dev->fops->ioctl(fd, GPU_IOCTL_MEM_POOL_EXPORT, &args);
    if (ret != 0) {
      std::cerr << "FAIL [1]: MEM_POOL_EXPORT valid pool returned " << ret << std::endl;
      failures++;
    } else if (args.fd_out < 0) {
      std::cerr << "FAIL [1]: fd_out = " << args.fd_out << " (expected >= 0)" << std::endl;
      failures++;
    } else {
      int export_fd = args.fd_out;
      int fflags = fcntl(export_fd, F_GETFD);
      if (!(fflags & FD_CLOEXEC)) {
        std::cerr << "FAIL [1]: FD is not O_CLOEXEC" << std::endl;
        failures++;
      } else {
        std::cout << "PASS [1]: MEM_POOL_EXPORT returned valid FD=" << export_fd
                  << " with O_CLOEXEC" << std::endl;
      }
      close(export_fd);
    }
  }

  /* Test 2: Invalid pool handle returns -EINVAL (error code < 0). */
  {
    struct gpu_mem_pool_export_args args = {};
    args.pool_handle = 0xDEADBEEF;
    args.handle_type = 1;
    args.flags       = 0;

    ret = dev->fops->ioctl(fd, GPU_IOCTL_MEM_POOL_EXPORT, &args);
    if (ret >= 0) {
      std::cerr << "FAIL [2]: Expected error for invalid pool, got " << ret << std::endl;
      failures++;
    } else {
      std::cout << "PASS [2]: Invalid pool handle returned " << ret << std::endl;
    }
  }

  /* Test 3: Non-POSIX handle_type returns -EINVAL. */
  {
    struct gpu_mem_pool_export_args args = {};
    args.pool_handle = pool_handle;
    args.handle_type = 99;  /* not CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR */
    args.flags       = 0;

    ret = dev->fops->ioctl(fd, GPU_IOCTL_MEM_POOL_EXPORT, &args);
    if (ret >= 0) {
      std::cerr << "FAIL [3]: Expected error for handle_type=99, got " << ret << std::endl;
      failures++;
    } else {
      std::cout << "PASS [3]: Non-POSIX handle_type returned " << ret << std::endl;
    }
  }

  /* Test 4: Non-zero flags returns -EINVAL. */
  {
    struct gpu_mem_pool_export_args args = {};
    args.pool_handle = pool_handle;
    args.handle_type = 1;
    args.flags       = 42;

    ret = dev->fops->ioctl(fd, GPU_IOCTL_MEM_POOL_EXPORT, &args);
    if (ret >= 0) {
      std::cerr << "FAIL [4]: Expected error for flags=42, got " << ret << std::endl;
      failures++;
    } else {
      std::cout << "PASS [4]: Non-zero flags returned " << ret << std::endl;
    }
  }

  /* Test 5: FD is closeable without affecting pool state. */
  {
    struct gpu_mem_pool_export_args args = {};
    args.pool_handle = pool_handle;
    args.handle_type = 1;
    args.flags       = 0;

    ret = dev->fops->ioctl(fd, GPU_IOCTL_MEM_POOL_EXPORT, &args);
    if (ret != 0 || args.fd_out < 0) {
      std::cerr << "FAIL [5]: export failed: ret=" << ret
                << " fd_out=" << args.fd_out << std::endl;
      failures++;
    } else {
      int export_fd = args.fd_out;
      close(export_fd);
      bool close_ok = (fcntl(export_fd, F_GETFD) < 0);
      if (!close_ok) {
        std::cerr << "FAIL [5]: FD still valid after close()" << std::endl;
        failures++;
      } else {
        /* Additional check: pool still exists after exporting and closing FD. */
        struct gpu_mem_pool_trim_args trim_args = {};
        trim_args.pool_handle = pool_handle;
        trim_args.min_bytes   = 0;
        ret = dev->fops->ioctl(fd, GPU_IOCTL_MEM_POOL_TRIM, &trim_args);
        if (ret != 0) {
          std::cerr << "FAIL [5]: Pool TRIM failed after export+close: " << ret << std::endl;
          failures++;
        } else {
          std::cout << "PASS [5]: FD closeable without affecting pool state" << std::endl;
        }
      }
    }
  }

  dev.reset();
  ModuleLoader::unload_plugins();

  if (failures == 0) {
    std::cout << "\nAll 5 MEM_POOL_EXPORT tests passed." << std::endl;
  } else {
    std::cout << "\n" << failures << " MEM_POOL_EXPORT tests FAILED." << std::endl;
  }
  return failures;
}

int main() {
  return run_tests();
}