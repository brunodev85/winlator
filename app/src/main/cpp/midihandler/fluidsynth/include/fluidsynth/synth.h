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

#ifndef _FLUIDSYNTH_SYNTH_H
#define _FLUIDSYNTH_SYNTH_H


#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup synth Synthesizer
 *
 * SoundFont synthesizer
 *
 * You create a new synthesizer with new_fluid_synth() and you destroy
 * it with delete_fluid_synth(). Use the fluid_settings_t structure to specify
 * the synthesizer characteristics.
 *
 * You have to load a SoundFont in order to hear any sound. For that
 * you use the fluid_synth_sfload() function.
 *
 * You can use the audio driver functions to open
 * the audio device and create a background audio thread.
 *
 * The API for sending MIDI events is probably what you expect:
 * fluid_synth_noteon(), fluid_synth_noteoff(), ...
 *
 * @{
 */

/** @startlifecycle{Synthesizer} */
FLUIDSYNTH_API fluid_synth_t *new_fluid_synth(fluid_settings_t *settings);
FLUIDSYNTH_API void delete_fluid_synth(fluid_synth_t *synth);
/** @endlifecycle */

FLUIDSYNTH_API double fluid_synth_get_cpu_load(fluid_synth_t *synth);
FLUID_DEPRECATED FLUIDSYNTH_API const char *fluid_synth_error(fluid_synth_t *synth);
/** @} */

/**
 * @defgroup midi_messages MIDI Channel Messages
 * @ingroup synth
 *
 * The MIDI channel message functions are mostly directly named after their
 * counterpart MIDI messages. They are a high-level interface to controlling
 * the synthesizer, playing notes and changing note and channel parameters.
 *
 * @{
 */
FLUIDSYNTH_API int fluid_synth_noteon(fluid_synth_t *synth, int chan, int key, int vel);
FLUIDSYNTH_API int fluid_synth_noteoff(fluid_synth_t *synth, int chan, int key);
FLUIDSYNTH_API int fluid_synth_cc(fluid_synth_t *synth, int chan, int ctrl, int val);
FLUIDSYNTH_API int fluid_synth_get_cc(fluid_synth_t *synth, int chan, int ctrl, int *pval);
FLUIDSYNTH_API int fluid_synth_sysex(fluid_synth_t *synth, const char *data, int len,
                                     char *response, int *response_len, int *handled, int dryrun);
FLUIDSYNTH_API int fluid_synth_pitch_bend(fluid_synth_t *synth, int chan, int val);
FLUIDSYNTH_API int fluid_synth_get_pitch_bend(fluid_synth_t *synth, int chan, int *ppitch_bend);
FLUIDSYNTH_API int fluid_synth_pitch_wheel_sens(fluid_synth_t *synth, int chan, int val);
FLUIDSYNTH_API int fluid_synth_get_pitch_wheel_sens(fluid_synth_t *synth, int chan, int *pval);
FLUIDSYNTH_API int fluid_synth_program_change(fluid_synth_t *synth, int chan, int program);
FLUIDSYNTH_API int fluid_synth_channel_pressure(fluid_synth_t *synth, int chan, int val);
FLUIDSYNTH_API int fluid_synth_key_pressure(fluid_synth_t *synth, int chan, int key, int val);
FLUIDSYNTH_API int fluid_synth_bank_select(fluid_synth_t *synth, int chan, int bank);
FLUIDSYNTH_API int fluid_synth_sfont_select(fluid_synth_t *synth, int chan, int sfont_id);
FLUIDSYNTH_API
int fluid_synth_program_select(fluid_synth_t *synth, int chan, int sfont_id,
                               int bank_num, int preset_num);
FLUIDSYNTH_API int
fluid_synth_program_select_by_sfont_name(fluid_synth_t *synth, int chan,
        const char *sfont_name, int bank_num,
        int preset_num);
FLUIDSYNTH_API
int fluid_synth_get_program(fluid_synth_t *synth, int chan, int *sfont_id,
                            int *bank_num, int *preset_num);
FLUIDSYNTH_API int fluid_synth_unset_program(fluid_synth_t *synth, int chan);
FLUIDSYNTH_API int fluid_synth_program_reset(fluid_synth_t *synth);
FLUIDSYNTH_API int fluid_synth_system_reset(fluid_synth_t *synth);

FLUIDSYNTH_API int fluid_synth_all_notes_off(fluid_synth_t *synth, int chan);
FLUIDSYNTH_API int fluid_synth_all_sounds_off(fluid_synth_t *synth, int chan);

FLUIDSYNTH_API int fluid_synth_set_gen(fluid_synth_t *synth, int chan,
                                       int param, float value);
FLUIDSYNTH_API float fluid_synth_get_gen(fluid_synth_t *synth, int chan, int param);
/** @} MIDI Channel Messages */


/**
 * @defgroup voice_control Synthesis Voice Control
 * @ingroup synth
 *
 * Low-level access to synthesis voices.
 *
 * @{
 */
FLUIDSYNTH_API int fluid_synth_start(fluid_synth_t *synth, unsigned int id,
                                     fluid_preset_t *preset, int audio_chan,
                                     int midi_chan, int key, int vel);
FLUIDSYNTH_API int fluid_synth_stop(fluid_synth_t *synth, unsigned int id);

FLUIDSYNTH_API fluid_voice_t *fluid_synth_alloc_voice(fluid_synth_t *synth,
        fluid_sample_t *sample,
        int channum, int key, int vel);
FLUIDSYNTH_API void fluid_synth_start_voice(fluid_synth_t *synth, fluid_voice_t *voice);
FLUIDSYNTH_API void fluid_synth_get_voicelist(fluid_synth_t *synth,
        fluid_voice_t *buf[], int bufsize, int ID);
/** @} Voice Control */


/**
 * @defgroup soundfont_management SoundFont Management
 * @ingroup synth
 *
 * Functions to load and unload SoundFonts.
 *
 * @{
 */
FLUIDSYNTH_API
int fluid_synth_sfload(fluid_synth_t *synth, const char *filename, int reset_presets);
FLUIDSYNTH_API int fluid_synth_sfreload(fluid_synth_t *synth, int id);
FLUIDSYNTH_API int fluid_synth_sfunload(fluid_synth_t *synth, int id, int reset_presets);
FLUIDSYNTH_API int fluid_synth_add_sfont(fluid_synth_t *synth, fluid_sfont_t *sfont);
FLUIDSYNTH_API int fluid_synth_remove_sfont(fluid_synth_t *synth, fluid_sfont_t *sfont);
FLUIDSYNTH_API int fluid_synth_sfcount(fluid_synth_t *synth);
FLUIDSYNTH_API fluid_sfont_t *fluid_synth_get_sfont(fluid_synth_t *synth, unsigned int num);
FLUIDSYNTH_API fluid_sfont_t *fluid_synth_get_sfont_by_id(fluid_synth_t *synth, int id);
FLUIDSYNTH_API fluid_sfont_t *fluid_synth_get_sfont_by_name(fluid_synth_t *synth,
        const char *name);
FLUIDSYNTH_API int fluid_synth_set_bank_offset(fluid_synth_t *synth, int sfont_id, int offset);
FLUIDSYNTH_API int fluid_synth_get_bank_offset(fluid_synth_t *synth, int sfont_id);
/** @} Soundfont Management */


/**
 * @defgroup reverb_effect Effect - Reverb
 * @ingroup synth
 *
 * Functions for configuring the built-in reverb effect
 *
 * @{
 */
FLUID_DEPRECATED FLUIDSYNTH_API void fluid_synth_set_reverb_on(fluid_synth_t *synth, int on);
FLUIDSYNTH_API int fluid_synth_reverb_on(fluid_synth_t *synth, int fx_group, int on);

FLUID_DEPRECATED FLUIDSYNTH_API int fluid_synth_set_reverb(fluid_synth_t *synth, double roomsize,
                                          double damping, double width, double level);
FLUID_DEPRECATED FLUIDSYNTH_API int fluid_synth_set_reverb_roomsize(fluid_synth_t *synth, double roomsize);
FLUID_DEPRECATED FLUIDSYNTH_API int fluid_synth_set_reverb_damp(fluid_synth_t *synth, double damping);
FLUID_DEPRECATED FLUIDSYNTH_API int fluid_synth_set_reverb_width(fluid_synth_t *synth, double width);
FLUID_DEPRECATED FLUIDSYNTH_API int fluid_synth_set_reverb_level(fluid_synth_t *synth, double level);

FLUID_DEPRECATED FLUIDSYNTH_API double fluid_synth_get_reverb_roomsize(fluid_synth_t *synth);
FLUID_DEPRECATED FLUIDSYNTH_API double fluid_synth_get_reverb_damp(fluid_synth_t *synth);
FLUID_DEPRECATED FLUIDSYNTH_API double fluid_synth_get_reverb_level(fluid_synth_t *synth);
FLUID_DEPRECATED FLUIDSYNTH_API double fluid_synth_get_reverb_width(fluid_synth_t *synth);

FLUIDSYNTH_API int fluid_synth_set_reverb_group_roomsize(fluid_synth_t *synth, int fx_group, double roomsize);
FLUIDSYNTH_API int fluid_synth_set_reverb_group_damp(fluid_synth_t *synth, int fx_group, double damping);
FLUIDSYNTH_API int fluid_synth_set_reverb_group_width(fluid_synth_t *synth, int fx_group, double width);
FLUIDSYNTH_API int fluid_synth_set_reverb_group_level(fluid_synth_t *synth, int fx_group, double level);

FLUIDSYNTH_API int fluid_synth_get_reverb_group_roomsize(fluid_synth_t *synth, int fx_group, double *roomsize);
FLUIDSYNTH_API int fluid_synth_get_reverb_group_damp(fluid_synth_t *synth, int fx_group, double *damping);
FLUIDSYNTH_API int fluid_synth_get_reverb_group_width(fluid_synth_t *synth, int fx_group, double *width);
FLUIDSYNTH_API int fluid_synth_get_reverb_group_level(fluid_synth_t *synth, int fx_group, double *level);
 /** @} Reverb */


/**
 * @defgroup chorus_effect Effect - Chorus
 * @ingroup synth
 *
 * Functions for configuring the built-in chorus effect
 *
 * @{
 */

/**
 * Chorus modulation waveform type.
 */
enum fluid_chorus_mod
{
    FLUID_CHORUS_MOD_SINE = 0,            /**< Sine wave chorus modulation */
    FLUID_CHORUS_MOD_TRIANGLE = 1         /**< Triangle wave chorus modulation */
};


FLUID_DEPRECATED FLUIDSYNTH_API void fluid_synth_set_chorus_on(fluid_synth_t *synth, int on);
FLUIDSYNTH_API int fluid_synth_chorus_on(fluid_synth_t *synth, int fx_group, int on);

FLUID_DEPRECATED FLUIDSYNTH_API int fluid_synth_set_chorus(fluid_synth_t *synth, int nr, double level,
                                          double speed, double depth_ms, int type);
FLUID_DEPRECATED FLUIDSYNTH_API int fluid_synth_set_chorus_nr(fluid_synth_t *synth, int nr);
FLUID_DEPRECATED FLUIDSYNTH_API int fluid_synth_set_chorus_level(fluid_synth_t *synth, double level);
FLUID_DEPRECATED FLUIDSYNTH_API int fluid_synth_set_chorus_speed(fluid_synth_t *synth, double speed);
FLUID_DEPRECATED FLUIDSYNTH_API int fluid_synth_set_chorus_depth(fluid_synth_t *synth, double depth_ms);
FLUID_DEPRECATED FLUIDSYNTH_API int fluid_synth_set_chorus_type(fluid_synth_t *synth, int type);

FLUID_DEPRECATED FLUIDSYNTH_API int fluid_synth_get_chorus_nr(fluid_synth_t *synth);
FLUID_DEPRECATED FLUIDSYNTH_API double fluid_synth_get_chorus_level(fluid_synth_t *synth);
FLUID_DEPRECATED FLUIDSYNTH_API double fluid_synth_get_chorus_speed(fluid_synth_t *synth);
FLUID_DEPRECATED FLUIDSYNTH_API double fluid_synth_get_chorus_depth(fluid_synth_t *synth);
FLUID_DEPRECATED FLUIDSYNTH_API int fluid_synth_get_chorus_type(fluid_synth_t *synth); /* see fluid_chorus_mod */

FLUIDSYNTH_API int fluid_synth_set_chorus_group_nr(fluid_synth_t *synth, int fx_group, int nr);
FLUIDSYNTH_API int fluid_synth_set_chorus_group_level(fluid_synth_t *synth, int fx_group, double level);
FLUIDSYNTH_API int fluid_synth_set_chorus_group_speed(fluid_synth_t *synth, int fx_group, double speed);
FLUIDSYNTH_API int fluid_synth_set_chorus_group_depth(fluid_synth_t *synth, int fx_group, double depth_ms);
FLUIDSYNTH_API int fluid_synth_set_chorus_group_type(fluid_synth_t *synth, int fx_group, int type);

FLUIDSYNTH_API int fluid_synth_get_chorus_group_nr(fluid_synth_t *synth, int fx_group, int *nr);
FLUIDSYNTH_API int fluid_synth_get_chorus_group_level(fluid_synth_t *synth, int fx_group, double *level);
FLUIDSYNTH_API int fluid_synth_get_chorus_group_speed(fluid_synth_t *synth, int fx_group, double *speed);
FLUIDSYNTH_API int fluid_synth_get_chorus_group_depth(fluid_synth_t *synth, int fx_group, double *depth_ms);
FLUIDSYNTH_API int fluid_synth_get_chorus_group_type(fluid_synth_t *synth, int fx_group, int *type);
/** @} Chorus */

/**
 * @defgroup synthesis_params Synthesis Parameters
 * @ingroup synth
 *
 * Functions to control and query synthesis parameters like gain and
 * polyphony count.
 *
 * @{
 */
FLUIDSYNTH_API int fluid_synth_count_midi_channels(fluid_synth_t *synth);
FLUIDSYNTH_API int fluid_synth_count_audio_channels(fluid_synth_t *synth);
FLUIDSYNTH_API int fluid_synth_count_audio_groups(fluid_synth_t *synth);
FLUIDSYNTH_API int fluid_synth_count_effects_channels(fluid_synth_t *synth);
FLUIDSYNTH_API int fluid_synth_count_effects_groups(fluid_synth_t *synth);

FLUID_DEPRECATED FLUIDSYNTH_API void fluid_synth_set_sample_rate(fluid_synth_t *synth, float sample_rate);
FLUIDSYNTH_API void fluid_synth_set_gain(fluid_synth_t *synth, float gain);
FLUIDSYNTH_API float fluid_synth_get_gain(fluid_synth_t *synth);
FLUIDSYNTH_API int fluid_synth_set_polyphony(fluid_synth_t *synth, int polyphony);
FLUIDSYNTH_API int fluid_synth_get_polyphony(fluid_synth_t *synth);
FLUIDSYNTH_API int fluid_synth_get_active_voice_count(fluid_synth_t *synth);
FLUIDSYNTH_API int fluid_synth_get_internal_bufsize(fluid_synth_t *synth);

FLUIDSYNTH_API
int fluid_synth_set_interp_method(fluid_synth_t *synth, int chan, int interp_method);

/**
 * Synthesis interpolation method.
 */
enum fluid_interp
{
    FLUID_INTERP_NONE = 0,        /**< No interpolation: Fastest, but questionable audio quality */
    FLUID_INTERP_LINEAR = 1,      /**< Straight-line interpolation: A bit slower, reasonable audio quality */
    FLUID_INTERP_4THORDER = 4,    /**< Fourth-order interpolation, best quality, the default */

    /**
     * Seventh-point sinc interpolation
     * @note This interpolation method was believed to provide highest quality. However, in Feb. 2025 it was discovered
     * that for certain samples it does introduce ringing artifacts, which
     * are not present in the 4th order interpolation. This is not a bug, it's rather a limitation of only using 7 points for the sinc interpolation.
     */
    FLUID_INTERP_7THORDER = 7,

    FLUID_INTERP_DEFAULT = FLUID_INTERP_4THORDER, /**< Default interpolation method */
    FLUID_INTERP_HIGHEST = FLUID_INTERP_7THORDER, /**< Highest interpolation method */
};

/**
 * Enum used with fluid_synth_add_default_mod() to specify how to handle duplicate modulators.
 */
enum fluid_synth_add_mod
{
    FLUID_SYNTH_OVERWRITE,        /**< Overwrite any existing matching modulator */
    FLUID_SYNTH_ADD,              /**< Sum up modulator amounts */
};

FLUIDSYNTH_API int fluid_synth_add_default_mod(fluid_synth_t *synth, const fluid_mod_t *mod, int mode);
FLUIDSYNTH_API int fluid_synth_remove_default_mod(fluid_synth_t *synth, const fluid_mod_t *mod);
/** @} Synthesis Parameters */


/**
 * @defgroup tuning MIDI Tuning
 * @ingroup synth
 *
 * The functions in this section implement the MIDI Tuning Standard interface.
 *
 * @{
 */
FLUIDSYNTH_API
int fluid_synth_activate_key_tuning(fluid_synth_t *synth, int bank, int prog,
                                    const char *name, const double *pitch, int apply);
FLUIDSYNTH_API
int fluid_synth_activate_octave_tuning(fluid_synth_t *synth, int bank, int prog,
                                       const char *name, const double *pitch, int apply);
FLUIDSYNTH_API
int fluid_synth_tune_notes(fluid_synth_t *synth, int bank, int prog,
                           int len, const int *keys, const double *pitch, int apply);
FLUIDSYNTH_API
int fluid_synth_activate_tuning(fluid_synth_t *synth, int chan, int bank, int prog,
                                int apply);
FLUIDSYNTH_API
int fluid_synth_deactivate_tuning(fluid_synth_t *synth, int chan, int apply);
FLUIDSYNTH_API void fluid_synth_tuning_iteration_start(fluid_synth_t *synth);
FLUIDSYNTH_API
int fluid_synth_tuning_iteration_next(fluid_synth_t *synth, int *bank, int *prog);
FLUIDSYNTH_API int fluid_synth_tuning_dump(fluid_synth_t *synth, int bank, int prog,
        char *name, int len, double *pitch);
/** @} MIDI Tuning */


/**
 * @defgroup audio_rendering Audio Rendering
 * @ingroup synth
 *
 * The functions in this section can be used to render audio directly to
 * memory buffers. They are used internally by the \ref audio_driver and \ref file_renderer,
 * but can also be used manually for custom processing of the rendered audio.
 *
 * @note Please note that all following functions block during rendering. If your goal is to
 * render real-time audio, ensure that you call these functions from a high-priority
 * thread with little to no other duties other than calling the rendering functions.
 *
 * @warning
 * If a concurrently running thread calls any other sound affecting synth function
 * (e.g. fluid_synth_noteon(), fluid_synth_cc(), etc.) it is unspecified whether the event triggered by such a call
 * will be effective in the recently synthesized audio. While this is inaudible when only requesting small chunks from the
 * synth with every call (cf. fluid_synth_get_internal_bufsize()), it will become evident when requesting larger sample chunks:
 * With larger sample chunks it will get harder for the synth to react on those spontaneously occurring events in time
 * (like events received from a MIDI driver, or directly made synth API calls).
 * In those real-time scenarios, prefer requesting smaller
 * sample chunks from the synth with each call, to avoid poor quantization of your events in the synthesized audio.
 * This issue is not applicable when using the MIDI player or sequencer for event dispatching. Also
 * refer to the documentation of \setting{audio_period-size}.
 *
 * @{
 */
FLUIDSYNTH_API int fluid_synth_write_s16(fluid_synth_t *synth, int len,
        void *lout, int loff, int lincr,
        void *rout, int roff, int rincr);
FLUIDSYNTH_API int fluid_synth_write_float(fluid_synth_t *synth, int len,
        void *lout, int loff, int lincr,
        void *rout, int roff, int rincr);
FLUID_DEPRECATED FLUIDSYNTH_API int fluid_synth_nwrite_float(fluid_synth_t *synth, int len,
        float **left, float **right,
        float **fx_left, float **fx_right);
FLUIDSYNTH_API int fluid_synth_process(fluid_synth_t *synth, int len,
                                       int nfx, float *fx[],
                                       int nout, float *out[]);
/** @} Audio Rendering */


/**
 * @defgroup iir_filter Effect - IIR Filter
 * @ingroup synth
 *
 * Functions for configuring the built-in IIR filter effect
 *
 * @{
 */

/**
 * Specifies the type of filter to use for the custom IIR filter
 */
enum fluid_iir_filter_type
{
    FLUID_IIR_DISABLED = 0, /**< Custom IIR filter is not operating */
    FLUID_IIR_LOWPASS, /**< Custom IIR filter is operating as low-pass filter */
    FLUID_IIR_HIGHPASS, /**< Custom IIR filter is operating as high-pass filter */
    FLUID_IIR_LAST /**< @internal Value defines the count of filter types (#fluid_iir_filter_type) @warning This symbol is not part of the public API and ABI stability guarantee and may change at any time! */
};

/**
 * Specifies optional settings to use for the custom IIR filter. Can be bitwise ORed.
 */
enum fluid_iir_filter_flags
{
    FLUID_IIR_Q_LINEAR = 1 << 0, /**< The Soundfont spec requires the filter Q to be interpreted in dB. If this flag is set the filter Q is instead assumed to be in a linear range */
    FLUID_IIR_Q_ZERO_OFF = 1 << 1, /**< If this flag the filter is switched off if Q == 0 (prior to any transformation) */
    FLUID_IIR_NO_GAIN_AMP = 1 << 2 /**< The Soundfont spec requires to correct the gain of the filter depending on the filter's Q. If this flag is set the filter gain will not be corrected. */
};

FLUIDSYNTH_API int fluid_synth_set_custom_filter(fluid_synth_t *, int type, int flags);
/** @} IIR Filter */




/**
 * @defgroup channel_setup MIDI Channel Setup
 * @ingroup synth
 *
 * The functions in this section provide interfaces to change the channel type
 * and to configure basic channels, legato and portamento setups.
 *
 * @{
 */

/** @name Channel Type
 * @{
 */

/**
 * The midi channel type used by fluid_synth_set_channel_type()
 */
enum fluid_midi_channel_type
{
    CHANNEL_TYPE_MELODIC = 0, /**< Melodic midi channel */
    CHANNEL_TYPE_DRUM = 1 /**< Drum midi channel */
};

FLUIDSYNTH_API int fluid_synth_set_channel_type(fluid_synth_t *synth, int chan, int type);
/** @} Channel Type */


/** @name Basic Channel Mode
 * @{
 */

/**
 * Channel mode bits OR-ed together so that it matches with the midi spec: poly omnion (0), mono omnion (1), poly omnioff (2), mono omnioff (3)
 */
enum fluid_channel_mode_flags
{
    FLUID_CHANNEL_POLY_OFF = 0x01, /**< if flag is set, the basic channel is in mono on state, if not set poly is on */
    FLUID_CHANNEL_OMNI_OFF = 0x02, /**< if flag is set, the basic channel is in omni off state, if not set omni is on */
};

/**
 * Indicates the mode a basic channel is set to
 */
enum fluid_basic_channel_modes
{
    FLUID_CHANNEL_MODE_MASK = (FLUID_CHANNEL_OMNI_OFF | FLUID_CHANNEL_POLY_OFF), /**< Mask Poly and Omni bits of #fluid_channel_mode_flags, usually only used internally */
    FLUID_CHANNEL_MODE_OMNION_POLY = FLUID_CHANNEL_MODE_MASK & (~FLUID_CHANNEL_OMNI_OFF & ~FLUID_CHANNEL_POLY_OFF), /**< corresponds to MIDI mode 0 */
    FLUID_CHANNEL_MODE_OMNION_MONO = FLUID_CHANNEL_MODE_MASK & (~FLUID_CHANNEL_OMNI_OFF & FLUID_CHANNEL_POLY_OFF), /**< corresponds to MIDI mode 1 */
    FLUID_CHANNEL_MODE_OMNIOFF_POLY = FLUID_CHANNEL_MODE_MASK & (FLUID_CHANNEL_OMNI_OFF & ~FLUID_CHANNEL_POLY_OFF), /**< corresponds to MIDI mode 2 */
    FLUID_CHANNEL_MODE_OMNIOFF_MONO = FLUID_CHANNEL_MODE_MASK & (FLUID_CHANNEL_OMNI_OFF | FLUID_CHANNEL_POLY_OFF), /**< corresponds to MIDI mode 3 */
    FLUID_CHANNEL_MODE_LAST /**< @internal Value defines the count of basic channel modes (#fluid_basic_channel_modes) @warning This symbol is not part of the public API and ABI stability guarantee and may change at any time! */
};

FLUIDSYNTH_API int fluid_synth_reset_basic_channel(fluid_synth_t *synth, int chan);

FLUIDSYNTH_API int  fluid_synth_get_basic_channel(fluid_synth_t *synth, int chan,
        int *basic_chan_out,
        int *mode_chan_out,
        int *basic_val_out);
FLUIDSYNTH_API int fluid_synth_set_basic_channel(fluid_synth_t *synth, int chan, int mode, int val);

/** @} Basic Channel Mode */

/** @name Legato Mode
 * @{
 */

/**
 * Indicates the legato mode a channel is set to
 * n1,n2,n3,.. is a legato passage. n1 is the first note, and n2,n3,n4 are played legato with previous note. */
enum fluid_channel_legato_mode
{
    FLUID_CHANNEL_LEGATO_MODE_RETRIGGER, /**< Mode 0 - Release previous note, start a new note */
    FLUID_CHANNEL_LEGATO_MODE_MULTI_RETRIGGER, /**< Mode 1 - On contiguous notes retrigger in attack section using current value, shape attack using current dynamic and make use of previous voices if any */
    FLUID_CHANNEL_LEGATO_MODE_LAST /**< @internal Value defines the count of legato modes (#fluid_channel_legato_mode) @warning This symbol is not part of the public API and ABI stability guarantee and may change at any time! */
};

FLUIDSYNTH_API int fluid_synth_set_legato_mode(fluid_synth_t *synth, int chan, int legatomode);
FLUIDSYNTH_API int fluid_synth_get_legato_mode(fluid_synth_t *synth, int chan, int  *legatomode);
/** @} Legato Mode */

/** @name Portamento Mode
 * @{
 */

/**
 * Indicates the portamento mode a channel is set to
 */
enum fluid_channel_portamento_mode
{
    FLUID_CHANNEL_PORTAMENTO_MODE_EACH_NOTE, /**< Mode 0 - Portamento on each note (staccato or legato) */
    FLUID_CHANNEL_PORTAMENTO_MODE_LEGATO_ONLY, /**< Mode 1 - Portamento only on legato note */
    FLUID_CHANNEL_PORTAMENTO_MODE_STACCATO_ONLY, /**< Mode 2 - Portamento only on staccato note */
    FLUID_CHANNEL_PORTAMENTO_MODE_LAST /**< @internal Value defines the count of portamento modes
                                         @warning This symbol is not part of the public API and ABI
                                         stability guarantee and may change at any time! */
};

FLUIDSYNTH_API int fluid_synth_set_portamento_mode(fluid_synth_t *synth,
        int chan, int portamentomode);
FLUIDSYNTH_API int fluid_synth_get_portamento_mode(fluid_synth_t *synth,
        int chan, int   *portamentomode);
/** @} Portamento Mode */

/**@name Breath Mode
 * @{
 */

/**
 * Indicates the breath mode a channel is set to
 */
enum fluid_channel_breath_flags
{
    FLUID_CHANNEL_BREATH_POLY = 0x10,  /**< when channel is poly, this flag indicates that the default velocity to initial attenuation modulator is replaced by a breath to initial attenuation modulator */
    FLUID_CHANNEL_BREATH_MONO = 0x20,  /**< when channel is mono, this flag indicates that the default velocity to initial attenuation modulator is replaced by a breath modulator */
    FLUID_CHANNEL_BREATH_SYNC = 0x40,  /**< when channel is mono, this flag indicates that the breath controller(MSB)triggers noteon/noteoff on the running note */
};

FLUIDSYNTH_API int fluid_synth_set_breath_mode(fluid_synth_t *synth,
        int chan, int breathmode);
FLUIDSYNTH_API int fluid_synth_get_breath_mode(fluid_synth_t *synth,
        int chan, int  *breathmode);
/** @} Breath Mode */
/** @} MIDI Channel Setup */


/** @ingroup settings */
FLUIDSYNTH_API fluid_settings_t *fluid_synth_get_settings(fluid_synth_t *synth);

/** @ingroup soundfont_loader */
FLUIDSYNTH_API void fluid_synth_add_sfloader(fluid_synth_t *synth, fluid_sfloader_t *loader);

/** @ingroup soundfont_loader */
FLUIDSYNTH_API fluid_preset_t *fluid_synth_get_channel_preset(fluid_synth_t *synth, int chan);

/** @ingroup midi_input */
FLUIDSYNTH_API int fluid_synth_handle_midi_event(void *data, fluid_midi_event_t *event);

/** @ingroup soundfonts */
FLUIDSYNTH_API
int fluid_synth_pin_preset(fluid_synth_t *synth, int sfont_id, int bank_num, int preset_num);

/** @ingroup soundfonts */
FLUIDSYNTH_API
int fluid_synth_unpin_preset(fluid_synth_t *synth, int sfont_id, int bank_num, int preset_num);

/** @ingroup ladspa */
FLUIDSYNTH_API fluid_ladspa_fx_t *fluid_synth_get_ladspa_fx(fluid_synth_t *synth);

#ifdef __cplusplus
}
#endif

#endif /* _FLUIDSYNTH_SYNTH_H */
