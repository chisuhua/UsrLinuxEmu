/*
 * iommu_group.h — IOMMU device group abstraction (user-space)
 *
 * Mirrors real kernel include/linux/iommu.h for portability.
 * When porting to kernel, replace with <linux/iommu.h>.
 *
 * Stage 1.1 (IOMMU + ATS): per docs/roadmap/stage-1-kernel-emu.md §子阶段 1.1.
 *
 * An iommu_group represents a set of devices that are isolated together
 * from the rest of the system. Stage-1.1 derives group topology from
 * PCIe device enumeration (one PCIe device -> one iommu_group, per
 * design.md Decision 5).
 */

#pragma once

#include <linux_compat/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct iommu_domain;
struct device;

/*
 * IOMMU group: set of isolated devices sharing one default domain.
 */
struct iommu_group {
	int id;				/* unique group identifier */
	int ndevices;			/* number of devices in group */
	struct iommu_group_member *members; /* linked list head */
	struct iommu_domain *default_domain;
	void *priv;			/* implementation-specific state */
};

/*
 * IOMMU group member: links a device to its containing group.
 * Multiple devices per group is supported by the data structure but
 * stage-1.1 enforces 1-device-per-group topology (see design.md Decision 5).
 */
struct iommu_group_member {
	struct device *dev;
	struct iommu_group *group;
	struct iommu_group_member *next;
};

/*
 * Allocate a new IOMMU group with no members. Returns NULL on failure.
 */
struct iommu_group *iommu_group_alloc(void);

/*
 * Free an empty IOMMU group previously allocated via iommu_group_alloc().
 * All members must be removed first via iommu_group_remove_device().
 */
void iommu_group_free(struct iommu_group *group);

/*
 * Add a device to a group. Stage-1.1 enforces uniqueness — a device
 * already in a group returns IOMMU_ERR_EBUSY (-16).
 */
int iommu_group_add_device(struct iommu_group *group, struct device *dev);

/*
 * Remove a device from a group. Returns IOMMU_ERR_ENODEV (-19) if the
 * device is not in this group.
 */
int iommu_group_remove_device(struct iommu_group *group, struct device *dev);

/*
 * Get the group containing the given device, or NULL if the device
 * has not been added to any group yet.
 */
struct iommu_group *iommu_group_get(struct device *dev);

/*
 * Get the default domain of a group. Returns NULL if no default
 * domain has been attached yet.
 */
struct iommu_domain *iommu_group_default_domain(struct iommu_group *group);

/*
 * Set the default domain of a group. Replaces any prior default domain
 * (caller is responsible for freeing the prior one).
 */
int iommu_group_set_default_domain(struct iommu_group *group,
				   struct iommu_domain *domain);

#ifdef __cplusplus
}
#endif
