/*
 * net_compat.h - Stage 2.2.5 3-way regression proof
 *
 * Per ADR-038 D1: ② portable driver code is C-linkage clean and
 * does not depend on UsrLinuxEmu-specific C++ infrastructure. This
 * file is the EXACT same API surface as plugins/net_driver/drv/net_driver.h
 * but placed under drivers/ to simulate "copying to drivers/net/xxx/".
 *
 * The only difference: the sk_buff forward reference (since real kernel
 * would include <linux/skbuff.h> instead of our opaque token shim).
 * Here we keep the identical struct sk_buff (opaque) so the same
 * source compiles unchanged.
 *
 * Compile test: this file must compile standalone in drivers/net_compat/
 * without any UsrLinuxEmu-specific path (proving 3-way separation).
 */

#pragma once

#include <linux_compat/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sk_buff;

void* net_device_create(const char* name);
void  net_device_destroy(void* dev);
int   net_device_open(void* dev);
int   net_device_close(void* dev);
int   net_device_xmit(void* dev, struct sk_buff* skb);
int   net_device_recv(void* dev, void* buf, unsigned long max_len);
int   net_device_nic_tx_count(void* dev);
void  net_device_nic_reset(void* dev);
const char* net_device_get_name(void* dev);

#ifdef __cplusplus
}
#endif