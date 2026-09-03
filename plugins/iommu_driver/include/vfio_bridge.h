#ifndef USR_LINUX_EMU_IOMMU_VFIO_BRIDGE_H
#define USR_LINUX_EMU_IOMMU_VFIO_BRIDGE_H

#include <linux_compat/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int  us_iommu_vfio_available(void);
int  us_iommu_vfio_invalidate(unsigned long iova, unsigned long size);
void us_iommu_vfio_reset(void);

#ifdef __cplusplus
}
#endif

#endif