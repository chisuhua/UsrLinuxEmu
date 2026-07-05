#include <kernel/net/sk_buff.h>

#include <cstdlib>
#include <cstring>

namespace {

struct sk_buff_impl {
    unsigned long data_len;
    unsigned long refcount;
    unsigned char payload[];
};

sk_buff_impl* impl_of(const struct sk_buff* skb) {
  return reinterpret_cast<sk_buff_impl*>(
      reinterpret_cast<char*>(const_cast<struct sk_buff*>(skb)) -
      offsetof(sk_buff_impl, payload));
}

struct sk_buff* skb_alloc_impl(unsigned long data_len) {
  if (data_len == 0) return nullptr;
  void* mem = std::malloc(sizeof(sk_buff_impl) + data_len);
  if (!mem) return nullptr;
  auto* impl = new (mem) sk_buff_impl{};
  impl->data_len = data_len;
  impl->refcount = 1;
  std::memset(impl->payload, 0, data_len);
  return reinterpret_cast<struct sk_buff*>(impl->payload);
}

void skb_free_impl(struct sk_buff* skb) {
  if (!skb) return;
  auto* impl = impl_of(skb);
  if (--impl->refcount == 0) {
    impl->~sk_buff_impl();
    std::free(impl);
  }
}

void* skb_data_impl(const struct sk_buff* skb) {
  return const_cast<void*>(static_cast<const void*>(skb));
}

unsigned long skb_len_impl(const struct sk_buff* skb) {
  return skb ? impl_of(skb)->data_len : 0;
}

struct sk_buff* skb_ref_up_impl(struct sk_buff* skb) {
  if (!skb) return nullptr;
  impl_of(skb)->refcount++;
  return skb;
}

struct sk_buff* skb_ref_down_impl(struct sk_buff* skb) {
  if (!skb) return nullptr;
  if (impl_of(skb)->refcount <= 1) {
    skb_free_impl(skb);
    return nullptr;
  }
  impl_of(skb)->refcount--;
  return skb;
}

const struct sk_buff_ops g_ops = {
  .alloc   = skb_alloc_impl,
  .free    = skb_free_impl,
  .data    = skb_data_impl,
  .len     = skb_len_impl,
  .ref_up  = skb_ref_up_impl,
  .ref_down = skb_ref_down_impl,
};

}  // namespace

extern "C" {

const struct sk_buff_ops* sk_buff_ops_get(void) { return &g_ops; }

unsigned long sk_buff_get_refcount(const struct sk_buff* skb) {
  return skb ? impl_of(skb)->refcount : 0;
}

}  // extern "C"