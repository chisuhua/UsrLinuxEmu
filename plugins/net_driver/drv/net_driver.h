/*
 * net_driver.h - ② portable net driver public interface
 */

#pragma once

#include <linux_compat/types.h>

#ifdef __cplusplus
extern "C" {
#endif

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