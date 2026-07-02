// test_iommu_emu_standalone.cpp — Stage 1.1 IOMMU emulator lifecycle tests
// Per tasks.md §8.1: covers group creation, ioasid allocation, basic map/unmap.
// Per spec Requirement: IOMMU 数据结构定义 / PCIe device → iommu_group 拓扑自动推导.

#include <catch_amalgamated.hpp>

#include <cstdint>
#include <cstddef>

extern "C" {
#include <linux_compat/iommu/iommu.h>
#include <linux_compat/iommu/iommu_domain.h>
#include <linux_compat/iommu/iommu_group.h>
#include <linux_compat/iommu/ioasid.h>
}

namespace {

/* Ensure the emulator is initialized before any domain/group/ioasid test. */
struct IommuEmuFixture {
  IommuEmuFixture() { REQUIRE(iommu_emu_init() == 0); }
  /* Do NOT call iommu_emu_shutdown() in destructor — it would wipe state
   * for tests that run after. Tests are designed to allocate and free
   * their own resources. */
};

/* Mock device — any non-null pointer is valid since we only hash by address. */
struct MockDevice {
  int id;
  char pad[60]; /* avoid overlapping test addresses */
};

MockDevice g_dev_a{1, {}};
MockDevice g_dev_b{2, {}};

}  // namespace

TEST_CASE_METHOD(IommuEmuFixture, "iommu_domain_alloc_free_basic", "[iommu_emu]") {
  /* SPEC: Requirement: IOMMU 数据结构定义 — Data structures compilable */
  struct iommu_domain *d = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(d != nullptr);
  CHECK(d->type == IOMMU_DOMAIN_DMA);
  CHECK(d->pgsize_bitmap == 0x1000);  /* 4KB only in stage-1.1 */
  iommu_domain_free(d);
}

TEST_CASE_METHOD(IommuEmuFixture, "iommu_group_alloc_add_remove_device", "[iommu_emu]") {
  /* SPEC: Requirement: IOMMU 数据结构定义 — iommu_ops hook completeness */
  struct iommu_group *g = iommu_group_alloc();
  REQUIRE(g != nullptr);
  CHECK(g->ndevices == 0);
  CHECK(g->members == nullptr);

  int rc = iommu_group_add_device(g, (struct device *)&g_dev_a);
  CHECK(rc == IOMMU_ERR_OK);
  CHECK(g->ndevices == 1);
  CHECK(g->members != nullptr);

  struct iommu_group *found = iommu_group_get((struct device *)&g_dev_a);
  CHECK(found == g);

  rc = iommu_group_remove_device(g, (struct device *)&g_dev_a);
  CHECK(rc == IOMMU_ERR_OK);
  CHECK(g->ndevices == 0);

  iommu_group_free(g);
}

TEST_CASE_METHOD(IommuEmuFixture, "iommu_group_duplicate_add_returns_ebusy", "[iommu_emu][error]") {
  /* SPEC: Requirement: IOMMU 数据结构定义 — group behavior under duplicate add */
  struct iommu_group *g1 = iommu_group_alloc();
  struct iommu_group *g2 = iommu_group_alloc();
  REQUIRE(g1 != nullptr);
  REQUIRE(g2 != nullptr);

  CHECK(iommu_group_add_device(g1, (struct device *)&g_dev_a) == IOMMU_ERR_OK);
  /* Adding same device to another group must return -EBUSY (-16) */
  CHECK(iommu_group_add_device(g2, (struct device *)&g_dev_a) == IOMMU_ERR_EBUSY);

  iommu_group_free(g1);
  iommu_group_free(g2);
}

TEST_CASE_METHOD(IommuEmuFixture, "iommu_group_remove_unknown_returns_enodev", "[iommu_emu][error]") {
  struct iommu_group *g = iommu_group_alloc();
  CHECK(iommu_group_remove_device(g, (struct device *)&g_dev_b) == IOMMU_ERR_ENODEV);
  iommu_group_free(g);
}

TEST_CASE_METHOD(IommuEmuFixture, "iommu_register_pci_device_creates_group", "[iommu_emu][pcie]") {
  /* SPEC: Requirement: PCIe device → iommu_group 拓扑自动推导 */
  /* Use a unique device to avoid colliding with other tests' group state. */
  MockDevice dev_c{3, {}};

  struct iommu_group *g = iommu_register_pci_device(&dev_c);
  REQUIRE(g != nullptr);
  CHECK(g->ndevices == 1);
  CHECK(g->default_domain != nullptr);
  CHECK(g->default_domain->ops != nullptr);

  /* Idempotent re-registration returns same group */
  struct iommu_group *g_again = iommu_register_pci_device(&dev_c);
  CHECK(g_again == g);

  CHECK(iommu_unregister_pci_device(&dev_c) == IOMMU_ERR_OK);
}

TEST_CASE_METHOD(IommuEmuFixture, "ioasid_alloc_and_free_roundtrip", "[iommu_emu][ioasid]") {
  /* SPEC: Requirement: ioasid 32-bit allocator */
  struct ioasid *id1 = ioasid_alloc();
  REQUIRE(id1 != nullptr);
  unsigned int numeric = ioasid_id(id1);
  CHECK(numeric != 0);  /* 0 is reserved as invalid */

  /* Look up the same id */
  struct ioasid *id2 = ioasid_find(numeric);
  CHECK(id2 == id1);

  CHECK(ioasid_free(id1) == IOMMU_ERR_OK);
  /* After free, lookup returns null */
  CHECK(ioasid_find(numeric) == nullptr);
}

