/**************************************************************************
 * 
 * Copyright 2008 VMware, Inc.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

/**
 * @file
 * Cross-platform debugging helpers.
 * 
 * For now it just has assert and printf replacements, but it might be extended 
 * with stack trace reports and more advanced logging in the near future. 
 * 
 * @author Jose Fonseca <jfonseca@vmware.com>
 */

#ifndef U_DEBUG_H_
#define U_DEBUG_H_


#include "os/os_misc.h"

#include "pipe/p_format.h"


#ifdef	__cplusplus
extern "C" {
#endif


#if defined(__GNUC__)
#define _util_printf_format(fmt, list) __attribute__ ((format (printf, fmt, list)))
#else
#define _util_printf_format(fmt, list)
#endif

void _debug_vprintf(const char *format, va_list ap);
   

static inline void
_debug_printf(const char *format, ...)
{
   va_list ap;
   va_start(ap, format);
   _debug_vprintf(format, ap);
   va_end(ap);
}


/**
 * Print debug messages.
 *
 * The actual channel used to output debug message is platform specific. To
 * avoid misformating or truncation, follow these rules of thumb:
 * - output whole lines
 * - avoid outputing large strings (512 bytes is the current maximum length
 * that is guaranteed to be printed in all platforms)
 */
#if !defined(PIPE_OS_HAIKU)
static inline void
debug_printf(const char *format, ...) _util_printf_format(1,2);

static inline void
debug_printf(const char *format, ...)
{
#ifdef DEBUG
   va_list ap;
   va_start(ap, format);
   _debug_vprintf(format, ap);
   va_end(ap);
#else
   (void) format; /* silence warning */
#endif
}
#else /* is Haiku */
/* Haiku provides debug_printf in libroot with OS.h */
#include <OS.h>
#endif


/*
 * ... isn't portable so we need to pass arguments in parentheses.
 *
 * usage:
 *    debug_printf_once(("answer: %i\n", 42));
 */
#define debug_printf_once(args) \
   do { \
      static boolean once = TRUE; \
      if (once) { \
         once = FALSE; \
         debug_printf args; \
      } \
   } while (0)


#ifdef DEBUG
#define debug_vprintf(_format, _ap) _debug_vprintf(_format, _ap)
#else
#define debug_vprintf(_format, _ap) ((void)0)
#endif


#ifdef DEBUG
/**
 * Dump a blob in hex to the same place that debug_printf sends its
 * messages.
 */
void debug_print_blob( const char *name, const void *blob, unsigned size );

/* Print a message along with a prettified format string
 */
void debug_print_format(const char *msg, unsigned fmt );
#else
#define debug_print_blob(_name, _blob, _size) ((void)0)
#define debug_print_format(_msg, _fmt) ((void)0)
#endif


/**
 * Disable interactive error message boxes.
 *
 * Should be called as soon as possible for effectiveness.
 */
void
debug_disable_error_message_boxes(void);


/**
 * Hard-coded breakpoint.
 */
#ifdef DEBUG
#define debug_break() os_break()
#else /* !DEBUG */
#define debug_break() ((void)0)
#endif /* !DEBUG */


#ifdef _MSC_VER
__declspec(noreturn)
#endif
void _debug_assert_fail(const char *expr, 
                        const char *file, 
                        unsigned line, 
                        const char *function)
#if defined(__GNUC__) && !defined(DEBUG)
   __attribute__((noreturn))
#endif
;


/** 
 * Assert macro
 * 
 * Do not expect that the assert call terminates -- errors must be handled 
 * regardless of assert behavior.
 *
 * For non debug builds the assert macro will expand to a no-op, so do not
 * call functions with side effects in the assert expression.
 */
#ifdef DEBUG
#define debug_assert(expr) ((expr) ? (void)0 : _debug_assert_fail(#expr, __FILE__, __LINE__, __FUNCTION__))
#else
#define debug_assert(expr) (void)(0 && (expr))
#endif


/** Override standard assert macro */
#ifdef assert
#undef assert
#endif
#define assert(expr) debug_assert(expr)


/**
 * Output the current function name.
 */
#ifdef DEBUG
#define debug_checkpoint() \
   _debug_printf("%s\n", __FUNCTION__)
#else
#define debug_checkpoint() \
   ((void)0) 
#endif


/**
 * Output the full source code position.
 */
#ifdef DEBUG
#define debug_checkpoint_full() \
   _debug_printf("%s:%u:%s\n", __FILE__, __LINE__, __FUNCTION__)
#else
#define debug_checkpoint_full() \
   ((void)0) 
#endif


/**
 * Output a warning message. Muted on release version.
 */
#ifdef DEBUG
#define debug_warning(__msg) \
   _debug_printf("%s:%u:%s: warning: %s\n", __FILE__, __LINE__, __FUNCTION__, __msg)
#else
#define debug_warning(__msg) \
   ((void)0) 
#endif


/**
 * Emit a warning message, but only once.
 */
#ifdef DEBUG
#define debug_warn_once(__msg) \
   do { \
      static bool warned = FALSE; \
      if (!warned) { \
         _debug_printf("%s:%u:%s: one time warning: %s\n", \
                       __FILE__, __LINE__, __FUNCTION__, __msg); \
         warned = TRUE; \
      } \
   } while (0)
#else
#define debug_warn_once(__msg) \
   ((void)0) 
#endif


/**
 * Output an error message. Not muted on release version.
 */
#ifdef DEBUG
#define debug_error(__msg) \
   _debug_printf("%s:%u:%s: error: %s\n", __FILE__, __LINE__, __FUNCTION__, __msg) 
#else
#define debug_error(__msg) \
   _debug_printf("error: %s\n", __msg)
#endif


/**
 * Used by debug_dump_enum and debug_dump_flags to describe symbols.
 */
struct debug_named_value
{
   const char *name;
   unsigned long value;
   const char *desc;
};


/**
 * Some C pre-processor magic to simplify creating named values.
 * 
 * Example:
 * @code
 * static const debug_named_value my_names[] = {
 *    DEBUG_NAMED_VALUE(MY_ENUM_VALUE_X),
 *    DEBUG_NAMED_VALUE(MY_ENUM_VALUE_Y),
 *    DEBUG_NAMED_VALUE(MY_ENUM_VALUE_Z),
 *    DEBUG_NAMED_VALUE_END
 * };
 * 
 *    ...
 *    debug_printf("%s = %s\n", 
 *                 name,
 *                 debug_dump_enum(my_names, my_value));
 *    ...
 * @endcode
 */
#define DEBUG_NAMED_VALUE(__symbol) {#__symbol, (unsigned long)__symbol, NULL}
#define DEBUG_NAMED_VALUE_WITH_DESCRIPTION(__symbol, __desc) {#__symbol, (unsigned long)__symbol, __desc}
#define DEBUG_NAMED_VALUE_END {NULL, 0, NULL}


/**
 * Convert a enum value to a string.
 */
const char *
debug_dump_enum(const struct debug_named_value *names, 
                unsigned long value);

const char *
debug_dump_enum_noprefix(const struct debug_named_value *names, 
                         const char *prefix,
                         unsigned long value);


/**
 * Convert binary flags value to a string.
 */
const char *
debug_dump_flags(const struct debug_named_value *names, 
                 unsigned long value);


/**
 * Function enter exit loggers
 */
#ifdef DEBUG
int debug_funclog_enter(const char* f, const int line, const char* file);
void debug_funclog_exit(const char* f, const int line, const char* file);
void debug_funclog_enter_exit(const char* f, const int line, const char* file);

#define DEBUG_FUNCLOG_ENTER() \
   int __debug_decleration_work_around = \
      debug_funclog_enter(__FUNCTION__, __LINE__, __FILE__)
#define DEBUG_FUNCLOG_EXIT() \
   do { \
      (void)__debug_decleration_work_around; \
      debug_funclog_exit(__FUNCTION__, __LINE__, __FILE__); \
      return; \
   } while(0)
#define DEBUG_FUNCLOG_EXIT_RET(ret) \
   do { \
      (void)__debug_decleration_work_around; \
      debug_funclog_exit(__FUNCTION__, __LINE__, __FILE__); \
      return ret; \
   } while(0)
#define DEBUG_FUNCLOG_ENTER_EXIT() \
   debug_funclog_enter_exit(__FUNCTION__, __LINE__, __FILE__)

#else
#define DEBUG_FUNCLOG_ENTER() \
   int __debug_decleration_work_around
#define DEBUG_FUNCLOG_EXIT() \
   do { (void)__debug_decleration_work_around; return; } while(0)
#define DEBUG_FUNCLOG_EXIT_RET(ret) \
   do { (void)__debug_decleration_work_around; return ret; } while(0)
#define DEBUG_FUNCLOG_ENTER_EXIT()
#endif


/**
 * Get option.
 * 
 * It is an alias for getenv on Linux. 
 * 
 * On Windows it reads C:\gallium.cfg, which is a text file with CR+LF line 
 * endings with one option per line as
 *  
 *   NAME=value
 * 
 * This file must be terminated with an extra empty line.
 */
const char *
debug_get_option(const char *name, const char *dfault);

boolean
debug_get_bool_option(const char *name, boolean dfault);

long
debug_get_num_option(const char *name, long dfault);

unsigned long
debug_get_flags_option(const char *name, 
                       const struct debug_named_value *flags,
                       unsigned long dfault);

#define DEBUG_GET_ONCE_BOOL_OPTION(sufix, name, dfault) \
static boolean \
debug_get_option_ ## sufix (void) \
{ \
   static boolean first = TRUE; \
   static boolean value; \
   if (first) { \
      first = FALSE; \
      value = debug_get_bool_option(name, dfault); \
   } \
   return value; \
}

#define DEBUG_GET_ONCE_NUM_OPTION(sufix, name, dfault) \
static long \
debug_get_option_ ## sufix (void) \
{ \
   static boolean first = TRUE; \
   static long value; \
   if (first) { \
      first = FALSE; \
      value = debug_get_num_option(name, dfault); \
   } \
   return value; \
}

#define DEBUG_GET_ONCE_FLAGS_OPTION(sufix, name, flags, dfault) \
static unsigned long \
debug_get_option_ ## sufix (void) \
{ \
   static boolean first = TRUE; \
   static unsigned long value; \
   if (first) { \
      first = FALSE; \
      value = debug_get_flags_option(name, flags, dfault); \
   } \
   return value; \
}


unsigned long
debug_memory_begin(void);

void 
debug_memory_end(unsigned long beginning);


#ifdef DEBUG
struct pipe_context;
struct pipe_surface;
struct pipe_transfer;
struct pipe_resource;

void debug_dump_image(const char *prefix,
                      enum pipe_format format, unsigned cpp,
                      unsigned width, unsigned height,
                      unsigned stride,
                      const void *data);
void debug_dump_surface(struct pipe_context *pipe,
			const char *prefix,
                        struct pipe_surface *surface);   
void debug_dump_texture(struct pipe_context *pipe,
			const char *prefix,
                        struct pipe_resource *texture);
#else
#define debug_dump_image(prefix, format, cpp, width, height, stride, data) ((void)0)
#define debug_dump_surface(pipe, prefix, surface) ((void)0)
#endif


void
debug_print_transfer_flags(const char *msg, unsigned usage);


#ifdef	__cplusplus
}
#endif

#endif /* U_DEBUG_H_ */
