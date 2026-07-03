/*
 * drm_prime.h — Prime (dma-buf sharing) simulation (user-space)
 *
 * Stage 1.2 of Linux kernel environment emulation roadmap.
 * Provides dma_buf_dynamic_attach/detach/map_attachment/unmap_attachment/
 * pin/unpin aligned with Linux 6.12 LTS API signatures (per ADR-027).
 *
 * Key correction (librarian 2026-07-02 verification):
 *   amdgpu uses dma_buf_dynamic_attach(), NOT dma_buf_attach().
 *   This header exposes dynamic_attach + dma_buf_attach_ops per
 *   design.md Decision 6 (dma_buf API target correction).
 *
 * When porting to kernel, replace with:
 *   #include <linux/dma-buf.h>
 *   #include <drm/drm_prime.h>
 */

#pragma once

#include <linux_compat/types.h>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct dma_buf_attachment;
struct drm_device;
struct device;

/* dma_buf_attach_ops — importer callbacks
 *
 * Verified present in Linux v6.12:
 *   include/linux/dma-buf.h:L327-L354
 */
struct dma_buf_attach_ops {
  bool allow_peer2peer;  /* importer can handle peer resources without struct page */
  void (*move_notify)(struct dma_buf_attachment *attach);  /* buffer relocation notif */
};

/* dma_buf — minimal buffer abstraction */
struct dma_buf {
  struct file *file;           /* backing shmem file (GEM equivalent) */
  const void *ops;             /* dma_buf_ops pointer */
  void *priv;                  /* GEM object or exporter-private data */
  struct dma_buf_attachment *attachments; /* linked list of importers */
};

/* dma_buf_attachment — one importer's view of a buffer */
struct dma_buf_attachment {
  struct dma_buf *dmabuf;          /* the shared buffer */
  struct device *dev;              /* importing device */
  struct dma_buf_attach_ops *importer_ops; /* importer's peer2peer + move_notify callbacks */
  void *priv;                      /* importer-private data */
  struct dma_buf_attachment *next; /* list linkage (user-space simplification) */
};

/* sg_table — scatter-gather table (minimal)
 *
 * In user-space there is no real DMA; this struct exists so that
 * dma_buf_map_attachment can return a non-null sg_table and KFD
 * prime import paths compile without modification.
 */
struct scatterlist {
  unsigned long page_link;
  unsigned int  offset;
  unsigned int  length;
};

struct sg_table {
  struct scatterlist *sgl;
  unsigned int nents;
  unsigned int orig_nents;
};

/* Prime attach/detach — signatures aligned with Linux 6.12
 *
 * These are the corrected API set per librarian analysis:
 *   amdgpu calls dma_buf_dynamic_attach() (NOT dma_buf_attach()).
 */
struct dma_buf_attachment *
dma_buf_dynamic_attach(struct dma_buf *dmabuf, struct device *dev,
                       const struct dma_buf_attach_ops *importer_ops,
                       void *importer_priv);
void dma_buf_detach(struct dma_buf *dmabuf, struct dma_buf_attachment *attach);

/* Mapping — CPU access to dma-buf scatterlist */
struct sg_table *
dma_buf_map_attachment(struct dma_buf_attachment *attach,
                       int direction);
void dma_buf_unmap_attachment(struct dma_buf_attachment *attach,
                              struct sg_table *sg_table,
                              int direction);

/* Pinning — stabilize backing storage (optional hooks) */
int  dma_buf_pin(struct dma_buf_attachment *attach);
void dma_buf_unpin(struct dma_buf_attachment *attach);

/* Lifecycle */
struct dma_buf *dma_buf_get(int fd);
void dma_buf_put(struct dma_buf *dmabuf);

#ifdef __cplusplus
}
#endif