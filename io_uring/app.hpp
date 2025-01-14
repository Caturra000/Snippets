#pragma once

/*
 * Library interface to io_uring
 */
struct SQ_ref {
	unsigned *p_head;
	unsigned *p_tail;
	unsigned *p_flags;
	unsigned *p_dropped;
	unsigned *p_array;
	struct io_uring_sqe *sqes;

	unsigned sqe_head;
	unsigned sqe_tail;

	size_t ring_sz;
	void *ring_ptr;

	unsigned ring_mask;
	unsigned ring_entries;
};

struct CQ_ref {
	unsigned *p_head;
	unsigned *p_tail;
	unsigned *p_flags;
	unsigned *p_overflow;
	struct io_uring_cqe *cqes;

	size_t ring_sz;
	void *ring_ptr;

	unsigned ring_mask;
	unsigned ring_entries;
};
