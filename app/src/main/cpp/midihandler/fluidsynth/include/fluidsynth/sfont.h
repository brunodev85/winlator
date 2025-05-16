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

#ifndef _FLUIDSYNTH_SFONT_H
#define _FLUIDSYNTH_SFONT_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup soundfonts SoundFonts
 *
 * SoundFont related functions
 *
 * This part of the API contains functions, defines and types that are mostly
 * only used by internal or custom SoundFont loaders or client code that
 * modifies loaded presets, SoundFonts or voices directly.
 */

/**
 * @defgroup soundfont_loader SoundFont Loader
 * @ingroup soundfonts
 *
 * Create custom SoundFont loaders
 *
 * It is possible to add new SoundFont loaders to the
 * synthesizer. This API allows for virtual SoundFont files to be loaded
 * and synthesized, which may not actually be SoundFont files, as long as they
 * can be represented by the SoundFont synthesis model.
 *
 * To add a new SoundFont loader to the synthesizer, call
 * fluid_synth_add_sfloader() and pass a pointer to an
 * #fluid_sfloader_t instance created by new_fluid_sfloader().
 * On creation, you must specify a callback function \p load
 * that will be called for every file attempting to load it and
 * if successful returns a #fluid_sfont_t instance, or NULL if it fails.
 *
 * The #fluid_sfont_t structure contains a callback to obtain the
 * name of the SoundFont. It contains two functions to iterate
 * though the contained presets, and one function to obtain a
 * preset corresponding to a bank and preset number. This
 * function should return a #fluid_preset_t instance.
 *
 * The #fluid_preset_t instance contains some functions to obtain
 * information from the preset (name, bank, number). The most
 * important callback is the noteon function. The noteon function
 * is called by fluidsynth internally and
 * should call fluid_synth_alloc_voice() for every sample that has
 * to be played. fluid_synth_alloc_voice() expects a pointer to a
 * #fluid_sample_t instance and returns a pointer to the opaque
 * #fluid_voice_t structure. To set or increment the values of a
 * generator, use fluid_voice_gen_set() or fluid_voice_gen_incr(). When you are
 * finished initializing the voice call fluid_voice_start() to
 * start playing the synthesis voice.
 *
 * @{
 */

/**
 * Some notification enums for presets and samples.
 */
enum
{
    FLUID_PRESET_SELECTED,                /**< Preset selected notify */
    FLUID_PRESET_UNSELECTED,              /**< Preset unselected notify */
    FLUID_SAMPLE_DONE,                    /**< Sample no longer needed notify */
    FLUID_PRESET_PIN,                     /**< Request to pin preset samples to cache */
    FLUID_PRESET_UNPIN                    /**< Request to unpin preset samples from cache */
};

/**
 * Indicates the type of a sample used by the _fluid_sample_t::sampletype field.
 *
 * This enum corresponds to the \c SFSampleLink enum in the SoundFont spec.
 * One \c flag may be bit-wise OR-ed with one \c value.
 */
enum fluid_sample_type
{
    FLUID_SAMPLETYPE_MONO = 0x1, /**< Value used for mono samples */
    FLUID_SAMPLETYPE_RIGHT = 0x2, /**< Value used for right samples of a stereo pair */
    FLUID_SAMPLETYPE_LEFT = 0x4, /**< Value used for left samples of a stereo pair */
    FLUID_SAMPLETYPE_LINKED = 0x8, /**< Value used for linked sample, which is currently not supported */
    FLUID_SAMPLETYPE_OGG_VORBIS = 0x10, /**< Flag used for Ogg Vorbis compressed samples (non-standard compliant extension) as found in the program "sftools" developed by Werner Schweer from MuseScore @since 1.1.7 */
    FLUID_SAMPLETYPE_ROM = 0x8000 /**< Flag that indicates ROM samples, causing the sample to be ignored */
};


/**
 * Method to load an instrument file (does not actually need to be a real file name,
 * could be another type of string identifier that the \a loader understands).
 *
 * @param loader SoundFont loader
 * @param filename File name or other string identifier
 * @return The loaded instrument file (SoundFont) or NULL if an error occurred.
 */
typedef fluid_sfont_t *(*fluid_sfloader_load_t)(fluid_sfloader_t *loader, const char *filename);

/**
 * The free method should free the memory allocated for a fluid_sfloader_t instance in
 * addition to any private data.
 *
 * @param loader SoundFont loader
 *
 * Any custom user provided cleanup function must ultimately call
 * delete_fluid_sfloader() to ensure proper cleanup of the #fluid_sfloader_t struct. If no private data
 * needs to be freed, setting this to delete_fluid_sfloader() is sufficient.
 *
 */
typedef void (*fluid_sfloader_free_t)(fluid_sfloader_t *loader);


/** @startlifecycle{SoundFont Loader} */
FLUIDSYNTH_API fluid_sfloader_t *new_fluid_sfloader(fluid_sfloader_load_t load, fluid_sfloader_free_t free);
FLUIDSYNTH_API void delete_fluid_sfloader(fluid_sfloader_t *loader);

