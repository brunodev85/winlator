/*
   Samba Unix SMB/CIFS implementation.

   Samba trivial allocation library - new interface

   NOTE: Please read talloc_guide.txt for full documentation

   Copyright (C) Andrew Tridgell 2004
   Copyright (C) Stefan Metzmacher 2006

     ** NOTE! The following LGPL license applies to the talloc
     ** library. This does NOT imply that all of Samba is released
     ** under the LGPL

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/

/*
  inspired by http://swapped.cc/halloc/
*/

#include "talloc.h"

#ifdef HAVE_SYS_AUXV_H
#include <sys/auxv.h>
#endif

/* Special macros that are no-ops except when run under Valgrind on
 * x86.  They've moved a little bit from valgrind 1.0.4 to 1.9.4 */
#ifdef HAVE_VALGRIND_MEMCHECK_H
        /* memcheck.h includes valgrind.h */
#include <valgrind/memcheck.h>
#elif defined(HAVE_VALGRIND_H)
#include <valgrind.h>
#endif

/* use this to force every realloc to change the pointer, to stress test
   code that might not cope */
#define ALWAYS_REALLOC 0

#define TALLOC_BUILD_VERSION_MINOR 1
#define TALLOC_BUILD_VERSION_MAJOR 2
#define TALLOC_BUILD_VERSION_RELEASE 14

#define _PUBLIC_ __attribute__((visibility("default")))

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#define MAX_TALLOC_SIZE 0x10000000

#define TALLOC_FLAG_FREE 0x01
#define TALLOC_FLAG_LOOP 0x02
#define TALLOC_FLAG_POOL 0x04		/* This is a talloc pool */
#define TALLOC_FLAG_POOLMEM 0x08	/* This is allocated in a pool */

/*
 * Bits above this are random, used to make it harder to fake talloc
 * headers during an attack.  Try not to change this without good reason.
 */
#define TALLOC_FLAG_MASK 0x0F

#define TALLOC_MAGIC_REFERENCE ((const char *)1)

#define TALLOC_MAGIC_BASE 0xe814ec70
#define TALLOC_MAGIC_NON_RANDOM ( \
	~TALLOC_FLAG_MASK & ( \
		TALLOC_MAGIC_BASE + \
		(TALLOC_BUILD_VERSION_MAJOR << 24) + \
		(TALLOC_BUILD_VERSION_MINOR << 16) + \
		(TALLOC_BUILD_VERSION_RELEASE << 8)))
static unsigned int talloc_magic = TALLOC_MAGIC_NON_RANDOM;

/* by default we abort when given a bad pointer (such as when talloc_free() is called
   on a pointer that came from malloc() */
#ifndef TALLOC_ABORT
#define TALLOC_ABORT(reason) abort()
#endif

#ifndef discard_const_p
#if defined(__intptr_t_defined) || defined(HAVE_INTPTR_T)
# define discard_const_p(type, ptr) ((type *)((intptr_t)(ptr)))
#else
# define discard_const_p(type, ptr) ((type *)(ptr))
#endif
#endif

/* these macros gain us a few percent of speed on gcc */
#if (__GNUC__ >= 3)
/* the strange !! is to ensure that __builtin_expect() takes either 0 or 1
   as its first argument */
#ifndef likely
#define likely(x)   __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x), 0)
#endif
#else
#ifndef likely
#define likely(x) (x)
#endif
#ifndef unlikely
#define unlikely(x) (x)
#endif
#endif

/* this null_context is only used if talloc_enable_leak_report() or
   talloc_enable_leak_report_full() is called, otherwise it remains
   NULL
*/
static void *null_context;
static bool talloc_report_null;
static bool talloc_report_null_full;
static void *autofree_context;

static void talloc_setup_atexit(void);

/* used to enable fill of memory on free, which can be useful for
 * catching use after free errors when valgrind is too slow
 */
static struct {
	bool initialised;
	bool enabled;
	uint8_t fill_value;
} talloc_fill;

#define TALLOC_FILL_ENV "TALLOC_FREE_FILL"

/*
 * do not wipe the header, to allow the
 * double-free logic to still work
 */
#define TC_INVALIDATE_FULL_FILL_CHUNK(_tc) do { \
	if (unlikely(talloc_fill.enabled)) { \
		size_t _flen = (_tc)->size; \
		char *_fptr = (char *)TC_PTR_FROM_CHUNK(_tc); \
		memset(_fptr, talloc_fill.fill_value, _flen); \
	} \
} while (0)

#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_NOACCESS)
/* Mark the whole chunk as not accessable */
#define TC_INVALIDATE_FULL_VALGRIND_CHUNK(_tc) do { \
	size_t _flen = TC_HDR_SIZE + (_tc)->size; \
	char *_fptr = (char *)(_tc); \
	VALGRIND_MAKE_MEM_NOACCESS(_fptr, _flen); \
} while(0)
#else
#define TC_INVALIDATE_FULL_VALGRIND_CHUNK(_tc) do { } while (0)
#endif

#define TC_INVALIDATE_FULL_CHUNK(_tc) do { \
	TC_INVALIDATE_FULL_FILL_CHUNK(_tc); \
	TC_INVALIDATE_FULL_VALGRIND_CHUNK(_tc); \
} while (0)

#define TC_INVALIDATE_SHRINK_FILL_CHUNK(_tc, _new_size) do { \
	if (unlikely(talloc_fill.enabled)) { \
		size_t _flen = (_tc)->size - (_new_size); \
		char *_fptr = (char *)TC_PTR_FROM_CHUNK(_tc); \
		_fptr += (_new_size); \
		memset(_fptr, talloc_fill.fill_value, _flen); \
	} \
} while (0)

#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_NOACCESS)
/* Mark the unused bytes not accessable */
#define TC_INVALIDATE_SHRINK_VALGRIND_CHUNK(_tc, _new_size) do { \
	size_t _flen = (_tc)->size - (_new_size); \
	char *_fptr = (char *)TC_PTR_FROM_CHUNK(_tc); \
	_fptr += (_new_size); \
	VALGRIND_MAKE_MEM_NOACCESS(_fptr, _flen); \
} while (0)
#else
#define TC_INVALIDATE_SHRINK_VALGRIND_CHUNK(_tc, _new_size) do { } while (0)
#endif

#define TC_INVALIDATE_SHRINK_CHUNK(_tc, _new_size) do { \
	TC_INVALIDATE_SHRINK_FILL_CHUNK(_tc, _new_size); \
	TC_INVALIDATE_SHRINK_VALGRIND_CHUNK(_tc, _new_size); \
} while (0)

#define TC_UNDEFINE_SHRINK_FILL_CHUNK(_tc, _new_size) do { \
	if (unlikely(talloc_fill.enabled)) { \
		size_t _flen = (_tc)->size - (_new_size); \
		char *_fptr = (char *)TC_PTR_FROM_CHUNK(_tc); \
		_fptr += (_new_size); \
		memset(_fptr, talloc_fill.fill_value, _flen); \
	} \
} while (0)

#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_UNDEFINED)
/* Mark the unused bytes as undefined */
#define TC_UNDEFINE_SHRINK_VALGRIND_CHUNK(_tc, _new_size) do { \
	size_t _flen = (_tc)->size - (_new_size); \
	char *_fptr = (char *)TC_PTR_FROM_CHUNK(_tc); \
	_fptr += (_new_size); \
	VALGRIND_MAKE_MEM_UNDEFINED(_fptr, _flen); \
} while (0)
#else
#define TC_UNDEFINE_SHRINK_VALGRIND_CHUNK(_tc, _new_size) do { } while (0)
#endif

#define TC_UNDEFINE_SHRINK_CHUNK(_tc, _new_size) do { \
	TC_UNDEFINE_SHRINK_FILL_CHUNK(_tc, _new_size); \
	TC_UNDEFINE_SHRINK_VALGRIND_CHUNK(_tc, _new_size); \
} while (0)

#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_UNDEFINED)
/* Mark the new bytes as undefined */
#define TC_UNDEFINE_GROW_VALGRIND_CHUNK(_tc, _new_size) do { \
	size_t _old_used = TC_HDR_SIZE + (_tc)->size; \
	size_t _new_used = TC_HDR_SIZE + (_new_size); \
	size_t _flen = _new_used - _old_used; \
	char *_fptr = _old_used + (char *)(_tc); \
	VALGRIND_MAKE_MEM_UNDEFINED(_fptr, _flen); \
} while (0)
#else
#define TC_UNDEFINE_GROW_VALGRIND_CHUNK(_tc, _new_size) do { } while (0)
#endif

#define TC_UNDEFINE_GROW_CHUNK(_tc, _new_size) do { \
	TC_UNDEFINE_GROW_VALGRIND_CHUNK(_tc, _new_size); \
} while (0)

struct talloc_reference_handle {
	struct talloc_reference_handle *next, *prev;
	void *ptr;
	const char *location;
};

struct talloc_memlimit {
	struct talloc_chunk *parent;
	struct talloc_memlimit *upper;
	size_t max_size;
	size_t cur_size;
};

static inline bool talloc_memlimit_check(struct talloc_memlimit *limit, size_t size);
static inline void talloc_memlimit_grow(struct talloc_memlimit *limit,
				size_t size);
static inline void talloc_memlimit_shrink(struct talloc_memlimit *limit,
				size_t size);
static inline void tc_memlimit_update_on_free(struct talloc_chunk *tc);

static inline void _tc_set_name_const(struct talloc_chunk *tc,
				const char *name);
static struct talloc_chunk *_vasprintf_tc(const void *t,
				const char *fmt,
				va_list ap);

typedef int (*talloc_destructor_t)(void *);

struct talloc_pool_hdr;

struct talloc_chunk {
	/*
	 * flags includes the talloc magic, which is randomised to
	 * make overwrite attacks harder
	 */
	unsigned flags;

	/*
	 * If you have a logical tree like:
	 *
	 *           <parent>
	 *           /   |   \
	 *          /    |    \
	 *         /     |     \
	 * <child 1> <child 2> <child 3>
	 *
	 * The actual talloc tree is:
	 *
	 *  <parent>
	 *     |
	 *  <child 1> - <child 2> - <child 3>
	 *
	 * The children are linked with next/prev pointers, and
	 * child 1 is linked to the parent with parent/child
	 * pointers.
	 */

	struct talloc_chunk *next, *prev;
	struct talloc_chunk *parent, *child;
	struct talloc_reference_handle *refs;
	talloc_destructor_t destructor;
	const char *name;
	size_t size;

	/*
	 * limit semantics:
	 * if 'limit' is set it means all *new* children of the context will
	 * be limited to a total aggregate size ox max_size for memory
	 * allocations.
	 * cur_size is used to keep track of the current use
	 */
	struct talloc_memlimit *limit;

