/* Anthony Korzan
 * akorzan@gwu.edu
 * October 4, 2012 */

#define BUFFER_LENGTH 128

struct ring {
	int head;
	int tail;
	void *data[BUFFER_LENGTH];
} ring;

/* Allocates and returns a pointer to a ring buffer. If unable
   to allocate returns NULL. */
struct ring* rb_create(void) {
	struct ring* rb = malloc(sizeof(struct ring));
	/* Check malloc */
	if (rb) {
		/* Initialize */
		rb->head = 0;
		rb->tail = 1;
	}
	return rb;
}

/* Deallocates an entire ring buffer. */
void rb_delete(struct ring *rb) {
	free(rb);
}

/* Returns 0 if not empty and -1 if empty. */
int rb_isempty(struct ring *rb) {
	if (rb->tail == (rb->head + 1) % BUFFER_LENGTH) {
		return -1;
	} else {
		return 0;
	}
}

/* Returns 0 if not full and -1 if full. */
int rb_isfull(struct ring *rb) {
	if (rb->tail == rb->head) {
		return -1;
	} else {
		return 0;
	}
}

/* If the ring buer is non-full, this function inserts a value at tail-1, and advances the tail.
   Returns -1 (logical true) if successful.
   Returns 0 if NOT successful.
*/
int rb_enqueue(struct ring *rb, void *value) {
	/* Check if full. */
	if(rb_isfull(rb)) {
		return 0;
	} else {
		rb->data[(rb->tail - 1 + BUFFER_LENGTH) % BUFFER_LENGTH] = value;
		rb->tail = (rb->tail + 1) % BUFFER_LENGTH;
		return -1;
	}
}

/* Removes a value and advances a head.  Returns a void*.
   If empty, returns NULL */
void * rb_dequeue(struct ring *rb) {
	void *value;
	if(rb_isempty(rb)) {
		return NULL;
	}
	value = rb->data[rb->head];
	rb->head = (rb->head + 1) % BUFFER_LENGTH;
	return value;	
}
