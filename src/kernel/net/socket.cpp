#include <kernel/net/socket.h>
#include <kernel/net/sk_buff.h>

#include <linux_compat/types.h>
#include <cerrno>

#include <cstring>
#include <map>
#include <vector>

namespace {

struct socket_state {
    int bound;
    unsigned long addr;
    unsigned short port;
    std::vector<unsigned char> rx_buffer;
};

struct skb_meta {
    unsigned long src_addr;
    unsigned short src_port;
    struct sk_buff* skb;
};

std::map<int, socket_state> g_sockets;
std::map<int, std::vector<skb_meta>> g_rx_queues;
int g_next_fd = 1000;

int alloc_fd(void) { return g_next_fd++; }

int find_socket(int fd) {
  auto it = g_sockets.find(fd);
  return it != g_sockets.end() ? fd : -1;
}

}  // namespace

extern "C" {

int us_net_socket(int /*domain*/, int /*type*/, int /*protocol*/) {
  int fd = alloc_fd();
  if (fd < 0) return -12;  /* -ENOMEM */
  socket_state s{};
  s.bound = 0;
  s.addr = 0;
  s.port = 0;
  g_sockets[fd] = s;
  g_rx_queues[fd] = {};
  return fd;
}

int us_net_bind(int fd, unsigned long addr, unsigned short port) {
  if (find_socket(fd) < 0) return -9;  /* -EBADF */
  if (addr == 0 || port == 0) return -22;  /* -EINVAL */
  auto& s = g_sockets[fd];
  s.bound = 1;
  s.addr = addr;
  s.port = port;
  return 0;
}

int us_net_sendto(int fd, const void* buf, unsigned long len,
                  unsigned long dst_addr, unsigned short dst_port) {
  if (find_socket(fd) < 0) return -9;  /* -EBADF */
  if (!g_sockets[fd].bound) return -1;  /* -EDESTADDRREQ */
  if (!buf || len == 0) return -22;     /* -EINVAL */

  auto* ops = sk_buff_ops_get();
  struct sk_buff* skb = ops->alloc(len);
  if (!skb) return -12;  /* -ENOMEM */
  std::memcpy(ops->data(skb), buf, len);

  int dst_fd = -1;
  for (const auto& kv : g_sockets) {
    if (kv.second.bound && kv.second.addr == dst_addr &&
        kv.second.port == dst_port) {
      dst_fd = kv.first;
      break;
    }
  }
  if (dst_fd < 0) {
    ops->free(skb);
    return -1;  /* loopback: dst unreachable */
  }
  skb_meta m{};
  m.src_addr = g_sockets[fd].addr;
  m.src_port = g_sockets[fd].port;
  m.skb = skb;
  g_rx_queues[dst_fd].push_back(m);
  return (int)len;
}

int us_net_recvfrom(int fd, void* buf, unsigned long max_len,
                    unsigned long* src_addr, unsigned short* src_port) {
  if (find_socket(fd) < 0) return -9;  /* -EBADF */
  if (!buf || max_len == 0) return -22;
  auto& q = g_rx_queues[fd];
  if (q.empty()) return -11;  /* -EAGAIN: no data */
  skb_meta m = q.front();
  q.erase(q.begin());

  auto* ops = sk_buff_ops_get();
  unsigned long len = ops->len(m.skb);
  if (len > max_len) len = max_len;
  std::memcpy(buf, ops->data(m.skb), len);
  if (src_addr) *src_addr = m.src_addr;
  if (src_port) *src_port = m.src_port;
  ops->free(m.skb);
  return (int)len;
}

int us_net_close(int fd) {
  if (find_socket(fd) < 0) return -9;  /* -EBADF */
  g_sockets.erase(fd);
  auto it = g_rx_queues.find(fd);
  if (it != g_rx_queues.end()) {
    auto* ops = sk_buff_ops_get();
    for (auto& m : it->second) ops->free(m.skb);
    g_rx_queues.erase(it);
  }
  return 0;
}

unsigned long skb_to_addr(const struct sk_buff* /*skb*/) { return 0; }
unsigned short skb_to_port(const struct sk_buff* /*skb*/) { return 0; }

}  // extern "C"