	/*
	 * For members of a pool (i.e. TALLOC_FLAG_POOLMEM is set), "pool"
	 * is a pointer to the struct talloc_chunk of the pool that it was
	 * allocated from. This way children can quickly find the pool to chew
	 * from.
	 */
	struct talloc_pool_hdr *pool;
};

/* 16 byte alignment seems to keep everyone happy */
#define TC_ALIGN16(s) (((s)+15)&~15)
#define TC_HDR_SIZE TC_ALIGN16(sizeof(struct talloc_chunk))
#define TC_PTR_FROM_CHUNK(tc) ((void *)(TC_HDR_SIZE + (char*)tc))

_PUBLIC_ int talloc_version_major(void)
{
	return TALLOC_VERSION_MAJOR;
}

_PUBLIC_ int talloc_version_minor(void)
{
	return TALLOC_VERSION_MINOR;
}

_PUBLIC_ int talloc_test_get_magic(void)
{
	return talloc_magic;
}

static inline void _talloc_chunk_set_free(struct talloc_chunk *tc,
			      const char *location)
{
	/*
	 * Mark this memory as free, and also over-stamp the talloc
	 * magic with the old-style magic.
	 *
	 * Why?  This tries to avoid a memory read use-after-free from
	 * disclosing our talloc magic, which would then allow an
	 * attacker to prepare a valid header and so run a destructor.
	 *
	 */
	tc->flags = TALLOC_MAGIC_NON_RANDOM | TALLOC_FLAG_FREE
		| (tc->flags & TALLOC_FLAG_MASK);

	/* we mark the freed memory with where we called the free
	 * from. This means on a double free error we can report where
	 * the first free came from
	 */
	if (location) {
		tc->name = location;
	}
}

static inline void _talloc_chunk_set_not_free(struct talloc_chunk *tc)
{
	/*
	 * Mark this memory as not free.
	 *
	 * Why? This is memory either in a pool (and so available for
	 * talloc's re-use or after the realloc().  We need to mark
	 * the memory as free() before any realloc() call as we can't
	 * write to the memory after that.
	 *
	 * We put back the normal magic instead of the 'not random'
	 * magic.
	 */

	tc->flags = talloc_magic |
		((tc->flags & TALLOC_FLAG_MASK) & ~TALLOC_FLAG_FREE);
}

static void (*talloc_log_fn)(const char *message);

_PUBLIC_ void talloc_set_log_fn(void (*log_fn)(const char *message))
{
	talloc_log_fn = log_fn;
}

void talloc_lib_init(void) __attribute__((constructor));
void talloc_lib_init(void)
{
	uint32_t random_value;
#if defined(HAVE_GETAUXVAL) && defined(AT_RANDOM)
	uint8_t *p;
	/*
	 * Use the kernel-provided random values used for
	 * ASLR.  This won't change per-exec, which is ideal for us
	 */
	p = (uint8_t *) getauxval(AT_RANDOM);
	if (p) {
		/*
		 * We get 16 bytes from getauxval.  By calling rand(),
		 * a totally insecure PRNG, but one that will
		 * deterministically have a different value when called
		 * twice, we ensure that if two talloc-like libraries
		 * are somehow loaded in the same address space, that
		 * because we choose different bytes, we will keep the
		 * protection against collision of multiple talloc
		 * libs.
		 *
		 * This protection is important because the effects of
		 * passing a talloc pointer from one to the other may
		 * be very hard to determine.
		 */
		int offset = rand() % (16 - sizeof(random_value));
		memcpy(&random_value, p + offset, sizeof(random_value));
	} else
#endif
	{
		/*
		 * Otherwise, hope the location we are loaded in
		 * memory is randomised by someone else
		 */
		random_value = ((uintptr_t)talloc_lib_init & 0xFFFFFFFF);
	}
	talloc_magic = random_value & ~TALLOC_FLAG_MASK;
}

static void talloc_lib_atexit(void)
{
	TALLOC_FREE(autofree_context);

	if (talloc_total_size(null_context) == 0) {
		return;
	}

	if (talloc_report_null_full) {
		talloc_report_full(null_context, stderr);
	} else if (talloc_report_null) {
		talloc_report(null_context, stderr);
	}
}

static void talloc_setup_atexit(void)
{
	static bool done;

	if (done) {
		return;
	}

	atexit(talloc_lib_atexit);
	done = true;
}

static void talloc_log(const char *fmt, ...) PRINTF_ATTRIBUTE(1,2);
static void talloc_log(const char *fmt, ...)
{
	va_list ap;
	char *message;

	if (!talloc_log_fn) {
		return;
	}

	va_start(ap, fmt);
	message = talloc_vasprintf(NULL, fmt, ap);
	va_end(ap);

	talloc_log_fn(message);
	talloc_free(message);
}

static void talloc_log_stderr(const char *message)
{
	fprintf(stderr, "%s", message);
}

_PUBLIC_ void talloc_set_log_stderr(void)
{
	talloc_set_log_fn(talloc_log_stderr);
}

static void (*talloc_abort_fn)(const char *reason);

_PUBLIC_ void talloc_set_abort_fn(void (*abort_fn)(const char *reason))
{
	talloc_abort_fn = abort_fn;
}

static void talloc_abort(const char *reason)
{
	talloc_log("%s\n", reason);

	if (!talloc_abort_fn) {
		TALLOC_ABORT(reason);
	}

	talloc_abort_fn(reason);
}

static void talloc_abort_access_after_free(void)
{
	talloc_abort("Bad talloc magic value - access after free");
}

static void talloc_abort_unknown_value(void)
{
	talloc_abort("Bad talloc magic value - unknown value");
}

/* panic if we get a bad magic value */
static inline struct talloc_chunk *talloc_chunk_from_ptr(const void *ptr)
{
	const char *pp = (const char *)ptr;
	struct talloc_chunk *tc = discard_const_p(struct talloc_chunk, pp - TC_HDR_SIZE);
	if (unlikely((tc->flags & (TALLOC_FLAG_FREE | ~TALLOC_FLAG_MASK)) != talloc_magic)) {
		if ((tc->flags & (TALLOC_FLAG_FREE | ~TALLOC_FLAG_MASK))
		    == (TALLOC_MAGIC_NON_RANDOM | TALLOC_FLAG_FREE)) {
			talloc_log("talloc: access after free error - first free may be at %s\n", tc->name);
			talloc_abort_access_after_free();
			return NULL;
		}

		talloc_abort_unknown_value();
		return NULL;
	}
	return tc;
}

/* hook into the front of the list */
#define _TLIST_ADD(list, p) \
do { \
        if (!(list)) { \
		(list) = (p); \
		(p)->next = (p)->prev = NULL; \
	} else { \
		(list)->prev = (p); \
		(p)->next = (list); \
		(p)->prev = NULL; \
		(list) = (p); \
	}\
} while (0)

/* remove an element from a list - element doesn't have to be in list. */
#define _TLIST_REMOVE(list, p) \
do { \
	if ((p) == (list)) { \
		(list) = (p)->next; \
		if (list) (list)->prev = NULL; \
	} else { \
		if ((p)->prev) (p)->prev->next = (p)->next; \
		if ((p)->next) (p)->next->prev = (p)->prev; \
	} \
	if ((p) && ((p) != (list))) (p)->next = (p)->prev = NULL; \
} while (0)


/*
  return the parent chunk of a pointer
*/
static inline struct talloc_chunk *talloc_parent_chunk(const void *ptr)
{
	struct talloc_chunk *tc;

	if (unlikely(ptr == NULL)) {
		return NULL;
	}

	tc = talloc_chunk_from_ptr(ptr);
	while (tc->prev) tc=tc->prev;

	return tc->parent;
}

_PUBLIC_ void *talloc_parent(const void *ptr)
{
	struct talloc_chunk *tc = talloc_parent_chunk(ptr);
	return tc? TC_PTR_FROM_CHUNK(tc) : NULL;
}

/*
  find parents name
*/
_PUBLIC_ const char *talloc_parent_name(const void *ptr)
{
	struct talloc_chunk *tc = talloc_parent_chunk(ptr);
	return tc? tc->name : NULL;
}

/*
  A pool carries an in-pool object count count in the first 16 bytes.
  bytes. This is done to support talloc_steal() to a parent outside of the
  pool. The count includes the pool itself, so a talloc_free() on a pool will
  only destroy the pool if the count has dropped to zero. A talloc_free() of a
  pool member will reduce the count, and eventually also call free(3) on the
  pool memory.

  The object count is not put into "struct talloc_chunk" because it is only
  relevant for talloc pools and the alignment to 16 bytes would increase the
  memory footprint of each talloc chunk by those 16 bytes.
*/

struct talloc_pool_hdr {
	void *end;
	unsigned int object_count;
	size_t poolsize;
};

#define TP_HDR_SIZE TC_ALIGN16(sizeof(struct talloc_pool_hdr))

static inline struct talloc_pool_hdr *talloc_pool_from_chunk(struct talloc_chunk *c)
{
	return (struct talloc_pool_hdr *)((char *)c - TP_HDR_SIZE);
}

static inline struct talloc_chunk *talloc_chunk_from_pool(struct talloc_pool_hdr *h)
{
	return (struct talloc_chunk *)((char *)h + TP_HDR_SIZE);
}

static inline void *tc_pool_end(struct talloc_pool_hdr *pool_hdr)
{
	struct talloc_chunk *tc = talloc_chunk_from_pool(pool_hdr);
	return (char *)tc + TC_HDR_SIZE + pool_hdr->poolsize;
}

static inline size_t tc_pool_space_left(struct talloc_pool_hdr *pool_hdr)
{
	return (char *)tc_pool_end(pool_hdr) - (char *)pool_hdr->end;
}

/* If tc is inside a pool, this gives the next neighbour. */
static inline void *tc_next_chunk(struct talloc_chunk *tc)
{
	return (char *)tc + TC_ALIGN16(TC_HDR_SIZE + tc->size);
}

static inline void *tc_pool_first_chunk(struct talloc_pool_hdr *pool_hdr)
{
	struct talloc_chunk *tc = talloc_chunk_from_pool(pool_hdr);
	return tc_next_chunk(tc);
}

/* Mark the whole remaining pool as not accessable */
static inline void tc_invalidate_pool(struct talloc_pool_hdr *pool_hdr)
{
	size_t flen = tc_pool_space_left(pool_hdr);

	if (unlikely(talloc_fill.enabled)) {
		memset(pool_hdr->end, talloc_fill.fill_value, flen);
	}

#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_NOACCESS)
	VALGRIND_MAKE_MEM_NOACCESS(pool_hdr->end, flen);
#endif
}

/*
  Allocate from a pool
*/

