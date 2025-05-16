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

#ifndef _FLUIDSYNTH_GEN_H
#define _FLUIDSYNTH_GEN_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup generators SoundFont Generators
 * @ingroup soundfonts
 *
 * Functions and defines for SoundFont generator effects.
 *
 * @{
 */

/**
 * Generator (effect) numbers (Soundfont 2.01 specifications section 8.1.3)
 */
enum fluid_gen_type
{
    GEN_STARTADDROFS,		/**< Sample start address offset (0-32767) */
    GEN_ENDADDROFS,		/**< Sample end address offset (-32767-0) */
    GEN_STARTLOOPADDROFS,		/**< Sample loop start address offset (-32767-32767) */
    GEN_ENDLOOPADDROFS,		/**< Sample loop end address offset (-32767-32767) */
    GEN_STARTADDRCOARSEOFS,	/**< Sample start address coarse offset (X 32768) */
    GEN_MODLFOTOPITCH,		/**< Modulation LFO to pitch */
    GEN_VIBLFOTOPITCH,		/**< Vibrato LFO to pitch */
    GEN_MODENVTOPITCH,		/**< Modulation envelope to pitch */
    GEN_FILTERFC,			/**< Filter cutoff */
    GEN_FILTERQ,			/**< Filter Q */
    GEN_MODLFOTOFILTERFC,		/**< Modulation LFO to filter cutoff */
    GEN_MODENVTOFILTERFC,		/**< Modulation envelope to filter cutoff */
    GEN_ENDADDRCOARSEOFS,		/**< Sample end address coarse offset (X 32768) */
    GEN_MODLFOTOVOL,		/**< Modulation LFO to volume */
    GEN_UNUSED1,			/**< Unused */
    GEN_CHORUSSEND,		/**< Chorus send amount */
    GEN_REVERBSEND,		/**< Reverb send amount */
    GEN_PAN,			/**< Stereo panning */
    GEN_UNUSED2,			/**< Unused */
    GEN_UNUSED3,			/**< Unused */
    GEN_UNUSED4,			/**< Unused */
    GEN_MODLFODELAY,		/**< Modulation LFO delay */
    GEN_MODLFOFREQ,		/**< Modulation LFO frequency */
    GEN_VIBLFODELAY,		/**< Vibrato LFO delay */
    GEN_VIBLFOFREQ,		/**< Vibrato LFO frequency */
    GEN_MODENVDELAY,		/**< Modulation envelope delay */
    GEN_MODENVATTACK,		/**< Modulation envelope attack */
    GEN_MODENVHOLD,		/**< Modulation envelope hold */
    GEN_MODENVDECAY,		/**< Modulation envelope decay */
    GEN_MODENVSUSTAIN,		/**< Modulation envelope sustain */
    GEN_MODENVRELEASE,		/**< Modulation envelope release */
    GEN_KEYTOMODENVHOLD,		/**< Key to modulation envelope hold */
    GEN_KEYTOMODENVDECAY,		/**< Key to modulation envelope decay */
    GEN_VOLENVDELAY,		/**< Volume envelope delay */
    GEN_VOLENVATTACK,		/**< Volume envelope attack */
    GEN_VOLENVHOLD,		/**< Volume envelope hold */
    GEN_VOLENVDECAY,		/**< Volume envelope decay */
    GEN_VOLENVSUSTAIN,		/**< Volume envelope sustain */
    GEN_VOLENVRELEASE,		/**< Volume envelope release */
    GEN_KEYTOVOLENVHOLD,		/**< Key to volume envelope hold */
    GEN_KEYTOVOLENVDECAY,		/**< Key to volume envelope decay */
    GEN_INSTRUMENT,		/**< Instrument ID (shouldn't be set by user) */
    GEN_RESERVED1,		/**< Reserved */
    GEN_KEYRANGE,			/**< MIDI note range */
    GEN_VELRANGE,			/**< MIDI velocity range */
    GEN_STARTLOOPADDRCOARSEOFS,	/**< Sample start loop address coarse offset (X 32768) */
    GEN_KEYNUM,			/**< Fixed MIDI note number */
    GEN_VELOCITY,			/**< Fixed MIDI velocity value */
    GEN_ATTENUATION,		/**< Initial volume attenuation */
    GEN_RESERVED2,		/**< Reserved */
    GEN_ENDLOOPADDRCOARSEOFS,	/**< Sample end loop address coarse offset (X 32768) */
    GEN_COARSETUNE,		/**< Coarse tuning */
    GEN_FINETUNE,			/**< Fine tuning */
    GEN_SAMPLEID,			/**< Sample ID (shouldn't be set by user) */
    GEN_SAMPLEMODE,		/**< Sample mode flags */
    GEN_RESERVED3,		/**< Reserved */
    GEN_SCALETUNE,		/**< Scale tuning */
    GEN_EXCLUSIVECLASS,		/**< Exclusive class number */
    GEN_OVERRIDEROOTKEY,		/**< Sample root note override */

    /**
     * Initial Pitch
     *
     * @note This is not "standard" SoundFont generator, because it is not
     * mentioned in the list of generators in the SF2 specifications.
     * It is used by FluidSynth internally to compute the nominal pitch of
     * a note on note-on event. By nature it shouldn't be allowed to be modulated,
     * however the specification defines a default modulator having "Initial Pitch"
     * as destination (cf. SF2.01 page 57 section 8.4.10 MIDI Pitch Wheel to Initial Pitch).
     * Thus it is impossible to cancel this default modulator, which would be required
     * to let the MIDI Pitch Wheel controller modulate a different generator.
     * In order to provide this flexibility, FluidSynth >= 2.1.0 uses a default modulator
     * "Pitch Wheel to Fine Tune", rather than Initial Pitch. The same "compromise" can
     * be found on the Audigy 2 ZS for instance.
     */
    GEN_PITCH,

    GEN_CUSTOM_BALANCE,          /**< Balance @note Not a real SoundFont generator */
    /* non-standard generator for an additional custom high- or low-pass filter */
    GEN_CUSTOM_FILTERFC,		/**< Custom filter cutoff frequency */
    GEN_CUSTOM_FILTERQ,		/**< Custom filter Q */

    GEN_LAST			/**< @internal Value defines the count of generators (#fluid_gen_type)
                          @warning This symbol is not part of the public API and ABI
                          stability guarantee and may change at any time! */
};
/** @} */

#ifdef __cplusplus
}
#endif
#endif /* _FLUIDSYNTH_GEN_H */

