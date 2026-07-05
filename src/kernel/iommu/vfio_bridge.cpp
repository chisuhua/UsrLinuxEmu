#include "vfio_bridge.h"

#include <linux_compat/iommu/iommu.h>

#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#ifndef VFIO_TYPE
#define VFIO_TYPE 1
#endif
#ifndef VFIO_TYPE1_IOMMU
#define VFIO_TYPE1_IOMMU 1
#endif

struct vfio_iommu_type1_dma_unmap {
    unsigned long long iova;
    unsigned long long size;
};

#ifndef VFIO_IOMMU_UNMAP_DMA
#define VFIO_IOMMU_UNMAP_DMA _IO(VFIO_TYPE, 2)
#endif

namespace {
int g_vfio_fd = -1;
bool g_vfio_warned = false;
}

extern "C" {

int us_iommu_vfio_available(void) {
  if (!getenv("USR_LINUX_EMU_VFIO")) return 0;
  if (g_vfio_fd >= 0) return 1;
  g_vfio_fd = open("/dev/vfio/vfio", O_RDWR);
  if (g_vfio_fd < 0) {
    if (!g_vfio_warned) {
      std::fprintf(stderr,
                   "[vfio] /dev/vfio/vfio not accessible; "
                   "degrading iommu_flush_iotlb to page-table walk\n");
      g_vfio_warned = true;
    }
    return 0;
  }
  return 1;
}

int us_iommu_vfio_invalidate(unsigned long iova, unsigned long size) {
  if (!us_iommu_vfio_available()) return -ENOSYS;
  struct vfio_iommu_type1_dma_unmap arg;
  arg.iova = (unsigned long long)iova;
  arg.size = (unsigned long long)size;
  return ioctl(g_vfio_fd, VFIO_IOMMU_UNMAP_DMA, &arg);
}

void us_iommu_vfio_reset(void) {
  if (g_vfio_fd >= 0) {
    close(g_vfio_fd);
    g_vfio_fd = -1;
  }
  g_vfio_warned = false;
}

}  // extern "C"