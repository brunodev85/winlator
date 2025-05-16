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

#ifndef _FLUIDSYNTH_MIDI_H
#define _FLUIDSYNTH_MIDI_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup midi_input MIDI Input
 *
 * MIDI Input Subsystem
 *
 * There are multiple ways to send MIDI events to the synthesizer. They can come
 * from MIDI files, from external MIDI sequencers or raw MIDI event sources,
 * can be modified via MIDI routers and also generated manually.
 *
 * The interface connecting all sources and sinks of MIDI events in libfluidsynth
 * is \ref handle_midi_event_func_t.
 *
 * @{
 */

/**
 * Generic callback function for MIDI event handler.
 *
 * @param data User defined data pointer
 * @param event The MIDI event
 * @return Should return #FLUID_OK on success, #FLUID_FAILED otherwise
 *
 * This callback is used to pass MIDI events
 * - from \ref midi_player, \ref midi_router or \ref midi_driver
 * - to  \ref midi_router via fluid_midi_router_handle_midi_event()
 * - or to \ref synth via fluid_synth_handle_midi_event().
 *
 * Additionally, there is a translation layer to pass MIDI events to
 * a \ref sequencer via fluid_sequencer_add_midi_event_to_buffer().
 */
typedef int (*handle_midi_event_func_t)(void *data, fluid_midi_event_t *event);

/**
 * Generic callback function fired once by MIDI tick change.
 *
 * @param data User defined data pointer
 * @param tick The current (zero-based) tick, which triggered the callback
 * @return Should return #FLUID_OK on success, #FLUID_FAILED otherwise
 *
 * This callback is fired at a constant rate depending on the current BPM and PPQ.
 * e.g. for PPQ = 192 and BPM = 140 the callback is fired 192 * 140 times per minute (448/sec).
 *
 * It can be used to sync external elements with the beat,
 * or stop / loop the song on a given tick.
 * Ticks being BPM-dependent, you can manipulate values such as bars or beats,
 * without having to care about BPM.
 *
 * For example, this callback loops the song whenever it reaches the 5th bar :
 *
 * @code{.cpp}
int handle_tick(void *data, int tick)
{
    fluid_player_t *player = (fluid_player_t *)data;
    int ppq = 192; // From MIDI header
    int beatsPerBar = 4; // From the song's time signature
    int loopBar = 5;
    int loopTick = (loopBar - 1) * ppq * beatsPerBar;

    if (tick == loopTick)
    {
        return fluid_player_seek(player, 0);
    }

    return FLUID_OK;
}
 * @endcode
 */
typedef int (*handle_midi_tick_func_t)(void *data, int tick);
/** @} */

/**
 * @defgroup midi_events MIDI Events
 * @ingroup midi_input
 *
 * Functions to create, modify, query and delete MIDI events.
 *
 * These functions are intended to be used in MIDI routers and other filtering
 * and processing functions in the MIDI event path. If you want to simply
 * send MIDI messages to the synthesizer, you can use the more convenient
 * \ref midi_messages interface.
 *
 * @{
 */
/** @startlifecycle{MIDI Event} */
FLUIDSYNTH_API fluid_midi_event_t *new_fluid_midi_event(void);
FLUIDSYNTH_API void delete_fluid_midi_event(fluid_midi_event_t *event);
/** @endlifecycle */

FLUIDSYNTH_API int fluid_midi_event_set_type(fluid_midi_event_t *evt, int type);
FLUIDSYNTH_API int fluid_midi_event_get_type(const fluid_midi_event_t *evt);
FLUIDSYNTH_API int fluid_midi_event_set_channel(fluid_midi_event_t *evt, int chan);
FLUIDSYNTH_API int fluid_midi_event_get_channel(const fluid_midi_event_t *evt);
FLUIDSYNTH_API int fluid_midi_event_get_key(const fluid_midi_event_t *evt);
FLUIDSYNTH_API int fluid_midi_event_set_key(fluid_midi_event_t *evt, int key);
FLUIDSYNTH_API int fluid_midi_event_get_velocity(const fluid_midi_event_t *evt);
FLUIDSYNTH_API int fluid_midi_event_set_velocity(fluid_midi_event_t *evt, int vel);
FLUIDSYNTH_API int fluid_midi_event_get_control(const fluid_midi_event_t *evt);
FLUIDSYNTH_API int fluid_midi_event_set_control(fluid_midi_event_t *evt, int ctrl);
FLUIDSYNTH_API int fluid_midi_event_get_value(const fluid_midi_event_t *evt);
FLUIDSYNTH_API int fluid_midi_event_set_value(fluid_midi_event_t *evt, int val);
FLUIDSYNTH_API int fluid_midi_event_get_program(const fluid_midi_event_t *evt);
FLUIDSYNTH_API int fluid_midi_event_set_program(fluid_midi_event_t *evt, int val);
FLUIDSYNTH_API int fluid_midi_event_get_pitch(const fluid_midi_event_t *evt);
FLUIDSYNTH_API int fluid_midi_event_set_pitch(fluid_midi_event_t *evt, int val);
FLUIDSYNTH_API int fluid_midi_event_set_sysex(fluid_midi_event_t *evt, void *data,
        int size, int dynamic);
FLUIDSYNTH_API int fluid_midi_event_set_text(fluid_midi_event_t *evt,
        void *data, int size, int dynamic);
FLUIDSYNTH_API int fluid_midi_event_get_text(fluid_midi_event_t *evt,
        void **data, int *size);
FLUIDSYNTH_API int fluid_midi_event_set_lyrics(fluid_midi_event_t *evt,
        void *data, int size, int dynamic);
FLUIDSYNTH_API int fluid_midi_event_get_lyrics(fluid_midi_event_t *evt,
        void **data, int *size);
/** @} */

/**
 * @defgroup midi_router MIDI Router
 * @ingroup midi_input
 *
 * Rule based transformation and filtering of MIDI events.
 *
 * @{
 */

/**
 * MIDI router rule type.
 *
 * @since 1.1.0
 */
typedef enum
{
    FLUID_MIDI_ROUTER_RULE_NOTE,                  /**< MIDI note rule */
    FLUID_MIDI_ROUTER_RULE_CC,                    /**< MIDI controller rule */
    FLUID_MIDI_ROUTER_RULE_PROG_CHANGE,           /**< MIDI program change rule */
    FLUID_MIDI_ROUTER_RULE_PITCH_BEND,            /**< MIDI pitch bend rule */
    FLUID_MIDI_ROUTER_RULE_CHANNEL_PRESSURE,      /**< MIDI channel pressure rule */
    FLUID_MIDI_ROUTER_RULE_KEY_PRESSURE,          /**< MIDI key pressure rule */
    FLUID_MIDI_ROUTER_RULE_COUNT                  /**< @internal Total count of rule types. This symbol
                                                    is not part of the public API and ABI stability
                                                    guarantee and may change at any time!*/
} fluid_midi_router_rule_type;


/** @startlifecycle{MIDI Router} */
FLUIDSYNTH_API fluid_midi_router_t *new_fluid_midi_router(fluid_settings_t *settings,
        handle_midi_event_func_t handler,
        void *event_handler_data);
FLUIDSYNTH_API void delete_fluid_midi_router(fluid_midi_router_t *handler);
/** @endlifecycle */

FLUIDSYNTH_API int fluid_midi_router_set_default_rules(fluid_midi_router_t *router);
FLUIDSYNTH_API int fluid_midi_router_clear_rules(fluid_midi_router_t *router);
FLUIDSYNTH_API int fluid_midi_router_add_rule(fluid_midi_router_t *router,
        fluid_midi_router_rule_t *rule, int type);


/** @startlifecycle{MIDI Router Rule} */
FLUIDSYNTH_API fluid_midi_router_rule_t *new_fluid_midi_router_rule(void);
FLUIDSYNTH_API void delete_fluid_midi_router_rule(fluid_midi_router_rule_t *rule);
/** @endlifecycle */

FLUIDSYNTH_API void fluid_midi_router_rule_set_chan(fluid_midi_router_rule_t *rule,
        int min, int max, float mul, int add);
FLUIDSYNTH_API void fluid_midi_router_rule_set_param1(fluid_midi_router_rule_t *rule,
        int min, int max, float mul, int add);
FLUIDSYNTH_API void fluid_midi_router_rule_set_param2(fluid_midi_router_rule_t *rule,
        int min, int max, float mul, int add);
FLUIDSYNTH_API int fluid_midi_router_handle_midi_event(void *data, fluid_midi_event_t *event);
FLUIDSYNTH_API int fluid_midi_dump_prerouter(void *data, fluid_midi_event_t *event);
FLUIDSYNTH_API int fluid_midi_dump_postrouter(void *data, fluid_midi_event_t *event);
/** @} */

/**
 * @defgroup midi_driver MIDI Driver
 * @ingroup midi_input
 *
 * Functions for managing MIDI drivers.
 *
 * The available MIDI drivers depend on your platform. See \ref settings_midi for all
 * available configuration options.
 *
 * To create a MIDI driver, you need to specify a source for the MIDI events to be
 * forwarded to via the \ref fluid_midi_event_t callback. Normally this will be
 * either a \ref midi_router via fluid_midi_router_handle_midi_event() or the synthesizer
 * via fluid_synth_handle_midi_event().
 *
 * But you can also write your own handler function that preprocesses the events and
 * forwards them on to the router or synthesizer instead.
 *
 * @{
 */

/** @startlifecycle{MIDI Driver} */
FLUIDSYNTH_API
fluid_midi_driver_t *new_fluid_midi_driver(fluid_settings_t *settings,
        handle_midi_event_func_t handler,
        void *event_handler_data);

FLUIDSYNTH_API void delete_fluid_midi_driver(fluid_midi_driver_t *driver);
/** @endlifecycle */

/** @} */

/**
 * @defgroup midi_player MIDI File Player
 * @ingroup midi_input
 *
 * Parse standard MIDI files and emit MIDI events.
 *
 * @{
 */

/**
 * MIDI File Player status enum.
 * @since 1.1.0
 */
enum fluid_player_status
{
    FLUID_PLAYER_READY,           /**< Player is ready */
    FLUID_PLAYER_PLAYING,         /**< Player is currently playing */
    FLUID_PLAYER_STOPPING,        /**< Player is stopping, but hasn't finished yet (currently unused) */
    FLUID_PLAYER_DONE             /**< Player is finished playing */
};

/**
 * MIDI File Player tempo enum.
 * @since 2.2.0
 */
enum fluid_player_set_tempo_type
{
    FLUID_PLAYER_TEMPO_INTERNAL,      /**< Use midi file tempo set in midi file (120 bpm by default). Multiplied by a factor */
    FLUID_PLAYER_TEMPO_EXTERNAL_BPM,  /**< Set player tempo in bpm, supersede midi file tempo */
    FLUID_PLAYER_TEMPO_EXTERNAL_MIDI, /**< Set player tempo in us per quarter note, supersede midi file tempo */
    FLUID_PLAYER_TEMPO_NBR        /**< @internal Value defines the count of player tempo type (#fluid_player_set_tempo_type) @warning This symbol is not part of the public API and ABI stability guarantee and may change at any time! */
};

/** @startlifecycle{MIDI File Player} */
FLUIDSYNTH_API fluid_player_t *new_fluid_player(fluid_synth_t *synth);
FLUIDSYNTH_API void delete_fluid_player(fluid_player_t *player);
/** @endlifecycle */

FLUIDSYNTH_API int fluid_player_add(fluid_player_t *player, const char *midifile);
FLUIDSYNTH_API int fluid_player_add_mem(fluid_player_t *player, const void *buffer, size_t len);
FLUIDSYNTH_API int fluid_player_play(fluid_player_t *player);
FLUIDSYNTH_API int fluid_player_stop(fluid_player_t *player);
FLUIDSYNTH_API int fluid_player_join(fluid_player_t *player);
FLUIDSYNTH_API int fluid_player_set_loop(fluid_player_t *player, int loop);
FLUIDSYNTH_API int fluid_player_set_tempo(fluid_player_t *player, int tempo_type, double tempo);
FLUID_DEPRECATED FLUIDSYNTH_API int fluid_player_set_midi_tempo(fluid_player_t *player, int tempo);
FLUID_DEPRECATED FLUIDSYNTH_API int fluid_player_set_bpm(fluid_player_t *player, int bpm);
FLUIDSYNTH_API int fluid_player_set_playback_callback(fluid_player_t *player, handle_midi_event_func_t handler, void *handler_data);
FLUIDSYNTH_API int fluid_player_set_tick_callback(fluid_player_t *player, handle_midi_tick_func_t handler, void *handler_data);

FLUIDSYNTH_API int fluid_player_get_status(fluid_player_t *player);
FLUIDSYNTH_API int fluid_player_get_current_tick(fluid_player_t *player);
FLUIDSYNTH_API int fluid_player_get_total_ticks(fluid_player_t *player);
FLUIDSYNTH_API int fluid_player_get_bpm(fluid_player_t *player);
FLUIDSYNTH_API int fluid_player_get_division(fluid_player_t *player);
FLUIDSYNTH_API int fluid_player_get_midi_tempo(fluid_player_t *player);
FLUIDSYNTH_API int fluid_player_seek(fluid_player_t *player, int ticks);
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _FLUIDSYNTH_MIDI_H */
