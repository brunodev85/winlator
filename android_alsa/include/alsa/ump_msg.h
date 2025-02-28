/* SPDX-License-Identifier: LGPL-2.1+ */
/**
 * \file include/ump_msg.h
 * \brief API library for ALSA rawmidi/UMP interface
 *
 * API library for ALSA rawmidi/UMP interface
 */

#ifndef __ALSA_UMP_MSG_H
#define __ALSA_UMP_MSG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** general UMP packet header in 32bit word */
typedef struct _snd_ump_msg_hdr {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t channel:4;	/**< Channel */
	uint8_t byte1;		/**< First data byte */
	uint8_t byte2;		/**< Second data byte */
#else
	uint8_t byte2;		/**< Second data byte */
	uint8_t byte1;		/**< First data byte */
	uint8_t channel:4;	/**< Channel */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */
#endif
} __attribute((packed)) snd_ump_msg_hdr_t;

/** MIDI 1.0 Note Off / Note On (32bit) */
typedef struct _snd_ump_msg_midi1_note {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t channel:4;	/**< Channel */
	uint8_t note;		/**< Note (7bit) */
	uint8_t velocity;	/**< Velocity (7bit) */
#else
	uint8_t velocity;	/**< Velocity (7bit) */
	uint8_t note;		/**< Note (7bit) */
	uint8_t channel:4;	/**< Channel */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */
#endif
} __attribute((packed)) snd_ump_msg_midi1_note_t;

/** MIDI 1.0 Poly Pressure (32bit) */
typedef struct _snd_ump_msg_midi1_paf {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t channel:4;	/**< Channel */
	uint8_t note;		/**< Note (7bit) */
	uint8_t data;		/**< Pressure (7bit) */
#else
	uint8_t data;		/**< Pressure (7bit) */
	uint8_t note;		/**< Note (7bit) */
	uint8_t channel:4;	/**< Channel */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */
#endif
} __attribute((packed)) snd_ump_msg_midi1_paf_t;

/** MIDI 1.0 Control Change (32bit) */
typedef struct _snd_ump_msg_midi1_cc {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t channel:4;	/**< Channel */
	uint8_t index;		/**< Control index (7bit) */
	uint8_t data;		/**< Control data (7bit) */
#else
	uint8_t data;		/**< Control data (7bit) */
	uint8_t index;		/**< Control index (7bit) */
	uint8_t channel:4;	/**< Channel */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */
#endif
} __attribute((packed)) snd_ump_msg_midi1_cc_t;

/** MIDI 1.0 Program Change (32bit) */
typedef struct _snd_ump_msg_midi1_program {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t channel:4;	/**< Channel */
	uint8_t program;	/**< Program number (7bit) */
	uint8_t reserved;	/**< Unused */
#else
	uint8_t reserved;	/**< Unused */
	uint8_t program;	/**< Program number (7bit) */
	uint8_t channel:4;	/**< Channel */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */
#endif
} __attribute((packed)) snd_ump_msg_midi1_program_t;

/** MIDI 1.0 Channel Pressure (32bit) */
typedef struct _snd_ump_msg_midi1_caf {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t channel:4;	/**< Channel */
	uint8_t data;		/**< Pressure (7bit) */
	uint8_t reserved;	/**< Unused */
#else
	uint8_t reserved;	/**< Unused */
	uint8_t data;		/**< Pressure (7bit) */
	uint8_t channel:4;	/**< Channel */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */
#endif
} __attribute((packed)) snd_ump_msg_midi1_caf_t;

/** MIDI 1.0 Pitch Bend (32bit) */
typedef struct _snd_ump_msg_midi1_pitchbend {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t channel:4;	/**< Channel */
	uint8_t data_lsb;	/**< LSB of pitchbend (7bit) */
	uint8_t data_msb;	/**< MSB of pitchbend (7bit) */
#else
	uint8_t data_msb;	/**< MSB of pitchbend (7bit) */
	uint8_t data_lsb;	/**< LSB of pitchbend (7bit) */
	uint8_t channel:4;	/**< Channel */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */
#endif
} __attribute((packed)) snd_ump_msg_midi1_pitchbend_t;

/** System Common and Real Time messages (32bit); no channel field */
typedef struct snd_ump_msg_system {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status;		/**< Status */
	uint8_t parm1;		/**< First parameter */
	uint8_t parm2;		/**< Second parameter */
#else
	uint8_t parm2;		/**< Second parameter */
	uint8_t parm1;		/**< First parameter */
	uint8_t status;		/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */
#endif
} __attribute((packed)) snd_ump_msg_system_t;

/** MIDI 1.0 UMP CVM (32bit) */
typedef union _snd_ump_msg_midi1 {
	snd_ump_msg_midi1_note_t	note_on;	/**< MIDI1 note-on message */
	snd_ump_msg_midi1_note_t	note_off;	/**< MIDI1 note-off message */
	snd_ump_msg_midi1_paf_t		poly_pressure;	/**< MIDI1 poly-pressure message */
	snd_ump_msg_midi1_cc_t		control_change;	/**< MIDI1 control-change message */
	snd_ump_msg_midi1_program_t	program_change;	/**< MIDI1 program-change message */
	snd_ump_msg_midi1_caf_t		channel_pressure; /**< MIDI1 channel-pressure message */
	snd_ump_msg_midi1_pitchbend_t	pitchbend;	/**< MIDI1 pitch-bend message */
	snd_ump_msg_system_t		system;		/**< system message */
	snd_ump_msg_hdr_t		hdr;		/**< UMP header */
	uint32_t			raw;		/**< raw UMP packet */
} snd_ump_msg_midi1_t;

