#ifndef DL_H
#define DL_H

#include <stdlib.h>

#define DL_HEAD(name) struct dl_head name = {&name, &name} 

#define container_of(ptr, type, member) \
	((type *) ((char *) ptr - offsetof(type, member)))

#define list_entry(ptr, type, member) container_of(ptr, type, member)

#define dl_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next); 

#define dl_for_each_entry(pos, head, member) \
	for ( \
		pos = list_entry((head)->next, typeof(*pos), member); \
		&pos->member != (head); \
		pos = list_entry(pos->member.next, typeof(*pos), member) \
	)

struct dl_head {
	struct dl_head *next;
	struct dl_head *prev;
};

static inline void dl_init(struct dl_head *head)
{
	head->next = head; 
	head->prev = head;
}

static inline void dl_add(struct dl_head *next, struct dl_head *prev, 
		struct dl_head *n)
{
	next->prev = n;
	n->next = next;
	n->prev = prev;
	prev->next = n;
}

static inline void dl_push_front(struct dl_head *head, struct dl_head *n) 
{
	dl_add(head->next, head, n);
}

static inline void dl_push_back(struct dl_head *head, struct dl_head *n)
{
	dl_add(head, head->prev, n);
}

static inline void dl_del(struct dl_head *next, struct dl_head *prev)
{
	next->prev = prev;
	prev->next = next;
}

static inline void dl_remove(struct dl_head *n) 
{
	dl_del(n->next, n->prev);	
	n->next = NULL;
	n->prev = NULL;
}

#endif
