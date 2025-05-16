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

#ifndef _FLUIDSYNTH_SEQBIND_H
#define _FLUIDSYNTH_SEQBIND_H

#include "seq.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup sequencer
 *
 * @{
 */
FLUIDSYNTH_API
fluid_seq_id_t fluid_sequencer_register_fluidsynth(fluid_sequencer_t *seq, fluid_synth_t *synth);
FLUIDSYNTH_API 
int fluid_sequencer_add_midi_event_to_buffer(void *data, fluid_midi_event_t *event);
/** @} */

#ifdef __cplusplus
}
#endif
#endif /* _FLUIDSYNTH_SEQBIND_H */