/** MIDI 2.0 Note-on/off attribute type */
enum {
	SND_UMP_MIDI2_NOTE_ATTR_NO_DATA		= 0x00,	/**< No attribute data */
	SND_UMP_MIDI2_NOTE_ATTR_MANUFACTURER	= 0x01,	/**< Manufacturer specific */
	SND_UMP_MIDI2_NOTE_ATTR_PROFILE		= 0x02,	/**< Profile specific */
	SND_UMP_MIDI2_NOTE_ATTR_PITCH79		= 0x03,	/**< Pitch 7.9 */
};

/** MIDI 2.0 Note Off / Note On (64bit) */
typedef struct _snd_ump_msg_midi2_note {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t channel:4;	/**< Channel */
	uint8_t note;		/**< Note (7bit) */
	uint8_t attr_type;	/**< Attribute type */

	uint16_t velocity;	/**< Velocity (16bit) */
	uint16_t attr_data;	/**< Attribute data (16bit) */
#else
	uint8_t attr_type;	/**< Attribute type */
	uint8_t note;		/**< Note (7bit) */
	uint8_t channel:4;	/**< Channel */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */

	uint16_t attr_data;	/**< Attribute data (16bit) */
	uint16_t velocity;	/**< Velocity (16bit) */
#endif
} __attribute((packed)) snd_ump_msg_midi2_note_t;

/** MIDI 2.0 Poly Pressure (64bit) */
typedef struct _snd_ump_msg_midi2_paf {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t channel:4;	/**< Channel */
	uint8_t note;		/**< Note (7bit) */
	uint8_t reserved;	/**< Unused */

	uint32_t data;		/**< Pressure (32bit) */
#else
	uint8_t reserved;	/**< Unused */
	uint8_t note;		/**< Note (7bit) */
	uint8_t channel:4;	/**< Channel */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */

	uint32_t data;		/**< Pressure (32bit) */
#endif
} __attribute((packed)) snd_ump_msg_midi2_paf_t;

/** MIDI 2.0 Per-Note Controller (64bit) */
typedef struct _snd_ump_msg_midi2_per_note_cc {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t channel:4;	/**< Channel */
	uint8_t note;		/**< Note (7bit) */
	uint8_t index;		/**< Control index (8bit) */

	uint32_t data;		/**< Data (32bit) */
#else
	uint8_t index;		/**< Control index (8bit) */
	uint8_t note;		/**< Note (7bit) */
	uint8_t channel:4;	/**< Channel */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */

	uint32_t data;		/**< Data (32bit) */
#endif
} __attribute((packed)) snd_ump_msg_midi2_per_note_cc_t;

/** MIDI 2.0 per-note management flag bits */
enum {
	SND_UMP_MIDI2_PNMGMT_RESET_CONTROLLERS	= 0x01,	/**< Reset (set) per-note controllers */
	SND_UMP_MIDI2_PNMGMT_DETACH_CONTROLLERS	= 0x02,	/**< Detach per-note controllers */
};

/** MIDI 2.0 Per-Note Management (64bit) */
typedef struct _snd_ump_msg_midi2_per_note_mgmt {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t channel:4;	/**< Channel */
	uint8_t note;		/**< Note (7bit) */
	uint8_t flags;		/**< Option flags (8bit) */

	uint32_t reserved;	/**< Unused */
#else
	uint8_t flags;		/**< Option flags (8bit) */
	uint8_t note;		/**< Note (7bit) */
	uint8_t channel:4;	/**< Channel */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */

	uint32_t reserved;	/**< Unused */
#endif
} __attribute((packed)) snd_ump_msg_midi2_per_note_mgmt_t;

/** MIDI 2.0 Control Change (64bit) */
typedef struct _snd_ump_msg_midi2_cc {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t channel:4;	/**< Channel */
	uint8_t index;		/**< Control index (7bit) */
	uint8_t reserved;	/**< Unused */

	uint32_t data;		/**< Control data (32bit) */
#else
	uint8_t reserved;	/**< Unused */
	uint8_t index;		/**< Control index (7bit) */
	uint8_t channel:4;	/**< Channel */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */

	uint32_t data;		/**< Control data (32bit) */
#endif
} __attribute((packed)) snd_ump_msg_midi2_cc_t;

/** MIDI 2.0 Registered Controller (RPN) / Assignable Controller (NRPN) (64bit) */
typedef struct _snd_ump_msg_midi2_rpn {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t channel:4;	/**< Channel */
	uint8_t bank;		/**< Bank number (7bit) */
	uint8_t index;		/**< Control index (7bit) */

	uint32_t data;		/**< Data (32bit) */
#else
	uint8_t index;		/**< Control index (7bit) */
	uint8_t bank;		/**< Bank number (7bit) */
	uint8_t channel:4;	/**< Channel */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */

	uint32_t data;		/**< Data (32bit) */
#endif
} __attribute((packed)) snd_ump_msg_midi2_rpn_t;

/** MIDI 2.0 Program Change (64bit) */
typedef struct _snd_ump_msg_midi2_program {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t channel:4;	/**< Channel */
	uint16_t reserved:15;	/**< Unused */
	uint16_t bank_valid:1;	/**< Option flag: bank valid */

	uint8_t program;	/**< Program number (7bit) */
	uint8_t reserved2;	/**< Unused */
	uint8_t bank_msb;	/**< MSB of bank (8bit) */
	uint8_t bank_lsb;	/**< LSB of bank (7bit) */
#else
	uint16_t bank_valid:1;	/**< Option flag: bank valid */
	uint16_t reserved:15;	/**< Unused */
	uint8_t channel:4;	/**< Channel */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */

	uint8_t bank_lsb;	/**< LSB of bank (7bit) */
	uint8_t bank_msb;	/**< MSB of bank (8bit) */
	uint8_t reserved2;	/**< Unused */
	uint8_t program;	/**< Program number (7bit) */
#endif
} __attribute((packed)) snd_ump_msg_midi2_program_t;

