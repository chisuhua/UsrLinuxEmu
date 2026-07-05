#ifndef USR_LINUX_EMU_BLOCK_BIO_COMPAT_H
#define USR_LINUX_EMU_BLOCK_BIO_COMPAT_H

#include <linux_compat/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* L2 pass-through scope per Stage 2.3 plan: read/write/open/close.
 * No request queue, no bio layer (per plan §2.3.1 决策). */

int us_block_open(const char* path, unsigned long flags);
int us_block_close(int fd);
long us_block_read(int fd, void* buf, unsigned long count);
long us_block_write(int fd, const void* buf, unsigned long count);
long us_block_seek(int fd, long offset, int whence);
unsigned long us_block_size(int fd);
const char* us_block_path(int fd);

#ifdef __cplusplus
}
#endif

#endif