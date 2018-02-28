#include "lwt_ring.h"
#include "lwt_ll.h"

/* Setting to non-zero displays print statements */
#define DEBUG 0

struct __lwt_chan_t {
	unsigned int snd_cnt; /* Number of references to this channel */
	struct __lwt_cgrp_t *group; /* The group this channel belongs to.
				       NULL if it does not belong to a group.
				       Can only belong to one group. */
	/* A counter to keep track of msgs since adding to
	   the group since the channel could already have messages
	   waiting in the queue when it is added to the group. */
	unsigned int grp_events;
	/* Receiver’s data */
	struct ring *rcv_msgs;
	void *mark_data;  /* Arbitrary value receiver can store */
	void *chan_data;  /* Used for passing channels */
	int rcv_blocked;  /* If the receiver is blocked (true=-1) */
	struct __lwt_t *rcv_thread;  /* The receiver */
};

struct __lwt_cgrp_t {
	struct lwt_ll *ready;
	unsigned int count;
};

/* TODO: size... make rb of variable size */
lwt_chan_t lwt_chan(unsigned int size)
{
	struct __lwt_chan_t *c = malloc(sizeof(struct __lwt_chan_t));

	/* Current may still be NULL as the tcb for this thread has not 
	   been set up yet.  This will happen if chan creation occurs before
	   the first lwt creation. */
	if (!lwt_runnable) {
		__lwt_init();
	}

	c->snd_cnt = 1;
	c->group = NULL;
	c->grp_events = 0;

	c->rcv_msgs = rb_create();
	c->mark_data = NULL;
	c->chan_data = NULL;
	c->rcv_blocked = 0;
	c->rcv_thread = current;
	return c;
}

/* De-references the channel.  If channel has no senders or
   receivers, then free. */
void lwt_chan_deref(lwt_chan_t c)
{
	/* Check if we're the receiver */
	if (c->rcv_thread == current)
		c->rcv_thread = NULL;

	c->snd_cnt--;
	if (c->snd_cnt == 0) {
		rb_delete(c->rcv_msgs);
		free(c);
	}
}

int lwt_snd(lwt_chan_t c, void *data)
{
	assert(data);

	/* Check if receiver has exited */
	if (!c->rcv_thread)
		return -1;


	/* If we have a group, do our feature creep */
	if (c->group) {
		if (c->grp_events == 0)
			lwt_ll_enqueue(c->group->ready, c);
		c->grp_events++;
	}


	while (!rb_enqueue(c->rcv_msgs, data))
		lwt_yield(NULL);

	return 0;
}

void * lwt_rcv(lwt_chan_t c)
{
	void *ret;
	while (!(ret = rb_dequeue(c->rcv_msgs)))
		lwt_yield(NULL);
	/* If we have a group, do our feature creep */
	if (c->group) {
		c->grp_events--;
		if (c->grp_events == 0)
			lwt_ll_remove(c->group->ready, c);
#if DEBUG
		printf("lwt_rcv: ret=%d; c->grp_events=%d\n",ret,c->grp_events);
#endif
	}
	return ret;
}

void lwt_snd_chan(lwt_chan_t c, lwt_chan_t sending)
{
	c->chan_data = (void *) sending;

	/* Check if receiver read data */
	while (c->chan_data)
		lwt_yield(NULL);
}

lwt_chan_t lwt_rcv_chan(lwt_chan_t c)
{
	struct __lwt_chan_t *ret;

	while(!(ret = c->chan_data)) {
		lwt_yield(NULL);
	}

	c->chan_data = NULL;
	ret->snd_cnt++;
	return ret;
}

lwt_cgrp_t lwt_cgrp(void)
{
	struct __lwt_cgrp_t *group = malloc(sizeof(struct __lwt_cgrp_t));
	if (unlikely(!group))
		return LWT_NULL;

	group->ready = lwt_ll_create();
	group->count = 0;
	if (unlikely(!group->ready)) {
		free(group);
		return LWT_NULL;
	}
	return group;
}

int lwt_cgrp_free(lwt_cgrp_t group)
{
#if DEBUG
	printf("lwt_cgrp_free: group->count=%d\n", group->count);
#endif
	if (unlikely(group->count > 0)) {
		return 1;
	}
	lwt_ll_free(group->ready);
	free(group);
	return 0;
}

int lwt_cgrp_add(lwt_cgrp_t group, lwt_chan_t c)
{
	if (unlikely(c->group))
		return -1;
	group->count++;
	c->group = group;
	c->grp_events = 0;
	return 0;
}

int lwt_cgrp_rem(lwt_cgrp_t group, lwt_chan_t c)
{
#if DEBUG
	/* Debug */
	printf("lwt_cgrp_ rem: group->count=%d; c->grp_events=%d\n",group->count, c->grp_events);
#endif
#if 0
	/* Unimplemented Functionality */
	if (c->grp_events > 0)
		return 1;
#endif
	c->group = NULL;
	group->count--;
	return 0;
}

lwt_chan_t lwt_cgrp_wait(lwt_cgrp_t group)
{
	struct __lwt_chan_t *ret;
	while ( !(ret = lwt_ll_dequeue(group->ready)) ) {
		lwt_yield(NULL);
	}
	/* grp_events > 0 */
	if (ret->grp_events) {
		/* requeue at the end */
		lwt_ll_enqueue(group->ready, ret);
	}
	return ret;
}

void lwt_chan_mark_set(lwt_chan_t c, void *data)
{
	c->mark_data = data;
}

void *lwt_chan_mark_get(lwt_chan_t c)
{
	return c->mark_data;
}