/** MIDI 2.0 Channel Pressure (64bit) */
typedef struct _snd_ump_msg_midi2_caf {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t channel:4;	/**< Channel */
	uint16_t reserved;	/**< Unused */

	uint32_t data;		/**< Data (32bit) */
#else
	uint16_t reserved;	/**< Unused */
	uint8_t channel:4;	/**< Channel */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */

	uint32_t data;		/**< Data (32bit) */
#endif
} __attribute((packed)) snd_ump_msg_midi2_caf_t;

/** MIDI 2.0 Pitch Bend (64bit) */
typedef struct _snd_ump_msg_midi2_pitchbend {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t channel:4;	/**< Channel */
	uint16_t reserved;	/**< Unused */

	uint32_t data;		/**< Data (32bit) */
#else
	uint16_t reserved;	/**< Unused */
	uint8_t channel:4;	/**< Channel */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */

	uint32_t data;		/**< Data (32bit) */
#endif
} __attribute((packed)) snd_ump_msg_midi2_pitchbend_t;

/** MIDI 2.0 Per-Note Pitch Bend (64bit) */
typedef struct _snd_ump_msg_midi2_per_note_pitchbend {
#ifdef __BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t channel:4;	/**< Channel */
	uint8_t note;		/**< Note (7bit) */
	uint8_t reserved;	/**< Unused */

	uint32_t data;		/**< Data (32bit) */
#else
	uint8_t reserved;	/**< Unused */
	uint8_t note;		/**< Note (7bit) */
	uint8_t channel:4;	/**< Channel */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */

	uint32_t data;		/**< Data (32bit) */
#endif
} __attribute((packed)) snd_ump_msg_midi2_per_note_pitchbend_t;

/** MIDI2 UMP packet (64bit) */
typedef union _snd_ump_msg_midi2 {
	snd_ump_msg_midi2_note_t	note_on;	/**< MIDI2 note-on message */
	snd_ump_msg_midi2_note_t	note_off;	/**< MIDI2 note-off message */
	snd_ump_msg_midi2_paf_t		poly_pressure;	/**< MIDI2 poly-pressure message */
	snd_ump_msg_midi2_per_note_cc_t	per_note_acc;	/**< MIDI2 per-note ACC message */
	snd_ump_msg_midi2_per_note_cc_t	per_note_rcc;	/**< MIDI2 per-note RCC message */
	snd_ump_msg_midi2_per_note_mgmt_t per_note_mgmt; /**< MIDI2 per-note management message */
	snd_ump_msg_midi2_cc_t		control_change;	/**< MIDI2 control-change message */
	snd_ump_msg_midi2_rpn_t		rpn;		/**< MIDI2 RPN message */
	snd_ump_msg_midi2_rpn_t		nrpn;		/**< MIDI2 NRPN message */
	snd_ump_msg_midi2_rpn_t		relative_rpn;	/**< MIDI2 relative-RPN message */
	snd_ump_msg_midi2_rpn_t		relative_nrpn;	/**< MIDI2 relative-NRPN message */
	snd_ump_msg_midi2_program_t	program_change;	/**< MIDI2 program-change message */
	snd_ump_msg_midi2_caf_t		channel_pressure; /**< MIDI2 channel-pressure message */
	snd_ump_msg_midi2_pitchbend_t	pitchbend;	/**< MIDI2 pitch-bend message */
	snd_ump_msg_midi2_per_note_pitchbend_t per_note_pitchbend; /**< MIDI2 per-note pitch-bend message */
	snd_ump_msg_hdr_t		hdr;		/**< UMP header */
	uint32_t			raw[2];		/**< raw UMP packet */
} snd_ump_msg_midi2_t;

/** Stream Message (generic) (128bit) */
typedef struct _snd_ump_msg_stream_gen {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint16_t type:4;	/**< UMP packet type */
	uint16_t format:2;	/**< Format */
	uint16_t status:10;	/**< Status */
	uint16_t data1;		/**< Data */
	uint32_t data2;		/**< Data */
	uint32_t data3;		/**< Data */
	uint32_t data4;		/**< Data */
#else
	uint16_t data1;		/**< Data */
	uint16_t status:10;	/**< Status */
	uint16_t format:2;	/**< Format */
	uint16_t type:4;	/**< UMP packet type */
	uint32_t data2;		/**< Data */
	uint32_t data3;		/**< Data */
	uint32_t data4;		/**< Data */
#endif
} __attribute((packed)) snd_ump_msg_stream_gen_t;

/** Stream Message (128bit) */
typedef union _snd_ump_msg_stream {
	snd_ump_msg_stream_gen_t	gen;	/**< Generic Stream message */
	snd_ump_msg_hdr_t		hdr;		/**< UMP header */
	uint32_t			raw[4];		/**< raw UMP packet */
} snd_ump_msg_stream_t;

/** Metadata / Text Message (128bit) */
typedef struct _snd_ump_msg_flex_data_meta {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t format:2;	/**< Format */
	uint8_t addrs:2;	/**< Address */
	uint8_t channel:4;	/**< Channel */
	uint8_t status_bank;	/**< Status Bank */
	uint8_t status;		/**< Status */
	uint32_t data[3];	/**< Data (96 bits) */
#else
	uint8_t status;		/**< Status */
	uint8_t status_bank;	/**< Status Bank */
	uint8_t channel:4;	/**< Channel */
	uint8_t addrs:2;	/**< Address */
	uint8_t format:2;	/**< Format */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */
	uint32_t data[3];	/**< Data (96 bits) */
#endif
} __attribute((packed)) snd_ump_msg_flex_data_meta_t;

