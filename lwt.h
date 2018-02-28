#define LWT_NULL NULL

#ifdef __GNUC__
#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)
#else
#define likely(x)       (x)
#define unlikely(x)     (x)
#endif

typedef struct __lwt_chan_t * lwt_chan_t;
typedef struct __lwt_cgrp_t * lwt_cgrp_t;
typedef struct __lwt_t *lwt_t;
typedef void *(*lwt_fn_t)(void *, lwt_chan_t);

typedef enum {
	/* Thread is runnable */
	LWT_RUNNABLE,
	/* Thread is blocked and not running */
	LWT_BLOCKED,
	/* Thread is dead and needs to be joined */
	LWT_DEAD,
	/* Thread is waiting to join. */
	LWT_WAITING
} lwt_state_t;

typedef enum {
	LWT_NO_FLAGS = 0b00,
	LWT_NO_JOIN = 0b01
} lwt_flags_t;

typedef enum {
	LWT_INFO_NTHD_RUNNABLE = LWT_RUNNABLE,
	LWT_INFO_NTHD_ZOMBIES = LWT_DEAD,
	LWT_INFO_NTHD_BLOCKED = LWT_BLOCKED
} lwt_info_t;

struct __lwt_t {
	void *eip;
	void **esp;
	void **ebp;
	/* Order of struct organization no longer matters
	   for lwt_asm.S */
	void *return_val;
	/* Pointer of originally malloced memory */
	void *memory;
	int id;
	lwt_flags_t flags;
	lwt_state_t state;
	struct __lwt_kthd *kthd;
	struct __lwt_t *parent;
	struct __lwt_t *previous;
	struct __lwt_t *next;
	/* Stack located below */
};

/* TODO: Will eventually overflow -- warning */
__thread unsigned int id_counter = 0;
/* Not static so the linker can link lwt_asm.S with this variable */
__thread struct __lwt_t *current = NULL;
/* TODO: Padding */

/* Linked lists of threads in different states */
__thread struct __lwt_t *lwt_runnable = NULL;
__thread struct __lwt_t *lwt_blocked = NULL;
__thread struct __lwt_t *lwt_dead = NULL;

/* Thread local identifier for running kthd */
__thread struct __lwt_kthd *current_kthd = NULL;

extern void __lwt_trampoline(void);

extern void __lwt_dispatch(struct __lwt_t *next, struct __lwt_t *old);

void __lwt_schedule(void)
{
	struct __lwt_t *next, *old;
	next = current->next;
	if (current->state == LWT_BLOCKED || current->state == LWT_WAITING) {	
		/* Remove from runnable queue.
		   We always should have someone in the runnable queue */
		if (current == lwt_runnable)
			lwt_runnable = current->next;
		current->previous->next = current->next;
		current->next->previous = current->previous;

		/* Append to the rear of the blocked queue */
		if (!lwt_blocked) {
			lwt_blocked = current;
			current->next = current;
			current->previous = current;
		} else {
			current->next = lwt_blocked;
			current->previous = lwt_blocked->previous;
			lwt_blocked->previous->next = current;
			lwt_blocked->previous = current;
		}
	} else if (current->state == LWT_DEAD) {
		/* Remove from runnable queue.
		   We always should have someone in the runnable queue */
		if (current == lwt_runnable)
			lwt_runnable = current->next;
		current->previous->next = current->next;
		current->next->previous = current->previous;

		/* Append to the rear of the dead queue */
		if (!lwt_dead) {
			lwt_dead = current;
			current->next = current;
			current->previous = current;
		} else {
			current->next = lwt_dead;
			current->previous = lwt_dead->previous;
			lwt_dead->previous->next = current;
			lwt_dead->previous = current;
		}
	}
	old = current;
	current = next;
	__lwt_dispatch(next, old);
}

void * lwt_join(struct __lwt_t *tcb)
{
	/* Check if we're its parent! */
	if (tcb->parent != current)
		return NULL;

	/* Already dead, so free and return */
	if (tcb->state == LWT_DEAD) {
		void *return_val = tcb->return_val;
		/* Remove thread from dead queue */
		if (tcb->next == tcb) {
			lwt_dead = NULL;
		} else {
			if (tcb == lwt_dead)
				lwt_dead = tcb->next;
			tcb->previous->next = tcb->next;
			tcb->next->previous = tcb->previous;
		}	
		/* tcb was a manufactured address */
		free(tcb->memory);

		return return_val;
	}

	current->state = LWT_WAITING;
	__lwt_schedule();
	/* If we get put back on the run queue, then our child
	   has joined.  */
	return lwt_join(tcb);
}

void __lwt_garbage_collect(void)
{
	if (!lwt_dead)
		return;
	/* This is where we will do garabage collection of threads
	   who died with the NO_JOIN flag. */
	if (lwt_dead->next != lwt_dead) {
		struct __lwt_t *cursor = lwt_dead;
		do {
			if (cursor->flags & LWT_NO_JOIN) {
				struct __lwt_t *old = cursor;
				/* Remove from list */
				if (cursor == lwt_dead)
					lwt_dead = cursor->next;
				cursor->previous->next = cursor->next;
				cursor->next->previous = cursor->previous;
					
				cursor = cursor->next;
				/* Free */
				free(old->memory);
			} else
				cursor = cursor->next;
		} while (cursor->next != lwt_dead);
	} else if (lwt_dead->flags & LWT_NO_JOIN) {
		free(lwt_dead->memory);
		lwt_dead = NULL;
	}
}