static inline struct talloc_chunk *tc_alloc_pool(struct talloc_chunk *parent,
						     size_t size, size_t prefix_len)
{
	struct talloc_pool_hdr *pool_hdr = NULL;
	size_t space_left;
	struct talloc_chunk *result;
	size_t chunk_size;

	if (parent == NULL) {
		return NULL;
	}

	if (parent->flags & TALLOC_FLAG_POOL) {
		pool_hdr = talloc_pool_from_chunk(parent);
	}
	else if (parent->flags & TALLOC_FLAG_POOLMEM) {
		pool_hdr = parent->pool;
	}

	if (pool_hdr == NULL) {
		return NULL;
	}

	space_left = tc_pool_space_left(pool_hdr);

	/*
	 * Align size to 16 bytes
	 */
	chunk_size = TC_ALIGN16(size + prefix_len);

	if (space_left < chunk_size) {
		return NULL;
	}

	result = (struct talloc_chunk *)((char *)pool_hdr->end + prefix_len);

#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_UNDEFINED)
	VALGRIND_MAKE_MEM_UNDEFINED(pool_hdr->end, chunk_size);
#endif

	pool_hdr->end = (void *)((char *)pool_hdr->end + chunk_size);

	result->flags = talloc_magic | TALLOC_FLAG_POOLMEM;
	result->pool = pool_hdr;

	pool_hdr->object_count++;

	return result;
}

/*
   Allocate a bit of memory as a child of an existing pointer
*/
static inline void *__talloc_with_prefix(const void *context,
					size_t size,
					size_t prefix_len,
					struct talloc_chunk **tc_ret)
{
	struct talloc_chunk *tc = NULL;
	struct talloc_memlimit *limit = NULL;
	size_t total_len = TC_HDR_SIZE + size + prefix_len;
	struct talloc_chunk *parent = NULL;

	if (unlikely(context == NULL)) {
		context = null_context;
	}

	if (unlikely(size >= MAX_TALLOC_SIZE)) {
		return NULL;
	}

	if (unlikely(total_len < TC_HDR_SIZE)) {
		return NULL;
	}

	if (likely(context != NULL)) {
		parent = talloc_chunk_from_ptr(context);

		if (parent->limit != NULL) {
			limit = parent->limit;
		}

		tc = tc_alloc_pool(parent, TC_HDR_SIZE+size, prefix_len);
	}

	if (tc == NULL) {
		char *ptr;

		/*
		 * Only do the memlimit check/update on actual allocation.
		 */
		if (!talloc_memlimit_check(limit, total_len)) {
			errno = ENOMEM;
			return NULL;
		}

		ptr = malloc(total_len);
		if (unlikely(ptr == NULL)) {
			return NULL;
		}
		tc = (struct talloc_chunk *)(ptr + prefix_len);
		tc->flags = talloc_magic;
		tc->pool  = NULL;

		talloc_memlimit_grow(limit, total_len);
	}

	tc->limit = limit;
	tc->size = size;
	tc->destructor = NULL;
	tc->child = NULL;
	tc->name = NULL;
	tc->refs = NULL;

	if (likely(context != NULL)) {
		if (parent->child) {
			parent->child->parent = NULL;
			tc->next = parent->child;
			tc->next->prev = tc;
		} else {
			tc->next = NULL;
		}
		tc->parent = parent;
		tc->prev = NULL;
		parent->child = tc;
	} else {
		tc->next = tc->prev = tc->parent = NULL;
	}

	*tc_ret = tc;
	return TC_PTR_FROM_CHUNK(tc);
}

static inline void *__talloc(const void *context,
			size_t size,
			struct talloc_chunk **tc)
{
	return __talloc_with_prefix(context, size, 0, tc);
}

/*
 * Create a talloc pool
 */

static inline void *_talloc_pool(const void *context, size_t size)
{
	struct talloc_chunk *tc;
	struct talloc_pool_hdr *pool_hdr;
	void *result;

	result = __talloc_with_prefix(context, size, TP_HDR_SIZE, &tc);

	if (unlikely(result == NULL)) {
		return NULL;
	}

	pool_hdr = talloc_pool_from_chunk(tc);

	tc->flags |= TALLOC_FLAG_POOL;
	tc->size = 0;

	pool_hdr->object_count = 1;
	pool_hdr->end = result;
	pool_hdr->poolsize = size;

	tc_invalidate_pool(pool_hdr);

	return result;
}

_PUBLIC_ void *talloc_pool(const void *context, size_t size)
{
	return _talloc_pool(context, size);
}

/*
 * Create a talloc pool correctly sized for a basic size plus
 * a number of subobjects whose total size is given. Essentially
 * a custom allocator for talloc to reduce fragmentation.
 */

_PUBLIC_ void *_talloc_pooled_object(const void *ctx,
				     size_t type_size,
				     const char *type_name,
				     unsigned num_subobjects,
				     size_t total_subobjects_size)
{
	size_t poolsize, subobjects_slack, tmp;
	struct talloc_chunk *tc;
	struct talloc_pool_hdr *pool_hdr;
	void *ret;

	poolsize = type_size + total_subobjects_size;

	if ((poolsize < type_size) || (poolsize < total_subobjects_size)) {
		goto overflow;
	}

	if (num_subobjects == UINT_MAX) {
		goto overflow;
	}
	num_subobjects += 1;       /* the object body itself */

	/*
	 * Alignment can increase the pool size by at most 15 bytes per object
	 * plus alignment for the object itself
	 */
	subobjects_slack = (TC_HDR_SIZE + TP_HDR_SIZE + 15) * num_subobjects;
	if (subobjects_slack < num_subobjects) {
		goto overflow;
	}

	tmp = poolsize + subobjects_slack;
	if ((tmp < poolsize) || (tmp < subobjects_slack)) {
		goto overflow;
	}
	poolsize = tmp;

	ret = _talloc_pool(ctx, poolsize);
	if (ret == NULL) {
		return NULL;
	}

	tc = talloc_chunk_from_ptr(ret);
	tc->size = type_size;

	pool_hdr = talloc_pool_from_chunk(tc);

#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_UNDEFINED)
	VALGRIND_MAKE_MEM_UNDEFINED(pool_hdr->end, type_size);
#endif

	pool_hdr->end = ((char *)pool_hdr->end + TC_ALIGN16(type_size));

	_tc_set_name_const(tc, type_name);
	return ret;

overflow:
	return NULL;
}

/*
  setup a destructor to be called on free of a pointer
  the destructor should return 0 on success, or -1 on failure.
  if the destructor fails then the free is failed, and the memory can
  be continued to be used
*/
_PUBLIC_ void _talloc_set_destructor(const void *ptr, int (*destructor)(void *))
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ptr);
	tc->destructor = destructor;
}

/*
  increase the reference count on a piece of memory.
*/
_PUBLIC_ int talloc_increase_ref_count(const void *ptr)
{
	if (unlikely(!talloc_reference(null_context, ptr))) {
		return -1;
	}
	return 0;
}

/*
  helper for talloc_reference()

  this is referenced by a function pointer and should not be inline
*/
static int talloc_reference_destructor(struct talloc_reference_handle *handle)
{
	struct talloc_chunk *ptr_tc = talloc_chunk_from_ptr(handle->ptr);
	_TLIST_REMOVE(ptr_tc->refs, handle);
	return 0;
}

/*
   more efficient way to add a name to a pointer - the name must point to a
   true string constant
*/
static inline void _tc_set_name_const(struct talloc_chunk *tc,
					const char *name)
{
	tc->name = name;
}

/*
  internal talloc_named_const()
*/
static inline void *_talloc_named_const(const void *context, size_t size, const char *name)
{
	void *ptr;
	struct talloc_chunk *tc;

	ptr = __talloc(context, size, &tc);
	if (unlikely(ptr == NULL)) {
		return NULL;
	}

	_tc_set_name_const(tc, name);

	return ptr;
}

/*
  make a secondary reference to a pointer, hanging off the given context.
  the pointer remains valid until both the original caller and this given
  context are freed.

  the major use for this is when two different structures need to reference the
  same underlying data, and you want to be able to free the two instances separately,
  and in either order
*/
_PUBLIC_ void *_talloc_reference_loc(const void *context, const void *ptr, const char *location)
{
	struct talloc_chunk *tc;
	struct talloc_reference_handle *handle;
	if (unlikely(ptr == NULL)) return NULL;

	tc = talloc_chunk_from_ptr(ptr);
	handle = (struct talloc_reference_handle *)_talloc_named_const(context,
						   sizeof(struct talloc_reference_handle),
						   TALLOC_MAGIC_REFERENCE);
	if (unlikely(handle == NULL)) return NULL;

	/* note that we hang the destructor off the handle, not the
	   main context as that allows the caller to still setup their
	   own destructor on the context if they want to */
	talloc_set_destructor(handle, talloc_reference_destructor);
	handle->ptr = discard_const_p(void, ptr);
	handle->location = location;
	_TLIST_ADD(tc->refs, handle);
	return handle->ptr;
}

static void *_talloc_steal_internal(const void *new_ctx, const void *ptr);

static inline void _tc_free_poolmem(struct talloc_chunk *tc,
					const char *location)
{
	struct talloc_pool_hdr *pool;
	struct talloc_chunk *pool_tc;
	void *next_tc;

	pool = tc->pool;
	pool_tc = talloc_chunk_from_pool(pool);
	next_tc = tc_next_chunk(tc);

	_talloc_chunk_set_free(tc, location);

	TC_INVALIDATE_FULL_CHUNK(tc);

	if (unlikely(pool->object_count == 0)) {
		talloc_abort("Pool object count zero!");
		return;
	}

	pool->object_count--;

	if (unlikely(pool->object_count == 1
		     && !(pool_tc->flags & TALLOC_FLAG_FREE))) {
		/*
		 * if there is just one object left in the pool
		 * and pool->flags does not have TALLOC_FLAG_FREE,
		 * it means this is the pool itself and
		 * the rest is available for new objects
		 * again.
		 */
		pool->end = tc_pool_first_chunk(pool);
		tc_invalidate_pool(pool);
		return;
	}

	if (unlikely(pool->object_count == 0)) {
		/*
		 * we mark the freed memory with where we called the free
		 * from. This means on a double free error we can report where
		 * the first free came from
		 */
		pool_tc->name = location;

		if (pool_tc->flags & TALLOC_FLAG_POOLMEM) {
			_tc_free_poolmem(pool_tc, location);
		} else {
			/*
			 * The tc_memlimit_update_on_free()
			 * call takes into account the
			 * prefix TP_HDR_SIZE allocated before
			 * the pool talloc_chunk.
			 */
			tc_memlimit_update_on_free(pool_tc);
			TC_INVALIDATE_FULL_CHUNK(pool_tc);
			free(pool);
		}
		return;
	}

	if (pool->end == next_tc) {
		/*
		 * if pool->pool still points to end of
		 * 'tc' (which is stored in the 'next_tc' variable),
		 * we can reclaim the memory of 'tc'.
		 */
		pool->end = tc;
		return;
	}

	/*
	 * Do nothing. The memory is just "wasted", waiting for the pool
	 * itself to be freed.
	 */
}