/** Set Tempo Message (128bit) */
typedef struct _snd_ump_msg_set_tempo {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t format:2;	/**< Format */
	uint8_t addrs:2;	/**< Address */
	uint8_t channel:4;	/**< Channel */
	uint8_t status_bank;	/**< Status Bank */
	uint8_t status;		/**< Status */

	uint32_t tempo;		/**< Number of 10nsec units per quarter note */

	uint32_t reserved[2];	/**< Unused */
#else
	uint8_t status;		/**< Status */
	uint8_t status_bank;	/**< Status Bank */
	uint8_t channel:4;	/**< Channel */
	uint8_t addrs:2;	/**< Address */
	uint8_t format:2;	/**< Format */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */

	uint32_t tempo;		/**< Number of 10nsec units per quarter note */

	uint32_t reserved[2];	/**< Unused */
#endif
} __attribute((packed)) snd_ump_msg_set_tempo_t;

/** Set Time Signature Message (128bit) */
typedef struct _snd_ump_msg_set_time_sig {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t format:2;	/**< Format */
	uint8_t addrs:2;	/**< Address */
	uint8_t channel:4;	/**< Channel */
	uint8_t status_bank;	/**< Status Bank */
	uint8_t status;		/**< Status */

	uint8_t numerator;	/**< Numerator */
	uint8_t denominator;	/**< Denominator */
	uint8_t num_notes;	/**< Number of 1/32 notes */
	uint8_t reserved1;	/**< Unused */

	uint32_t reserved[2];	/**< Unused */
#else
	uint8_t status;		/**< Status */
	uint8_t status_bank;	/**< Status Bank */
	uint8_t channel:4;	/**< Channel */
	uint8_t addrs:2;	/**< Address */
	uint8_t format:2;	/**< Format */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */

	uint8_t reserved1;	/**< Unused */
	uint8_t num_notes;	/**< Number of 1/32 notes */
	uint8_t denominator;	/**< Denominator */
	uint8_t numerator;	/**< Numerator */

	uint32_t reserved[2];	/**< Unused */
#endif
} __attribute((packed)) snd_ump_msg_set_time_sig_t;

/** Set Metronome Message (128bit) */
typedef struct _snd_ump_msg_set_metronome {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t format:2;	/**< Format */
	uint8_t addrs:2;	/**< Address */
	uint8_t channel:4;	/**< Channel */
	uint8_t status_bank;	/**< Status Bank */
	uint8_t status;		/**< Status */

	uint8_t clocks_primary;	/**< Num clocks per primary clock */
	uint8_t bar_accent_1;	/**< Bar accent part 1 */
	uint8_t bar_accent_2;	/**< Bar accent part 2 */
	uint8_t bar_accent_3;	/**< Bar accent part 3 */

	uint8_t subdivision_1;	/**< Num subdivision clicks 1 */
	uint8_t subdivision_2;	/**< Num subdivision clicks 1 */
	uint16_t reserved1;	/**< Unused */

	uint32_t reserved2;	/**< Unused */
#else
	uint8_t status;		/**< Status */
	uint8_t status_bank;	/**< Status Bank */
	uint8_t channel:4;	/**< Channel */
	uint8_t addrs:2;	/**< Address */
	uint8_t format:2;	/**< Format */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */

	uint8_t bar_accent_3;	/**< Bar accent part 3 */
	uint8_t bar_accent_2;	/**< Bar accent part 2 */
	uint8_t bar_accent_1;	/**< Bar accent part 1 */
	uint8_t clocks_primary;	/**< Num clocks per primary clock */

	uint16_t reserved1;	/**< Unused */
	uint8_t subdivision_2;	/**< Num subdivision clicks 1 */
	uint8_t subdivision_1;	/**< Num subdivision clicks 1 */

	uint32_t reserved2;	/**< Unused */
#endif
} __attribute((packed)) snd_ump_msg_set_metronome_t;

/** Set Key Signature Message (128bit) */
typedef struct _snd_ump_msg_set_key_sig {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t format:2;	/**< Format */
	uint8_t addrs:2;	/**< Address */
	uint8_t channel:4;	/**< Channel */
	uint8_t status_bank;	/**< Status Bank */
	uint8_t status;		/**< Status */

	uint8_t sharps_flats:4;	/**< Sharps/Flats */
	uint8_t tonic_note:4;	/**< Tonic Note 1 */
	uint8_t reserved1[3];	/**< Unused */

	uint32_t reserved2[2];	/**< Unused */
#else
	uint8_t status;		/**< Status */
	uint8_t status_bank;	/**< Status Bank */
	uint8_t channel:4;	/**< Channel */
	uint8_t addrs:2;	/**< Address */
	uint8_t format:2;	/**< Format */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */

	uint8_t reserved1[3];	/**< Unused */
	uint8_t tonic_note:4;	/**< Tonic Note */
	uint8_t sharps_flats:4;	/**< Sharps/Flats */

	uint32_t reserved2[2];	/**< Unused */
#endif
} __attribute((packed)) snd_ump_msg_set_key_sig_t;

