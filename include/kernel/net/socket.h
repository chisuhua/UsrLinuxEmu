#ifndef USR_LINUX_EMU_NET_SOCKET_H
#define USR_LINUX_EMU_NET_SOCKET_H

#include <linux_compat/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* L2 直通 scope per ADR-038 D2: socket/bind/sendto/recvfrom/close.
 * No TCP/IP protocol stack simulated. */

int us_net_socket(int domain, int type, int protocol);
int us_net_bind(int fd, unsigned long addr, unsigned short port);
int us_net_sendto(int fd, const void* buf, unsigned long len,
                  unsigned long dst_addr, unsigned short dst_port);
int us_net_recvfrom(int fd, void* buf, unsigned long max_len,
                    unsigned long* src_addr, unsigned short* src_port);
int us_net_close(int fd);

unsigned long skb_to_addr(const struct sk_buff* skb);
unsigned short skb_to_port(const struct sk_buff* skb);

#ifdef __cplusplus
}
#endif

#endif