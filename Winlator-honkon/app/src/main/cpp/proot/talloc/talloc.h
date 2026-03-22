#ifndef _TALLOC_H_
#define _TALLOC_H_
/*
   Unix SMB/CIFS implementation.
   Samba temporary memory allocation functions

   Copyright (C) Andrew Tridgell 2004-2005
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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup talloc The talloc API
 *
 * talloc is a hierarchical, reference counted memory pool system with
 * destructors. It is the core memory allocator used in Samba.
 *
 * @{
 */

#define TALLOC_VERSION_MAJOR 2
#define TALLOC_VERSION_MINOR 1

int talloc_version_major(void);
int talloc_version_minor(void);
/* This is mostly useful only for testing */
int talloc_test_get_magic(void);

/**
 * @brief Define a talloc parent type
 *
 * As talloc is a hierarchial memory allocator, every talloc chunk is a
 * potential parent to other talloc chunks. So defining a separate type for a
 * talloc chunk is not strictly necessary. TALLOC_CTX is defined nevertheless,
 * as it provides an indicator for function arguments. You will frequently
 * write code like
 *
 * @code
 *      struct foo *foo_create(TALLOC_CTX *mem_ctx)
 *      {
 *              struct foo *result;
 *              result = talloc(mem_ctx, struct foo);
 *              if (result == NULL) return NULL;
 *                      ... initialize foo ...
 *              return result;
 *      }
 * @endcode
 *
 * In this type of allocating functions it is handy to have a general
 * TALLOC_CTX type to indicate which parent to put allocated structures on.
 */
typedef void TALLOC_CTX;

/*
  this uses a little trick to allow __LINE__ to be stringified
*/
#ifndef __location__
#define __TALLOC_STRING_LINE1__(s)    #s
#define __TALLOC_STRING_LINE2__(s)   __TALLOC_STRING_LINE1__(s)
#define __TALLOC_STRING_LINE3__  __TALLOC_STRING_LINE2__(__LINE__)
#define __location__ __FILE__ ":" __TALLOC_STRING_LINE3__
#endif

#ifndef TALLOC_DEPRECATED
#define TALLOC_DEPRECATED 0
#endif

#ifndef PRINTF_ATTRIBUTE
#if (__GNUC__ >= 3)
/** Use gcc attribute to check printf fns.  a1 is the 1-based index of
 * the parameter containing the format, and a2 the index of the first
 * argument. Note that some gcc 2.x versions don't handle this
 * properly **/
#define PRINTF_ATTRIBUTE(a1, a2) __attribute__ ((format (__printf__, a1, a2)))
#else
#define PRINTF_ATTRIBUTE(a1, a2)
#endif
#endif

#ifdef DOXYGEN
/**
 * @brief Create a new talloc context.
 *
 * The talloc() macro is the core of the talloc library. It takes a memory
 * context and a type, and returns a pointer to a new area of memory of the
 * given type.
 *
 * The returned pointer is itself a talloc context, so you can use it as the
 * context argument to more calls to talloc if you wish.
 *
 * The returned pointer is a "child" of the supplied context. This means that if
 * you talloc_free() the context then the new child disappears as well.
 * Alternatively you can free just the child.
 *
 * @param[in]  ctx      A talloc context to create a new reference on or NULL to
 *                      create a new top level context.
 *
 * @param[in]  type     The type of memory to allocate.
 *
 * @return              A type casted talloc context or NULL on error.
 *
 * @code
 *      unsigned int *a, *b;
 *
 *      a = talloc(NULL, unsigned int);
 *      b = talloc(a, unsigned int);
 * @endcode
 *
 * @see talloc_zero
 * @see talloc_array
 * @see talloc_steal
 * @see talloc_free
 */