/** Set Chord Name Message (128bit) */
typedef struct _snd_ump_msg_set_chord_name {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t format:2;	/**< Format */
	uint8_t addrs:2;	/**< Address */
	uint8_t channel:4;	/**< Channel */
	uint8_t status_bank;	/**< Status Bank */
	uint8_t status;		/**< Status */

	uint8_t tonic_sharp:4;	/**< Tonic Sharps/Flats */
	uint8_t chord_tonic:4;	/**< Chord Tonic Note */
	uint8_t chord_type;	/**< Chord Type */
	uint8_t alter1_type:4;	/**< Alteration 1 Type */
	uint8_t alter1_degree:4; /**< Alteration 1 Degree */
	uint8_t alter2_type:4;	/**< Alteration 2 Type */
	uint8_t alter2_degree:4; /**< Alteration 2 Degree */

	uint8_t alter3_type:4;	/**< Alteration 3 Type */
	uint8_t alter3_degree:4; /**< Alteration 3 Degree */
	uint8_t alter4_type:4;	/**< Alteration 4 Type */
	uint8_t alter4_degree:4; /**< Alteration 4 Degree */
	uint16_t reserved;	/**< Unused */

	uint8_t bass_sharp:4;	/**< Bass Sharps/Flats */
	uint8_t bass_note:4;	/**< Bass Note */
	uint8_t bass_type;	/**< Bass Chord Type */
	uint8_t bass_alter1_type:4; /**< Bass Alteration 1 Type */
	uint8_t bass_alter1_degree:4; /**< Bass Alteration 1 Degree */
	uint8_t bass_alter2_type:4; /**< Bass Alteration 2 Type */
	uint8_t bass_alter2_degree:4; /**< Bass Alteration 2 Degree */
#else
	uint8_t status;		/**< Status */
	uint8_t status_bank;	/**< Status Bank */
	uint8_t channel:4;	/**< Channel */
	uint8_t addrs:2;	/**< Address */
	uint8_t format:2;	/**< Format */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */

	uint8_t alter2_degree:4; /**< Alteration 2 Degree */
	uint8_t alter2_type:4;	/**< Alteration 2 Type */
	uint8_t alter1_degree:4; /**< Alteration 1 Degree */
	uint8_t alter1_type:4;	/**< Alteration 1 Type */
	uint8_t chord_type;	/**< Chord Type */
	uint8_t chord_tonic:4;	/**< Chord Tonic Note */
	uint8_t tonic_sharp:4;	/**< Tonic Sharps/Flats */

	uint16_t reserved;	/**< Unused */
	uint8_t alter4_degree:4; /**< Alteration 4 Degree */
	uint8_t alter4_type:4;	/**< Alteration 4 Type */
	uint8_t alter3_degree:4; /**< Alteration 3 Degree */
	uint8_t alter3_type:4;	/**< Alteration 3 Type */

	uint8_t bass_alter2_degree:4; /**< Bass Alteration 2 Degree */
	uint8_t bass_alter2_type:4; /**< Bass Alteration 2 Type */
	uint8_t bass_alter1_degree:4; /**< Bass Alteration 1 Degree */
	uint8_t bass_alter1_type:4; /**< Bass Alteration 1 Type */
	uint8_t bass_type;	/**< Bass Chord Type */
	uint8_t bass_note:4;	/**< Bass Note */
	uint8_t bass_sharp:4;	/**< Bass Sharps/Flats */
#endif
} __attribute((packed)) snd_ump_msg_set_chord_name_t;

/** Flex Data Message (128bit) */
typedef union _snd_ump_msg_flex_data {
	snd_ump_msg_flex_data_meta_t	meta;		/**< Metadata */
	snd_ump_msg_flex_data_meta_t	text;		/**< Text data */
	snd_ump_msg_set_tempo_t		set_tempo;	/**< Set Tempo */
	snd_ump_msg_set_time_sig_t	set_time_sig;	/**< Set Time Signature */
	snd_ump_msg_set_metronome_t	set_metronome;	/**< Set Metronome */
	snd_ump_msg_set_key_sig_t	set_key_sig;	/**< Set Key Signature */
	snd_ump_msg_set_chord_name_t	set_chord_name;	/**< Set Chord Name */
	snd_ump_msg_hdr_t		hdr;		/**< UMP header */
	uint32_t			raw[4];		/**< raw UMP packet */
} snd_ump_msg_flex_data_t;

/** Mixed Data Set Chunk Header Message (128bit) */
typedef struct _snd_ump_msg_mixed_data_header {
#ifdef __BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t mds_id:4;	/**< Mixed Data Set ID */
	uint16_t bytes;		/**< Number of valid bytes in this chunk */

	uint16_t chunks;	/**< Number of chunks in mixed data set */
	uint16_t chunk;		/**< Number of this chunk */

	uint16_t manufacturer;	/**< Manufacturer ID */
	uint16_t device;	/**< Device ID */

	uint16_t sub_id_1;	/**< Sub ID \# 1 */
	uint16_t sub_id_2;	/**< Sub ID \# 2 */
#else
	uint16_t bytes;		/**< Number of valid bytes in this chunk */
	uint8_t mds_id:4;	/**< Mixed Data Set ID */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */

	uint16_t chunk;		/**< Number of this chunk */
	uint16_t chunks;	/**< Number of chunks in mixed data set */

	uint16_t device;	/**< Device ID */
	uint16_t manufacturer;	/**< Manufacturer ID */

	uint16_t sub_id_2;	/**< Sub ID \# 2 */
	uint16_t sub_id_1;	/**< Sub ID \# 1 */
#endif
} snd_ump_msg_mixed_data_header_t;

/** Mixed Data Set Chunk Payload Message (128bit) */
typedef struct _snd_ump_msg_mixed_data_payload {
#ifdef __BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t mds_id:4;	/**< Mixed Data Set ID */
	uint16_t payload1;	/**< Payload */

	uint32_t payloads[3];	/**< Payload */
#else
	uint16_t payload1;	/**< Payload */
	uint8_t mds_id:4;	/**< Mixed Data Set ID */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */

	uint32_t payloads[3];	/**< Payload */
#endif
} snd_ump_msg_mixed_data_payload_t;

