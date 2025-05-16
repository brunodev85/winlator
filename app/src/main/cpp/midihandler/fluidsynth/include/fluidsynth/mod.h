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

#ifndef _FLUIDSYNTH_MOD_H
#define _FLUIDSYNTH_MOD_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup modulators SoundFont Modulators
 * @ingroup soundfonts
 *
 * SoundFont modulator functions and constants.
 *
 * @{
 */

/**
 * Flags defining the polarity, mapping function and type of a modulator source.
 * Compare with SoundFont 2.04 PDF section 8.2.
 *
 * Note: Bit values do not correspond to the SoundFont spec!  Also note that
 * #FLUID_MOD_GC and #FLUID_MOD_CC are in the flags field instead of the source field.
 */
enum fluid_mod_flags
{
    FLUID_MOD_POSITIVE = 0,       /**< Mapping function is positive */
    FLUID_MOD_NEGATIVE = 1,       /**< Mapping function is negative */
    FLUID_MOD_UNIPOLAR = 0,       /**< Mapping function is unipolar */
    FLUID_MOD_BIPOLAR = 2,        /**< Mapping function is bipolar */
    FLUID_MOD_LINEAR = 0,         /**< Linear mapping function */
    FLUID_MOD_CONCAVE = 4,        /**< Concave mapping function */
    FLUID_MOD_CONVEX = 8,         /**< Convex mapping function */
    FLUID_MOD_SWITCH = 12,        /**< Switch (on/off) mapping function */
    FLUID_MOD_GC = 0,             /**< General controller source type (#fluid_mod_src) */
    FLUID_MOD_CC = 16,             /**< MIDI CC controller (source will be a MIDI CC number) */

    FLUID_MOD_SIN = 0x80,            /**< Custom non-standard sinus mapping function */
};

/**
 * Transform types for the SoundFont2 modulators as defined by SoundFont 2.04 section 8.3.
 */
enum fluid_mod_transforms
{
    FLUID_MOD_TRANSFORM_LINEAR = 0, /**< Linear: directly add the computed value to summing node */
    FLUID_MOD_TRANSFORM_ABS = 2     /**< Abs: add the absolute value of the computed to summing node */
};

/**
 * General controller (if #FLUID_MOD_GC in flags).  This
 * corresponds to SoundFont 2.04 PDF section 8.2.1
 */
enum fluid_mod_src
{
    FLUID_MOD_NONE = 0,                   /**< No source controller */
    FLUID_MOD_VELOCITY = 2,               /**< MIDI note-on velocity */
    FLUID_MOD_KEY = 3,                    /**< MIDI note-on note number */
    FLUID_MOD_KEYPRESSURE = 10,           /**< MIDI key pressure */
    FLUID_MOD_CHANNELPRESSURE = 13,       /**< MIDI channel pressure */
    FLUID_MOD_PITCHWHEEL = 14,            /**< Pitch wheel */
    FLUID_MOD_PITCHWHEELSENS = 16         /**< Pitch wheel sensitivity */
};

/** @startlifecycle{Modulator} */
FLUIDSYNTH_API fluid_mod_t *new_fluid_mod(void);
FLUIDSYNTH_API void delete_fluid_mod(fluid_mod_t *mod);
/** @endlifecycle */

FLUIDSYNTH_API size_t fluid_mod_sizeof(void);

FLUIDSYNTH_API void fluid_mod_set_source1(fluid_mod_t *mod, int src, int flags);
FLUIDSYNTH_API void fluid_mod_set_source2(fluid_mod_t *mod, int src, int flags);
FLUIDSYNTH_API void fluid_mod_set_dest(fluid_mod_t *mod, int dst);
FLUIDSYNTH_API void fluid_mod_set_amount(fluid_mod_t *mod, double amount);
FLUIDSYNTH_API void fluid_mod_set_transform(fluid_mod_t *mod, int type);

FLUIDSYNTH_API int fluid_mod_get_source1(const fluid_mod_t *mod);
FLUIDSYNTH_API int fluid_mod_get_flags1(const fluid_mod_t *mod);
FLUIDSYNTH_API int fluid_mod_get_source2(const fluid_mod_t *mod);
FLUIDSYNTH_API int fluid_mod_get_flags2(const fluid_mod_t *mod);
FLUIDSYNTH_API int fluid_mod_get_dest(const fluid_mod_t *mod);
FLUIDSYNTH_API double fluid_mod_get_amount(const fluid_mod_t *mod);
FLUIDSYNTH_API int fluid_mod_get_transform(fluid_mod_t *mod);

FLUIDSYNTH_API int fluid_mod_test_identity(const fluid_mod_t *mod1, const fluid_mod_t *mod2);
FLUIDSYNTH_API int fluid_mod_has_source(const fluid_mod_t *mod, int cc, int ctrl);
FLUIDSYNTH_API int fluid_mod_has_dest(const fluid_mod_t *mod, int gen);

FLUIDSYNTH_API void fluid_mod_clone(fluid_mod_t *mod, const fluid_mod_t *src);
/** @} */

#ifdef __cplusplus
}
#endif
#endif /* _FLUIDSYNTH_MOD_H */

