struct __lwt_kthd_params {
	void *data;
	lwt_chan_t c;
	lwt_fn_t fn;
	struct __lwt_kthd *kthd;
};

struct __lwt_kthd {
	/* A boolean to mark weather this kthread is being used */
	int alive;
	pthread_t pthread;
	pthread_attr_t attr;
};

void __lwt_kthd_start(struct __lwt_kthd_params *p)
{
	lwt_t lwt;
	current_kthd = p->kthd;
	lwt = lwt_create(p->fn, p->data, LWT_NO_FLAGS, p->c);
	free(p);
	/* Block ourselfs until our child is dead */
	lwt_join(lwt);
	/* Once dead, there could be grandchildren still alive.
	 We need to block ourselfs until they die. */
	/* TODO: Garbage collection of kthd */
	current_kthd->alive = 0;
}

int lwt_kthd_create(lwt_fn_t fn, void *data, lwt_chan_t c)
{
	struct __lwt_kthd *kthd = malloc(sizeof(struct __lwt_kthd));
	struct __lwt_kthd_params *p = malloc(sizeof(struct __lwt_kthd_params));

	/* If this is the first thread we're creating, we need
	   to create a thread control block, tcb, for the parent. */
	if (!lwt_runnable) {
		__lwt_init();
	}

	/* Start setting up kthd */
	kthd->alive = -1;
	pthread_attr_init(&kthd->attr);
	pthread_attr_setdetachstate(&kthd->attr, PTHREAD_CREATE_DETACHED);

	p->fn = fn;
	p->data = data;
	p->c = c;
	p->kthd = kthd;

	/* Start kthd */
	if(unlikely(pthread_create( &kthd->pthread,
				    &kthd->attr,
				    (void * (*)(void *)) __lwt_kthd_start,
				    (void *) p)) ) {
		return -1;
	}
	return 0;
}

void __lwt_kthd_free(struct __lwt_kthd_t *kthd)
{
	/* Close pthread */
	/* Free lwt */
	/* Free kthd */
}