static inline void _tc_free_children_internal(struct talloc_chunk *tc,
						  void *ptr,
						  const char *location);

static inline int _talloc_free_internal(void *ptr, const char *location);

/*
   internal free call that takes a struct talloc_chunk *.
*/
static inline int _tc_free_internal(struct talloc_chunk *tc,
				const char *location)
{
	void *ptr_to_free;
	void *ptr = TC_PTR_FROM_CHUNK(tc);

	if (unlikely(tc->refs)) {
		int is_child;
		/* check if this is a reference from a child or
		 * grandchild back to it's parent or grandparent
		 *
		 * in that case we need to remove the reference and
		 * call another instance of talloc_free() on the current
		 * pointer.
		 */
		is_child = talloc_is_parent(tc->refs, ptr);
		_talloc_free_internal(tc->refs, location);
		if (is_child) {
			return _talloc_free_internal(ptr, location);
		}
		return -1;
	}

	if (unlikely(tc->flags & TALLOC_FLAG_LOOP)) {
		/* we have a free loop - stop looping */
		return 0;
	}

	if (unlikely(tc->destructor)) {
		talloc_destructor_t d = tc->destructor;

		/*
		 * Protect the destructor against some overwrite
		 * attacks, by explicitly checking it has the right
		 * magic here.
		 */
		if (talloc_chunk_from_ptr(ptr) != tc) {
			/*
			 * This can't actually happen, the
			 * call itself will panic.
			 */
			TALLOC_ABORT("talloc_chunk_from_ptr failed!");
		}

		if (d == (talloc_destructor_t)-1) {
			return -1;
		}
		tc->destructor = (talloc_destructor_t)-1;
		if (d(ptr) == -1) {
			/*
			 * Only replace the destructor pointer if
			 * calling the destructor didn't modify it.
			 */
			if (tc->destructor == (talloc_destructor_t)-1) {
				tc->destructor = d;
			}
			return -1;
		}
		tc->destructor = NULL;
	}

	if (tc->parent) {
		_TLIST_REMOVE(tc->parent->child, tc);
		if (tc->parent->child) {
			tc->parent->child->parent = tc->parent;
		}
	} else {
		if (tc->prev) tc->prev->next = tc->next;
		if (tc->next) tc->next->prev = tc->prev;
		tc->prev = tc->next = NULL;
	}

	tc->flags |= TALLOC_FLAG_LOOP;

	_tc_free_children_internal(tc, ptr, location);

	_talloc_chunk_set_free(tc, location);

	if (tc->flags & TALLOC_FLAG_POOL) {
		struct talloc_pool_hdr *pool;

		pool = talloc_pool_from_chunk(tc);

		if (unlikely(pool->object_count == 0)) {
			talloc_abort("Pool object count zero!");
			return 0;
		}

		pool->object_count--;

		if (likely(pool->object_count != 0)) {
			return 0;
		}

		/*
		 * With object_count==0, a pool becomes a normal piece of
		 * memory to free. If it's allocated inside a pool, it needs
		 * to be freed as poolmem, else it needs to be just freed.
		*/
		ptr_to_free = pool;
	} else {
		ptr_to_free = tc;
	}

	if (tc->flags & TALLOC_FLAG_POOLMEM) {
		_tc_free_poolmem(tc, location);
		return 0;
	}

	tc_memlimit_update_on_free(tc);

	TC_INVALIDATE_FULL_CHUNK(tc);
	free(ptr_to_free);
	return 0;
}

/*
   internal talloc_free call
*/
static inline int _talloc_free_internal(void *ptr, const char *location)
{
	struct talloc_chunk *tc;

	if (unlikely(ptr == NULL)) {
		return -1;
	}

	/* possibly initialised the talloc fill value */
	if (unlikely(!talloc_fill.initialised)) {
		const char *fill = getenv(TALLOC_FILL_ENV);
		if (fill != NULL) {
			talloc_fill.enabled = true;
			talloc_fill.fill_value = strtoul(fill, NULL, 0);
		}
		talloc_fill.initialised = true;
	}

	tc = talloc_chunk_from_ptr(ptr);
	return _tc_free_internal(tc, location);
}

static inline size_t _talloc_total_limit_size(const void *ptr,
					struct talloc_memlimit *old_limit,
					struct talloc_memlimit *new_limit);

/*
   move a lump of memory from one talloc context to another return the
   ptr on success, or NULL if it could not be transferred.
   passing NULL as ptr will always return NULL with no side effects.
*/
static void *_talloc_steal_internal(const void *new_ctx, const void *ptr)
{
	struct talloc_chunk *tc, *new_tc;
	size_t ctx_size = 0;

	if (unlikely(!ptr)) {
		return NULL;
	}

	if (unlikely(new_ctx == NULL)) {
		new_ctx = null_context;
	}

	tc = talloc_chunk_from_ptr(ptr);

	if (tc->limit != NULL) {

		ctx_size = _talloc_total_limit_size(ptr, NULL, NULL);

		/* Decrement the memory limit from the source .. */
		talloc_memlimit_shrink(tc->limit->upper, ctx_size);

		if (tc->limit->parent == tc) {
			tc->limit->upper = NULL;
		} else {
			tc->limit = NULL;
		}
	}

	if (unlikely(new_ctx == NULL)) {
		if (tc->parent) {
			_TLIST_REMOVE(tc->parent->child, tc);
			if (tc->parent->child) {
				tc->parent->child->parent = tc->parent;
			}
		} else {
			if (tc->prev) tc->prev->next = tc->next;
			if (tc->next) tc->next->prev = tc->prev;
		}

		tc->parent = tc->next = tc->prev = NULL;
		return discard_const_p(void, ptr);
	}

	new_tc = talloc_chunk_from_ptr(new_ctx);

	if (unlikely(tc == new_tc || tc->parent == new_tc)) {
		return discard_const_p(void, ptr);
	}

	if (tc->parent) {
		_TLIST_REMOVE(tc->parent->child, tc);
		if (tc->parent->child) {
			tc->parent->child->parent = tc->parent;
		}
	} else {
		if (tc->prev) tc->prev->next = tc->next;
		if (tc->next) tc->next->prev = tc->prev;
		tc->prev = tc->next = NULL;
	}

	tc->parent = new_tc;
	if (new_tc->child) new_tc->child->parent = NULL;
	_TLIST_ADD(new_tc->child, tc);

	if (tc->limit || new_tc->limit) {
		ctx_size = _talloc_total_limit_size(ptr, tc->limit,
						    new_tc->limit);
		/* .. and increment it in the destination. */
		if (new_tc->limit) {
			talloc_memlimit_grow(new_tc->limit, ctx_size);
		}
	}

	return discard_const_p(void, ptr);
}

/*
   move a lump of memory from one talloc context to another return the
   ptr on success, or NULL if it could not be transferred.
   passing NULL as ptr will always return NULL with no side effects.
*/
_PUBLIC_ void *_talloc_steal_loc(const void *new_ctx, const void *ptr, const char *location)
{
	struct talloc_chunk *tc;

	if (unlikely(ptr == NULL)) {
		return NULL;
	}

	tc = talloc_chunk_from_ptr(ptr);

	if (unlikely(tc->refs != NULL) && talloc_parent(ptr) != new_ctx) {
		struct talloc_reference_handle *h;

		talloc_log("WARNING: talloc_steal with references at %s\n",
			   location);

		for (h=tc->refs; h; h=h->next) {
			talloc_log("\treference at %s\n",
				   h->location);
		}
	}

#if 0
	/* this test is probably too expensive to have on in the
	   normal build, but it useful for debugging */
	if (talloc_is_parent(new_ctx, ptr)) {
		talloc_log("WARNING: stealing into talloc child at %s\n", location);
	}
#endif

	return _talloc_steal_internal(new_ctx, ptr);
}

/*
   this is like a talloc_steal(), but you must supply the old
   parent. This resolves the ambiguity in a talloc_steal() which is
   called on a context that has more than one parent (via references)

   The old parent can be either a reference or a parent
*/
_PUBLIC_ void *talloc_reparent(const void *old_parent, const void *new_parent, const void *ptr)
{
	struct talloc_chunk *tc;
	struct talloc_reference_handle *h;

	if (unlikely(ptr == NULL)) {
		return NULL;
	}

	if (old_parent == talloc_parent(ptr)) {
		return _talloc_steal_internal(new_parent, ptr);
	}

	tc = talloc_chunk_from_ptr(ptr);
	for (h=tc->refs;h;h=h->next) {
		if (talloc_parent(h) == old_parent) {
			if (_talloc_steal_internal(new_parent, h) != h) {
				return NULL;
			}
			return discard_const_p(void, ptr);
		}
	}

	/* it wasn't a parent */
	return NULL;
}

/*
  remove a secondary reference to a pointer. This undo's what
  talloc_reference() has done. The context and pointer arguments
  must match those given to a talloc_reference()
*/
static inline int talloc_unreference(const void *context, const void *ptr)
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ptr);
	struct talloc_reference_handle *h;

	if (unlikely(context == NULL)) {
		context = null_context;
	}

	for (h=tc->refs;h;h=h->next) {
		struct talloc_chunk *p = talloc_parent_chunk(h);
		if (p == NULL) {
			if (context == NULL) break;
		} else if (TC_PTR_FROM_CHUNK(p) == context) {
			break;
		}
	}
	if (h == NULL) {
		return -1;
	}

	return _talloc_free_internal(h, __location__);
}

/*
  remove a specific parent context from a pointer. This is a more
  controlled variant of talloc_free()
*/
_PUBLIC_ int talloc_unlink(const void *context, void *ptr)
{
	struct talloc_chunk *tc_p, *new_p, *tc_c;
	void *new_parent;

	if (ptr == NULL) {
		return -1;
	}

	if (context == NULL) {
		context = null_context;
	}

	if (talloc_unreference(context, ptr) == 0) {
		return 0;
	}

	if (context != NULL) {
		tc_c = talloc_chunk_from_ptr(context);
	} else {
		tc_c = NULL;
	}
	if (tc_c != talloc_parent_chunk(ptr)) {
		return -1;
	}

	tc_p = talloc_chunk_from_ptr(ptr);

	if (tc_p->refs == NULL) {
		return _talloc_free_internal(ptr, __location__);
	}

	new_p = talloc_parent_chunk(tc_p->refs);
	if (new_p) {
		new_parent = TC_PTR_FROM_CHUNK(new_p);
	} else {
		new_parent = NULL;
	}

	if (talloc_unreference(new_parent, ptr) != 0) {
		return -1;
	}

	_talloc_steal_internal(new_parent, ptr);

	return 0;
}

/*
  add a name to an existing pointer - va_list version
*/
static inline const char *tc_set_name_v(struct talloc_chunk *tc,
				const char *fmt,
				va_list ap) PRINTF_ATTRIBUTE(2,0);