FLUIDSYNTH_API fluid_sfloader_t *new_fluid_defsfloader(fluid_settings_t *settings);
/** @endlifecycle */

/**
 * Opens the file or memory indicated by \c filename in binary read mode.
 *
 * @return returns a file handle on success, NULL otherwise
 *
 * \c filename matches the string provided during the fluid_synth_sfload() call.
 */
typedef void *(* fluid_sfloader_callback_open_t)(const char *filename);

/**
 * Reads \c count bytes to the specified buffer \c buf.
 *
 * @return returns #FLUID_OK if exactly \c count bytes were successfully read, else returns #FLUID_FAILED and leaves \a buf unmodified.
 */
typedef int (* fluid_sfloader_callback_read_t)(void *buf, fluid_long_long_t count, void *handle);

/**
 * Same purpose and behaviour as fseek.
 *
 * @param origin either \c SEEK_SET, \c SEEK_CUR or \c SEEK_END
 * @return returns #FLUID_OK if the seek was successfully performed while not seeking beyond a buffer or file, #FLUID_FAILED otherwise
 */
typedef int (* fluid_sfloader_callback_seek_t)(void *handle, fluid_long_long_t offset, int origin);

/**
 * Closes the handle returned by #fluid_sfloader_callback_open_t and frees used resources.
 *
 * @return returns #FLUID_OK on success, #FLUID_FAILED on error
 */
typedef int (* fluid_sfloader_callback_close_t)(void *handle);

/** @return returns current file offset or #FLUID_FAILED on error */
typedef fluid_long_long_t (* fluid_sfloader_callback_tell_t)(void *handle);


FLUIDSYNTH_API int fluid_sfloader_set_callbacks(fluid_sfloader_t *loader,
        fluid_sfloader_callback_open_t open,
        fluid_sfloader_callback_read_t read,
        fluid_sfloader_callback_seek_t seek,
        fluid_sfloader_callback_tell_t tell,
        fluid_sfloader_callback_close_t close);

FLUIDSYNTH_API int fluid_sfloader_set_data(fluid_sfloader_t *loader, void *data);
FLUIDSYNTH_API void *fluid_sfloader_get_data(fluid_sfloader_t *loader);



/**
 * Method to return the name of a virtual SoundFont.
 *
 * @param sfont Virtual SoundFont
 * @return The name of the virtual SoundFont.
 */
typedef const char *(*fluid_sfont_get_name_t)(fluid_sfont_t *sfont);

/**
 * Get a virtual SoundFont preset by bank and program numbers.
 *
 * @param sfont Virtual SoundFont
 * @param bank MIDI bank number (0-16383)
 * @param prenum MIDI preset number (0-127)
 * @return Should return an allocated virtual preset or NULL if it could not
 *   be found.
 */
typedef fluid_preset_t *(*fluid_sfont_get_preset_t)(fluid_sfont_t *sfont, int bank, int prenum);

/**
 * Start virtual SoundFont preset iteration method.
 *
 * @param sfont Virtual SoundFont
 *
 * Starts/re-starts virtual preset iteration in a SoundFont.
 */
typedef void (*fluid_sfont_iteration_start_t)(fluid_sfont_t *sfont);

/**
 * Virtual SoundFont preset iteration function.
 *
 * @param sfont Virtual SoundFont
 * @return NULL when no more presets are available, otherwise the a pointer to the current preset
 *
 * Returns preset information to the caller. The returned buffer is only valid until a subsequent
 * call to this function.
 */
typedef fluid_preset_t *(*fluid_sfont_iteration_next_t)(fluid_sfont_t *sfont);

/**
 * Method to free a virtual SoundFont bank.
 *
 * @param sfont Virtual SoundFont to free.
 * @return Should return 0 when it was able to free all resources or non-zero
 *   if some of the samples could not be freed because they are still in use,
 *   in which case the free will be tried again later, until success.
 *
 * Any custom user provided cleanup function must ultimately call
 * delete_fluid_sfont() to ensure proper cleanup of the #fluid_sfont_t struct. If no private data
 * needs to be freed, setting this to delete_fluid_sfont() is sufficient.
 */
typedef int (*fluid_sfont_free_t)(fluid_sfont_t *sfont);


/** @startlifecycle{SoundFont} */
FLUIDSYNTH_API fluid_sfont_t *new_fluid_sfont(fluid_sfont_get_name_t get_name,
        fluid_sfont_get_preset_t get_preset,
        fluid_sfont_iteration_start_t iter_start,
        fluid_sfont_iteration_next_t iter_next,
        fluid_sfont_free_t free);

FLUIDSYNTH_API int delete_fluid_sfont(fluid_sfont_t *sfont);
/** @endlifecycle */

FLUIDSYNTH_API int fluid_sfont_set_data(fluid_sfont_t *sfont, void *data);
FLUIDSYNTH_API void *fluid_sfont_get_data(fluid_sfont_t *sfont);

