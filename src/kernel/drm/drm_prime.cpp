/*
 * drm_prime.cpp — dma_buf sharing implementation (user-space)
 *
 * Stage 1.2 task group 2.3 — minimal dma_buf attach/detach/map/unmap/pin/unpin
 * aligned with Linux 6.12 LTS.  In user-space, this is a thin shim: there is
 * no real DMA, so sg_table is a single-page placeholder.
 *
 * Key correction (librarian): amdgpu calls dma_buf_dynamic_attach(), not
 * dma_buf_attach().  This implementation provides dynamic_attach.
 */

#include "linux_compat/drm/drm_prime.h"
#include "linux_compat/drm/drm_device.h"

#include <cstdlib>
#include <cstring>

extern "C" {

/* ----- attach / detach ------------------------------------------------ */

struct dma_buf_attachment *
dma_buf_dynamic_attach(struct dma_buf *dmabuf, struct device *dev,
                       const struct dma_buf_attach_ops *importer_ops,
                       void *importer_priv)
{
  (void)dev;
  struct dma_buf_attachment *attach
      = (struct dma_buf_attachment *)std::calloc(1, sizeof(*attach));
  if (!attach) return nullptr;

  attach->dmabuf = dmabuf;
  attach->importer_ops = const_cast<struct dma_buf_attach_ops *>(importer_ops);
  attach->priv = importer_priv;

  return attach;
}

void dma_buf_detach(struct dma_buf *dmabuf, struct dma_buf_attachment *attach)
{
  (void)dmabuf;
  if (!attach) return;
  attach->dmabuf = nullptr;
  std::free(attach);
}

/* ----- map / unmap ---------------------------------------------------- */

struct sg_table *
dma_buf_map_attachment(struct dma_buf_attachment *attach, int direction)
{
  (void)attach;
  (void)direction;

  struct sg_table *sg = (struct sg_table *)std::calloc(1, sizeof(*sg));
  if (!sg) return nullptr;

  sg->sgl = (struct scatterlist *)std::calloc(1, sizeof(*sg->sgl));
  if (!sg->sgl) { std::free(sg); return nullptr; }

  sg->nents = 1;
  sg->orig_nents = 1;
  return sg;
}

void dma_buf_unmap_attachment(struct dma_buf_attachment *attach,
                              struct sg_table *sg_table, int direction)
{
  (void)attach;
  (void)direction;
  if (!sg_table) return;
  std::free(sg_table->sgl);
  std::free(sg_table);
}

/* ----- pin / unpin ---------------------------------------------------- */

int dma_buf_pin(struct dma_buf_attachment *attach)
{
  (void)attach;
  return 0;  /* user-space: always succeeds */
}

void dma_buf_unpin(struct dma_buf_attachment *attach)
{
  (void)attach;
}

/* ----- get / put ------------------------------------------------------ */

static struct dma_buf g_dummy_buf;  /* single global for simulation */

struct dma_buf *dma_buf_get(int fd)
{
  (void)fd;
  std::memset(&g_dummy_buf, 0, sizeof(g_dummy_buf));
  return &g_dummy_buf;
}

void dma_buf_put(struct dma_buf *dmabuf)
{
  (void)dmabuf;
}

} /* extern "C" */