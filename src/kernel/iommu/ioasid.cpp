/*
 * ioasid.cpp — IOASID 32-bit allocator (user-space)
 *
 * Stage 1.1 (IOMMU + ATS): 32-bit ID space allocator for SVM/HMM.
 * Per Linux 6.6 LTS ioasid.h semantics.
 */

#include "iommu_internal.h"

#include <cstdlib>
#include <cstring>

extern "C" {

static unsigned int g_ioasid_next = 1; /* 0 reserved as invalid */

struct ioasid *ioasid_alloc(void)
{
	auto *g = usr_linux_emu::iommu_emu_global_state();
	if (!g)
		return nullptr;

	auto *ioasid = (struct ioasid *)std::malloc(sizeof(struct ioasid));
	if (!ioasid)
		return nullptr;

	ioasid->priv = nullptr;

	/* First-fit allocation starting from g_ioasid_next (wraps on exhaustion) */
	unsigned int start = g_ioasid_next;
	for (unsigned int i = 0; i < 0xFFFF; i++) {
		unsigned int candidate = g_ioasid_next;
		g_ioasid_next = (g_ioasid_next + 1) & 0xFFFF;
		if (g_ioasid_next == 0)
			g_ioasid_next = 1; /* skip reserved 0 */
		if (!g->ioasid_table[candidate]) {
			ioasid->id = candidate;
			g->ioasid_table[candidate] = ioasid;
			return ioasid;
		}
		if (g_ioasid_next == start) {
			/* Wrapped: no free IDs */
			std::free(ioasid);
			return nullptr;
		}
	}
	std::free(ioasid);
	return nullptr;
}

struct ioasid *ioasid_alloc_id(unsigned int id)
{
	if (id == 0 || id >= 0xFFFF)
		return nullptr;

	auto *g = usr_linux_emu::iommu_emu_global_state();
	if (!g)
		return nullptr;
	if (g->ioasid_table[id])
		return nullptr;

	auto *ioasid = (struct ioasid *)std::malloc(sizeof(struct ioasid));
	if (!ioasid)
		return nullptr;
	ioasid->id = id;
	ioasid->priv = nullptr;
	g->ioasid_table[id] = ioasid;
	return ioasid;
}

int ioasid_free(struct ioasid *ioasid)
{
	if (!ioasid)
		return IOMMU_ERR_EINVAL;

	auto *g = usr_linux_emu::iommu_emu_global_state();
	if (!g)
		return IOMMU_ERR_ENOSYS;

	unsigned int id = ioasid->id;
	if (id == 0 || id >= 0xFFFF)
		return IOMMU_ERR_ENODEV;

	if (g->ioasid_table[id] != ioasid)
		return IOMMU_ERR_ENODEV;

	g->ioasid_table[id] = nullptr;
	std::free(ioasid);
	return IOMMU_ERR_OK;
}

struct ioasid *ioasid_find(unsigned int id)
{
	if (id == 0 || id >= 0xFFFF)
		return nullptr;
	auto *g = usr_linux_emu::iommu_emu_global_state();
	if (!g)
		return nullptr;
	return g->ioasid_table[id];
}

unsigned int ioasid_id(struct ioasid *ioasid)
{
	if (!ioasid)
		return 0;
	return ioasid->id;
}

}  /* extern "C" */
