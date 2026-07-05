/*
 * net_driver.cpp - ② portable network driver code (per ADR-038)
 *
 * Per ADR-038 D1: ② calls sk_buff_ops_get() to access sk_buff
 * functionality, never directly touches sk_buff internal layout.
 * Portable to drivers/net/xxx/ (verification in 2.2.5).
 *
 * Provides struct net_device with ndo_open/stop/start_xmit ops.
 * Transmitted packets are forwarded to the sim layer (loopback NIC).
 */

#include <linux_compat/types.h>

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "kernel/net/sk_buff.h"

namespace usr_linux_emu {

class LoopbackNic;

struct NetDevice {
    std::string name;
    LoopbackNic* nic;
    bool opened;

    int (*ndo_open)(NetDevice*);
    int (*ndo_stop)(NetDevice*);
    int (*ndo_start_xmit)(NetDevice*, struct sk_buff*);
};

class LoopbackNic {
 public:
    int xmit(struct sk_buff* skb) {
        auto* ops = sk_buff_ops_get();
        const auto* data = static_cast<const unsigned char*>(ops->data(skb));
        captured_.emplace_back(data, data + ops->len(skb));
        ++tx_count_;
        return 0;
    }
    int recv(void* buf, unsigned long max_len) {
        if (captured_.empty()) return -11;  /* -EAGAIN */
        auto& p = captured_.front();
        unsigned long n = p.size() < max_len ? p.size() : max_len;
        std::memcpy(buf, p.data(), n);
        captured_.erase(captured_.begin());
        ++rx_count_;
        return static_cast<int>(n);
    }
    int tx_count() const { return tx_count_; }
    int rx_count() const { return rx_count_; }
    void reset() { tx_count_ = 0; rx_count_ = 0; captured_.clear(); }

 private:
    std::vector<std::vector<unsigned char>> captured_;
    int tx_count_ = 0;
    int rx_count_ = 0;
};

static int net_open(NetDevice* dev) {
    dev->opened = true;
    std::cout << "[NetDriver] ndo_open " << dev->name << "\n";
    return 0;
}

static int net_stop(NetDevice* dev) {
    dev->opened = false;
    std::cout << "[NetDriver] ndo_stop " << dev->name << "\n";
    return 0;
}

static int net_start_xmit(NetDevice* dev, struct sk_buff* skb) {
    if (!dev->opened) return -1;
    std::cout << "[NetDriver] ndo_start_xmit " << dev->name
              << " len=" << sk_buff_ops_get()->len(skb) << "\n";
    return dev->nic->xmit(skb);
}

extern "C" {

NetDevice* net_device_create(const char* name) {
    auto* dev = new NetDevice{};
    dev->name = name ? name : "net0";
    dev->nic = new LoopbackNic{};
    dev->opened = false;
    dev->ndo_open = net_open;
    dev->ndo_stop = net_stop;
    dev->ndo_start_xmit = net_start_xmit;
    return dev;
}

void net_device_destroy(NetDevice* dev) {
    if (!dev) return;
    delete dev->nic;
    delete dev;
}

int net_device_xmit(NetDevice* dev, struct sk_buff* skb) {
    if (!dev || !skb) return -22;
    return dev->ndo_start_xmit(dev, skb);
}

int net_device_recv(NetDevice* dev, void* buf, unsigned long max_len) {
    if (!dev || !buf) return -22;
    return dev->nic->recv(buf, max_len);
}

int net_device_nic_tx_count(NetDevice* dev) {
  return dev ? dev->nic->tx_count() : 0;
}

void net_device_nic_reset(NetDevice* dev) {
  if (dev) dev->nic->reset();
}

const char* net_device_get_name(NetDevice* dev) {
  return dev ? dev->name.c_str() : nullptr;
}

int net_device_open(NetDevice* dev) {
  if (!dev || !dev->ndo_open) return -22;
  return dev->ndo_open(dev);
}

int net_device_close(NetDevice* dev) {
  if (!dev || !dev->ndo_stop) return -22;
  return dev->ndo_stop(dev);
}

}  // extern "C"

}  // namespace usr_linux_emu