/** Mixed Data Set Chunk Message (128bit) */
typedef union _snd_ump_msg_mixed_data {
	snd_ump_msg_mixed_data_header_t	header;		/**< Header */
	snd_ump_msg_mixed_data_payload_t payload;	/**< Payload */
	uint32_t			raw[4];		/**< raw UMP packet */
} snd_ump_msg_mixed_data_t;

/** Jitter Reduction Clock / Timestamp Message (32bit) */
typedef struct _snd_ump_msg_jr_clock {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t reserved:4;	/**< Unused */
	uint16_t time;		/**< clock time / timestamp */
#else
	uint16_t time;		/**< clock time / timestamp */
	uint8_t reserved:4;	/**< Unused */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */
#endif
} __attribute((packed)) snd_ump_msg_jr_clock_t;

/** Delta Clockstamp Ticks Per Quarter Note (DCTPQ) (32bit) */
typedef struct _snd_ump_msg_dctpq {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint8_t type:4;		/**< UMP packet type */
	uint8_t group:4;	/**< UMP Group */
	uint8_t status:4;	/**< Status */
	uint8_t reserved:4;	/**< Unused */
	uint16_t ticks;		/**< number of ticks per quarter note */
#else
	uint16_t ticks;		/**< number of ticks per quarter note */
	uint8_t reserved:4;	/**< Unused */
	uint8_t status:4;	/**< Status */
	uint8_t group:4;	/**< UMP Group */
	uint8_t type:4;		/**< UMP packet type */
#endif
} __attribute((packed)) snd_ump_msg_dctpq_t;

/** Delta Clockstamp (DC) (32bit) */
typedef struct _snd_ump_msg_dc {
#ifdef SNDRV_BIG_ENDIAN_BITFIELD
	uint32_t type:4;		/**< UMP packet type */
	uint32_t group:4;		/**< UMP Group */
	uint32_t status:4;		/**< Status */
	uint32_t ticks:20;		/**< number of ticks since last event */
#else
	uint32_t ticks:20;		/**< number of ticks since last event */
	uint32_t status:4;		/**< Status */
	uint32_t group:4;		/**< UMP Group */
	uint32_t type:4;		/**< UMP packet type */
#endif
} __attribute((packed)) snd_ump_msg_dc_t;

/** Utility Message (32bit) */
typedef union _snd_ump_msg_utility {
	snd_ump_msg_jr_clock_t	jr_clock;	/**< JR Clock messages */
	snd_ump_msg_dctpq_t	dctpq;		/**< DCTPQ message */
	snd_ump_msg_dc_t	dc;		/**< DC message */
	snd_ump_msg_hdr_t	hdr;		/**< UMP header */
	uint32_t		raw;		/**< raw UMP packet */
} snd_ump_msg_utility_t;

/**
 * UMP message type
 */
enum {
	SND_UMP_MSG_TYPE_UTILITY		= 0x00,	/* Utility messages */
	SND_UMP_MSG_TYPE_SYSTEM			= 0x01,	/* System messages */
	SND_UMP_MSG_TYPE_MIDI1_CHANNEL_VOICE	= 0x02,	/* MIDI 1.0 messages */
	SND_UMP_MSG_TYPE_DATA			= 0x03,	/* 7bit SysEx messages */
	SND_UMP_MSG_TYPE_MIDI2_CHANNEL_VOICE	= 0x04,	/* MIDI 2.0 messages */
	SND_UMP_MSG_TYPE_EXTENDED_DATA		= 0x05,	/* 8bit data message */
	SND_UMP_MSG_TYPE_FLEX_DATA		= 0x0d,	/* Flexible data messages */
	SND_UMP_MSG_TYPE_STREAM			= 0x0f,	/* Stream messages */
};

/**
 * UMP MIDI 1.0 / 2.0 message status code (4bit)
 */
enum {
	SND_UMP_MSG_PER_NOTE_RCC	= 0x0,
	SND_UMP_MSG_PER_NOTE_ACC	= 0x1,
	SND_UMP_MSG_RPN			= 0x2,
	SND_UMP_MSG_NRPN		= 0x3,
	SND_UMP_MSG_RELATIVE_RPN	= 0x4,
	SND_UMP_MSG_RELATIVE_NRPN	= 0x5,
	SND_UMP_MSG_PER_NOTE_PITCHBEND	= 0x6,
	SND_UMP_MSG_NOTE_OFF		= 0x8,
	SND_UMP_MSG_NOTE_ON		= 0x9,
	SND_UMP_MSG_POLY_PRESSURE	= 0xa,
	SND_UMP_MSG_CONTROL_CHANGE	= 0xb,
	SND_UMP_MSG_PROGRAM_CHANGE	= 0xc,
	SND_UMP_MSG_CHANNEL_PRESSURE	= 0xd,
	SND_UMP_MSG_PITCHBEND		= 0xe,
	SND_UMP_MSG_PER_NOTE_MGMT	= 0xf,
};

/**
 * MIDI System / Realtime message status code (8bit)
 */
enum {
	SND_UMP_MSG_REALTIME		= 0xf0, /* mask */
	SND_UMP_MSG_SYSEX_START		= 0xf0,
	SND_UMP_MSG_MIDI_TIME_CODE	= 0xf1,
	SND_UMP_MSG_SONG_POSITION	= 0xf2,
	SND_UMP_MSG_SONG_SELECT		= 0xf3,
	SND_UMP_MSG_TUNE_REQUEST	= 0xf6,
	SND_UMP_MSG_SYSEX_END		= 0xf7,
	SND_UMP_MSG_TIMING_CLOCK	= 0xf8,
	SND_UMP_MSG_START		= 0xfa,
	SND_UMP_MSG_CONTINUE		= 0xfb,
	SND_UMP_MSG_STOP		= 0xfc,
	SND_UMP_MSG_ACTIVE_SENSING	= 0xfe,
	SND_UMP_MSG_RESET		= 0xff,
};