void *talloc(const void *ctx, #type);
#else
#define talloc(ctx, type) (type *)talloc_named_const(ctx, sizeof(type), #type)
void *_talloc(const void *context, size_t size);
#endif

/**
 * @brief Create a new top level talloc context.
 *
 * This function creates a zero length named talloc context as a top level
 * context. It is equivalent to:
 *
 * @code
 *      talloc_named(NULL, 0, fmt, ...);
 * @endcode
 * @param[in]  fmt      Format string for the name.
 *
 * @param[in]  ...      Additional printf-style arguments.
 *
 * @return              The allocated memory chunk, NULL on error.
 *
 * @see talloc_named()
 */
void *talloc_init(const char *fmt, ...) PRINTF_ATTRIBUTE(1,2);

#ifdef DOXYGEN
/**
 * @brief Free a chunk of talloc memory.
 *
 * The talloc_free() function frees a piece of talloc memory, and all its
 * children. You can call talloc_free() on any pointer returned by
 * talloc().
 *
 * The return value of talloc_free() indicates success or failure, with 0
 * returned for success and -1 for failure. A possible failure condition
 * is if the pointer had a destructor attached to it and the destructor
 * returned -1. See talloc_set_destructor() for details on
 * destructors. Likewise, if "ptr" is NULL, then the function will make
 * no modifications and return -1.
 *
 * From version 2.0 and onwards, as a special case, talloc_free() is
 * refused on pointers that have more than one parent associated, as talloc
 * would have no way of knowing which parent should be removed. This is
 * different from older versions in the sense that always the reference to
 * the most recently established parent has been destroyed. Hence to free a
 * pointer that has more than one parent please use talloc_unlink().
 *
 * To help you find problems in your code caused by this behaviour, if
 * you do try and free a pointer with more than one parent then the
 * talloc logging function will be called to give output like this:
 *
 * @code
 *   ERROR: talloc_free with references at some_dir/source/foo.c:123
 *     reference at some_dir/source/other.c:325
 *     reference at some_dir/source/third.c:121
 * @endcode
 *
 * Please see the documentation for talloc_set_log_fn() and
 * talloc_set_log_stderr() for more information on talloc logging
 * functions.
 *
 * If <code>TALLOC_FREE_FILL</code> environment variable is set,
 * the memory occupied by the context is filled with the value of this variable.
 * The value should be a numeric representation of the character you want to
 * use.
 *
 * talloc_free() operates recursively on its children.
 *
 * @param[in]  ptr      The chunk to be freed.
 *
 * @return              Returns 0 on success and -1 on error. A possible
 *                      failure condition is if the pointer had a destructor
 *                      attached to it and the destructor returned -1. Likewise,
 *                      if "ptr" is NULL, then the function will make no
 *                      modifications and returns -1.
 *
 * Example:
 * @code
 *      unsigned int *a, *b;
 *      a = talloc(NULL, unsigned int);
 *      b = talloc(a, unsigned int);
 *
 *      talloc_free(a); // Frees a and b
 * @endcode
 *
 * @see talloc_set_destructor()
 * @see talloc_unlink()
 */
int talloc_free(void *ptr);
#else
#define talloc_free(ctx) _talloc_free(ctx, __location__)
int _talloc_free(void *ptr, const char *location);
#endif

/**
 * @brief Free a talloc chunk's children.
 *
 * The function walks along the list of all children of a talloc context and
 * talloc_free()s only the children, not the context itself.
 *
 * A NULL argument is handled as no-op.
 *
 * @param[in]  ptr      The chunk that you want to free the children of
 *                      (NULL is allowed too)
 */
void talloc_free_children(void *ptr);

#ifdef DOXYGEN
/**
 * @brief Assign a destructor function to be called when a chunk is freed.
 *
 * The function talloc_set_destructor() sets the "destructor" for the pointer
 * "ptr". A destructor is a function that is called when the memory used by a
 * pointer is about to be released. The destructor receives the pointer as an
 * argument, and should return 0 for success and -1 for failure.
 *
 * The destructor can do anything it wants to, including freeing other pieces
 * of memory. A common use for destructors is to clean up operating system
 * resources (such as open file descriptors) contained in the structure the
 * destructor is placed on.
 *
 * You can only place one destructor on a pointer. If you need more than one
 * destructor then you can create a zero-length child of the pointer and place
 * an additional destructor on that.
 *
 * To remove a destructor call talloc_set_destructor() with NULL for the
 * destructor.
 *
 * If your destructor attempts to talloc_free() the pointer that it is the
 * destructor for then talloc_free() will return -1 and the free will be
 * ignored. This would be a pointless operation anyway, as the destructor is
 * only called when the memory is just about to go away.
 *
 * @param[in]  ptr      The talloc chunk to add a destructor to.
 *
 * @param[in]  destructor  The destructor function to be called. NULL to remove
 *                         it.
 *
 * Example:
 * @code
 *      static int destroy_fd(int *fd) {
 *              close(*fd);
 *              return 0;
 *      }
 *
 *      int *open_file(const char *filename) {
 *              int *fd = talloc(NULL, int);
 *              *fd = open(filename, O_RDONLY);
 *              if (*fd < 0) {
 *                      talloc_free(fd);
 *                      return NULL;
 *              }
 *              // Whenever they free this, we close the file.
 *              talloc_set_destructor(fd, destroy_fd);
 *              return fd;
 *      }
 * @endcode
 *
 * @see talloc()
 * @see talloc_free()
 */
void talloc_set_destructor(const void *ptr, int (*destructor)(void *));

/**
 * @brief Change a talloc chunk's parent.
 *
 * The talloc_steal() function changes the parent context of a talloc
 * pointer. It is typically used when the context that the pointer is
 * currently a child of is going to be freed and you wish to keep the
 * memory for a longer time.
 *
 * To make the changed hierarchy less error-prone, you might consider to use
 * talloc_move().
 *
 * If you try and call talloc_steal() on a pointer that has more than one
 * parent then the result is ambiguous. Talloc will choose to remove the
 * parent that is currently indicated by talloc_parent() and replace it with
 * the chosen parent. You will also get a message like this via the talloc
 * logging functions:
 *
 * @code
 *   WARNING: talloc_steal with references at some_dir/source/foo.c:123
 *     reference at some_dir/source/other.c:325
 *     reference at some_dir/source/third.c:121
 * @endcode
 *
 * To unambiguously change the parent of a pointer please see the function
 * talloc_reparent(). See the talloc_set_log_fn() documentation for more
 * information on talloc logging.
 *
 * @param[in]  new_ctx  The new parent context.
 *
 * @param[in]  ptr      The talloc chunk to move.
 *
 * @return              Returns the pointer that you pass it. It does not have
 *                      any failure modes.
 *
 * @note It is possible to produce loops in the parent/child relationship
 * if you are not careful with talloc_steal(). No guarantees are provided
 * as to your sanity or the safety of your data if you do this.
 */
void *talloc_steal(const void *new_ctx, const void *ptr);
#else /* DOXYGEN */
/* try to make talloc_set_destructor() and talloc_steal() type safe,
   if we have a recent gcc */
#if (__GNUC__ >= 3)
#define _TALLOC_TYPEOF(ptr) __typeof__(ptr)
#define talloc_set_destructor(ptr, function)				      \
	do {								      \
		int (*_talloc_destructor_fn)(_TALLOC_TYPEOF(ptr)) = (function);	      \
		_talloc_set_destructor((ptr), (int (*)(void *))_talloc_destructor_fn); \
	} while(0)
/* this extremely strange macro is to avoid some braindamaged warning
   stupidity in gcc 4.1.x */
#define talloc_steal(ctx, ptr) ({ _TALLOC_TYPEOF(ptr) __talloc_steal_ret = (_TALLOC_TYPEOF(ptr))_talloc_steal_loc((ctx),(ptr), __location__); __talloc_steal_ret; })
#else /* __GNUC__ >= 3 */
#define talloc_set_destructor(ptr, function) \
	_talloc_set_destructor((ptr), (int (*)(void *))(function))
#define _TALLOC_TYPEOF(ptr) void *
#define talloc_steal(ctx, ptr) (_TALLOC_TYPEOF(ptr))_talloc_steal_loc((ctx),(ptr), __location__)
#endif /* __GNUC__ >= 3 */
void _talloc_set_destructor(const void *ptr, int (*_destructor)(void *));
void *_talloc_steal_loc(const void *new_ctx, const void *ptr, const char *location);
#endif /* DOXYGEN */

/**
 * @brief Assign a name to a talloc chunk.
 *
 * Each talloc pointer has a "name". The name is used principally for
 * debugging purposes, although it is also possible to set and get the name on
 * a pointer in as a way of "marking" pointers in your code.
 *
 * The main use for names on pointer is for "talloc reports". See
 * talloc_report() and talloc_report_full() for details. Also see
 * talloc_enable_leak_report() and talloc_enable_leak_report_full().
 *
 * The talloc_set_name() function allocates memory as a child of the
 * pointer. It is logically equivalent to:
 *
 * @code
 *      talloc_set_name_const(ptr, talloc_asprintf(ptr, fmt, ...));
 * @endcode
 *
 * @param[in]  ptr      The talloc chunk to assign a name to.
 *
 * @param[in]  fmt      Format string for the name.
 *
 * @param[in]  ...      Add printf-style additional arguments.
 *
 * @return              The assigned name, NULL on error.
 *
 * @note Multiple calls to talloc_set_name() will allocate more memory without
 * releasing the name. All of the memory is released when the ptr is freed
 * using talloc_free().
 */
const char *talloc_set_name(const void *ptr, const char *fmt, ...) PRINTF_ATTRIBUTE(2,3);

#ifdef DOXYGEN
/**
 * @brief Change a talloc chunk's parent.
 *
 * This function has the same effect as talloc_steal(), and additionally sets
 * the source pointer to NULL. You would use it like this:
 *
 * @code
 *      struct foo *X = talloc(tmp_ctx, struct foo);
 *      struct foo *Y;
 *      Y = talloc_move(new_ctx, &X);
 * @endcode
 *
 * @param[in]  new_ctx  The new parent context.
 *
 * @param[in]  pptr     Pointer to a pointer to the talloc chunk to move.
 *
 * @return              The pointer to the talloc chunk that moved.
 *                      It does not have any failure modes.
 *
 */
void *talloc_move(const void *new_ctx, void **pptr);
#else
#define talloc_move(ctx, pptr) (_TALLOC_TYPEOF(*(pptr)))_talloc_move((ctx),(void *)(pptr))
void *_talloc_move(const void *new_ctx, const void *pptr);
#endif

/**
 * @brief Assign a name to a talloc chunk.
 *
 * The function is just like talloc_set_name(), but it takes a string constant,
 * and is much faster. It is extensively used by the "auto naming" macros, such
 * as talloc_p().
 *
 * This function does not allocate any memory. It just copies the supplied
 * pointer into the internal representation of the talloc ptr. This means you
 * must not pass a name pointer to memory that will disappear before the ptr
 * is freed with talloc_free().
 *
 * @param[in]  ptr      The talloc chunk to assign a name to.
 *
 * @param[in]  name     Format string for the name.
 */
void talloc_set_name_const(const void *ptr, const char *name);

/**
 * @brief Create a named talloc chunk.
 *
 * The talloc_named() function creates a named talloc pointer. It is
 * equivalent to:
 *
 * @code
 *      ptr = talloc_size(context, size);
 *      talloc_set_name(ptr, fmt, ....);
 * @endcode
 *
 * @param[in]  context  The talloc context to hang the result off.
 *
 * @param[in]  size     Number of char's that you want to allocate.
 *
 * @param[in]  fmt      Format string for the name.
 *
 * @param[in]  ...      Additional printf-style arguments.
 *
 * @return              The allocated memory chunk, NULL on error.
 *
 * @see talloc_set_name()
 */
void *talloc_named(const void *context, size_t size,
		   const char *fmt, ...) PRINTF_ATTRIBUTE(3,4);

/**
 * @brief Basic routine to allocate a chunk of memory.
 *
 * This is equivalent to:
 *
 * @code
 *      ptr = talloc_size(context, size);
 *      talloc_set_name_const(ptr, name);
 * @endcode
 *
 * @param[in]  context  The parent context.
 *
 * @param[in]  size     The number of char's that we want to allocate.
 *
 * @param[in]  name     The name the talloc block has.
 *
 * @return             The allocated memory chunk, NULL on error.
 */
void *talloc_named_const(const void *context, size_t size, const char *name);

#ifdef DOXYGEN
/**
 * @brief Untyped allocation.
 *
 * The function should be used when you don't have a convenient type to pass to
 * talloc(). Unlike talloc(), it is not type safe (as it returns a void *), so
 * you are on your own for type checking.
 *
 * Best to use talloc() or talloc_array() instead.
 *
 * @param[in]  ctx     The talloc context to hang the result off.
 *
 * @param[in]  size    Number of char's that you want to allocate.
 *
 * @return             The allocated memory chunk, NULL on error.
 *
 * Example:
 * @code
 *      void *mem = talloc_size(NULL, 100);
 * @endcode
 */
void *talloc_size(const void *ctx, size_t size);
#else
#define talloc_size(ctx, size) talloc_named_const(ctx, size, __location__)
#endif

#ifdef DOXYGEN
/**
 * @brief Allocate into a typed pointer.
 *
 * The talloc_ptrtype() macro should be used when you have a pointer and want
 * to allocate memory to point at with this pointer. When compiling with
 * gcc >= 3 it is typesafe. Note this is a wrapper of talloc_size() and
 * talloc_get_name() will return the current location in the source file and
 * not the type.
 *
 * @param[in]  ctx      The talloc context to hang the result off.
 *
 * @param[in]  type     The pointer you want to assign the result to.
 *
 * @return              The properly casted allocated memory chunk, NULL on
 *                      error.
 *
 * Example:
 * @code
 *       unsigned int *a = talloc_ptrtype(NULL, a);
 * @endcode
 */
void *talloc_ptrtype(const void *ctx, #type);
#else
#define talloc_ptrtype(ctx, ptr) (_TALLOC_TYPEOF(ptr))talloc_size(ctx, sizeof(*(ptr)))
#endif

#ifdef DOXYGEN
/**
 * @brief Allocate a new 0-sized talloc chunk.
 *
 * This is a utility macro that creates a new memory context hanging off an
 * existing context, automatically naming it "talloc_new: __location__" where
 * __location__ is the source line it is called from. It is particularly
 * useful for creating a new temporary working context.
 *
 * @param[in]  ctx      The talloc parent context.
 *
 * @return              A new talloc chunk, NULL on error.
 */
void *talloc_new(const void *ctx);
#else
#define talloc_new(ctx) talloc_named_const(ctx, 0, "talloc_new: " __location__)
#endif

#ifdef DOXYGEN
/**
 * @brief Allocate a 0-initizialized structure.
 *
 * The macro is equivalent to:
 *
 * @code
 *      ptr = talloc(ctx, type);
 *      if (ptr) memset(ptr, 0, sizeof(type));
 * @endcode
 *
 * @param[in]  ctx      The talloc context to hang the result off.
 *
 * @param[in]  type     The type that we want to allocate.
 *
 * @return              Pointer to a piece of memory, properly cast to 'type *',
 *                      NULL on error.
 *
 * Example:
 * @code
 *      unsigned int *a, *b;
 *      a = talloc_zero(NULL, unsigned int);
 *      b = talloc_zero(a, unsigned int);
 * @endcode
 *
 * @see talloc()
 * @see talloc_zero_size()
 * @see talloc_zero_array()
 */
void *talloc_zero(const void *ctx, #type);

/**
 * @brief Allocate untyped, 0-initialized memory.
 *
 * @param[in]  ctx      The talloc context to hang the result off.
 *
 * @param[in]  size     Number of char's that you want to allocate.
 *
 * @return              The allocated memory chunk.
 */
void *talloc_zero_size(const void *ctx, size_t size);
#else
#define talloc_zero(ctx, type) (type *)_talloc_zero(ctx, sizeof(type), #type)
#define talloc_zero_size(ctx, size) _talloc_zero(ctx, size, __location__)
void *_talloc_zero(const void *ctx, size_t size, const char *name);
#endif

/**
 * @brief Return the name of a talloc chunk.
 *
 * @param[in]  ptr      The talloc chunk.
 *
 * @return              The current name for the given talloc pointer.
 *
 * @see talloc_set_name()
 */
const char *talloc_get_name(const void *ptr);

/**
 * @brief Verify that a talloc chunk carries a specified name.
 *
 * This function checks if a pointer has the specified name. If it does
 * then the pointer is returned.
 *
 * @param[in]  ptr       The talloc chunk to check.
 *
 * @param[in]  name      The name to check against.
 *
 * @return               The pointer if the name matches, NULL if it doesn't.
 */
void *talloc_check_name(const void *ptr, const char *name);

/**
 * @brief Get the parent chunk of a pointer.
 *
 * @param[in]  ptr      The talloc pointer to inspect.
 *
 * @return              The talloc parent of ptr, NULL on error.
 */
void *talloc_parent(const void *ptr);

/**
 * @brief Get a talloc chunk's parent name.
 *
 * @param[in]  ptr      The talloc pointer to inspect.
 *
 * @return              The name of ptr's parent chunk.
 */
const char *talloc_parent_name(const void *ptr);

/**
 * @brief Get the total size of a talloc chunk including its children.
 *
 * The function returns the total size in bytes used by this pointer and all
 * child pointers. Mostly useful for debugging.
 *
 * Passing NULL is allowed, but it will only give a meaningful result if
 * talloc_enable_leak_report() or talloc_enable_leak_report_full() has
 * been called.
 *
 * @param[in]  ptr      The talloc chunk.
 *
 * @return              The total size.
 */
size_t talloc_total_size(const void *ptr);

/**
 * @brief Get the number of talloc chunks hanging off a chunk.
 *
 * The talloc_total_blocks() function returns the total memory block
 * count used by this pointer and all child pointers. Mostly useful for
 * debugging.
 *
 * Passing NULL is allowed, but it will only give a meaningful result if
 * talloc_enable_leak_report() or talloc_enable_leak_report_full() has
 * been called.
 *
 * @param[in]  ptr      The talloc chunk.
 *
 * @return              The total size.
 */
size_t talloc_total_blocks(const void *ptr);

#ifdef DOXYGEN
/**
 * @brief Duplicate a memory area into a talloc chunk.
 *
 * The function is equivalent to:
 *
 * @code
 *      ptr = talloc_size(ctx, size);
 *      if (ptr) memcpy(ptr, p, size);
 * @endcode
 *
 * @param[in]  t        The talloc context to hang the result off.
 *
 * @param[in]  p        The memory chunk you want to duplicate.
 *
 * @param[in]  size     Number of char's that you want copy.
 *
 * @return              The allocated memory chunk.
 *
 * @see talloc_size()
 */
void *talloc_memdup(const void *t, const void *p, size_t size);
#else
#define talloc_memdup(t, p, size) _talloc_memdup(t, p, size, __location__)
void *_talloc_memdup(const void *t, const void *p, size_t size, const char *name);
#endif

#ifdef DOXYGEN
/**
 * @brief Assign a type to a talloc chunk.
 *
 * This macro allows you to force the name of a pointer to be of a particular
 * type. This can be used in conjunction with talloc_get_type() to do type
 * checking on void* pointers.
 *
 * It is equivalent to this:
 *
 * @code
 *      talloc_set_name_const(ptr, #type)
 * @endcode
 *
 * @param[in]  ptr      The talloc chunk to assign the type to.
 *
 * @param[in]  type     The type to assign.
 */
void talloc_set_type(const char *ptr, #type);

/**
 * @brief Get a typed pointer out of a talloc pointer.
 *
 * This macro allows you to do type checking on talloc pointers. It is
 * particularly useful for void* private pointers. It is equivalent to
 * this:
 *
 * @code
 *      (type *)talloc_check_name(ptr, #type)
 * @endcode
 *
 * @param[in]  ptr      The talloc pointer to check.
 *
 * @param[in]  type     The type to check against.
 *
 * @return              The properly casted pointer given by ptr, NULL on error.
 */
type *talloc_get_type(const void *ptr, #type);
#else
#define talloc_set_type(ptr, type) talloc_set_name_const(ptr, #type)
#define talloc_get_type(ptr, type) (type *)talloc_check_name(ptr, #type)
#endif

#ifdef DOXYGEN
/**
 * @brief Safely turn a void pointer into a typed pointer.
 *
 * This macro is used together with talloc(mem_ctx, struct foo). If you had to
 * assign the talloc chunk pointer to some void pointer variable,
 * talloc_get_type_abort() is the recommended way to get the convert the void
 * pointer back to a typed pointer.
 *
 * @param[in]  ptr      The void pointer to convert.
 *
 * @param[in]  type     The type that this chunk contains
 *
 * @return              The same value as ptr, type-checked and properly cast.
 */
void *talloc_get_type_abort(const void *ptr, #type);
#else
#ifdef TALLOC_GET_TYPE_ABORT_NOOP
#define talloc_get_type_abort(ptr, type) (type *)(ptr)
#else
#define talloc_get_type_abort(ptr, type) (type *)_talloc_get_type_abort(ptr, #type, __location__)
#endif
void *_talloc_get_type_abort(const void *ptr, const char *name, const char *location);
#endif

/**
 * @brief Find a parent context by name.
 *
 * Find a parent memory context of the current context that has the given
 * name. This can be very useful in complex programs where it may be
 * difficult to pass all information down to the level you need, but you
 * know the structure you want is a parent of another context.
 *
 * @param[in]  ctx      The talloc chunk to start from.
 *
 * @param[in]  name     The name of the parent we look for.
 *
 * @return              The memory context we are looking for, NULL if not
 *                      found.
 */
void *talloc_find_parent_byname(const void *ctx, const char *name);

#ifdef DOXYGEN
/**
 * @brief Find a parent context by type.
 *
 * Find a parent memory context of the current context that has the given
 * name. This can be very useful in complex programs where it may be
 * difficult to pass all information down to the level you need, but you
 * know the structure you want is a parent of another context.
 *
 * Like talloc_find_parent_byname() but takes a type, making it typesafe.
 *
 * @param[in]  ptr      The talloc chunk to start from.
 *
 * @param[in]  type     The type of the parent to look for.
 *
 * @return              The memory context we are looking for, NULL if not
 *                      found.
 */
void *talloc_find_parent_bytype(const void *ptr, #type);
#else
#define talloc_find_parent_bytype(ptr, type) (type *)talloc_find_parent_byname(ptr, #type)
#endif

/**
 * @brief Allocate a talloc pool.
 *
 * A talloc pool is a pure optimization for specific situations. In the
 * release process for Samba 3.2 we found out that we had become considerably
 * slower than Samba 3.0 was. Profiling showed that malloc(3) was a large CPU
 * consumer in benchmarks. For Samba 3.2 we have internally converted many
 * static buffers to dynamically allocated ones, so malloc(3) being beaten
 * more was no surprise. But it made us slower.
 *
 * talloc_pool() is an optimization to call malloc(3) a lot less for the use
 * pattern Samba has: The SMB protocol is mainly a request/response protocol
 * where we have to allocate a certain amount of memory per request and free
 * that after the SMB reply is sent to the client.
 *
 * talloc_pool() creates a talloc chunk that you can use as a talloc parent
 * exactly as you would use any other ::TALLOC_CTX. The difference is that
 * when you talloc a child of this pool, no malloc(3) is done. Instead, talloc
 * just increments a pointer inside the talloc_pool. This also works
 * recursively. If you use the child of the talloc pool as a parent for
 * grand-children, their memory is also taken from the talloc pool.
 *
 * If there is not enough memory in the pool to allocate the new child,
 * it will create a new talloc chunk as if the parent was a normal talloc
 * context.
 *
 * If you talloc_free() children of a talloc pool, the memory is not given
 * back to the system. Instead, free(3) is only called if the talloc_pool()
 * itself is released with talloc_free().
 *
 * The downside of a talloc pool is that if you talloc_move() a child of a
 * talloc pool to a talloc parent outside the pool, the whole pool memory is
 * not free(3)'ed until that moved chunk is also talloc_free()ed.
 *
 * @param[in]  context  The talloc context to hang the result off.
 *
 * @param[in]  size     Size of the talloc pool.
 *
 * @return              The allocated talloc pool, NULL on error.
 */
void *talloc_pool(const void *context, size_t size);

#ifdef DOXYGEN
/**
 * @brief Allocate a talloc object as/with an additional pool.
 *
 * This is like talloc_pool(), but's it's more flexible
 * and allows an object to be a pool for its children.
 *
 * @param[in] ctx                   The talloc context to hang the result off.
 *
 * @param[in] type                  The type that we want to allocate.
 *
 * @param[in] num_subobjects        The expected number of subobjects, which will
 *                                  be allocated within the pool. This allocates
 *                                  space for talloc_chunk headers.
 *
 * @param[in] total_subobjects_size The size that all subobjects can use in total.
 *
 *
 * @return              The allocated talloc object, NULL on error.
 */
void *talloc_pooled_object(const void *ctx, #type,
			   unsigned num_subobjects,
			   size_t total_subobjects_size);
#else
#define talloc_pooled_object(_ctx, _type, \
			     _num_subobjects, \
			     _total_subobjects_size) \
	(_type *)_talloc_pooled_object((_ctx), sizeof(_type), #_type, \
					(_num_subobjects), \
					(_total_subobjects_size))
void *_talloc_pooled_object(const void *ctx,
			    size_t type_size,
			    const char *type_name,
			    unsigned num_subobjects,
			    size_t total_subobjects_size);
#endif

/**
 * @brief Free a talloc chunk and NULL out the pointer.
 *
 * TALLOC_FREE() frees a pointer and sets it to NULL. Use this if you want
 * immediate feedback (i.e. crash) if you use a pointer after having free'ed
 * it.
 *
 * @param[in]  ctx      The chunk to be freed.
 */
#define TALLOC_FREE(ctx) do { if (ctx != NULL) { talloc_free(ctx); ctx=NULL; } } while(0)

/* @} ******************************************************************/

/**
 * \defgroup talloc_ref The talloc reference function.
 * @ingroup talloc
 *
 * This module contains the definitions around talloc references
 *
 * @{
 */

/**
 * @brief Increase the reference count of a talloc chunk.
 *
 * The talloc_increase_ref_count(ptr) function is exactly equivalent to:
 *
 * @code
 *      talloc_reference(NULL, ptr);
 * @endcode
 *
 * You can use either syntax, depending on which you think is clearer in
 * your code.
 *
 * @param[in]  ptr      The pointer to increase the reference count.
 *
 * @return              0 on success, -1 on error.
 */
int talloc_increase_ref_count(const void *ptr);

/**
 * @brief Get the number of references to a talloc chunk.
 *
 * @param[in]  ptr      The pointer to retrieve the reference count from.
 *
 * @return              The number of references.
 */
size_t talloc_reference_count(const void *ptr);

#ifdef DOXYGEN
/**
 * @brief Create an additional talloc parent to a pointer.
 *
 * The talloc_reference() function makes "context" an additional parent of
 * ptr. Each additional reference consumes around 48 bytes of memory on intel
 * x86 platforms.
 *
 * If ptr is NULL, then the function is a no-op, and simply returns NULL.
 *
 * After creating a reference you can free it in one of the following ways:
 *
 * - you can talloc_free() any parent of the original pointer. That
 *   will reduce the number of parents of this pointer by 1, and will
 *   cause this pointer to be freed if it runs out of parents.
 *
 * - you can talloc_free() the pointer itself if it has at maximum one
 *   parent. This behaviour has been changed since the release of version
 *   2.0. Further informations in the description of "talloc_free".
 *
 * For more control on which parent to remove, see talloc_unlink()
 * @param[in]  ctx      The additional parent.
 *
 * @param[in]  ptr      The pointer you want to create an additional parent for.
 *
 * @return              The original pointer 'ptr', NULL if talloc ran out of
 *                      memory in creating the reference.
 *
 * @warning You should try to avoid using this interface. It turns a beautiful
 *          talloc-tree into a graph. It is often really hard to debug if you
 *          screw something up by accident.
 *
 * Example:
 * @code
 *      unsigned int *a, *b, *c;
 *      a = talloc(NULL, unsigned int);
 *      b = talloc(NULL, unsigned int);
 *      c = talloc(a, unsigned int);
 *      // b also serves as a parent of c.
 *      talloc_reference(b, c);
 * @endcode
 *
 * @see talloc_unlink()
 */
void *talloc_reference(const void *ctx, const void *ptr);
#else
#define talloc_reference(ctx, ptr) (_TALLOC_TYPEOF(ptr))_talloc_reference_loc((ctx),(ptr), __location__)
void *_talloc_reference_loc(const void *context, const void *ptr, const char *location);
#endif

/**
 * @brief Remove a specific parent from a talloc chunk.
 *
 * The function removes a specific parent from ptr. The context passed must
 * either be a context used in talloc_reference() with this pointer, or must be
 * a direct parent of ptr.
 *
 * You can just use talloc_free() instead of talloc_unlink() if there
 * is at maximum one parent. This behaviour has been changed since the
 * release of version 2.0. Further informations in the description of
 * "talloc_free".
 *
 * @param[in]  context  The talloc parent to remove.
 *
 * @param[in]  ptr      The talloc ptr you want to remove the parent from.
 *
 * @return              0 on success, -1 on error.
 *
 * @note If the parent has already been removed using talloc_free() then
 * this function will fail and will return -1.  Likewise, if ptr is NULL,
 * then the function will make no modifications and return -1.
 *
 * @warning You should try to avoid using this interface. It turns a beautiful
 *          talloc-tree into a graph. It is often really hard to debug if you
 *          screw something up by accident.
 *
 * Example:
 * @code
 *      unsigned int *a, *b, *c;
 *      a = talloc(NULL, unsigned int);
 *      b = talloc(NULL, unsigned int);
 *      c = talloc(a, unsigned int);
 *      // b also serves as a parent of c.
 *      talloc_reference(b, c);
 *      talloc_unlink(b, c);
 * @endcode
 */
int talloc_unlink(const void *context, void *ptr);

/**
 * @brief Provide a talloc context that is freed at program exit.
 *
 * This is a handy utility function that returns a talloc context
 * which will be automatically freed on program exit. This can be used
 * to reduce the noise in memory leak reports.
 *
 * Never use this in code that might be used in objects loaded with
 * dlopen and unloaded with dlclose. talloc_autofree_context()
 * internally uses atexit(3). Some platforms like modern Linux handles
 * this fine, but for example FreeBSD does not deal well with dlopen()
 * and atexit() used simultaneously: dlclose() does not clean up the
 * list of atexit-handlers, so when the program exits the code that
 * was registered from within talloc_autofree_context() is gone, the
 * program crashes at exit.
 *
 * @return              A talloc context, NULL on error.
 */
void *talloc_autofree_context(void);

/**
 * @brief Get the size of a talloc chunk.
 *
 * This function lets you know the amount of memory allocated so far by
 * this context. It does NOT account for subcontext memory.
 * This can be used to calculate the size of an array.
 *
 * @param[in]  ctx      The talloc chunk.
 *
 * @return              The size of the talloc chunk.
 */
size_t talloc_get_size(const void *ctx);

/**
 * @brief Show the parentage of a context.
 *
 * @param[in]  context            The talloc context to look at.
 *
 * @param[in]  file               The output to use, a file, stdout or stderr.
 */
void talloc_show_parents(const void *context, FILE *file);

/**
 * @brief Check if a context is parent of a talloc chunk.
 *
 * This checks if context is referenced in the talloc hierarchy above ptr.
 *
 * @param[in]  context  The assumed talloc context.
 *
 * @param[in]  ptr      The talloc chunk to check.
 *
 * @return              Return 1 if this is the case, 0 if not.
 */
int talloc_is_parent(const void *context, const void *ptr);

/**
 * @brief Change the parent context of a talloc pointer.
 *
 * The function changes the parent context of a talloc pointer. It is typically
 * used when the context that the pointer is currently a child of is going to be
 * freed and you wish to keep the memory for a longer time.
 *
 * The difference between talloc_reparent() and talloc_steal() is that
 * talloc_reparent() can specify which parent you wish to change. This is
 * useful when a pointer has multiple parents via references.
 *
 * @param[in]  old_parent
 * @param[in]  new_parent
 * @param[in]  ptr
 *
 * @return              Return the pointer you passed. It does not have any
 *                      failure modes.
 */
void *talloc_reparent(const void *old_parent, const void *new_parent, const void *ptr);

/* @} ******************************************************************/

/**
 * @defgroup talloc_array The talloc array functions
 * @ingroup talloc
 *
 * Talloc contains some handy helpers for handling Arrays conveniently
 *
 * @{
 */

#ifdef DOXYGEN
/**
 * @brief Allocate an array.
 *
 * The macro is equivalent to:
 *
 * @code
 *      (type *)talloc_size(ctx, sizeof(type) * count);
 * @endcode
 *
 * except that it provides integer overflow protection for the multiply,
 * returning NULL if the multiply overflows.
 *
 * @param[in]  ctx      The talloc context to hang the result off.
 *
 * @param[in]  type     The type that we want to allocate.
 *
 * @param[in]  count    The number of 'type' elements you want to allocate.
 *
 * @return              The allocated result, properly cast to 'type *', NULL on
 *                      error.
 *
 * Example:
 * @code
 *      unsigned int *a, *b;
 *      a = talloc_zero(NULL, unsigned int);
 *      b = talloc_array(a, unsigned int, 100);
 * @endcode
 *
 * @see talloc()
 * @see talloc_zero_array()
 */
void *talloc_array(const void *ctx, #type, unsigned count);
#else
#define talloc_array(ctx, type, count) (type *)_talloc_array(ctx, sizeof(type), count, #type)
void *_talloc_array(const void *ctx, size_t el_size, unsigned count, const char *name);
#endif

#ifdef DOXYGEN
/**
 * @brief Allocate an array.
 *
 * @param[in]  ctx      The talloc context to hang the result off.
 *
 * @param[in]  size     The size of an array element.
 *
 * @param[in]  count    The number of elements you want to allocate.
 *
 * @return              The allocated result, NULL on error.
 */
void *talloc_array_size(const void *ctx, size_t size, unsigned count);
#else
#define talloc_array_size(ctx, size, count) _talloc_array(ctx, size, count, __location__)
#endif

#ifdef DOXYGEN
/**
 * @brief Allocate an array into a typed pointer.
 *
 * The macro should be used when you have a pointer to an array and want to
 * allocate memory of an array to point at with this pointer. When compiling
 * with gcc >= 3 it is typesafe. Note this is a wrapper of talloc_array_size()
 * and talloc_get_name() will return the current location in the source file
 * and not the type.
 *
 * @param[in]  ctx      The talloc context to hang the result off.
 *
 * @param[in]  ptr      The pointer you want to assign the result to.
 *
 * @param[in]  count    The number of elements you want to allocate.
 *
 * @return              The allocated memory chunk, properly casted. NULL on
 *                      error.
 */
void *talloc_array_ptrtype(const void *ctx, const void *ptr, unsigned count);
#else
#define talloc_array_ptrtype(ctx, ptr, count) (_TALLOC_TYPEOF(ptr))talloc_array_size(ctx, sizeof(*(ptr)), count)
#endif

#ifdef DOXYGEN
/**
 * @brief Get the number of elements in a talloc'ed array.
 *
 * A talloc chunk carries its own size, so for talloc'ed arrays it is not
 * necessary to store the number of elements explicitly.
 *
 * @param[in]  ctx      The allocated array.
 *
 * @return              The number of elements in ctx.
 */
size_t talloc_array_length(const void *ctx);
#else
#define talloc_array_length(ctx) (talloc_get_size(ctx)/sizeof(*ctx))
#endif

#ifdef DOXYGEN
/**
 * @brief Allocate a zero-initialized array
 *
 * @param[in]  ctx      The talloc context to hang the result off.
 *
 * @param[in]  type     The type that we want to allocate.
 *
 * @param[in]  count    The number of "type" elements you want to allocate.
 *
 * @return              The allocated result casted to "type *", NULL on error.
 *
 * The talloc_zero_array() macro is equivalent to:
 *
 * @code
 *     ptr = talloc_array(ctx, type, count);
 *     if (ptr) memset(ptr, 0, sizeof(type) * count);
 * @endcode
 */
void *talloc_zero_array(const void *ctx, #type, unsigned count);
#else
#define talloc_zero_array(ctx, type, count) (type *)_talloc_zero_array(ctx, sizeof(type), count, #type)
void *_talloc_zero_array(const void *ctx,
			 size_t el_size,
			 unsigned count,
			 const char *name);
#endif

#ifdef DOXYGEN
/**
 * @brief Change the size of a talloc array.
 *
 * The macro changes the size of a talloc pointer. The 'count' argument is the
 * number of elements of type 'type' that you want the resulting pointer to
 * hold.
 *
 * talloc_realloc() has the following equivalences:
 *
 * @code
 *      talloc_realloc(ctx, NULL, type, 1) ==> talloc(ctx, type);
 *      talloc_realloc(ctx, NULL, type, N) ==> talloc_array(ctx, type, N);
 *      talloc_realloc(ctx, ptr, type, 0)  ==> talloc_free(ptr);
 * @endcode
 *
 * The "context" argument is only used if "ptr" is NULL, otherwise it is
 * ignored.
 *
 * @param[in]  ctx      The parent context used if ptr is NULL.
 *
 * @param[in]  ptr      The chunk to be resized.
 *
 * @param[in]  type     The type of the array element inside ptr.
 *
 * @param[in]  count    The intended number of array elements.
 *
 * @return              The new array, NULL on error. The call will fail either
 *                      due to a lack of memory, or because the pointer has more
 *                      than one parent (see talloc_reference()).
 */
void *talloc_realloc(const void *ctx, void *ptr, #type, size_t count);
#else
#define talloc_realloc(ctx, p, type, count) (type *)_talloc_realloc_array(ctx, p, sizeof(type), count, #type)
void *_talloc_realloc_array(const void *ctx, void *ptr, size_t el_size, unsigned count, const char *name);
#endif

#ifdef DOXYGEN
/**
 * @brief Untyped realloc to change the size of a talloc array.
 *
 * The macro is useful when the type is not known so the typesafe
 * talloc_realloc() cannot be used.
 *
 * @param[in]  ctx      The parent context used if 'ptr' is NULL.
 *
 * @param[in]  ptr      The chunk to be resized.
 *
 * @param[in]  size     The new chunk size.
 *
 * @return              The new array, NULL on error.
 */
void *talloc_realloc_size(const void *ctx, void *ptr, size_t size);
#else
#define talloc_realloc_size(ctx, ptr, size) _talloc_realloc(ctx, ptr, size, __location__)
void *_talloc_realloc(const void *context, void *ptr, size_t size, const char *name);
#endif

/**
 * @brief Provide a function version of talloc_realloc_size.
 *
 * This is a non-macro version of talloc_realloc(), which is useful as
 * libraries sometimes want a ralloc function pointer. A realloc()
 * implementation encapsulates the functionality of malloc(), free() and
 * realloc() in one call, which is why it is useful to be able to pass around
 * a single function pointer.
 *
 * @param[in]  context  The parent context used if ptr is NULL.
 *
 * @param[in]  ptr      The chunk to be resized.
 *
 * @param[in]  size     The new chunk size.
 *
 * @return              The new chunk, NULL on error.
 */
void *talloc_realloc_fn(const void *context, void *ptr, size_t size);

/* @} ******************************************************************/

/**
 * @defgroup talloc_string The talloc string functions.
 * @ingroup talloc
 *
 * talloc string allocation and manipulation functions.
 * @{
 */

/**
 * @brief Duplicate a string into a talloc chunk.
 *
 * This function is equivalent to:
 *
 * @code
 *      ptr = talloc_size(ctx, strlen(p)+1);
 *      if (ptr) memcpy(ptr, p, strlen(p)+1);
 * @endcode
 *
 * This functions sets the name of the new pointer to the passed
 * string. This is equivalent to:
 *
 * @code
 *      talloc_set_name_const(ptr, ptr)
 * @endcode
 *
 * @param[in]  t        The talloc context to hang the result off.
 *
 * @param[in]  p        The string you want to duplicate.
 *
 * @return              The duplicated string, NULL on error.
 */
char *talloc_strdup(const void *t, const char *p);

/**
 * @brief Append a string to given string.
 *
 * The destination string is reallocated to take
 * <code>strlen(s) + strlen(a) + 1</code> characters.
 *
 * This functions sets the name of the new pointer to the new
 * string. This is equivalent to:
 *
 * @code
 *      talloc_set_name_const(ptr, ptr)
 * @endcode
 *
 * If <code>s == NULL</code> then new context is created.
 *
 * @param[in]  s        The destination to append to.
 *
 * @param[in]  a        The string you want to append.
 *
 * @return              The concatenated strings, NULL on error.
 *
 * @see talloc_strdup()
 * @see talloc_strdup_append_buffer()
 */
char *talloc_strdup_append(char *s, const char *a);

/**
 * @brief Append a string to a given buffer.
 *
 * This is a more efficient version of talloc_strdup_append(). It determines the
 * length of the destination string by the size of the talloc context.
 *
 * Use this very carefully as it produces a different result than
 * talloc_strdup_append() when a zero character is in the middle of the
 * destination string.
 *
 * @code
 *      char *str_a = talloc_strdup(NULL, "hello world");
 *      char *str_b = talloc_strdup(NULL, "hello world");
 *      str_a[5] = str_b[5] = '\0'
 *
 *      char *app = talloc_strdup_append(str_a, ", hello");
 *      char *buf = talloc_strdup_append_buffer(str_b, ", hello");
 *
 *      printf("%s\n", app); // hello, hello (app = "hello, hello")
 *      printf("%s\n", buf); // hello (buf = "hello\0world, hello")
 * @endcode
 *
 * If <code>s == NULL</code> then new context is created.
 *
 * @param[in]  s        The destination buffer to append to.
 *
 * @param[in]  a        The string you want to append.
 *
 * @return              The concatenated strings, NULL on error.
 *
 * @see talloc_strdup()
 * @see talloc_strdup_append()
 * @see talloc_array_length()
 */
char *talloc_strdup_append_buffer(char *s, const char *a);

/**
 * @brief Duplicate a length-limited string into a talloc chunk.
 *
 * This function is the talloc equivalent of the C library function strndup(3).
 *
 * This functions sets the name of the new pointer to the passed string. This is
 * equivalent to:
 *
 * @code
 *      talloc_set_name_const(ptr, ptr)
 * @endcode
 *
 * @param[in]  t        The talloc context to hang the result off.
 *
 * @param[in]  p        The string you want to duplicate.
 *
 * @param[in]  n        The maximum string length to duplicate.
 *
 * @return              The duplicated string, NULL on error.
 */
char *talloc_strndup(const void *t, const char *p, size_t n);

/**
 * @brief Append at most n characters of a string to given string.
 *
 * The destination string is reallocated to take
 * <code>strlen(s) + strnlen(a, n) + 1</code> characters.
 *
 * This functions sets the name of the new pointer to the new
 * string. This is equivalent to:
 *
 * @code
 *      talloc_set_name_const(ptr, ptr)
 * @endcode
 *
 * If <code>s == NULL</code> then new context is created.
 *
 * @param[in]  s        The destination string to append to.
 *
 * @param[in]  a        The source string you want to append.
 *
 * @param[in]  n        The number of characters you want to append from the
 *                      string.
 *
 * @return              The concatenated strings, NULL on error.
 *
 * @see talloc_strndup()
 * @see talloc_strndup_append_buffer()
 */
char *talloc_strndup_append(char *s, const char *a, size_t n);

/**
 * @brief Append at most n characters of a string to given buffer
 *
 * This is a more efficient version of talloc_strndup_append(). It determines
 * the length of the destination string by the size of the talloc context.
 *
 * Use this very carefully as it produces a different result than
 * talloc_strndup_append() when a zero character is in the middle of the
 * destination string.
 *
 * @code
 *      char *str_a = talloc_strdup(NULL, "hello world");
 *      char *str_b = talloc_strdup(NULL, "hello world");
 *      str_a[5] = str_b[5] = '\0'
 *
 *      char *app = talloc_strndup_append(str_a, ", hello", 7);
 *      char *buf = talloc_strndup_append_buffer(str_b, ", hello", 7);
 *
 *      printf("%s\n", app); // hello, hello (app = "hello, hello")
 *      printf("%s\n", buf); // hello (buf = "hello\0world, hello")
 * @endcode
 *
 * If <code>s == NULL</code> then new context is created.
 *
 * @param[in]  s        The destination buffer to append to.
 *
 * @param[in]  a        The source string you want to append.
 *
 * @param[in]  n        The number of characters you want to append from the
 *                      string.
 *
 * @return              The concatenated strings, NULL on error.
 *
 * @see talloc_strndup()
 * @see talloc_strndup_append()
 * @see talloc_array_length()
 */
char *talloc_strndup_append_buffer(char *s, const char *a, size_t n);

/**
 * @brief Format a string given a va_list.
 *
 * This function is the talloc equivalent of the C library function
 * vasprintf(3).
 *
 * This functions sets the name of the new pointer to the new string. This is
 * equivalent to:
 *
 * @code
 *      talloc_set_name_const(ptr, ptr)
 * @endcode
 *
 * @param[in]  t        The talloc context to hang the result off.
 *
 * @param[in]  fmt      The format string.
 *
 * @param[in]  ap       The parameters used to fill fmt.
 *
 * @return              The formatted string, NULL on error.
 */
char *talloc_vasprintf(const void *t, const char *fmt, va_list ap) PRINTF_ATTRIBUTE(2,0);

/**
 * @brief Format a string given a va_list and append it to the given destination
 *        string.
 *
 * @param[in]  s        The destination string to append to.
 *
 * @param[in]  fmt      The format string.
 *
 * @param[in]  ap       The parameters used to fill fmt.
 *
 * @return              The formatted string, NULL on error.
 *
 * @see talloc_vasprintf()
 */
char *talloc_vasprintf_append(char *s, const char *fmt, va_list ap) PRINTF_ATTRIBUTE(2,0);

/**
 * @brief Format a string given a va_list and append it to the given destination
 *        buffer.
 *
 * @param[in]  s        The destination buffer to append to.
 *
 * @param[in]  fmt      The format string.
 *
 * @param[in]  ap       The parameters used to fill fmt.
 *
 * @return              The formatted string, NULL on error.
 *
 * @see talloc_vasprintf()
 */
char *talloc_vasprintf_append_buffer(char *s, const char *fmt, va_list ap) PRINTF_ATTRIBUTE(2,0);

/**
 * @brief Format a string.
 *
 * This function is the talloc equivalent of the C library function asprintf(3).
 *
 * This functions sets the name of the new pointer to the new string. This is
 * equivalent to:
 *
 * @code
 *      talloc_set_name_const(ptr, ptr)
 * @endcode
 *
 * @param[in]  t        The talloc context to hang the result off.
 *
 * @param[in]  fmt      The format string.
 *
 * @param[in]  ...      The parameters used to fill fmt.
 *
 * @return              The formatted string, NULL on error.
 */
char *talloc_asprintf(const void *t, const char *fmt, ...) PRINTF_ATTRIBUTE(2,3);

/**
 * @brief Append a formatted string to another string.
 *
 * This function appends the given formatted string to the given string. Use
 * this variant when the string in the current talloc buffer may have been
 * truncated in length.
 *
 * This functions sets the name of the new pointer to the new
 * string. This is equivalent to:
 *
 * @code
 *      talloc_set_name_const(ptr, ptr)
 * @endcode
 *
 * If <code>s == NULL</code> then new context is created.
 *
 * @param[in]  s        The string to append to.
 *
 * @param[in]  fmt      The format string.
 *
 * @param[in]  ...      The parameters used to fill fmt.
 *
 * @return              The formatted string, NULL on error.
 */
char *talloc_asprintf_append(char *s, const char *fmt, ...) PRINTF_ATTRIBUTE(2,3);

/**
 * @brief Append a formatted string to another string.
 *
 * This is a more efficient version of talloc_asprintf_append(). It determines
 * the length of the destination string by the size of the talloc context.
 *
 * Use this very carefully as it produces a different result than
 * talloc_asprintf_append() when a zero character is in the middle of the
 * destination string.
 *
 * @code
 *      char *str_a = talloc_strdup(NULL, "hello world");
 *      char *str_b = talloc_strdup(NULL, "hello world");
 *      str_a[5] = str_b[5] = '\0'
 *
 *      char *app = talloc_asprintf_append(str_a, "%s", ", hello");
 *      char *buf = talloc_strdup_append_buffer(str_b, "%s", ", hello");
 *
 *      printf("%s\n", app); // hello, hello (app = "hello, hello")
 *      printf("%s\n", buf); // hello (buf = "hello\0world, hello")
 * @endcode
 *
 * If <code>s == NULL</code> then new context is created.
 *
 * @param[in]  s        The string to append to
 *
 * @param[in]  fmt      The format string.
 *
 * @param[in]  ...      The parameters used to fill fmt.
 *
 * @return              The formatted string, NULL on error.
 *
 * @see talloc_asprintf()
 * @see talloc_asprintf_append()
 */
char *talloc_asprintf_append_buffer(char *s, const char *fmt, ...) PRINTF_ATTRIBUTE(2,3);

/* @} ******************************************************************/

/**
 * @defgroup talloc_debug The talloc debugging support functions
 * @ingroup talloc
 *
 * To aid memory debugging, talloc contains routines to inspect the currently
 * allocated memory hierarchy.
 *
 * @{
 */

/**
 * @brief Walk a complete talloc hierarchy.
 *
 * This provides a more flexible reports than talloc_report(). It
 * will recursively call the callback for the entire tree of memory
 * referenced by the pointer. References in the tree are passed with
 * is_ref = 1 and the pointer that is referenced.
 *
 * You can pass NULL for the pointer, in which case a report is
 * printed for the top level memory context, but only if
 * talloc_enable_leak_report() or talloc_enable_leak_report_full()
 * has been called.
 *
 * The recursion is stopped when depth >= max_depth.
 * max_depth = -1 means only stop at leaf nodes.
 *
 * @param[in]  ptr      The talloc chunk.
 *
 * @param[in]  depth    Internal parameter to control recursion. Call with 0.
 *
 * @param[in]  max_depth  Maximum recursion level.
 *
 * @param[in]  callback  Function to be called on every chunk.
 *
 * @param[in]  private_data  Private pointer passed to callback.
 */
void talloc_report_depth_cb(const void *ptr, int depth, int max_depth,
			    void (*callback)(const void *ptr,
					     int depth, int max_depth,
					     int is_ref,
					     void *private_data),
			    void *private_data);

/**
 * @brief Print a talloc hierarchy.
 *
 * This provides a more flexible reports than talloc_report(). It
 * will let you specify the depth and max_depth.
 *
 * @param[in]  ptr      The talloc chunk.
 *
 * @param[in]  depth    Internal parameter to control recursion. Call with 0.
 *
 * @param[in]  max_depth  Maximum recursion level.
 *
 * @param[in]  f        The file handle to print to.
 */
void talloc_report_depth_file(const void *ptr, int depth, int max_depth, FILE *f);

/**
 * @brief Print a summary report of all memory used by ptr.
 *
 * This provides a more detailed report than talloc_report(). It will
 * recursively print the entire tree of memory referenced by the
 * pointer. References in the tree are shown by giving the name of the
 * pointer that is referenced.
 *
 * You can pass NULL for the pointer, in which case a report is printed
 * for the top level memory context, but only if
 * talloc_enable_leak_report() or talloc_enable_leak_report_full() has
 * been called.
 *
 * @param[in]  ptr      The talloc chunk.
 *
 * @param[in]  f        The file handle to print to.
 *
 * Example:
 * @code
 *      unsigned int *a, *b;
 *      a = talloc(NULL, unsigned int);
 *      b = talloc(a, unsigned int);
 *      fprintf(stderr, "Dumping memory tree for a:\n");
 *      talloc_report_full(a, stderr);
 * @endcode
 *
 * @see talloc_report()
 */
void talloc_report_full(const void *ptr, FILE *f);

/**
 * @brief Print a summary report of all memory used by ptr.
 *
 * This function prints a summary report of all memory used by ptr. One line of
 * report is printed for each immediate child of ptr, showing the total memory
 * and number of blocks used by that child.
 *
 * You can pass NULL for the pointer, in which case a report is printed
 * for the top level memory context, but only if talloc_enable_leak_report()
 * or talloc_enable_leak_report_full() has been called.
 *
 * @param[in]  ptr      The talloc chunk.
 *
 * @param[in]  f        The file handle to print to.
 *
 * Example:
 * @code
 *      unsigned int *a, *b;
 *      a = talloc(NULL, unsigned int);
 *      b = talloc(a, unsigned int);
 *      fprintf(stderr, "Summary of memory tree for a:\n");
 *      talloc_report(a, stderr);
 * @endcode
 *
 * @see talloc_report_full()
 */
void talloc_report(const void *ptr, FILE *f);

/**
 * @brief Enable tracking the use of NULL memory contexts.
 *
 * This enables tracking of the NULL memory context without enabling leak
 * reporting on exit. Useful for when you want to do your own leak
 * reporting call via talloc_report_null_full();
 */
void talloc_enable_null_tracking(void);

/**
 * @brief Enable tracking the use of NULL memory contexts.
 *
 * This enables tracking of the NULL memory context without enabling leak
 * reporting on exit. Useful for when you want to do your own leak
 * reporting call via talloc_report_null_full();
 */
void talloc_enable_null_tracking_no_autofree(void);

/**
 * @brief Disable tracking of the NULL memory context.
 *
 * This disables tracking of the NULL memory context.
 */
void talloc_disable_null_tracking(void);

/**
 * @brief Enable leak report when a program exits.
 *
 * This enables calling of talloc_report(NULL, stderr) when the program
 * exits. In Samba4 this is enabled by using the --leak-report command
 * line option.
 *
 * For it to be useful, this function must be called before any other
 * talloc function as it establishes a "null context" that acts as the
 * top of the tree. If you don't call this function first then passing
 * NULL to talloc_report() or talloc_report_full() won't give you the
 * full tree printout.
 *
 * Here is a typical talloc report:
 *
 * @code
 * talloc report on 'null_context' (total 267 bytes in 15 blocks)
 *      libcli/auth/spnego_parse.c:55  contains     31 bytes in   2 blocks
 *      libcli/auth/spnego_parse.c:55  contains     31 bytes in   2 blocks
 *      iconv(UTF8,CP850)              contains     42 bytes in   2 blocks
 *      libcli/auth/spnego_parse.c:55  contains     31 bytes in   2 blocks
 *      iconv(CP850,UTF8)              contains     42 bytes in   2 blocks
 *      iconv(UTF8,UTF-16LE)           contains     45 bytes in   2 blocks
 *      iconv(UTF-16LE,UTF8)           contains     45 bytes in   2 blocks
 * @endcode
 */
void talloc_enable_leak_report(void);

/**
 * @brief Enable full leak report when a program exits.
 *
 * This enables calling of talloc_report_full(NULL, stderr) when the
 * program exits. In Samba4 this is enabled by using the
 * --leak-report-full command line option.
 *
 * For it to be useful, this function must be called before any other
 * talloc function as it establishes a "null context" that acts as the
 * top of the tree. If you don't call this function first then passing
 * NULL to talloc_report() or talloc_report_full() won't give you the
 * full tree printout.
 *
 * Here is a typical full report:
 *
 * @code
 * full talloc report on 'root' (total 18 bytes in 8 blocks)
 *      p1                             contains     18 bytes in   7 blocks (ref 0)
 *      r1                             contains     13 bytes in   2 blocks (ref 0)
 *      reference to: p2
 *      p2                             contains      1 bytes in   1 blocks (ref 1)
 *      x3                             contains      1 bytes in   1 blocks (ref 0)
 *      x2                             contains      1 bytes in   1 blocks (ref 0)
 *      x1                             contains      1 bytes in   1 blocks (ref 0)
 * @endcode
 */
void talloc_enable_leak_report_full(void);

/**
 * @brief Set a custom "abort" function that is called on serious error.
 *
 * The default "abort" function is <code>abort()</code>.
 *
 * The "abort" function is called when:
 *
 * <ul>
 *  <li>talloc_get_type_abort() fails</li>
 *  <li>the provided pointer is not a valid talloc context</li>
 *  <li>when the context meta data are invalid</li>
 *  <li>when access after free is detected</li>
 * </ul>
 *
 * Example:
 *
 * @code
 * void my_abort(const char *reason)
 * {
 *      fprintf(stderr, "talloc abort: %s\n", reason);
 *      abort();
 * }
 *
 *      talloc_set_abort_fn(my_abort);
 * @endcode
 *
 * @param[in]  abort_fn      The new "abort" function.
 *
 * @see talloc_set_log_fn()
 * @see talloc_get_type()
 */
void talloc_set_abort_fn(void (*abort_fn)(const char *reason));

/**
 * @brief Set a logging function.
 *
 * @param[in]  log_fn      The logging function.
 *
 * @see talloc_set_log_stderr()
 * @see talloc_set_abort_fn()
 */
void talloc_set_log_fn(void (*log_fn)(const char *message));

/**
 * @brief Set stderr as the output for logs.
 *
 * @see talloc_set_log_fn()
 * @see talloc_set_abort_fn()
 */
void talloc_set_log_stderr(void);

/**
 * @brief Set a max memory limit for the current context hierarchy
 *	  This affects all children of this context and constrain any
 *	  allocation in the hierarchy to never exceed the limit set.
 *	  The limit can be removed by setting 0 (unlimited) as the
 *	  max_size by calling the function again on the same context.
 *	  Memory limits can also be nested, meaning a child can have
 *	  a stricter memory limit than a parent.
 *	  Memory limits are enforced only at memory allocation time.
 *	  Stealing a context into a 'limited' hierarchy properly
 *	  updates memory usage but does *not* cause failure if the
 *	  move causes the new parent to exceed its limits. However
 *	  any further allocation on that hierarchy will then fail.
 *
 * @param[in]	ctx		The talloc context to set the limit on
 * @param[in]	max_size	The (new) max_size
 */
int talloc_set_memlimit(const void *ctx, size_t max_size);

/* @} ******************************************************************/

#if TALLOC_DEPRECATED
#define talloc_zero_p(ctx, type) talloc_zero(ctx, type)
#define talloc_p(ctx, type) talloc(ctx, type)
#define talloc_array_p(ctx, type, count) talloc_array(ctx, type, count)
#define talloc_realloc_p(ctx, p, type, count) talloc_realloc(ctx, p, type, count)
#define talloc_destroy(ctx) talloc_free(ctx)
#define talloc_append_string(c, s, a) (s?talloc_strdup_append(s,a):talloc_strdup(c, a))
#endif

#ifndef TALLOC_MAX_DEPTH
#define TALLOC_MAX_DEPTH 10000
#endif

#ifdef __cplusplus
} /* end of extern "C" */
#endif

#endif