static inline const char *tc_set_name_v(struct talloc_chunk *tc,
				const char *fmt,
				va_list ap)
{
	struct talloc_chunk *name_tc = _vasprintf_tc(TC_PTR_FROM_CHUNK(tc),
							fmt,
							ap);
	if (likely(name_tc)) {
		tc->name = TC_PTR_FROM_CHUNK(name_tc);
		_tc_set_name_const(name_tc, ".name");
	} else {
		tc->name = NULL;
	}
	return tc->name;
}

/*
  add a name to an existing pointer
*/
_PUBLIC_ const char *talloc_set_name(const void *ptr, const char *fmt, ...)
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ptr);
	const char *name;
	va_list ap;
	va_start(ap, fmt);
	name = tc_set_name_v(tc, fmt, ap);
	va_end(ap);
	return name;
}


/*
  create a named talloc pointer. Any talloc pointer can be named, and
  talloc_named() operates just like talloc() except that it allows you
  to name the pointer.
*/
_PUBLIC_ void *talloc_named(const void *context, size_t size, const char *fmt, ...)
{
	va_list ap;
	void *ptr;
	const char *name;
	struct talloc_chunk *tc;

	ptr = __talloc(context, size, &tc);
	if (unlikely(ptr == NULL)) return NULL;

	va_start(ap, fmt);
	name = tc_set_name_v(tc, fmt, ap);
	va_end(ap);

	if (unlikely(name == NULL)) {
		_talloc_free_internal(ptr, __location__);
		return NULL;
	}

	return ptr;
}

/*
  return the name of a talloc ptr, or "UNNAMED"
*/
static inline const char *__talloc_get_name(const void *ptr)
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ptr);
	if (unlikely(tc->name == TALLOC_MAGIC_REFERENCE)) {
		return ".reference";
	}
	if (likely(tc->name)) {
		return tc->name;
	}
	return "UNNAMED";
}

_PUBLIC_ const char *talloc_get_name(const void *ptr)
{
	return __talloc_get_name(ptr);
}

/*
  check if a pointer has the given name. If it does, return the pointer,
  otherwise return NULL
*/
_PUBLIC_ void *talloc_check_name(const void *ptr, const char *name)
{
	const char *pname;
	if (unlikely(ptr == NULL)) return NULL;
	pname = __talloc_get_name(ptr);
	if (likely(pname == name || strcmp(pname, name) == 0)) {
		return discard_const_p(void, ptr);
	}
	return NULL;
}

static void talloc_abort_type_mismatch(const char *location,
					const char *name,
					const char *expected)
{
	const char *reason;

	reason = talloc_asprintf(NULL,
				 "%s: Type mismatch: name[%s] expected[%s]",
				 location,
				 name?name:"NULL",
				 expected);
	if (!reason) {
		reason = "Type mismatch";
	}

	talloc_abort(reason);
}

_PUBLIC_ void *_talloc_get_type_abort(const void *ptr, const char *name, const char *location)
{
	const char *pname;

	if (unlikely(ptr == NULL)) {
		talloc_abort_type_mismatch(location, NULL, name);
		return NULL;
	}

	pname = __talloc_get_name(ptr);
	if (likely(pname == name || strcmp(pname, name) == 0)) {
		return discard_const_p(void, ptr);
	}

	talloc_abort_type_mismatch(location, pname, name);
	return NULL;
}

/*
  this is for compatibility with older versions of talloc
*/
_PUBLIC_ void *talloc_init(const char *fmt, ...)
{
	va_list ap;
	void *ptr;
	const char *name;
	struct talloc_chunk *tc;

	ptr = __talloc(NULL, 0, &tc);
	if (unlikely(ptr == NULL)) return NULL;

	va_start(ap, fmt);
	name = tc_set_name_v(tc, fmt, ap);
	va_end(ap);

	if (unlikely(name == NULL)) {
		_talloc_free_internal(ptr, __location__);
		return NULL;
	}

	return ptr;
}

static inline void _tc_free_children_internal(struct talloc_chunk *tc,
						  void *ptr,
						  const char *location)
{
	while (tc->child) {
		/* we need to work out who will own an abandoned child
		   if it cannot be freed. In priority order, the first
		   choice is owner of any remaining reference to this
		   pointer, the second choice is our parent, and the
		   final choice is the null context. */
		void *child = TC_PTR_FROM_CHUNK(tc->child);
		const void *new_parent = null_context;
		if (unlikely(tc->child->refs)) {
			struct talloc_chunk *p = talloc_parent_chunk(tc->child->refs);
			if (p) new_parent = TC_PTR_FROM_CHUNK(p);
		}
		if (unlikely(_tc_free_internal(tc->child, location) == -1)) {
			if (talloc_parent_chunk(child) != tc) {
				/*
				 * Destructor already reparented this child.
				 * No further reparenting needed.
				 */
				continue;
			}
			if (new_parent == null_context) {
				struct talloc_chunk *p = talloc_parent_chunk(ptr);
				if (p) new_parent = TC_PTR_FROM_CHUNK(p);
			}
			_talloc_steal_internal(new_parent, child);
		}
	}
}

/*
  this is a replacement for the Samba3 talloc_destroy_pool functionality. It
  should probably not be used in new code. It's in here to keep the talloc
  code consistent across Samba 3 and 4.
*/
_PUBLIC_ void talloc_free_children(void *ptr)
{
	struct talloc_chunk *tc_name = NULL;
	struct talloc_chunk *tc;

	if (unlikely(ptr == NULL)) {
		return;
	}

	tc = talloc_chunk_from_ptr(ptr);

	/* we do not want to free the context name if it is a child .. */
	if (likely(tc->child)) {
		for (tc_name = tc->child; tc_name; tc_name = tc_name->next) {
			if (tc->name == TC_PTR_FROM_CHUNK(tc_name)) break;
		}
		if (tc_name) {
			_TLIST_REMOVE(tc->child, tc_name);
			if (tc->child) {
				tc->child->parent = tc;
			}
		}
	}

	_tc_free_children_internal(tc, ptr, __location__);

	/* .. so we put it back after all other children have been freed */
	if (tc_name) {
		if (tc->child) {
			tc->child->parent = NULL;
		}
		tc_name->parent = tc;
		_TLIST_ADD(tc->child, tc_name);
	}
}

/*
   Allocate a bit of memory as a child of an existing pointer
*/
_PUBLIC_ void *_talloc(const void *context, size_t size)
{
	struct talloc_chunk *tc;
	return __talloc(context, size, &tc);
}

/*
  externally callable talloc_set_name_const()
*/
_PUBLIC_ void talloc_set_name_const(const void *ptr, const char *name)
{
	_tc_set_name_const(talloc_chunk_from_ptr(ptr), name);
}

/*
  create a named talloc pointer. Any talloc pointer can be named, and
  talloc_named() operates just like talloc() except that it allows you
  to name the pointer.
*/
_PUBLIC_ void *talloc_named_const(const void *context, size_t size, const char *name)
{
	return _talloc_named_const(context, size, name);
}

/*
   free a talloc pointer. This also frees all child pointers of this
   pointer recursively

   return 0 if the memory is actually freed, otherwise -1. The memory
   will not be freed if the ref_count is > 1 or the destructor (if
   any) returns non-zero
*/
_PUBLIC_ int _talloc_free(void *ptr, const char *location)
{
	struct talloc_chunk *tc;

	if (unlikely(ptr == NULL)) {
		return -1;
	}

	tc = talloc_chunk_from_ptr(ptr);

	if (unlikely(tc->refs != NULL)) {
		struct talloc_reference_handle *h;

		if (talloc_parent(ptr) == null_context && tc->refs->next == NULL) {
			/* in this case we do know which parent should
			   get this pointer, as there is really only
			   one parent */
			return talloc_unlink(null_context, ptr);
		}

		talloc_log("ERROR: talloc_free with references at %s\n",
			   location);

		for (h=tc->refs; h; h=h->next) {
			talloc_log("\treference at %s\n",
				   h->location);
		}
		return -1;
	}

	return _talloc_free_internal(ptr, location);
}