void lwt_die(void *data)
{
	struct __lwt_t *parent = current->parent;
	/* Garbage collect other threads who will never join and are dead */
	__lwt_garbage_collect();

	/* Parent may be blocked indefinitely waiting to join, put
	   the parent back on the run queue. */
	if (!(current->flags & LWT_NO_JOIN) && parent->state == LWT_WAITING) {
		parent->state = LWT_RUNNABLE;
		/* Remove from blocked queue. */
		if (parent->next == parent) {
			lwt_blocked = NULL;
		} else {
			if (parent == lwt_blocked)
				lwt_blocked = parent->next;
			parent->previous->next = parent->next;
			parent->next->previous = parent->previous;
		}
		/* Append to the rear of the runnable queue.  We should always
		   have someone in the runnable queue. */
	       	parent->next = lwt_runnable;
		parent->previous = lwt_runnable->previous;
		lwt_runnable->previous->next = parent;
		lwt_runnable->previous = parent;
	}

	current->return_val = data;
	current->state = LWT_DEAD;
	/* Current thread is placed into dead queue in schedule */
	__lwt_schedule();
}

void __lwt_init(void)
{
	/* Parent already has a stack. */
	struct __lwt_t *tcb_parent = malloc(sizeof(struct __lwt_t));
	tcb_parent->eip = NULL;
	tcb_parent->esp = NULL;
	tcb_parent->ebp = NULL;
	tcb_parent->state = LWT_RUNNABLE;
	/* TODO: Integer overflow -- warning */
	tcb_parent->id = id_counter++;
	tcb_parent->return_val = NULL;
	tcb_parent->memory = NULL;
	tcb_parent->flags = LWT_NO_FLAGS;
	tcb_parent->kthd = current_kthd;
	tcb_parent->parent = NULL;
	/* Circular Linked List */
	tcb_parent->previous = tcb_parent;
	tcb_parent->next = tcb_parent;
	lwt_runnable = tcb_parent;
	lwt_blocked = NULL;
	lwt_dead = NULL;
	/* Current thread running */
	current = tcb_parent;
}

lwt_t lwt_create(lwt_fn_t fn, void *data, lwt_flags_t flags, lwt_chan_t c)
{
	struct __lwt_t *tcb;
	void *stack, *memory;
	int i;

	memory = malloc(4096);
	/* This address is the start of the thread control block.
	   The stack starts at one word less.  Stack grows down,
	   while thread control block grows up. */
	tcb = memory + 4096 - sizeof(struct __lwt_t);
	stack = tcb - sizeof(void *);

	/* If this is the first thread we're creating, we need
	   to create a thread control block, tcb, for the parent. */
	if (!lwt_runnable) {
		__lwt_init();
	}

	/* Offset to write the parameters */
	tcb->esp = stack - 3 * sizeof(void *);
	/* Write the two parameters in */
	tcb->esp[0] = fn;
	tcb->esp[1] = data;
	tcb->esp[2] = c;

	tcb->ebp = stack;
	tcb->eip = __lwt_trampoline;
	tcb->state = LWT_RUNNABLE;
	tcb->id = id_counter++;
	tcb->return_val = NULL;
	tcb->memory = memory;
	tcb->flags = flags;
	tcb->kthd = current_kthd;
	tcb->parent = current;

	/* Circular linked list */
	tcb->previous = lwt_runnable->previous;
	tcb->next = lwt_runnable;
	lwt_runnable->previous->next = tcb;
	lwt_runnable->previous = tcb;

	return (lwt_t) tcb;
}

void lwt_yield(lwt_t tcb)
{
	if (tcb == LWT_NULL) {
		__lwt_schedule();
	} else {
		/* TODO: Directed Yields. Removed for ease of KTHD
		   implementation */
		__lwt_schedule();
	}
}

lwt_t lwt_current(void)
{
	return (lwt_t) current;
}

int lwt_id(lwt_t tcb)
{
	return ((struct __lwt_t *)tcb)->id;
}

void __lwt_start(lwt_fn_t fn, void *data, lwt_chan_t c)
{
	void * return_val;
	return_val = fn(data, c);
	lwt_die(return_val);
}

int lwt_info(lwt_info_t flag)
{
	struct __lwt_t *cursor;
	int return_val = 0;

	switch (flag) {
	case LWT_INFO_NTHD_RUNNABLE:
		return_val = 0;
		cursor = lwt_runnable;
		if (cursor) {
			do {
				return_val++;
				cursor = cursor->next;
			} while (cursor != lwt_runnable);
		}
		break;
	case LWT_INFO_NTHD_ZOMBIES:
		return_val = 0;
		cursor = lwt_dead;
		if (cursor) {
			do {
				return_val++;
				cursor = cursor->next;
			} while (cursor != lwt_dead);
		}
		break;
	case LWT_INFO_NTHD_BLOCKED:
		return_val = 0;
		cursor = lwt_blocked;
		if (cursor) {
			do {
				return_val++;
				cursor = cursor->next;
			} while (cursor != lwt_blocked);
		}
		break;
	}
	return return_val;
}
