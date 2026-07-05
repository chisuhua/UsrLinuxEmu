/*
 * storage_driver.h - ② portable block storage driver public interface
 */

#pragma once

#include <linux_compat/types.h>

#ifdef __cplusplus
extern "C" {
#endif

void*       block_device_create(const char* name);
void        block_device_destroy(void* dev);
int         block_device_open(void* dev);
int         block_device_close(void* dev);
long        block_device_read(void* dev, void* buf, unsigned long count);
long        block_device_write(void* dev, const void* buf, unsigned long count);
unsigned long block_device_size(void* dev);
const char* block_device_get_name(void* dev);

#ifdef __cplusplus
}
#endif