/*
  A talloc version of realloc. The context argument is only used if
  ptr is NULL
*/
_PUBLIC_ void *_talloc_realloc(const void *context, void *ptr, size_t size, const char *name)
{
	struct talloc_chunk *tc;
	void *new_ptr;
	bool malloced = false;
	struct talloc_pool_hdr *pool_hdr = NULL;
	size_t old_size = 0;
	size_t new_size = 0;

	/* size zero is equivalent to free() */
	if (unlikely(size == 0)) {
		talloc_unlink(context, ptr);
		return NULL;
	}

	if (unlikely(size >= MAX_TALLOC_SIZE)) {
		return NULL;
	}

	/* realloc(NULL) is equivalent to malloc() */
	if (ptr == NULL) {
		return _talloc_named_const(context, size, name);
	}

	tc = talloc_chunk_from_ptr(ptr);

	/* don't allow realloc on referenced pointers */
	if (unlikely(tc->refs)) {
		return NULL;
	}

	/* don't let anybody try to realloc a talloc_pool */
	if (unlikely(tc->flags & TALLOC_FLAG_POOL)) {
		return NULL;
	}

	if (tc->limit && (size > tc->size)) {
		if (!talloc_memlimit_check(tc->limit, (size - tc->size))) {
			errno = ENOMEM;
			return NULL;
		}
	}

	/* handle realloc inside a talloc_pool */
	if (unlikely(tc->flags & TALLOC_FLAG_POOLMEM)) {
		pool_hdr = tc->pool;
	}

#if (ALWAYS_REALLOC == 0)
	/* don't shrink if we have less than 1k to gain */
	if (size < tc->size && tc->limit == NULL) {
		if (pool_hdr) {
			void *next_tc = tc_next_chunk(tc);
			TC_INVALIDATE_SHRINK_CHUNK(tc, size);
			tc->size = size;
			if (next_tc == pool_hdr->end) {
				/* note: tc->size has changed, so this works */
				pool_hdr->end = tc_next_chunk(tc);
			}
			return ptr;
		} else if ((tc->size - size) < 1024) {
			/*
			 * if we call TC_INVALIDATE_SHRINK_CHUNK() here
			 * we would need to call TC_UNDEFINE_GROW_CHUNK()
			 * after each realloc call, which slows down
			 * testing a lot :-(.
			 *
			 * That is why we only mark memory as undefined here.
			 */
			TC_UNDEFINE_SHRINK_CHUNK(tc, size);

			/* do not shrink if we have less than 1k to gain */
			tc->size = size;
			return ptr;
		}
	} else if (tc->size == size) {
		/*
		 * do not change the pointer if it is exactly
		 * the same size.
		 */
		return ptr;
	}
#endif

	/*
	 * by resetting magic we catch users of the old memory
	 *
	 * We mark this memory as free, and also over-stamp the talloc
	 * magic with the old-style magic.
	 *
	 * Why?  This tries to avoid a memory read use-after-free from
	 * disclosing our talloc magic, which would then allow an
	 * attacker to prepare a valid header and so run a destructor.
	 *
	 * What else?  We have to re-stamp back a valid normal magic
	 * on this memory once realloc() is done, as it will have done
	 * a memcpy() into the new valid memory.  We can't do this in
	 * reverse as that would be a real use-after-free.
	 */
	_talloc_chunk_set_free(tc, NULL);

#if ALWAYS_REALLOC
	if (pool_hdr) {
		new_ptr = tc_alloc_pool(tc, size + TC_HDR_SIZE, 0);
		pool_hdr->object_count--;

		if (new_ptr == NULL) {
			new_ptr = malloc(TC_HDR_SIZE+size);
			malloced = true;
			new_size = size;
		}

		if (new_ptr) {
			memcpy(new_ptr, tc, MIN(tc->size,size) + TC_HDR_SIZE);
			TC_INVALIDATE_FULL_CHUNK(tc);
		}
	} else {
		/* We're doing malloc then free here, so record the difference. */
		old_size = tc->size;
		new_size = size;
		new_ptr = malloc(size + TC_HDR_SIZE);
		if (new_ptr) {
			memcpy(new_ptr, tc, MIN(tc->size, size) + TC_HDR_SIZE);
			free(tc);
		}
	}
#else
	if (pool_hdr) {
		struct talloc_chunk *pool_tc;
		void *next_tc = tc_next_chunk(tc);
		size_t old_chunk_size = TC_ALIGN16(TC_HDR_SIZE + tc->size);
		size_t new_chunk_size = TC_ALIGN16(TC_HDR_SIZE + size);
		size_t space_needed;
		size_t space_left;
		unsigned int chunk_count = pool_hdr->object_count;

		pool_tc = talloc_chunk_from_pool(pool_hdr);
		if (!(pool_tc->flags & TALLOC_FLAG_FREE)) {
			chunk_count -= 1;
		}

		if (chunk_count == 1) {
			/*
			 * optimize for the case where 'tc' is the only
			 * chunk in the pool.
			 */
			char *start = tc_pool_first_chunk(pool_hdr);
			space_needed = new_chunk_size;
			space_left = (char *)tc_pool_end(pool_hdr) - start;

			if (space_left >= space_needed) {
				size_t old_used = TC_HDR_SIZE + tc->size;
				size_t new_used = TC_HDR_SIZE + size;
				new_ptr = start;

#if defined(DEVELOPER) && defined(VALGRIND_MAKE_MEM_UNDEFINED)
				{
					/*
					 * The area from
					 * start -> tc may have
					 * been freed and thus been marked as
					 * VALGRIND_MEM_NOACCESS. Set it to
					 * VALGRIND_MEM_UNDEFINED so we can
					 * copy into it without valgrind errors.
					 * We can't just mark
					 * new_ptr -> new_ptr + old_used
					 * as this may overlap on top of tc,
					 * (which is why we use memmove, not
					 * memcpy below) hence the MIN.
					 */
					size_t undef_len = MIN((((char *)tc) - ((char *)new_ptr)),old_used);
					VALGRIND_MAKE_MEM_UNDEFINED(new_ptr, undef_len);
				}
#endif

				memmove(new_ptr, tc, old_used);

				tc = (struct talloc_chunk *)new_ptr;
				TC_UNDEFINE_GROW_CHUNK(tc, size);

				/*
				 * first we do not align the pool pointer
				 * because we want to invalidate the padding
				 * too.
				 */
				pool_hdr->end = new_used + (char *)new_ptr;
				tc_invalidate_pool(pool_hdr);

				/* now the aligned pointer */
				pool_hdr->end = new_chunk_size + (char *)new_ptr;
				goto got_new_ptr;
			}

			next_tc = NULL;
		}

		if (new_chunk_size == old_chunk_size) {
			TC_UNDEFINE_GROW_CHUNK(tc, size);
			_talloc_chunk_set_not_free(tc);
			tc->size = size;
			return ptr;
		}

		if (next_tc == pool_hdr->end) {
			/*
			 * optimize for the case where 'tc' is the last
			 * chunk in the pool.
			 */
			space_needed = new_chunk_size - old_chunk_size;
			space_left = tc_pool_space_left(pool_hdr);

			if (space_left >= space_needed) {
				TC_UNDEFINE_GROW_CHUNK(tc, size);
				_talloc_chunk_set_not_free(tc);
				tc->size = size;
				pool_hdr->end = tc_next_chunk(tc);
				return ptr;
			}
		}

		new_ptr = tc_alloc_pool(tc, size + TC_HDR_SIZE, 0);

		if (new_ptr == NULL) {
			new_ptr = malloc(TC_HDR_SIZE+size);
			malloced = true;
			new_size = size;
		}

		if (new_ptr) {
			memcpy(new_ptr, tc, MIN(tc->size,size) + TC_HDR_SIZE);

			_tc_free_poolmem(tc, __location__ "_talloc_realloc");
		}
	}
	else {
		/* We're doing realloc here, so record the difference. */
		old_size = tc->size;
		new_size = size;
		new_ptr = realloc(tc, size + TC_HDR_SIZE);
	}
got_new_ptr:
#endif
	if (unlikely(!new_ptr)) {
		/*
		 * Ok, this is a strange spot.  We have to put back
		 * the old talloc_magic and any flags, except the
		 * TALLOC_FLAG_FREE as this was not free'ed by the
		 * realloc() call after all
		 */
		_talloc_chunk_set_not_free(tc);
		return NULL;
	}

	/*
	 * tc is now the new value from realloc(), the old memory we
	 * can't access any more and was preemptively marked as
	 * TALLOC_FLAG_FREE before the call.  Now we mark it as not
	 * free again
	 */
	tc = (struct talloc_chunk *)new_ptr;
	_talloc_chunk_set_not_free(tc);
	if (malloced) {
		tc->flags &= ~TALLOC_FLAG_POOLMEM;
	}
	if (tc->parent) {
		tc->parent->child = tc;
	}
	if (tc->child) {
		tc->child->parent = tc;
	}

	if (tc->prev) {
		tc->prev->next = tc;
	}
	if (tc->next) {
		tc->next->prev = tc;
	}

	if (new_size > old_size) {
		talloc_memlimit_grow(tc->limit, new_size - old_size);
	} else if (new_size < old_size) {
		talloc_memlimit_shrink(tc->limit, old_size - new_size);
	}

	tc->size = size;
	_tc_set_name_const(tc, name);

	return TC_PTR_FROM_CHUNK(tc);
}

/*
  a wrapper around talloc_steal() for situations where you are moving a pointer
  between two structures, and want the old pointer to be set to NULL
*/
_PUBLIC_ void *_talloc_move(const void *new_ctx, const void *_pptr)
{
	const void **pptr = discard_const_p(const void *,_pptr);
	void *ret = talloc_steal(new_ctx, discard_const_p(void, *pptr));
	(*pptr) = NULL;
	return ret;
}

enum talloc_mem_count_type {
	TOTAL_MEM_SIZE,
	TOTAL_MEM_BLOCKS,
	TOTAL_MEM_LIMIT,
};

static inline size_t _talloc_total_mem_internal(const void *ptr,
					 enum talloc_mem_count_type type,
					 struct talloc_memlimit *old_limit,
					 struct talloc_memlimit *new_limit)
{
	size_t total = 0;
	struct talloc_chunk *c, *tc;

	if (ptr == NULL) {
		ptr = null_context;
	}
	if (ptr == NULL) {
		return 0;
	}

	tc = talloc_chunk_from_ptr(ptr);

	if (old_limit || new_limit) {
		if (tc->limit && tc->limit->upper == old_limit) {
			tc->limit->upper = new_limit;
		}
	}

	/* optimize in the memlimits case */
	if (type == TOTAL_MEM_LIMIT &&
	    tc->limit != NULL &&
	    tc->limit != old_limit &&
	    tc->limit->parent == tc) {
		return tc->limit->cur_size;
	}

	if (tc->flags & TALLOC_FLAG_LOOP) {
		return 0;
	}

	tc->flags |= TALLOC_FLAG_LOOP;

	if (old_limit || new_limit) {
		if (old_limit == tc->limit) {
			tc->limit = new_limit;
		}
	}

	switch (type) {
	case TOTAL_MEM_SIZE:
		if (likely(tc->name != TALLOC_MAGIC_REFERENCE)) {
			total = tc->size;
		}
		break;
	case TOTAL_MEM_BLOCKS:
		total++;
		break;
	case TOTAL_MEM_LIMIT:
		if (likely(tc->name != TALLOC_MAGIC_REFERENCE)) {
			/*
			 * Don't count memory allocated from a pool
			 * when calculating limits. Only count the
			 * pool itself.
			 */
			if (!(tc->flags & TALLOC_FLAG_POOLMEM)) {
				if (tc->flags & TALLOC_FLAG_POOL) {
					/*
					 * If this is a pool, the allocated
					 * size is in the pool header, and
					 * remember to add in the prefix
					 * length.
					 */
					struct talloc_pool_hdr *pool_hdr
							= talloc_pool_from_chunk(tc);
					total = pool_hdr->poolsize +
							TC_HDR_SIZE +
							TP_HDR_SIZE;
				} else {
					total = tc->size + TC_HDR_SIZE;
				}
			}
		}
		break;
	}
	for (c = tc->child; c; c = c->next) {
		total += _talloc_total_mem_internal(TC_PTR_FROM_CHUNK(c), type,
						    old_limit, new_limit);
	}

	tc->flags &= ~TALLOC_FLAG_LOOP;

	return total;
}

/*
  return the total size of a talloc pool (subtree)
*/
_PUBLIC_ size_t talloc_total_size(const void *ptr)
{
	return _talloc_total_mem_internal(ptr, TOTAL_MEM_SIZE, NULL, NULL);
}

/*
  return the total number of blocks in a talloc pool (subtree)
*/
_PUBLIC_ size_t talloc_total_blocks(const void *ptr)
{
	return _talloc_total_mem_internal(ptr, TOTAL_MEM_BLOCKS, NULL, NULL);
}

/*
  return the number of external references to a pointer
*/
_PUBLIC_ size_t talloc_reference_count(const void *ptr)
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ptr);
	struct talloc_reference_handle *h;
	size_t ret = 0;

	for (h=tc->refs;h;h=h->next) {
		ret++;
	}
	return ret;
}

