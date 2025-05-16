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

#ifndef _FLUIDSYNTH_TYPES_H
#define _FLUIDSYNTH_TYPES_H



#ifdef __cplusplus
extern "C" {
#endif


/**
 * @defgroup Types Types
 * @brief Type declarations
 *
 * @{
 */

typedef struct _fluid_hashtable_t fluid_settings_t;             /**< Configuration settings instance */
typedef struct _fluid_synth_t fluid_synth_t;                    /**< Synthesizer instance */
typedef struct _fluid_voice_t fluid_voice_t;                    /**< Synthesis voice instance */
typedef struct _fluid_sfloader_t fluid_sfloader_t;              /**< SoundFont loader plugin */
typedef struct _fluid_sfont_t fluid_sfont_t;                    /**< SoundFont */
typedef struct _fluid_preset_t fluid_preset_t;                  /**< SoundFont preset */
typedef struct _fluid_sample_t fluid_sample_t;                  /**< SoundFont sample */
typedef struct _fluid_mod_t fluid_mod_t;                        /**< SoundFont modulator */
typedef struct _fluid_audio_driver_t fluid_audio_driver_t;      /**< Audio driver instance */
typedef struct _fluid_file_renderer_t fluid_file_renderer_t;    /**< Audio file renderer instance */
typedef struct _fluid_player_t fluid_player_t;                  /**< MIDI player instance */
typedef struct _fluid_midi_event_t fluid_midi_event_t;          /**< MIDI event */
typedef struct _fluid_midi_driver_t fluid_midi_driver_t;        /**< MIDI driver instance */
typedef struct _fluid_midi_router_t fluid_midi_router_t;        /**< MIDI router instance */
typedef struct _fluid_midi_router_rule_t fluid_midi_router_rule_t;      /**< MIDI router rule */
typedef struct _fluid_hashtable_t fluid_cmd_hash_t;             /**< Command handler hash table */
typedef struct _fluid_shell_t fluid_shell_t;                    /**< Command shell */
typedef struct _fluid_server_t fluid_server_t;                  /**< TCP/IP shell server instance */
typedef struct _fluid_event_t fluid_event_t;                    /**< Sequencer event */
typedef struct _fluid_sequencer_t fluid_sequencer_t;            /**< Sequencer instance */
typedef struct _fluid_cmd_handler_t fluid_cmd_handler_t;        /**< Shell Command Handler */
typedef struct _fluid_ladspa_fx_t fluid_ladspa_fx_t;            /**< LADSPA effects instance */
typedef struct _fluid_file_callbacks_t fluid_file_callbacks_t;  /**< Callback struct to perform custom file loading of soundfonts */

typedef int fluid_istream_t;    /**< Input stream descriptor */
typedef int fluid_ostream_t;    /**< Output stream descriptor */

typedef short fluid_seq_id_t; /**< Unique client IDs used by the sequencer and #fluid_event_t, obtained by fluid_sequencer_register_client() and fluid_sequencer_register_fluidsynth() */

#if defined(_MSC_VER) && (_MSC_VER < 1800)
typedef __int64 fluid_long_long_t; // even on 32bit windows
#else
/**
 * A typedef for C99's type long long, which is at least 64-bit wide, as guaranteed by the C99.
 * @p __int64 will be used as replacement for VisualStudio 2010 and older.
 */
typedef long long fluid_long_long_t;
#endif

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _FLUIDSYNTH_TYPES_H */