/** MIDI 2.0 SysEx / Data Status; same values for both 7-bit and 8-bit SysEx */
enum {
	SND_UMP_SYSEX_STATUS_SINGLE	= 0,
	SND_UMP_SYSEX_STATUS_START	= 1,
	SND_UMP_SYSEX_STATUS_CONTINUE	= 2,
	SND_UMP_SYSEX_STATUS_END	= 3,
};

/** MIDI 2.0 Mixed Data Set Status */
enum {
	SND_UMP_MIXED_DATA_SET_STATUS_HEADER	= 8,
	SND_UMP_MIXED_DATA_SET_STATUS_PAYLOAD	= 9,
};

/** UMP Utility Type Status (type 0x0) **/
enum {
	SND_UMP_UTILITY_MSG_STATUS_NOOP		= 0x00,
	SND_UMP_UTILITY_MSG_STATUS_JR_CLOCK	= 0x01,
	SND_UMP_UTILITY_MSG_STATUS_JR_TSTAMP	= 0x02,
	SND_UMP_UTILITY_MSG_STATUS_DCTPQ	= 0x03,
	SND_UMP_UTILITY_MSG_STATUS_DC		= 0x04,
};

/** UMP Stream Message Status (type 0xf) */
enum {
	SND_UMP_STREAM_MSG_STATUS_EP_DISCOVERY	= 0x00,
	SND_UMP_STREAM_MSG_STATUS_EP_INFO	= 0x01,
	SND_UMP_STREAM_MSG_STATUS_DEVICE_INFO	= 0x02,
	SND_UMP_STREAM_MSG_STATUS_EP_NAME	= 0x03,
	SND_UMP_STREAM_MSG_STATUS_PRODUCT_ID	= 0x04,
	SND_UMP_STREAM_MSG_STATUS_STREAM_CFG_REQUEST = 0x05,
	SND_UMP_STREAM_MSG_STATUS_STREAM_CFG	= 0x06,
	SND_UMP_STREAM_MSG_STATUS_FB_DISCOVERY	= 0x10,
	SND_UMP_STREAM_MSG_STATUS_FB_INFO	= 0x11,
	SND_UMP_STREAM_MSG_STATUS_FB_NAME	= 0x12,
	SND_UMP_STREAM_MSG_STATUS_START_CLIP	= 0x20,
	SND_UMP_STREAM_MSG_STATUS_END_CLIP	= 0x21,
};

/** UMP Endpoint Discovery filter bitmap */
enum {
	SND_UMP_STREAM_MSG_REQUEST_EP_INFO	= (1U << 0),
	SND_UMP_STREAM_MSG_REQUEST_DEVICE_INFO	= (1U << 1),
	SND_UMP_STREAM_MSG_REQUEST_EP_NAME	= (1U << 2),
	SND_UMP_STREAM_MSG_REQUEST_PRODUCT_ID	= (1U << 3),
	SND_UMP_STREAM_MSG_REQUEST_STREAM_CFG	= (1U << 4),
};

/** UMP Function Block Discovery filter bitmap */
enum {
	SND_UMP_STREAM_MSG_REQUEST_FB_INFO	= (1U << 0),
	SND_UMP_STREAM_MSG_REQUEST_FB_NAME	= (1U << 1),
};

/** UMP Endpoint Info capability bits (used for protocol request/notify, too) */
enum {
	SND_UMP_STREAM_MSG_EP_INFO_CAP_TXJR	= (1U << 0), /* Sending JRTS */
	SND_UMP_STREAM_MSG_EP_INFO_CAP_RXJR	= (1U << 1), /* Receiving JRTS */
	SND_UMP_STREAM_MSG_EP_INFO_CAP_MIDI1	= (1U << 8), /* MIDI 1.0 */
	SND_UMP_STREAM_MSG_EP_INFO_CAP_MIDI2	= (1U << 9), /* MIDI 2.0 */
};

/** UMP Endpoint / Function Block name string format bits */
enum {
	SND_UMP_STREAM_MSG_FORMAT_SINGLE	= 0,
	SND_UMP_STREAM_MSG_FORMAT_START		= 1,
	SND_UMP_STREAM_MSG_FORMAT_CONTINUE	= 2,
	SND_UMP_STREAM_MSG_FORMAT_END		= 3,
};

/** UMP Flex Data Format bits */
enum {
	SND_UMP_FLEX_DATA_MSG_FORMAT_SINGLE	= 0,
	SND_UMP_FLEX_DATA_MSG_FORMAT_START	= 1,
	SND_UMP_FLEX_DATA_MSG_FORMAT_CONTINUE	= 2,
	SND_UMP_FLEX_DATA_MSG_FORMAT_END	= 3,
};

/** UMP Flex Data Address bits */
enum {
	SND_UMP_FLEX_DATA_MSG_ADDR_CHANNEL	= 0,
	SND_UMP_FLEX_DATA_MSG_ADDR_GROUP	= 1,
};

/** UMP Flex Data Status Bank bits */
enum {
	SND_UMP_FLEX_DATA_MSG_BANK_SETUP	= 0,
	SND_UMP_FLEX_DATA_MSG_BANK_METADATA	= 1,
	SND_UMP_FLEX_DATA_MSG_BANK_PERF_TEXT	= 2,
};