FLUIDSYNTH_API int fluid_sfont_get_id(fluid_sfont_t *sfont);
FLUIDSYNTH_API const char *fluid_sfont_get_name(fluid_sfont_t *sfont);
FLUIDSYNTH_API fluid_preset_t *fluid_sfont_get_preset(fluid_sfont_t *sfont, int bank, int prenum);
FLUIDSYNTH_API void fluid_sfont_iteration_start(fluid_sfont_t *sfont);
FLUIDSYNTH_API fluid_preset_t *fluid_sfont_iteration_next(fluid_sfont_t *sfont);

/**
 * Method to get a virtual SoundFont preset name.
 *
 * @param preset Virtual SoundFont preset
 * @return Should return the name of the preset.  The returned string must be
 *   valid for the duration of the virtual preset (or the duration of the
 *   SoundFont, in the case of preset iteration).
 */
typedef const char *(*fluid_preset_get_name_t)(fluid_preset_t *preset);

/**
 * Method to get a virtual SoundFont preset MIDI bank number.
 *
 * @param preset Virtual SoundFont preset
 * @param return The bank number of the preset
 */
typedef int (*fluid_preset_get_banknum_t)(fluid_preset_t *preset);

/**
 * Method to get a virtual SoundFont preset MIDI program number.
 *
 * @param preset Virtual SoundFont preset
 * @param return The program number of the preset
 */
typedef int (*fluid_preset_get_num_t)(fluid_preset_t *preset);

/**
 * Method to handle a noteon event (synthesize the instrument).
 *
 * @param preset Virtual SoundFont preset
 * @param synth Synthesizer instance
 * @param chan MIDI channel number of the note on event
 * @param key MIDI note number (0-127)
 * @param vel MIDI velocity (0-127)
 * @return #FLUID_OK on success (0) or #FLUID_FAILED (-1) otherwise
 *
 * This method may be called from within synthesis context and therefore
 * should be as efficient as possible and not perform any operations considered
 * bad for realtime audio output (memory allocations and other OS calls).
 *
 * Call fluid_synth_alloc_voice() for every sample that has
 * to be played. fluid_synth_alloc_voice() expects a pointer to a
 * #fluid_sample_t structure and returns a pointer to the opaque
 * #fluid_voice_t structure. To set or increment the values of a
 * generator, use fluid_voice_gen_set() or fluid_voice_gen_incr(). When you are
 * finished initializing the voice call fluid_voice_start() to
 * start playing the synthesis voice.  Starting with FluidSynth 1.1.0 all voices
 * created will be started at the same time.
 */
typedef int (*fluid_preset_noteon_t)(fluid_preset_t *preset, fluid_synth_t *synth, int chan, int key, int vel);

/**
 * Method to free a virtual SoundFont preset.
 *
 * @param preset Virtual SoundFont preset
 * @return Should return 0
 *
 * Any custom user provided cleanup function must ultimately call
 * delete_fluid_preset() to ensure proper cleanup of the #fluid_preset_t struct. If no private data
 * needs to be freed, setting this to delete_fluid_preset() is sufficient.
 */
typedef void (*fluid_preset_free_t)(fluid_preset_t *preset);

/** @startlifecycle{Preset} */
FLUIDSYNTH_API fluid_preset_t *new_fluid_preset(fluid_sfont_t *parent_sfont,
        fluid_preset_get_name_t get_name,
        fluid_preset_get_banknum_t get_bank,
        fluid_preset_get_num_t get_num,
        fluid_preset_noteon_t noteon,
        fluid_preset_free_t free);
FLUIDSYNTH_API void delete_fluid_preset(fluid_preset_t *preset);
/** @endlifecycle */

FLUIDSYNTH_API int fluid_preset_set_data(fluid_preset_t *preset, void *data);
FLUIDSYNTH_API void *fluid_preset_get_data(fluid_preset_t *preset);

FLUIDSYNTH_API const char *fluid_preset_get_name(fluid_preset_t *preset);
FLUIDSYNTH_API int fluid_preset_get_banknum(fluid_preset_t *preset);
FLUIDSYNTH_API int fluid_preset_get_num(fluid_preset_t *preset);
FLUIDSYNTH_API fluid_sfont_t *fluid_preset_get_sfont(fluid_preset_t *preset);

/** @startlifecycle{Sample} */
FLUIDSYNTH_API fluid_sample_t *new_fluid_sample(void);
FLUIDSYNTH_API void delete_fluid_sample(fluid_sample_t *sample);
/** @endlifecycle */

FLUIDSYNTH_API size_t fluid_sample_sizeof(void);

FLUIDSYNTH_API int fluid_sample_set_name(fluid_sample_t *sample, const char *name);
FLUIDSYNTH_API int fluid_sample_set_sound_data(fluid_sample_t *sample,
        short *data,
        char *data24,
        unsigned int nbframes,
        unsigned int sample_rate,
        short copy_data);

FLUIDSYNTH_API int fluid_sample_set_loop(fluid_sample_t *sample, unsigned int loop_start, unsigned int loop_end);
FLUIDSYNTH_API int fluid_sample_set_pitch(fluid_sample_t *sample, int root_key, int fine_tune);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _FLUIDSYNTH_SFONT_H */
