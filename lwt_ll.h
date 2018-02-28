struct lwt_ll {
	struct __ll_node *head;
	struct __ll_node *tail;
	unsigned int count;
};

/* Doubly linked node */
struct __ll_node {
	/* Value */
	void *data;
	/* Which thread does this belong to? */
	struct __lwt_t *tcb;
	struct __ll_node *prev;
	struct __ll_node *next;
};

struct lwt_ll * lwt_ll_create() {
	struct lwt_ll *ll;
	ll = malloc(sizeof(struct lwt_ll));
	if (!ll)
		return NULL;
	ll->head = NULL;
	ll->tail = NULL;
	ll->count = 0;
	return ll;
}

/* Data is the value. */
int lwt_ll_enqueue(struct lwt_ll *ll, void *data) {
	struct __ll_node *new_node;
	new_node = malloc(sizeof(struct __ll_node));
	if (!new_node)
		return -1;

	/* Will be added to the end of the list */
	new_node->next = NULL;
	new_node->data = data;
	/* If the head is empty, the list is empty */
	if (!ll->head) {
		new_node->prev = NULL;
		ll->head = new_node;
		ll->tail = new_node;
	} else {
		/* Add to the end of the list */
		ll->tail->next = new_node;
		new_node->prev = ll->tail;
		ll->tail = new_node;
	}
	ll->count++;
	return 0;
}

/* Returns the value at the head of the list. FIFO */
void * lwt_ll_dequeue(struct lwt_ll *ll)
{
	void *ret;
	struct __ll_node *n = ll->head;

	if (!n) {
		return NULL;
	} else if (!n->next) {
		ll->head = NULL;
		ll->tail = NULL;
		ret = n->data;
		free(n);
	} else {
		ll->head = n->next;
		ll->head->prev = NULL;
		ret = n->data;
		free(n);
	}
	ll->count--;
	return ret;
}

/* Scans order N through the list for the data and removes. */
void * lwt_ll_remove(struct lwt_ll *ll, void *data)
{
	void *ret;
	struct __ll_node *cursor = ll->head;
	while (cursor) {
		if (cursor->data == data) {
			break;
		}
		cursor = cursor->next;
	}
	if (!cursor) {
		/* If we didn't find anything, don't do anything */
		return NULL;
	} else if (cursor == ll->head && cursor == ll->tail) {
		ll->head = NULL;
		ll->tail = NULL;
	} else if (cursor == ll->head) {
		ll->head = cursor->next;
		cursor->next->prev = NULL;
	} else if (cursor == ll->tail) {
		ll->tail = cursor->prev;
		cursor->prev->next = NULL;
	} else {
		cursor->prev->next = cursor->next;
		cursor->next->prev = cursor->prev;
	}
	ll->count--;
	ret = cursor->data; /* Not very necessary */
	free(cursor);
	return ret;
}

void lwt_ll_free(struct lwt_ll *ll)
{
	struct __ll_node *cursor = ll->head;
	while (cursor) {
		struct __ll_node *to_free = cursor;
		cursor = cursor->next;
		free(to_free);
	}
	free(ll);
}

/* Print the contents of the list.  For debug. */
void __lwt_ll_print(struct lwt_ll *ll)
{
	struct __ll_node *cursor = ll->head;
	printf("l=%d : ", ll->count);
	while (cursor) {
		/* Possibility of printing addresses -- bad */
		printf("%x, ", (unsigned int) cursor->data);
		cursor = cursor->next;
	}
	printf("\n");
}
