/*
 * iommu_group.cpp — IOMMU group allocation and member management (user-space)
 *
 * Stage 1.1 (IOMMU + ATS): per docs/roadmap/stage-1-kernel-emu.md §子阶段 1.1.
 * Per design.md Decision 5: 1 PCIe device = 1 iommu_group.
 */

#include "iommu_internal.h"

#include <cstdlib>
#include <cstring>
#include <algorithm>

extern "C" {

static int g_next_group_id = 0;

struct iommu_group *iommu_group_alloc(void)
{
	struct iommu_group *group = (struct iommu_group *)std::malloc(sizeof(struct iommu_group));
	if (!group)
		return nullptr;

	std::memset(group, 0, sizeof(*group));
	group->id = g_next_group_id++;

	auto *state = new (std::nothrow) usr_linux_emu::iommu_group_state();
	if (!state) {
		std::free(group);
		return nullptr;
	}
	state->group_id = group->id;
	group->priv = state;

	auto *g = usr_linux_emu::iommu_emu_global_state();
	if (g) {
		g->groups.push_back(group);
	}

	return group;
}

void iommu_group_free(struct iommu_group *group)
{
	if (!group)
		return;

	/* Caller must have removed all members first */
	if (group->priv) {
		auto *state = static_cast<usr_linux_emu::iommu_group_state *>(group->priv);
		delete state;
		group->priv = nullptr;
	}

	if (group->default_domain) {
		iommu_domain_free(group->default_domain);
		group->default_domain = nullptr;
	}

	auto *g = usr_linux_emu::iommu_emu_global_state();
	if (g) {
		auto it = std::find(g->groups.begin(), g->groups.end(), group);
		if (it != g->groups.end())
			g->groups.erase(it);
	}

	std::free(group);
}

int iommu_group_add_device(struct iommu_group *group, struct device *dev)
{
	if (!group || !dev)
		return IOMMU_ERR_EINVAL;

	auto *g = usr_linux_emu::iommu_emu_global_state();
	if (!g)
		return IOMMU_ERR_ENOSYS;

	/* Stage-1.1 enforces uniqueness — refuse if device already in a group */
	if (g->device_to_group.count(dev))
		return IOMMU_ERR_EBUSY;

	auto *member = (struct iommu_group_member *)std::malloc(sizeof(struct iommu_group_member));
	if (!member)
		return IOMMU_ERR_ENOMEM;

	member->dev = dev;
	member->group = group;
	member->next = group->members;
	group->members = member;
	group->ndevices++;

	auto *state = static_cast<usr_linux_emu::iommu_group_state *>(group->priv);
	if (state)
		state->members.push_back(member);

	g->device_to_group[dev] = group;

	return IOMMU_ERR_OK;
}

int iommu_group_remove_device(struct iommu_group *group, struct device *dev)
{
	if (!group || !dev)
		return IOMMU_ERR_EINVAL;

	/* Walk the singly-linked list */
	struct iommu_group_member **pp = &group->members;
	while (*pp) {
		if ((*pp)->dev == dev) {
			struct iommu_group_member *victim = *pp;
			*pp = victim->next;
			std::free(victim);
			group->ndevices--;
			auto *g = usr_linux_emu::iommu_emu_global_state();
			if (g) {
				auto it = g->device_to_group.find(dev);
				if (it != g->device_to_group.end())
					g->device_to_group.erase(it);
			}
			return IOMMU_ERR_OK;
		}
		pp = &(*pp)->next;
	}
	return IOMMU_ERR_ENODEV;
}

struct iommu_group *iommu_group_get(struct device *dev)
{
	if (!dev)
		return nullptr;
	auto *g = usr_linux_emu::iommu_emu_global_state();
	if (!g)
		return nullptr;
	auto it = g->device_to_group.find(dev);
	if (it == g->device_to_group.end())
		return nullptr;
	return it->second;
}

struct iommu_domain *iommu_group_default_domain(struct iommu_group *group)
{
	if (!group)
		return nullptr;
	return group->default_domain;
}

int iommu_group_set_default_domain(struct iommu_group *group,
				   struct iommu_domain *domain)
{
	if (!group)
		return IOMMU_ERR_EINVAL;
	/* Caller is responsible for prior domain lifecycle */
	group->default_domain = domain;
	return IOMMU_ERR_OK;
}

}  /* extern "C" */
