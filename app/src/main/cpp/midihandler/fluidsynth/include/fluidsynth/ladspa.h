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

#ifndef _FLUIDSYNTH_LADSPA_H
#define _FLUIDSYNTH_LADSPA_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup ladspa Effect - LADSPA
 * @ingroup synth
 *
 * Functions for configuring the LADSPA effects unit
 *
 * This header defines useful functions for programmatically manipulating the ladspa
 * effects unit of the synth that can be retrieved via fluid_synth_get_ladspa_fx().
 *
 * Using any of those functions requires fluidsynth to be compiled with LADSPA support.
 * Else all of those functions are useless dummies.
 *
 * @{
 */
FLUIDSYNTH_API int fluid_ladspa_is_active(fluid_ladspa_fx_t *fx);
FLUIDSYNTH_API int fluid_ladspa_activate(fluid_ladspa_fx_t *fx);
FLUIDSYNTH_API int fluid_ladspa_deactivate(fluid_ladspa_fx_t *fx);
FLUIDSYNTH_API int fluid_ladspa_reset(fluid_ladspa_fx_t *fx);
FLUIDSYNTH_API int fluid_ladspa_check(fluid_ladspa_fx_t *fx, char *err, int err_size);

FLUIDSYNTH_API int fluid_ladspa_host_port_exists(fluid_ladspa_fx_t *fx, const char *name);

FLUIDSYNTH_API int fluid_ladspa_add_buffer(fluid_ladspa_fx_t *fx, const char *name);
FLUIDSYNTH_API int fluid_ladspa_buffer_exists(fluid_ladspa_fx_t *fx, const char *name);

FLUIDSYNTH_API int fluid_ladspa_add_effect(fluid_ladspa_fx_t *fx, const char *effect_name,
        const char *lib_name, const char *plugin_name);
FLUIDSYNTH_API int fluid_ladspa_effect_can_mix(fluid_ladspa_fx_t *fx, const char *name);
FLUIDSYNTH_API int fluid_ladspa_effect_set_mix(fluid_ladspa_fx_t *fx, const char *name, int mix, float gain);
FLUIDSYNTH_API int fluid_ladspa_effect_port_exists(fluid_ladspa_fx_t *fx, const char *effect_name, const char *port_name);
FLUIDSYNTH_API int fluid_ladspa_effect_set_control(fluid_ladspa_fx_t *fx, const char *effect_name,
        const char *port_name, float val);
FLUIDSYNTH_API int fluid_ladspa_effect_link(fluid_ladspa_fx_t *fx, const char *effect_name,
        const char *port_name, const char *name);
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _FLUIDSYNTH_LADSPA_H */