/*
  report on memory usage by all children of a pointer, giving a full tree view
*/
_PUBLIC_ void talloc_report_depth_cb(const void *ptr, int depth, int max_depth,
			    void (*callback)(const void *ptr,
			  		     int depth, int max_depth,
					     int is_ref,
					     void *private_data),
			    void *private_data)
{
	struct talloc_chunk *c, *tc;

	if (ptr == NULL) {
		ptr = null_context;
	}
	if (ptr == NULL) return;

	tc = talloc_chunk_from_ptr(ptr);

	if (tc->flags & TALLOC_FLAG_LOOP) {
		return;
	}

	callback(ptr, depth, max_depth, 0, private_data);

	if (max_depth >= 0 && depth >= max_depth) {
		return;
	}

	tc->flags |= TALLOC_FLAG_LOOP;
	for (c=tc->child;c;c=c->next) {
		if (c->name == TALLOC_MAGIC_REFERENCE) {
			struct talloc_reference_handle *h = (struct talloc_reference_handle *)TC_PTR_FROM_CHUNK(c);
			callback(h->ptr, depth + 1, max_depth, 1, private_data);
		} else {
			talloc_report_depth_cb(TC_PTR_FROM_CHUNK(c), depth + 1, max_depth, callback, private_data);
		}
	}
	tc->flags &= ~TALLOC_FLAG_LOOP;
}

static void talloc_report_depth_FILE_helper(const void *ptr, int depth, int max_depth, int is_ref, void *_f)
{
	const char *name = __talloc_get_name(ptr);
	struct talloc_chunk *tc;
	FILE *f = (FILE *)_f;

	if (is_ref) {
		fprintf(f, "%*sreference to: %s\n", depth*4, "", name);
		return;
	}

	tc = talloc_chunk_from_ptr(ptr);
	if (tc->limit && tc->limit->parent == tc) {
		fprintf(f, "%*s%-30s is a memlimit context"
			" (max_size = %lu bytes, cur_size = %lu bytes)\n",
			depth*4, "",
			name,
			(unsigned long)tc->limit->max_size,
			(unsigned long)tc->limit->cur_size);
	}

	if (depth == 0) {
		fprintf(f,"%stalloc report on '%s' (total %6lu bytes in %3lu blocks)\n",
			(max_depth < 0 ? "full " :""), name,
			(unsigned long)talloc_total_size(ptr),
			(unsigned long)talloc_total_blocks(ptr));
		return;
	}

	fprintf(f, "%*s%-30s contains %6lu bytes in %3lu blocks (ref %d) %p\n",
		depth*4, "",
		name,
		(unsigned long)talloc_total_size(ptr),
		(unsigned long)talloc_total_blocks(ptr),
		(int)talloc_reference_count(ptr), ptr);

#if 0
	fprintf(f, "content: ");
	if (talloc_total_size(ptr)) {
		int tot = talloc_total_size(ptr);
		int i;

		for (i = 0; i < tot; i++) {
			if ((((char *)ptr)[i] > 31) && (((char *)ptr)[i] < 126)) {
				fprintf(f, "%c", ((char *)ptr)[i]);
			} else {
				fprintf(f, "~%02x", ((char *)ptr)[i]);
			}
		}
	}
	fprintf(f, "\n");
#endif
}

/*
  report on memory usage by all children of a pointer, giving a full tree view
*/
_PUBLIC_ void talloc_report_depth_file(const void *ptr, int depth, int max_depth, FILE *f)
{
	if (f) {
		talloc_report_depth_cb(ptr, depth, max_depth, talloc_report_depth_FILE_helper, f);
		fflush(f);
	}
}

/*
  report on memory usage by all children of a pointer, giving a full tree view
*/
_PUBLIC_ void talloc_report_full(const void *ptr, FILE *f)
{
	talloc_report_depth_file(ptr, 0, -1, f);
}

/*
  report on memory usage by all children of a pointer
*/
_PUBLIC_ void talloc_report(const void *ptr, FILE *f)
{
	talloc_report_depth_file(ptr, 0, 1, f);
}

/*
  enable tracking of the NULL context
*/
_PUBLIC_ void talloc_enable_null_tracking(void)
{
	if (null_context == NULL) {
		null_context = _talloc_named_const(NULL, 0, "null_context");
		if (autofree_context != NULL) {
			talloc_reparent(NULL, null_context, autofree_context);
		}
	}
}

/*
  enable tracking of the NULL context, not moving the autofree context
  into the NULL context. This is needed for the talloc testsuite
*/
_PUBLIC_ void talloc_enable_null_tracking_no_autofree(void)
{
	if (null_context == NULL) {
		null_context = _talloc_named_const(NULL, 0, "null_context");
	}
}

/*
  disable tracking of the NULL context
*/
_PUBLIC_ void talloc_disable_null_tracking(void)
{
	if (null_context != NULL) {
		/* we have to move any children onto the real NULL
		   context */
		struct talloc_chunk *tc, *tc2;
		tc = talloc_chunk_from_ptr(null_context);
		for (tc2 = tc->child; tc2; tc2=tc2->next) {
			if (tc2->parent == tc) tc2->parent = NULL;
			if (tc2->prev == tc) tc2->prev = NULL;
		}
		for (tc2 = tc->next; tc2; tc2=tc2->next) {
			if (tc2->parent == tc) tc2->parent = NULL;
			if (tc2->prev == tc) tc2->prev = NULL;
		}
		tc->child = NULL;
		tc->next = NULL;
	}
	talloc_free(null_context);
	null_context = NULL;
}

/*
  enable leak reporting on exit
*/
_PUBLIC_ void talloc_enable_leak_report(void)
{
	talloc_enable_null_tracking();
	talloc_report_null = true;
	talloc_setup_atexit();
}

/*
  enable full leak reporting on exit
*/
_PUBLIC_ void talloc_enable_leak_report_full(void)
{
	talloc_enable_null_tracking();
	talloc_report_null_full = true;
	talloc_setup_atexit();
}

/*
   talloc and zero memory.
*/
_PUBLIC_ void *_talloc_zero(const void *ctx, size_t size, const char *name)
{
	void *p = _talloc_named_const(ctx, size, name);

	if (p) {
		memset(p, '\0', size);
	}

	return p;
}

/*
  memdup with a talloc.
*/
_PUBLIC_ void *_talloc_memdup(const void *t, const void *p, size_t size, const char *name)
{
	void *newp = _talloc_named_const(t, size, name);

	if (likely(newp)) {
		memcpy(newp, p, size);
	}

	return newp;
}

static inline char *__talloc_strlendup(const void *t, const char *p, size_t len)
{
	char *ret;
	struct talloc_chunk *tc;

	ret = (char *)__talloc(t, len + 1, &tc);
	if (unlikely(!ret)) return NULL;

	memcpy(ret, p, len);
	ret[len] = 0;

	_tc_set_name_const(tc, ret);
	return ret;
}

/*
  strdup with a talloc
*/
_PUBLIC_ char *talloc_strdup(const void *t, const char *p)
{
	if (unlikely(!p)) return NULL;
	return __talloc_strlendup(t, p, strlen(p));
}

/*
  strndup with a talloc
*/
_PUBLIC_ char *talloc_strndup(const void *t, const char *p, size_t n)
{
	if (unlikely(!p)) return NULL;
	return __talloc_strlendup(t, p, strnlen(p, n));
}

static inline char *__talloc_strlendup_append(char *s, size_t slen,
					      const char *a, size_t alen)
{
	char *ret;

	ret = talloc_realloc(NULL, s, char, slen + alen + 1);
	if (unlikely(!ret)) return NULL;

	/* append the string and the trailing \0 */
	memcpy(&ret[slen], a, alen);
	ret[slen+alen] = 0;

	_tc_set_name_const(talloc_chunk_from_ptr(ret), ret);
	return ret;
}

/*
 * Appends at the end of the string.
 */
_PUBLIC_ char *talloc_strdup_append(char *s, const char *a)
{
	if (unlikely(!s)) {
		return talloc_strdup(NULL, a);
	}

	if (unlikely(!a)) {
		return s;
	}

	return __talloc_strlendup_append(s, strlen(s), a, strlen(a));
}

/*
 * Appends at the end of the talloc'ed buffer,
 * not the end of the string.
 */
_PUBLIC_ char *talloc_strdup_append_buffer(char *s, const char *a)
{
	size_t slen;

	if (unlikely(!s)) {
		return talloc_strdup(NULL, a);
	}

	if (unlikely(!a)) {
		return s;
	}

	slen = talloc_get_size(s);
	if (likely(slen > 0)) {
		slen--;
	}

	return __talloc_strlendup_append(s, slen, a, strlen(a));
}

/*
 * Appends at the end of the string.
 */
_PUBLIC_ char *talloc_strndup_append(char *s, const char *a, size_t n)
{
	if (unlikely(!s)) {
		return talloc_strndup(NULL, a, n);
	}

	if (unlikely(!a)) {
		return s;
	}

	return __talloc_strlendup_append(s, strlen(s), a, strnlen(a, n));
}

/*
 * Appends at the end of the talloc'ed buffer,
 * not the end of the string.
 */
_PUBLIC_ char *talloc_strndup_append_buffer(char *s, const char *a, size_t n)
{
	size_t slen;

	if (unlikely(!s)) {
		return talloc_strndup(NULL, a, n);
	}

	if (unlikely(!a)) {
		return s;
	}

	slen = talloc_get_size(s);
	if (likely(slen > 0)) {
		slen--;
	}

	return __talloc_strlendup_append(s, slen, a, strnlen(a, n));
}

static struct talloc_chunk *_vasprintf_tc(const void *t,
					  const char *fmt,
					  va_list ap) PRINTF_ATTRIBUTE(2,0);

static struct talloc_chunk *_vasprintf_tc(const void *t,
					  const char *fmt,
					  va_list ap)
{
	int vlen;
	size_t len;
	char *ret;
	va_list ap2;
	struct talloc_chunk *tc;
	char buf[1024];

	/* this call looks strange, but it makes it work on older solaris boxes */
	va_copy(ap2, ap);
	vlen = vsnprintf(buf, sizeof(buf), fmt, ap2);
	va_end(ap2);
	if (unlikely(vlen < 0)) {
		return NULL;
	}
	len = vlen;
	if (unlikely(len + 1 < len)) {
		return NULL;
	}

	ret = (char *)__talloc(t, len+1, &tc);
	if (unlikely(!ret)) return NULL;

	if (len < sizeof(buf)) {
		memcpy(ret, buf, len+1);
	} else {
		va_copy(ap2, ap);
		vsnprintf(ret, len+1, fmt, ap2);
		va_end(ap2);
	}

	_tc_set_name_const(tc, ret);
	return tc;
}

_PUBLIC_ char *talloc_vasprintf(const void *t, const char *fmt, va_list ap)
{
	struct talloc_chunk *tc = _vasprintf_tc(t, fmt, ap);
	if (tc == NULL) {
		return NULL;
	}
	return TC_PTR_FROM_CHUNK(tc);
}


/*
  Perform string formatting, and return a pointer to newly allocated
  memory holding the result, inside a memory pool.
 */
