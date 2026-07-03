#pragma once
// Stage 1.2 PoC: minimal Linux list.h stub

struct list_head {
  struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)
#define INIT_LIST_HEAD(ptr) do { (ptr)->next = (ptr); (ptr)->prev = (ptr); } while (0)

#define list_entry(ptr, type, member) container_of(ptr, type, member)
#define list_first_entry(ptr, type, member) list_entry((ptr)->next, type, member)
#define list_next_entry(pos, member) list_entry((pos)->member.next, typeof(*(pos)), member)

#define container_of(ptr, type, member) \
  ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

#define list_for_each_entry(pos, head, member) \
  for (pos = list_entry((head)->next, typeof(*pos), member); \
       &pos->member != (head); \
       pos = list_entry(pos->member.next, typeof(*pos), member))

#define list_add(new, head) do { \
  (new)->next = (head)->next; \
  (new)->prev = (head); \
  (head)->next->prev = (new); \
  (head)->next = (new); \
} while (0)

#define list_del(entry) do { \
  (entry)->next->prev = (entry)->prev; \
  (entry)->prev->next = (entry)->next; \
} while (0)

#define list_empty(head) ((head)->next == (head))