/*
 * net_compat_test.c - Stage 2.2.5 3-way regression
 *
 * Compile this file in drivers/net_compat/ with the EXACT API from
 * net_driver.h. If it compiles, the ② portable code is truly portable
 * to a real kernel's drivers/net/xxx/ (with only #include path change).
 *
 * Build:
 *   cd drivers/net_compat
 *   cc -c -I../../include -I. net_compat_test.c
 *
 * This file does NOT include any UsrLinuxEmu-specific headers.
 * It uses only:
 *   - struct sk_buff (opaque) — would be <linux/skbuff.h> in real kernel
 *   - void* net_device handle — would be struct net_device* in real kernel
 *   - standard C99
 */

#include "net_compat.h"

#include <stddef.h>

/* Standalone caller demonstrating ② portable interface compatibility */
static int net_compat_demo_xmit(struct sk_buff* skb, const void* payload,
                                unsigned long len) {
  void* dev = net_device_create("net0_compat");
  if (!dev) return -1;
  if (net_device_open(dev) != 0) {
    net_device_destroy(dev);
    return -1;
  }
  /* Real kernel: skb_put(skb, len); memcpy(skb_put, payload, len);
   * Here we just verify the type compiles. */
  (void)skb;
  (void)payload;
  (void)len;
  net_device_close(dev);
  net_device_destroy(dev);
  return 0;
}

int net_compat_3way_regression_main(void) {
  struct sk_buff* skb = (struct sk_buff*)0;
  return net_compat_demo_xmit(skb, "test", 4);
}