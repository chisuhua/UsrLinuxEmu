#ifndef USR_LINUX_EMU_NET_SK_BUFF_H
#define USR_LINUX_EMU_NET_SK_BUFF_H

#include <linux_compat/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sk_buff;

/* ops table (D1: sk_buff as opaque token per ADR-038) */
struct sk_buff_ops {
    struct sk_buff* (*alloc)(unsigned long data_len);
    void            (*free)(struct sk_buff*);
    void*           (*data)(const struct sk_buff*);
    unsigned long   (*len)(const struct sk_buff*);
    struct sk_buff* (*ref_up)(struct sk_buff*);
    struct sk_buff* (*ref_down)(struct sk_buff*);
};

const struct sk_buff_ops* sk_buff_ops_get(void);

unsigned long sk_buff_get_refcount(const struct sk_buff* skb);

#ifdef __cplusplus
}
#endif

#endif