_PUBLIC_ char *talloc_asprintf(const void *t, const char *fmt, ...)
{
	va_list ap;
	char *ret;

	va_start(ap, fmt);
	ret = talloc_vasprintf(t, fmt, ap);
	va_end(ap);
	return ret;
}

static inline char *__talloc_vaslenprintf_append(char *s, size_t slen,
						 const char *fmt, va_list ap)
						 PRINTF_ATTRIBUTE(3,0);

static inline char *__talloc_vaslenprintf_append(char *s, size_t slen,
						 const char *fmt, va_list ap)
{
	ssize_t alen;
	va_list ap2;
	char c;

	va_copy(ap2, ap);
	alen = vsnprintf(&c, 1, fmt, ap2);
	va_end(ap2);

	if (alen <= 0) {
		/* Either the vsnprintf failed or the format resulted in
		 * no characters being formatted. In the former case, we
		 * ought to return NULL, in the latter we ought to return
		 * the original string. Most current callers of this
		 * function expect it to never return NULL.
		 */
		return s;
	}

	s = talloc_realloc(NULL, s, char, slen + alen + 1);
	if (!s) return NULL;

	va_copy(ap2, ap);
	vsnprintf(s + slen, alen + 1, fmt, ap2);
	va_end(ap2);

	_tc_set_name_const(talloc_chunk_from_ptr(s), s);
	return s;
}

/**
 * Realloc @p s to append the formatted result of @p fmt and @p ap,
 * and return @p s, which may have moved.  Good for gradually
 * accumulating output into a string buffer. Appends at the end
 * of the string.
 **/
_PUBLIC_ char *talloc_vasprintf_append(char *s, const char *fmt, va_list ap)
{
	if (unlikely(!s)) {
		return talloc_vasprintf(NULL, fmt, ap);
	}

	return __talloc_vaslenprintf_append(s, strlen(s), fmt, ap);
}

/**
 * Realloc @p s to append the formatted result of @p fmt and @p ap,
 * and return @p s, which may have moved. Always appends at the
 * end of the talloc'ed buffer, not the end of the string.
 **/
_PUBLIC_ char *talloc_vasprintf_append_buffer(char *s, const char *fmt, va_list ap)
{
	size_t slen;

	if (unlikely(!s)) {
		return talloc_vasprintf(NULL, fmt, ap);
	}

	slen = talloc_get_size(s);
	if (likely(slen > 0)) {
		slen--;
	}

	return __talloc_vaslenprintf_append(s, slen, fmt, ap);
}

/*
  Realloc @p s to append the formatted result of @p fmt and return @p
  s, which may have moved.  Good for gradually accumulating output
  into a string buffer.
 */
_PUBLIC_ char *talloc_asprintf_append(char *s, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	s = talloc_vasprintf_append(s, fmt, ap);
	va_end(ap);
	return s;
}

/*
  Realloc @p s to append the formatted result of @p fmt and return @p
  s, which may have moved.  Good for gradually accumulating output
  into a buffer.
 */
_PUBLIC_ char *talloc_asprintf_append_buffer(char *s, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	s = talloc_vasprintf_append_buffer(s, fmt, ap);
	va_end(ap);
	return s;
}

/*
  alloc an array, checking for integer overflow in the array size
*/
_PUBLIC_ void *_talloc_array(const void *ctx, size_t el_size, unsigned count, const char *name)
{
	if (count >= MAX_TALLOC_SIZE/el_size) {
		return NULL;
	}
	return _talloc_named_const(ctx, el_size * count, name);
}

/*
  alloc an zero array, checking for integer overflow in the array size
*/
_PUBLIC_ void *_talloc_zero_array(const void *ctx, size_t el_size, unsigned count, const char *name)
{
	if (count >= MAX_TALLOC_SIZE/el_size) {
		return NULL;
	}
	return _talloc_zero(ctx, el_size * count, name);
}

/*
  realloc an array, checking for integer overflow in the array size
*/
_PUBLIC_ void *_talloc_realloc_array(const void *ctx, void *ptr, size_t el_size, unsigned count, const char *name)
{
	if (count >= MAX_TALLOC_SIZE/el_size) {
		return NULL;
	}
	return _talloc_realloc(ctx, ptr, el_size * count, name);
}

/*
  a function version of talloc_realloc(), so it can be passed as a function pointer
  to libraries that want a realloc function (a realloc function encapsulates
  all the basic capabilities of an allocation library, which is why this is useful)
*/
_PUBLIC_ void *talloc_realloc_fn(const void *context, void *ptr, size_t size)
{
	return _talloc_realloc(context, ptr, size, NULL);
}


static int talloc_autofree_destructor(void *ptr)
{
	autofree_context = NULL;
	return 0;
}

/*
  return a context which will be auto-freed on exit
  this is useful for reducing the noise in leak reports
*/
_PUBLIC_ void *talloc_autofree_context(void)
{
	if (autofree_context == NULL) {
		autofree_context = _talloc_named_const(NULL, 0, "autofree_context");
		talloc_set_destructor(autofree_context, talloc_autofree_destructor);
		talloc_setup_atexit();
	}
	return autofree_context;
}

_PUBLIC_ size_t talloc_get_size(const void *context)
{
	struct talloc_chunk *tc;

	if (context == NULL) {
		return 0;
	}

	tc = talloc_chunk_from_ptr(context);

	return tc->size;
}

/*
  find a parent of this context that has the given name, if any
*/
_PUBLIC_ void *talloc_find_parent_byname(const void *context, const char *name)
{
	struct talloc_chunk *tc;

	if (context == NULL) {
		return NULL;
	}

	tc = talloc_chunk_from_ptr(context);
	while (tc) {
		if (tc->name && strcmp(tc->name, name) == 0) {
			return TC_PTR_FROM_CHUNK(tc);
		}
		while (tc && tc->prev) tc = tc->prev;
		if (tc) {
			tc = tc->parent;
		}
	}
	return NULL;
}

/*
  show the parentage of a context
*/
_PUBLIC_ void talloc_show_parents(const void *context, FILE *file)
{
	struct talloc_chunk *tc;

	if (context == NULL) {
		fprintf(file, "talloc no parents for NULL\n");
		return;
	}

	tc = talloc_chunk_from_ptr(context);
	fprintf(file, "talloc parents of '%s'\n", __talloc_get_name(context));
	while (tc) {
		fprintf(file, "\t'%s'\n", __talloc_get_name(TC_PTR_FROM_CHUNK(tc)));
		while (tc && tc->prev) tc = tc->prev;
		if (tc) {
			tc = tc->parent;
		}
	}
	fflush(file);
}

/*
  return 1 if ptr is a parent of context
*/
static int _talloc_is_parent(const void *context, const void *ptr, int depth)
{
	struct talloc_chunk *tc;

	if (context == NULL) {
		return 0;
	}

	tc = talloc_chunk_from_ptr(context);
	while (tc) {
		if (depth <= 0) {
			return 0;
		}
		if (TC_PTR_FROM_CHUNK(tc) == ptr) return 1;
		while (tc && tc->prev) tc = tc->prev;
		if (tc) {
			tc = tc->parent;
			depth--;
		}
	}
	return 0;
}

/*
  return 1 if ptr is a parent of context
*/
_PUBLIC_ int talloc_is_parent(const void *context, const void *ptr)
{
	return _talloc_is_parent(context, ptr, TALLOC_MAX_DEPTH);
}

/*
  return the total size of memory used by this context and all children
*/
static inline size_t _talloc_total_limit_size(const void *ptr,
					struct talloc_memlimit *old_limit,
					struct talloc_memlimit *new_limit)
{
	return _talloc_total_mem_internal(ptr, TOTAL_MEM_LIMIT,
					  old_limit, new_limit);
}

static inline bool talloc_memlimit_check(struct talloc_memlimit *limit, size_t size)
{
	struct talloc_memlimit *l;

	for (l = limit; l != NULL; l = l->upper) {
		if (l->max_size != 0 &&
		    ((l->max_size <= l->cur_size) ||
		     (l->max_size - l->cur_size < size))) {
			return false;
		}
	}

	return true;
}

/*
  Update memory limits when freeing a talloc_chunk.
*/
static void tc_memlimit_update_on_free(struct talloc_chunk *tc)
{
	size_t limit_shrink_size;

	if (!tc->limit) {
		return;
	}

	/*
	 * Pool entries don't count. Only the pools
	 * themselves are counted as part of the memory
	 * limits. Note that this also takes care of
	 * nested pools which have both flags
	 * TALLOC_FLAG_POOLMEM|TALLOC_FLAG_POOL set.
	 */
	if (tc->flags & TALLOC_FLAG_POOLMEM) {
		return;
	}

	/*
	 * If we are part of a memory limited context hierarchy
	 * we need to subtract the memory used from the counters
	 */

	limit_shrink_size = tc->size+TC_HDR_SIZE;

	/*
	 * If we're deallocating a pool, take into
	 * account the prefix size added for the pool.
	 */

	if (tc->flags & TALLOC_FLAG_POOL) {
		limit_shrink_size += TP_HDR_SIZE;
	}

	talloc_memlimit_shrink(tc->limit, limit_shrink_size);

	if (tc->limit->parent == tc) {
		free(tc->limit);
	}

	tc->limit = NULL;
}

/*
  Increase memory limit accounting after a malloc/realloc.
*/
static void talloc_memlimit_grow(struct talloc_memlimit *limit,
				size_t size)
{
	struct talloc_memlimit *l;

	for (l = limit; l != NULL; l = l->upper) {
		size_t new_cur_size = l->cur_size + size;
		if (new_cur_size < l->cur_size) {
			talloc_abort("logic error in talloc_memlimit_grow\n");
			return;
		}
		l->cur_size = new_cur_size;
	}
}

/*
  Decrease memory limit accounting after a free/realloc.
*/
static void talloc_memlimit_shrink(struct talloc_memlimit *limit,
				size_t size)
{
	struct talloc_memlimit *l;

	for (l = limit; l != NULL; l = l->upper) {
		if (l->cur_size < size) {
			talloc_abort("logic error in talloc_memlimit_shrink\n");
			return;
		}
		l->cur_size = l->cur_size - size;
	}
}

_PUBLIC_ int talloc_set_memlimit(const void *ctx, size_t max_size)
{
	struct talloc_chunk *tc = talloc_chunk_from_ptr(ctx);
	struct talloc_memlimit *orig_limit;
	struct talloc_memlimit *limit = NULL;

	if (tc->limit && tc->limit->parent == tc) {
		tc->limit->max_size = max_size;
		return 0;
	}
	orig_limit = tc->limit;

	limit = malloc(sizeof(struct talloc_memlimit));
	if (limit == NULL) {
		return 1;
	}
	limit->parent = tc;
	limit->max_size = max_size;
	limit->cur_size = _talloc_total_limit_size(ctx, tc->limit, limit);

	if (orig_limit) {
		limit->upper = orig_limit;
	} else {
		limit->upper = NULL;
	}

	return 0;
}
