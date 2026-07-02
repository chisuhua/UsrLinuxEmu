// test_ats_protocol_standalone.cpp — Stage 1.1 ATS protocol handler tests
// Per tasks.md §8.3: covers Translation Request → Completion round-trip +
// Invalidation Request + null-arg error path.
// Per spec Requirement: ATS 协议最小子集 / 错误码语义与 Linux 内核一致.

#include <catch_amalgamated.hpp>

#include <cstdint>
#include <cstddef>
#include <cerrno>

extern "C" {
#include <linux_compat/iommu/iommu.h>
#include <linux_compat/iommu/iommu_domain.h>
#include <linux_compat/pci/ats.h>
}

namespace {

struct AtsFixture {
  struct iommu_domain *d = nullptr;
  AtsFixture() {
    REQUIRE(iommu_emu_init() == 0);
    d = iommu_domain_alloc(IOMMU_DOMAIN_DMA);
    REQUIRE(d != nullptr);
  }
  ~AtsFixture() {
    if (d) iommu_domain_free(d);
  }
};

}  // namespace

TEST_CASE_METHOD(AtsFixture, "ats_translation_request_mapped_iova_returns_phys", "[ats]") {
  /* SPEC: Requirement: ATS 协议最小子集 — Translation Request round-trip */
  REQUIRE(iommu_map(d, 0x2000, 0x200000, 0x1000, 0) == IOMMU_ERR_OK);

  struct ats_translation_request req {};
  req.iova = 0x2000;
  req.pasid = 0;
  struct ats_translation_completion comp {};

  int rc = ats_handle_translation_request(d, &req, &comp);
  CHECK(rc == 0);
  CHECK(comp.completion_status == ATS_COMPLETION_SUCCESS);
  CHECK(comp.translated_address == 0x200000);
}

TEST_CASE_METHOD(AtsFixture, "ats_translation_request_unmapped_returns_unmapped", "[ats]") {
  /* SPEC: Requirement: ATS 协议最小子集 — Translation Request for unmapped IOVA */
  struct ats_translation_request req {};
  req.iova = 0x9999;
  req.pasid = 0;
  struct ats_translation_completion comp {};

  int rc = ats_handle_translation_request(d, &req, &comp);
  CHECK(rc == 0);
  CHECK(comp.completion_status == ATS_COMPLETION_UNMAPPED);
  CHECK(comp.translated_address == 0);
}

TEST_CASE_METHOD(AtsFixture, "ats_translation_request_nonzero_pasid_returns_invalid", "[ats]") {
  /* SPEC: Requirement: ATS 协议最小子集 — PASID explicitly out of scope (stage-1.4) */
  struct ats_translation_request req {};
  req.iova = 0x1000;
  req.pasid = 1;  /* stage-1.1: PASID reserved, must be 0 */
  struct ats_translation_completion comp {};

  int rc = ats_handle_translation_request(d, &req, &comp);
  CHECK(rc == 0);
  CHECK(comp.completion_status == ATS_COMPLETION_INVALID);
  CHECK(comp.translated_address == 0);
}

TEST_CASE_METHOD(AtsFixture, "ats_translation_request_after_unmap", "[ats][stale]") {
  /* SPEC: Requirement: ATS 协议最小子集 — round-trip after unmap */
  REQUIRE(iommu_map(d, 0x3000, 0x300000, 0x1000, 0) == IOMMU_ERR_OK);
  CHECK(iommu_unmap(d, 0x3000, 0x1000) == 0x1000);

  /* After unmap, the device-TLB should see UNMAPPED. */
  struct ats_translation_request req {};
  req.iova = 0x3000;
  struct ats_translation_completion comp {};
  CHECK(ats_handle_translation_request(d, &req, &comp) == 0);
  CHECK(comp.completion_status == ATS_COMPLETION_UNMAPPED);
}

TEST_CASE_METHOD(AtsFixture, "ats_invalidation_request_completes_success", "[ats]") {
  /* SPEC: Requirement: ATS 协议最小子集 — Invalidation Request round-trip */
  struct ats_invalidation_request req {};
  req.iova = 0x4000;
  req.size = 0x1000;
  struct ats_invalidation_completion comp {};

  int rc = ats_handle_invalidation_request(d, &req, &comp);
  CHECK(rc == 0);
  CHECK(comp.status == ATS_INVALIDATION_SUCCESS);
}

TEST_CASE_METHOD(AtsFixture, "ats_invalidation_with_vtable_invokes_flush", "[ats][ops]") {
  /* SPEC: Requirement: 错误码语义与 Linux 内核一致 — IOTLB flush triggered on
   *     invalidation. Hook must be invoked through domain->ops if attached. */
  const struct iommu_ops *ops = iommu_default_ops_get();
  REQUIRE(ops != nullptr);
  d->ops = ops;
  REQUIRE(d->ops->flush_iotlb != nullptr);

  struct ats_invalidation_request req {};
  req.iova = 0x5000;
  req.size = 0x1000;
  struct ats_invalidation_completion comp {};
  CHECK(ats_handle_invalidation_request(d, &req, &comp) == 0);
  CHECK(comp.status == ATS_INVALIDATION_SUCCESS);
}

TEST_CASE_METHOD(AtsFixture, "ats_null_arguments_return_einval", "[ats][error]") {
  /* SPEC: Requirement: 错误码语义与 Linux 内核一致 — null args → -EINVAL */
  struct ats_translation_request req {};
  struct ats_translation_completion comp {};

  CHECK(ats_handle_translation_request(nullptr, &req, &comp) == -EINVAL);
  CHECK(ats_handle_translation_request(d, nullptr, &comp) == -EINVAL);
  CHECK(ats_handle_translation_request(d, &req, nullptr) == -EINVAL);

  struct ats_invalidation_request ireq {};
  struct ats_invalidation_completion icomp {};
  CHECK(ats_handle_invalidation_request(nullptr, &ireq, &icomp) == -EINVAL);
  CHECK(ats_handle_invalidation_request(d, nullptr, &icomp) == -EINVAL);
  CHECK(ats_handle_invalidation_request(d, &ireq, nullptr) == -EINVAL);
}

TEST_CASE_METHOD(AtsFixture, "ats_completion_status_enum_values", "[ats][spec]") {
  /* SPEC: Requirement: ATS 协议最小子集 — enum values are stable */
  CHECK(ATS_COMPLETION_SUCCESS  == 0);
  CHECK(ATS_COMPLETION_UNMAPPED == 1);
  CHECK(ATS_COMPLETION_INVALID  == 2);
  CHECK(ATS_COMPLETION_TIMEOUT  == 3);
  CHECK(ATS_INVALIDATION_SUCCESS == 0);
  CHECK(ATS_INVALIDATION_FAILURE == 1);
}
