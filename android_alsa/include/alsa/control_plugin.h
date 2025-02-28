/**
 * \file include/control_plugin.h
 * \brief Common control plugin code
 * \author Jaroslav Kysela <perex@perex.cz>
 * \date 2021
 *
 * Application interface library for the ALSA driver.
 * See the \ref control_plugins page for more details.
 *
 * \warning Using of contents of this header file might be dangerous
 *	    in the sense of compatibility reasons. The contents might be
 *	    freely changed in future.
 */
/*
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __ALSA_CONTROL_PLUGIN_H
#define __ALSA_CONTROL_PLUGIN_H

/**
 * \defgroup Control_Plugins Primitive Control Plugins
 * \ingroup Control
 * See the \ref control_plugins page for more details.
 * \{
 */

/*
 * Control HW
 */
int snd_ctl_hw_open(snd_ctl_t **handle, const char *name, int card, int mode);
int _snd_ctl_hw_open(snd_ctl_t **handlep, char *name, snd_config_t *root, snd_config_t *conf, int mode);

/*
 * Control Remap & Map
 */
int snd_ctl_remap_open(snd_ctl_t **handlep, const char *name, snd_config_t *remap,
		       snd_config_t *map, snd_ctl_t *child, int mode);
int _snd_ctl_remap_open(snd_ctl_t **handlep, char *name, snd_config_t *root, snd_config_t *conf, int mode);

/** \} */

#endif /* __ALSA_CONTROL_PLUGIN_H */
