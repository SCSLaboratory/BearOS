/* Public domain code from CCAN. */
#include <constants.h>
#include <kstdio.h>
#include <list.h>

static void *corrupt(const char *abortstr,
		     const struct list_node *head,
		     const struct list_node *node,
		     unsigned int count)
{
	if (abortstr) {
		kprintf("%s: prev corrupt in node %p (%u) of %p\n",
			abortstr, node, count, head);
		/* FIXME: Definitely need to provide some form of abort() at some point. */
#if 0
		abort();
#endif
	}
	return NULL;
}

struct list_node *list_check_node(const struct list_node *node,
				  const char *abortstr)
{
	const struct list_node *p, *n;
	int count = 0;

	for (p = node, n = node->next; n != node; p = n, n = n->next) {
		count++;
		if (n->prev != p)
			return corrupt(abortstr, node, n, count);
	}
	/* Check prev on head node. */
	if (node->prev != p)
		return corrupt(abortstr, node, node, 0);

	return (struct list_node *)node;
}

struct list_head *list_check(const struct list_head *h, const char *abortstr)
{
	if (!list_check_node(&h->n, abortstr))
		return NULL;
	return (struct list_head *)h;
}