/** UMP Flex Data Status bits for Setup (Status Bank = 0) */
enum {
	SND_UMP_FLEX_DATA_MSG_STATUS_SET_TEMPO		= 0x00,
	SND_UMP_FLEX_DATA_MSG_STATUS_SET_TIME_SIGNATURE	= 0x01,
	SND_UMP_FLEX_DATA_MSG_STATUS_SET_METRONOME	= 0x02,
	SND_UMP_FLEX_DATA_MSG_STATUS_SET_KEY_SIGNATURE	= 0x05,
	SND_UMP_FLEX_DATA_MSG_STATUS_SET_CHORD_NAME	= 0x06,
};

/** UMP Flex Data Status bits for Metadata (Status Bank = 1) */
enum {
	SND_UMP_FLEX_DATA_MSG_STATUS_PROJECT_NAME	= 0x01,
	SND_UMP_FLEX_DATA_MSG_STATUS_SONG_NAME		= 0x02,
	SND_UMP_FLEX_DATA_MSG_STATUS_MIDI_CLIP_NAME	= 0x03,
	SND_UMP_FLEX_DATA_MSG_STATUS_COPYRIGHT_NOTICE	= 0x04,
	SND_UMP_FLEX_DATA_MSG_STATUS_COMPOSER_NAME	= 0x05,
	SND_UMP_FLEX_DATA_MSG_STATUS_LYRICIST_NAME	= 0x06,
	SND_UMP_FLEX_DATA_MSG_STATUS_ARRANGER_NAME	= 0x07,
	SND_UMP_FLEX_DATA_MSG_STATUS_PUBLISHER_NAME	= 0x08,
	SND_UMP_FLEX_DATA_MSG_STATUS_PRIMARY_PERFORMER	= 0x09,
	SND_UMP_FLEX_DATA_MSG_STATUS_ACCOMPANY_PERFORMAER = 0x0a,
	SND_UMP_FLEX_DATA_MSG_STATUS_RECORDING_DATE	= 0x0b,
	SND_UMP_FLEX_DATA_MSG_STATUS_RECORDING_LOCATION	= 0x0c,
};

/** UMP Flex Data Status bits for Performance Text Events (Status Bank = 2) */
enum {
	SND_UMP_FLEX_DATA_MSG_STATUS_LYRICS		= 0x01,
	SND_UMP_FLEX_DATA_MSG_STATUS_LYRICS_LANGUAGE	= 0x02,
	SND_UMP_FLEX_DATA_MSG_STATUS_RUBY		= 0x03,
	SND_UMP_FLEX_DATA_MSG_STATUS_RUBY_LANGUAGE	= 0x04,
};

/**
 * \brief get UMP status (4bit) from 32bit UMP message header
 */
static inline uint8_t snd_ump_msg_hdr_status(uint32_t ump)
{
	return (ump >> 20) & 0x0f;
}

/**
 * \brief get UMP channel (4bit) from 32bit UMP message header
 */
static inline uint8_t snd_ump_msg_hdr_channel(uint32_t ump)
{
	return (ump >> 16) & 0x0f;
}

/**
 * \brief get UMP message type (4bit) from 32bit UMP message header
 */
static inline uint8_t snd_ump_msg_hdr_type(uint32_t ump)
{
	return (ump >> 28);
}

/**
 * \brief check if the given UMP type is a groupless message
 */
static inline int snd_ump_msg_type_is_groupless(uint8_t type)
{
	return type == SND_UMP_MSG_TYPE_UTILITY || type == SND_UMP_MSG_TYPE_STREAM;
}

/**
 * \brief get UMP group (4bit) from 32bit UMP message header
 */
static inline uint8_t snd_ump_msg_hdr_group(uint32_t ump)
{
	return (ump >> 24) & 0x0f;
}

/**
 * \brief get UMP status from UMP packet pointer
 */
static inline uint8_t snd_ump_msg_status(const uint32_t *ump)
{
	return snd_ump_msg_hdr_status(*ump);
}

/**
 * \brief get UMP channel from UMP packet pointer
 */
static inline uint8_t snd_ump_msg_channel(const uint32_t *ump)
{
	return snd_ump_msg_hdr_channel(*ump);
}

/**
 * \brief get UMP message type from UMP packet pointer
 */
static inline uint8_t snd_ump_msg_type(const uint32_t *ump)
{
	return snd_ump_msg_hdr_type(*ump);
}

/**
 * \brief get UMP group from UMP packet pointer
 */
static inline uint8_t snd_ump_msg_group(const uint32_t *ump)
{
	return snd_ump_msg_hdr_group(*ump);
}

/**
 * \brief get UMP sysex message status
 */
static inline uint8_t snd_ump_sysex_msg_status(const uint32_t *ump)
{
	return (*ump >> 20) & 0xf;
}

/**
 * \brief get UMP sysex message length
 */
static inline uint8_t snd_ump_sysex_msg_length(const uint32_t *ump)
{
	return (*ump >> 16) & 0xf;
}

/**
 * \brief extract one byte from a UMP packet
 */
static inline uint8_t snd_ump_get_byte(const uint32_t *ump, unsigned int offset)
{
#ifdef SNDRV_BIG_ENDIAN
	return ((const uint8_t *)ump)[offset];
#else
	return ((const uint8_t *)ump)[(offset & ~3) | (3 - (offset & 3))];
#endif
}

int snd_ump_msg_sysex_expand(const uint32_t *ump, uint8_t *buf, size_t maxlen,
			     size_t *filled);
int snd_ump_packet_length(unsigned int type);

#ifdef __cplusplus
}
#endif

#endif /* __ALSA_UMP_MSG_H */
