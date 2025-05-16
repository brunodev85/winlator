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

#ifndef _FLUIDSYNTH_EVENT_H
#define _FLUIDSYNTH_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup sequencer_events Sequencer Events
 * @ingroup sequencer
 *
 * Create, modify, query and destroy sequencer events.
 *
 * @{
 */

/**
 * Sequencer event type enumeration.
 */
enum fluid_seq_event_type
{
    FLUID_SEQ_NOTE = 0,		/**< Note event with duration */
    FLUID_SEQ_NOTEON,		/**< Note on event */
    FLUID_SEQ_NOTEOFF,		/**< Note off event */
    FLUID_SEQ_ALLSOUNDSOFF,	/**< All sounds off event */
    FLUID_SEQ_ALLNOTESOFF,	/**< All notes off event */
    FLUID_SEQ_BANKSELECT,		/**< Bank select message */
    FLUID_SEQ_PROGRAMCHANGE,	/**< Program change message */
    FLUID_SEQ_PROGRAMSELECT,	/**< Program select message */
    FLUID_SEQ_PITCHBEND,		/**< Pitch bend message */
    FLUID_SEQ_PITCHWHEELSENS,	/**< Pitch wheel sensitivity set message @since 1.1.0 was misspelled previously */
    FLUID_SEQ_MODULATION,		/**< Modulation controller event */
    FLUID_SEQ_SUSTAIN,		/**< Sustain controller event */
    FLUID_SEQ_CONTROLCHANGE,	/**< MIDI control change event */
    FLUID_SEQ_PAN,		/**< Stereo pan set event */
    FLUID_SEQ_VOLUME,		/**< Volume set event */
    FLUID_SEQ_REVERBSEND,		/**< Reverb send set event */
    FLUID_SEQ_CHORUSSEND,		/**< Chorus send set event */
    FLUID_SEQ_TIMER,		/**< Timer event (useful for giving a callback at a certain time) */
    FLUID_SEQ_CHANNELPRESSURE,    /**< Channel aftertouch event @since 1.1.0 */
    FLUID_SEQ_KEYPRESSURE,        /**< Polyphonic aftertouch event @since 2.0.0 */
    FLUID_SEQ_SYSTEMRESET,        /**< System reset event @since 1.1.0 */
    FLUID_SEQ_UNREGISTERING,      /**< Called when a sequencer client is being unregistered. @since 1.1.0 */
    FLUID_SEQ_SCALE,              /**< Sets a new time scale for the sequencer @since 2.2.0 */
    FLUID_SEQ_LASTEVENT		/**< @internal Defines the count of events enums @warning This symbol 
                              is not part of the public API and ABI stability guarantee and 
                              may change at any time! */
};

/* Event alloc/free */
/** @startlifecycle{Sequencer Event} */
FLUIDSYNTH_API fluid_event_t *new_fluid_event(void);
FLUIDSYNTH_API void delete_fluid_event(fluid_event_t *evt);
/** @endlifecycle */

/* Initializing events */
FLUIDSYNTH_API void fluid_event_set_source(fluid_event_t *evt, fluid_seq_id_t src);
FLUIDSYNTH_API void fluid_event_set_dest(fluid_event_t *evt, fluid_seq_id_t dest);

/* Timer events */
FLUIDSYNTH_API void fluid_event_timer(fluid_event_t *evt, void *data);

/* Note events */
FLUIDSYNTH_API void fluid_event_note(fluid_event_t *evt, int channel,
                                     short key, short vel,
                                     unsigned int duration);

FLUIDSYNTH_API void fluid_event_noteon(fluid_event_t *evt, int channel, short key, short vel);
FLUIDSYNTH_API void fluid_event_noteoff(fluid_event_t *evt, int channel, short key);
FLUIDSYNTH_API void fluid_event_all_sounds_off(fluid_event_t *evt, int channel);
FLUIDSYNTH_API void fluid_event_all_notes_off(fluid_event_t *evt, int channel);

/* Instrument selection */
FLUIDSYNTH_API void fluid_event_bank_select(fluid_event_t *evt, int channel, short bank_num);
FLUIDSYNTH_API void fluid_event_program_change(fluid_event_t *evt, int channel, int preset_num);
FLUIDSYNTH_API void fluid_event_program_select(fluid_event_t *evt, int channel, unsigned int sfont_id, short bank_num, short preset_num);

/* Real-time generic instrument controllers */
FLUIDSYNTH_API
void fluid_event_control_change(fluid_event_t *evt, int channel, short control, int val);

/* Real-time instrument controllers shortcuts */
FLUIDSYNTH_API void fluid_event_pitch_bend(fluid_event_t *evt, int channel, int val);
FLUIDSYNTH_API void fluid_event_pitch_wheelsens(fluid_event_t *evt, int channel, int val);
FLUIDSYNTH_API void fluid_event_modulation(fluid_event_t *evt, int channel, int val);
FLUIDSYNTH_API void fluid_event_sustain(fluid_event_t *evt, int channel, int val);
FLUIDSYNTH_API void fluid_event_pan(fluid_event_t *evt, int channel, int val);
FLUIDSYNTH_API void fluid_event_volume(fluid_event_t *evt, int channel, int val);
FLUIDSYNTH_API void fluid_event_reverb_send(fluid_event_t *evt, int channel, int val);
FLUIDSYNTH_API void fluid_event_chorus_send(fluid_event_t *evt, int channel, int val);

FLUIDSYNTH_API void fluid_event_key_pressure(fluid_event_t *evt, int channel, short key, int val);
FLUIDSYNTH_API void fluid_event_channel_pressure(fluid_event_t *evt, int channel, int val);
FLUIDSYNTH_API void fluid_event_system_reset(fluid_event_t *evt);

/* Only when unregistering clients */
FLUIDSYNTH_API void fluid_event_unregistering(fluid_event_t *evt);

FLUIDSYNTH_API void fluid_event_scale(fluid_event_t *evt, double new_scale);
FLUIDSYNTH_API int fluid_event_from_midi_event(fluid_event_t *, const fluid_midi_event_t *);

/* Accessing event data */
FLUIDSYNTH_API int fluid_event_get_type(fluid_event_t *evt);
FLUIDSYNTH_API fluid_seq_id_t fluid_event_get_source(fluid_event_t *evt);
FLUIDSYNTH_API fluid_seq_id_t fluid_event_get_dest(fluid_event_t *evt);
FLUIDSYNTH_API int fluid_event_get_channel(fluid_event_t *evt);
FLUIDSYNTH_API short fluid_event_get_key(fluid_event_t *evt);
FLUIDSYNTH_API short fluid_event_get_velocity(fluid_event_t *evt);
FLUIDSYNTH_API short fluid_event_get_control(fluid_event_t *evt);
FLUIDSYNTH_API int fluid_event_get_value(fluid_event_t *evt);
FLUIDSYNTH_API int fluid_event_get_program(fluid_event_t *evt);
FLUIDSYNTH_API void *fluid_event_get_data(fluid_event_t *evt);
FLUIDSYNTH_API unsigned int fluid_event_get_duration(fluid_event_t *evt);
FLUIDSYNTH_API short fluid_event_get_bank(fluid_event_t *evt);
FLUIDSYNTH_API int fluid_event_get_pitch(fluid_event_t *evt);
FLUIDSYNTH_API double fluid_event_get_scale(fluid_event_t *evt);
FLUIDSYNTH_API unsigned int fluid_event_get_sfont_id(fluid_event_t *evt);
/** @} */

#ifdef __cplusplus
}
#endif
#endif /* _FLUIDSYNTH_EVENT_H */
