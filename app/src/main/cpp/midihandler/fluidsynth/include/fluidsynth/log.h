/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#ifndef _FLUIDSYNTH_LOG_H
#define _FLUIDSYNTH_LOG_H


#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup logging Logging
 *
 * Logging interface
 *
 * The default logging function of the fluidsynth prints its messages to the
 * stderr. The synthesizer uses five level of messages: #FLUID_PANIC,
 * #FLUID_ERR, #FLUID_WARN, #FLUID_INFO, and #FLUID_DBG.
 *
 * A client application can install a new log function to handle the messages
 * differently. In the following example, the application sets a callback
 * function to display #FLUID_PANIC messages in a dialog, and ignores all other
 * messages by setting the log function to NULL:
 *
 * @code
 * fluid_set_log_function(FLUID_PANIC, show_dialog, (void*) root_window);
 * fluid_set_log_function(FLUID_ERR, NULL, NULL);
 * fluid_set_log_function(FLUID_WARN, NULL, NULL);
 * fluid_set_log_function(FLUID_DBG, NULL, NULL);
 * @endcode
 *
 * @note The logging configuration is global and not tied to a specific
 * synthesizer instance. That means that all synthesizer instances created in
 * the same process share the same logging configuration.
 *
 * @{
 */

/**
 * FluidSynth log levels.
 */
enum fluid_log_level
{
    FLUID_PANIC,   /**< The synth can't function correctly any more */
    FLUID_ERR,     /**< Serious error occurred */
    FLUID_WARN,    /**< Warning */
    FLUID_INFO,    /**< Verbose informational messages */
    FLUID_DBG,     /**< Debugging messages */
    LAST_LOG_LEVEL /**< @internal This symbol is not part of the public API and ABI
                     stability guarantee and may change at any time! */
};

/**
 * Log function handler callback type used by fluid_set_log_function().
 *
 * @param level Log level (#fluid_log_level)
 * @param message Log message text
 * @param data User data pointer supplied to fluid_set_log_function().
 */
typedef void (*fluid_log_function_t)(int level, const char *message, void *data);

FLUIDSYNTH_API
fluid_log_function_t fluid_set_log_function(int level, fluid_log_function_t fun, void *data);

FLUIDSYNTH_API void fluid_default_log_function(int level, const char *message, void *data);

FLUIDSYNTH_API int fluid_log(int level, const char *fmt, ...)
#if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
__attribute__ ((format (printf, 2, 3)))
#endif
;
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _FLUIDSYNTH_LOG_H */