TEST_CASE_METHOD(IommuEmuFixture, "ioasid_alloc_id_specific", "[iommu_emu][ioasid]") {
  struct ioasid *id = ioasid_alloc_id(0x1234);
  REQUIRE(id != nullptr);
  CHECK(ioasid_id(id) == 0x1234);
  /* Re-allocating same id returns null */
  CHECK(ioasid_alloc_id(0x1234) == nullptr);
  ioasid_free(id);
}

TEST_CASE_METHOD(IommuEmuFixture, "ioasid_alloc_id_zero_returns_null", "[iommu_emu][ioasid][error]") {
  /* ID 0 is reserved as invalid */
  CHECK(ioasid_alloc_id(0) == nullptr);
}

TEST_CASE_METHOD(IommuEmuFixture, "iommu_map_unmap_happy_path", "[iommu_emu][dma]") {
  /* SPEC: Requirement: DMA remapping 页表实现 — happy path */
  struct iommu_domain *d = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(d != nullptr);

  CHECK(iommu_map(d, 0x1000, 0x100000, 0x1000,
                  IOMMU_READ | IOMMU_WRITE) == IOMMU_ERR_OK);
  CHECK(iommu_iova_to_phys(d, 0x1000) == 0x100000);

  long ret = iommu_unmap(d, 0x1000, 0x1000);
  CHECK(ret == 0x1000);  /* returns size unmapped on success */
  CHECK(iommu_iova_to_phys(d, 0x1000) == 0);  /* unmapped returns 0 */

  iommu_domain_free(d);
}

TEST_CASE_METHOD(IommuEmuFixture, "iommu_map_iova_overlap_returns_eremoteio", "[iommu_emu][dma][error]") {
  /* SPEC: Requirement: DMA remapping 页表实现 — IOVA overlap returns EREMOTEIO */
  struct iommu_domain *d = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(d != nullptr);

  CHECK(iommu_map(d, 0x1000, 0x100000, 0x1000, 0) == IOMMU_ERR_OK);
  /* Second map to same IOVA must return -EREMOTEIO (-121) */
  CHECK(iommu_map(d, 0x1000, 0x200000, 0x1000, 0) == IOMMU_ERR_EREMOTEIO);
  /* Original mapping preserved */
  CHECK(iommu_iova_to_phys(d, 0x1000) == 0x100000);

  iommu_domain_free(d);
}

TEST_CASE_METHOD(IommuEmuFixture, "iommu_unmap_not_mapped_returns_enokey", "[iommu_emu][dma][error]") {
  /* SPEC: Requirement: DMA remapping 页表实现 — Unmap returns the IOVA on success */
  struct iommu_domain *d = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(d != nullptr);

  long ret = iommu_unmap(d, 0x9999, 0x1000);
  CHECK(ret == (long)IOMMU_ERR_ENOKEY);  /* -126 */
  iommu_domain_free(d);
}

TEST_CASE_METHOD(IommuEmuFixture, "iommu_map_size_must_be_4kb", "[iommu_emu][dma][error]") {
  /* SPEC: Requirement: DMA remapping 页表实现 — only 4KB mapping in stage-1.1 */
  struct iommu_domain *d = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(d != nullptr);

  CHECK(iommu_map(d, 0x1000, 0x100000, 0x2000, 0) == IOMMU_ERR_EINVAL);
  CHECK(iommu_map(d, 0x1000, 0x100000, 0, 0) == IOMMU_ERR_EINVAL);
  iommu_domain_free(d);
}

TEST_CASE_METHOD(IommuEmuFixture, "iommu_map_misaligned_paddr_returns_einval", "[iommu_emu][dma][error]") {
  /* SPEC: Requirement: DMA remapping 页表实现 — paddr not page-aligned returns -EINVAL */
  struct iommu_domain *d = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(d != nullptr);

  /* paddr=0x100001 is not 4KB-aligned */
  CHECK(iommu_map(d, 0x1000, 0x100001, 0x1000, 0) == IOMMU_ERR_EINVAL);
  /* iova=0x1001 is not 4KB-aligned */
  CHECK(iommu_map(d, 0x1001, 0x100000, 0x1000, 0) == IOMMU_ERR_EINVAL);
  iommu_domain_free(d);
}

TEST_CASE_METHOD(IommuEmuFixture, "iommu_default_ops_dispatches_to_map_unmap", "[iommu_emu][ops]") {
  /* SPEC: Requirement: IOMMU 数据结构定义 — iommu_ops hook completeness */
  struct iommu_domain *d = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
  REQUIRE(d != nullptr);

  const struct iommu_ops *ops = iommu_default_ops_get();
  REQUIRE(ops != nullptr);
  REQUIRE(ops->map_page != nullptr);
  REQUIRE(ops->unmap_page != nullptr);
  REQUIRE(ops->iova_to_phys != nullptr);

  d->ops = ops;

  /* Use ops vtable — should route through default implementation */
  CHECK(ops->map_page(d, 0x2000, 0x200000, 0x1000, 0) == IOMMU_ERR_OK);
  CHECK(ops->iova_to_phys(d, 0x2000) == 0x200000);
  CHECK(ops->unmap_page(d, 0x2000, 0x1000) == IOMMU_ERR_OK);

  iommu_domain_free(d);
}
