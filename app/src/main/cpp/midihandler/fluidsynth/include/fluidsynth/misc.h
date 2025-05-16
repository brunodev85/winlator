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

#ifndef _FLUIDSYNTH_MISC_H
#define _FLUIDSYNTH_MISC_H


#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup misc Miscellaneous
 *
 * Miscellaneous utility functions and defines
 *
 * @{
 */

/**
 * Value that indicates success, used by most libfluidsynth functions.
 *
 * @note This was not publicly defined prior to libfluidsynth 1.1.0.  When
 * writing code which should also be compatible with older versions, something
 * like the following can be used:
 *
 * @code
 *   #include <fluidsynth.h>
 *
 *   #ifndef FLUID_OK
 *   #define FLUID_OK      (0)
 *   #define FLUID_FAILED  (-1)
 *   #endif
 * @endcode
 *
 * @since 1.1.0
 */
#define FLUID_OK        (0)

/**
 * Value that indicates failure, used by most libfluidsynth functions.
 *
 * @note See #FLUID_OK for more details.
 *
 * @since 1.1.0
 */
#define FLUID_FAILED    (-1)


FLUIDSYNTH_API int fluid_is_soundfont(const char *filename);
FLUIDSYNTH_API int fluid_is_midifile(const char *filename);
FLUIDSYNTH_API void fluid_free(void* ptr);
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _FLUIDSYNTH_MISC_H */
