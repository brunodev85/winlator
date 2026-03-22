/*
 * Copyright 2014, 2015 Red Hat.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHOR(S) AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef VIRGL_HW_H
#define VIRGL_HW_H

#include <stdint.h>

/* formats known by the HW device - based on gallium subset */
enum virgl_formats {
   VIRGL_FORMAT_NONE                    = 0,
   VIRGL_FORMAT_B8G8R8A8_UNORM          = 1,
   VIRGL_FORMAT_B8G8R8X8_UNORM          = 2,
   VIRGL_FORMAT_A8R8G8B8_UNORM          = 3,
   VIRGL_FORMAT_X8R8G8B8_UNORM          = 4,
   VIRGL_FORMAT_B5G5R5A1_UNORM          = 5,
   VIRGL_FORMAT_B4G4R4A4_UNORM          = 6,
   VIRGL_FORMAT_B5G6R5_UNORM            = 7,
   VIRGL_FORMAT_R10G10B10A2_UNORM       = 8,
   VIRGL_FORMAT_L8_UNORM                = 9,    /**< ubyte luminance */
   VIRGL_FORMAT_A8_UNORM                = 10,   /**< ubyte alpha */
   VIRGL_FORMAT_I8_UNORM                = 11,
   VIRGL_FORMAT_L8A8_UNORM              = 12,   /**< ubyte alpha, luminance */
   VIRGL_FORMAT_L16_UNORM               = 13,   /**< ushort luminance */
   VIRGL_FORMAT_UYVY                    = 14,
   VIRGL_FORMAT_YUYV                    = 15,
   VIRGL_FORMAT_Z16_UNORM               = 16,
   VIRGL_FORMAT_Z32_UNORM               = 17,
   VIRGL_FORMAT_Z32_FLOAT               = 18,
   VIRGL_FORMAT_Z24_UNORM_S8_UINT       = 19,
   VIRGL_FORMAT_S8_UINT_Z24_UNORM       = 20,
   VIRGL_FORMAT_Z24X8_UNORM             = 21,
   VIRGL_FORMAT_X8Z24_UNORM             = 22,
   VIRGL_FORMAT_S8_UINT                 = 23,   /**< ubyte stencil */
   VIRGL_FORMAT_R64_FLOAT               = 24,
   VIRGL_FORMAT_R64G64_FLOAT            = 25,
   VIRGL_FORMAT_R64G64B64_FLOAT         = 26,
   VIRGL_FORMAT_R64G64B64A64_FLOAT      = 27,
   VIRGL_FORMAT_R32_FLOAT               = 28,
   VIRGL_FORMAT_R32G32_FLOAT            = 29,
   VIRGL_FORMAT_R32G32B32_FLOAT         = 30,
   VIRGL_FORMAT_R32G32B32A32_FLOAT      = 31,

   VIRGL_FORMAT_R32_UNORM               = 32,
   VIRGL_FORMAT_R32G32_UNORM            = 33,
   VIRGL_FORMAT_R32G32B32_UNORM         = 34,
   VIRGL_FORMAT_R32G32B32A32_UNORM      = 35,
   VIRGL_FORMAT_R32_USCALED             = 36,
   VIRGL_FORMAT_R32G32_USCALED          = 37,
   VIRGL_FORMAT_R32G32B32_USCALED       = 38,
   VIRGL_FORMAT_R32G32B32A32_USCALED    = 39,
   VIRGL_FORMAT_R32_SNORM               = 40,
   VIRGL_FORMAT_R32G32_SNORM            = 41,
   VIRGL_FORMAT_R32G32B32_SNORM         = 42,
   VIRGL_FORMAT_R32G32B32A32_SNORM      = 43,
   VIRGL_FORMAT_R32_SSCALED             = 44,
   VIRGL_FORMAT_R32G32_SSCALED          = 45,
   VIRGL_FORMAT_R32G32B32_SSCALED       = 46,
   VIRGL_FORMAT_R32G32B32A32_SSCALED    = 47,

   VIRGL_FORMAT_R16_UNORM               = 48,
   VIRGL_FORMAT_R16G16_UNORM            = 49,
   VIRGL_FORMAT_R16G16B16_UNORM         = 50,
   VIRGL_FORMAT_R16G16B16A16_UNORM      = 51,

   VIRGL_FORMAT_R16_USCALED             = 52,
   VIRGL_FORMAT_R16G16_USCALED          = 53,
   VIRGL_FORMAT_R16G16B16_USCALED       = 54,
   VIRGL_FORMAT_R16G16B16A16_USCALED    = 55,

   VIRGL_FORMAT_R16_SNORM               = 56,
   VIRGL_FORMAT_R16G16_SNORM            = 57,
   VIRGL_FORMAT_R16G16B16_SNORM         = 58,
   VIRGL_FORMAT_R16G16B16A16_SNORM      = 59,

   VIRGL_FORMAT_R16_SSCALED             = 60,
   VIRGL_FORMAT_R16G16_SSCALED          = 61,
   VIRGL_FORMAT_R16G16B16_SSCALED       = 62,
   VIRGL_FORMAT_R16G16B16A16_SSCALED    = 63,

   VIRGL_FORMAT_R8_UNORM                = 64,
   VIRGL_FORMAT_R8G8_UNORM              = 65,
   VIRGL_FORMAT_R8G8B8_UNORM            = 66,
   VIRGL_FORMAT_R8G8B8A8_UNORM          = 67,
   VIRGL_FORMAT_X8B8G8R8_UNORM          = 68,

   VIRGL_FORMAT_R8_USCALED              = 69,
   VIRGL_FORMAT_R8G8_USCALED            = 70,
   VIRGL_FORMAT_R8G8B8_USCALED          = 71,
   VIRGL_FORMAT_R8G8B8A8_USCALED        = 72,

   VIRGL_FORMAT_R8_SNORM                = 74,
   VIRGL_FORMAT_R8G8_SNORM              = 75,
   VIRGL_FORMAT_R8G8B8_SNORM            = 76,
   VIRGL_FORMAT_R8G8B8A8_SNORM          = 77,

   VIRGL_FORMAT_R8_SSCALED              = 82,
   VIRGL_FORMAT_R8G8_SSCALED            = 83,
   VIRGL_FORMAT_R8G8B8_SSCALED          = 84,
   VIRGL_FORMAT_R8G8B8A8_SSCALED        = 85,

   VIRGL_FORMAT_R32_FIXED               = 87,
   VIRGL_FORMAT_R32G32_FIXED            = 88,
   VIRGL_FORMAT_R32G32B32_FIXED         = 89,
   VIRGL_FORMAT_R32G32B32A32_FIXED      = 90,

   VIRGL_FORMAT_R16_FLOAT               = 91,
   VIRGL_FORMAT_R16G16_FLOAT            = 92,
   VIRGL_FORMAT_R16G16B16_FLOAT         = 93,
   VIRGL_FORMAT_R16G16B16A16_FLOAT      = 94,

   VIRGL_FORMAT_L8_SRGB                 = 95,
   VIRGL_FORMAT_L8A8_SRGB               = 96,
   VIRGL_FORMAT_R8G8B8_SRGB             = 97,
   VIRGL_FORMAT_A8B8G8R8_SRGB           = 98,
   VIRGL_FORMAT_X8B8G8R8_SRGB           = 99,
   VIRGL_FORMAT_B8G8R8A8_SRGB           = 100,
   VIRGL_FORMAT_B8G8R8X8_SRGB           = 101,
   VIRGL_FORMAT_A8R8G8B8_SRGB           = 102,
   VIRGL_FORMAT_X8R8G8B8_SRGB           = 103,
   VIRGL_FORMAT_R8G8B8A8_SRGB           = 104,

   /* compressed formats */
   VIRGL_FORMAT_DXT1_RGB                = 105,
   VIRGL_FORMAT_DXT1_RGBA               = 106,
   VIRGL_FORMAT_DXT3_RGBA               = 107,
   VIRGL_FORMAT_DXT5_RGBA               = 108,

   /* sRGB, compressed */
   VIRGL_FORMAT_DXT1_SRGB               = 109,
   VIRGL_FORMAT_DXT1_SRGBA              = 110,
   VIRGL_FORMAT_DXT3_SRGBA              = 111,
   VIRGL_FORMAT_DXT5_SRGBA              = 112,

   /* rgtc compressed */
   VIRGL_FORMAT_RGTC1_UNORM             = 113,
   VIRGL_FORMAT_RGTC1_SNORM             = 114,
   VIRGL_FORMAT_RGTC2_UNORM             = 115,
   VIRGL_FORMAT_RGTC2_SNORM             = 116,

   VIRGL_FORMAT_R8G8_B8G8_UNORM         = 117,
   VIRGL_FORMAT_G8R8_G8B8_UNORM         = 118,

   VIRGL_FORMAT_R8SG8SB8UX8U_NORM       = 119,
   VIRGL_FORMAT_R5SG5SB6U_NORM          = 120,

   VIRGL_FORMAT_A8B8G8R8_UNORM          = 121,
   VIRGL_FORMAT_B5G5R5X1_UNORM          = 122,
   VIRGL_FORMAT_R10G10B10A2_USCALED     = 123,
   VIRGL_FORMAT_R11G11B10_FLOAT         = 124,
   VIRGL_FORMAT_R9G9B9E5_FLOAT          = 125,
   VIRGL_FORMAT_Z32_FLOAT_S8X24_UINT    = 126,
   VIRGL_FORMAT_R1_UNORM                = 127,
   VIRGL_FORMAT_R10G10B10X2_USCALED     = 128,
   VIRGL_FORMAT_R10G10B10X2_SNORM       = 129,

   VIRGL_FORMAT_L4A4_UNORM              = 130,
   VIRGL_FORMAT_B10G10R10A2_UNORM       = 131,
   VIRGL_FORMAT_R10SG10SB10SA2U_NORM    = 132,
   VIRGL_FORMAT_R8G8Bx_SNORM            = 133,
   VIRGL_FORMAT_R8G8B8X8_UNORM          = 134,
   VIRGL_FORMAT_B4G4R4X4_UNORM          = 135,
   VIRGL_FORMAT_X24S8_UINT              = 136,
   VIRGL_FORMAT_S8X24_UINT              = 137,
   VIRGL_FORMAT_X32_S8X24_UINT          = 138,
   VIRGL_FORMAT_B2G3R3_UNORM            = 139,

   VIRGL_FORMAT_L16A16_UNORM            = 140,
   VIRGL_FORMAT_A16_UNORM               = 141,
   VIRGL_FORMAT_I16_UNORM               = 142,

   VIRGL_FORMAT_LATC1_UNORM             = 143,
   VIRGL_FORMAT_LATC1_SNORM             = 144,
   VIRGL_FORMAT_LATC2_UNORM             = 145,
   VIRGL_FORMAT_LATC2_SNORM             = 146,

   VIRGL_FORMAT_A8_SNORM                = 147,
   VIRGL_FORMAT_L8_SNORM                = 148,
   VIRGL_FORMAT_L8A8_SNORM              = 149,
   VIRGL_FORMAT_I8_SNORM                = 150,
   VIRGL_FORMAT_A16_SNORM               = 151,
   VIRGL_FORMAT_L16_SNORM               = 152,
   VIRGL_FORMAT_L16A16_SNORM            = 153,
   VIRGL_FORMAT_I16_SNORM               = 154,

   VIRGL_FORMAT_A16_FLOAT               = 155,
   VIRGL_FORMAT_L16_FLOAT               = 156,
   VIRGL_FORMAT_L16A16_FLOAT            = 157,
   VIRGL_FORMAT_I16_FLOAT               = 158,
   VIRGL_FORMAT_A32_FLOAT               = 159,
   VIRGL_FORMAT_L32_FLOAT               = 160,
   VIRGL_FORMAT_L32A32_FLOAT            = 161,
   VIRGL_FORMAT_I32_FLOAT               = 162,

   VIRGL_FORMAT_YV12                    = 163,
   VIRGL_FORMAT_YV16                    = 164,
   VIRGL_FORMAT_IYUV                    = 165,  /**< aka I420 */
   VIRGL_FORMAT_NV12                    = 166,
   VIRGL_FORMAT_NV21                    = 167,

   VIRGL_FORMAT_A4R4_UNORM              = 168,
   VIRGL_FORMAT_R4A4_UNORM              = 169,
   VIRGL_FORMAT_R8A8_UNORM              = 170,
   VIRGL_FORMAT_A8R8_UNORM              = 171,

   VIRGL_FORMAT_R10G10B10A2_SSCALED     = 172,
   VIRGL_FORMAT_R10G10B10A2_SNORM       = 173,
   VIRGL_FORMAT_B10G10R10A2_USCALED     = 174,
   VIRGL_FORMAT_B10G10R10A2_SSCALED     = 175,
   VIRGL_FORMAT_B10G10R10A2_SNORM       = 176,

   VIRGL_FORMAT_R8_UINT                 = 177,
   VIRGL_FORMAT_R8G8_UINT               = 178,
   VIRGL_FORMAT_R8G8B8_UINT             = 179,
   VIRGL_FORMAT_R8G8B8A8_UINT           = 180,

   VIRGL_FORMAT_R8_SINT                 = 181,
   VIRGL_FORMAT_R8G8_SINT               = 182,
   VIRGL_FORMAT_R8G8B8_SINT             = 183,
   VIRGL_FORMAT_R8G8B8A8_SINT           = 184,

   VIRGL_FORMAT_R16_UINT                = 185,
   VIRGL_FORMAT_R16G16_UINT             = 186,
   VIRGL_FORMAT_R16G16B16_UINT          = 187,
   VIRGL_FORMAT_R16G16B16A16_UINT       = 188,

   VIRGL_FORMAT_R16_SINT                = 189,
   VIRGL_FORMAT_R16G16_SINT             = 190,
   VIRGL_FORMAT_R16G16B16_SINT          = 191,
   VIRGL_FORMAT_R16G16B16A16_SINT       = 192,
   VIRGL_FORMAT_R32_UINT                = 193,
   VIRGL_FORMAT_R32G32_UINT             = 194,
   VIRGL_FORMAT_R32G32B32_UINT          = 195,
   VIRGL_FORMAT_R32G32B32A32_UINT       = 196,

   VIRGL_FORMAT_R32_SINT                = 197,
   VIRGL_FORMAT_R32G32_SINT             = 198,
   VIRGL_FORMAT_R32G32B32_SINT          = 199,
   VIRGL_FORMAT_R32G32B32A32_SINT       = 200,

   VIRGL_FORMAT_A8_UINT                 = 201,
   VIRGL_FORMAT_I8_UINT                 = 202,
   VIRGL_FORMAT_L8_UINT                 = 203,
   VIRGL_FORMAT_L8A8_UINT               = 204,

   VIRGL_FORMAT_A8_SINT                 = 205,
   VIRGL_FORMAT_I8_SINT                 = 206,
   VIRGL_FORMAT_L8_SINT                 = 207,
   VIRGL_FORMAT_L8A8_SINT               = 208,

   VIRGL_FORMAT_A16_UINT                = 209,
   VIRGL_FORMAT_I16_UINT                = 210,
   VIRGL_FORMAT_L16_UINT                = 211,
   VIRGL_FORMAT_L16A16_UINT             = 212,

   VIRGL_FORMAT_A16_SINT                = 213,
   VIRGL_FORMAT_I16_SINT                = 214,
   VIRGL_FORMAT_L16_SINT                = 215,
   VIRGL_FORMAT_L16A16_SINT             = 216,

   VIRGL_FORMAT_A32_UINT                = 217,
   VIRGL_FORMAT_I32_UINT                = 218,
   VIRGL_FORMAT_L32_UINT                = 219,
   VIRGL_FORMAT_L32A32_UINT             = 220,

   VIRGL_FORMAT_A32_SINT                = 221,
   VIRGL_FORMAT_I32_SINT                = 222,
   VIRGL_FORMAT_L32_SINT                = 223,
   VIRGL_FORMAT_L32A32_SINT             = 224,

   VIRGL_FORMAT_B10G10R10A2_UINT        = 225,
   VIRGL_FORMAT_ETC1_RGB8               = 226,
   VIRGL_FORMAT_R8G8_R8B8_UNORM         = 227,
   VIRGL_FORMAT_G8R8_B8R8_UNORM         = 228,
   VIRGL_FORMAT_R8G8B8X8_SNORM          = 229,

   VIRGL_FORMAT_R8G8B8X8_SRGB           = 230,

   VIRGL_FORMAT_R8G8B8X8_UINT           = 231,
   VIRGL_FORMAT_R8G8B8X8_SINT           = 232,
   VIRGL_FORMAT_B10G10R10X2_UNORM       = 233,
   VIRGL_FORMAT_R16G16B16X16_UNORM      = 234,
   VIRGL_FORMAT_R16G16B16X16_SNORM      = 235,
   VIRGL_FORMAT_R16G16B16X16_FLOAT      = 236,
   VIRGL_FORMAT_R16G16B16X16_UINT       = 237,
   VIRGL_FORMAT_R16G16B16X16_SINT       = 238,
   VIRGL_FORMAT_R32G32B32X32_FLOAT      = 239,
   VIRGL_FORMAT_R32G32B32X32_UINT       = 240,
   VIRGL_FORMAT_R32G32B32X32_SINT       = 241,
   VIRGL_FORMAT_R8A8_SNORM              = 242,
   VIRGL_FORMAT_R16A16_UNORM            = 243,
   VIRGL_FORMAT_R16A16_SNORM            = 244,
   VIRGL_FORMAT_R16A16_FLOAT            = 245,
   VIRGL_FORMAT_R32A32_FLOAT            = 246,
   VIRGL_FORMAT_R8A8_UINT               = 247,
   VIRGL_FORMAT_R8A8_SINT               = 248,
   VIRGL_FORMAT_R16A16_UINT             = 249,
   VIRGL_FORMAT_R16A16_SINT             = 250,
   VIRGL_FORMAT_R32A32_UINT             = 251,
   VIRGL_FORMAT_R32A32_SINT             = 252,

   VIRGL_FORMAT_R10G10B10A2_UINT        = 253,
   VIRGL_FORMAT_B5G6R5_SRGB             = 254,

   VIRGL_FORMAT_BPTC_RGBA_UNORM         = 255,
   VIRGL_FORMAT_BPTC_SRGBA              = 256,
   VIRGL_FORMAT_BPTC_RGB_FLOAT          = 257,
   VIRGL_FORMAT_BPTC_RGB_UFLOAT         = 258,

   VIRGL_FORMAT_A16L16_UNORM            = 262,

   VIRGL_FORMAT_G8R8_UNORM              = 263,
   VIRGL_FORMAT_G8R8_SNORM              = 264,
   VIRGL_FORMAT_G16R16_UNORM            = 265,
   VIRGL_FORMAT_G16R16_SNORM            = 266,
   VIRGL_FORMAT_A8B8G8R8_SNORM          = 267,

   VIRGL_FORMAT_A8L8_UNORM              = 259,
   VIRGL_FORMAT_A8L8_SNORM              = 260,
   VIRGL_FORMAT_A8L8_SRGB               = 261,

   VIRGL_FORMAT_X8B8G8R8_SNORM          = 268,

   VIRGL_FORMAT_R10G10B10X2_UNORM       = 308,
   VIRGL_FORMAT_A4B4G4R4_UNORM          = 311,

   VIRGL_FORMAT_R8_SRGB                 = 312,
   VIRGL_FORMAT_MAX /* = PIPE_FORMAT_COUNT */,

   /* Below formats must not be used in the guest. */
   VIRGL_FORMAT_B8G8R8X8_UNORM_EMULATED,
   VIRGL_FORMAT_B8G8R8A8_UNORM_EMULATED,
   VIRGL_FORMAT_MAX_EXTENDED
};

/* These are used by the capability_bits field in virgl_caps_v2. */
#define VIRGL_CAP_NONE 0
#define VIRGL_CAP_TGSI_INVARIANT       (1 << 0)
#define VIRGL_CAP_TEXTURE_VIEW         (1 << 1)
#define VIRGL_CAP_SET_MIN_SAMPLES      (1 << 2)
#define VIRGL_CAP_COPY_IMAGE           (1 << 3)
#define VIRGL_CAP_TGSI_PRECISE         (1 << 4)
#define VIRGL_CAP_TXQS                 (1 << 5)
#define VIRGL_CAP_MEMORY_BARRIER       (1 << 6)
#define VIRGL_CAP_COMPUTE_SHADER       (1 << 7)
#define VIRGL_CAP_FB_NO_ATTACH         (1 << 8)
#define VIRGL_CAP_ROBUST_BUFFER_ACCESS (1 << 9)
#define VIRGL_CAP_TGSI_FBFETCH         (1 << 10)
#define VIRGL_CAP_SHADER_CLOCK         (1 << 11)
#define VIRGL_CAP_TEXTURE_BARRIER      (1 << 12)
#define VIRGL_CAP_TGSI_COMPONENTS      (1 << 13)
#define VIRGL_CAP_GUEST_MAY_INIT_LOG   (1 << 14)
#define VIRGL_CAP_SRGB_WRITE_CONTROL   (1 << 15)
#define VIRGL_CAP_QBO                  (1 << 16)
#define VIRGL_CAP_TRANSFER             (1 << 17)
#define VIRGL_CAP_FBO_MIXED_COLOR_FORMATS  (1 << 18)
#define VIRGL_CAP_FAKE_FP64            (1 << 19)
#define VIRGL_CAP_BIND_COMMAND_ARGS    (1 << 20)
#define VIRGL_CAP_MULTI_DRAW_INDIRECT  (1 << 21)
#define VIRGL_CAP_INDIRECT_PARAMS      (1 << 22)
#define VIRGL_CAP_TRANSFORM_FEEDBACK3  (1 << 23)
#define VIRGL_CAP_3D_ASTC              (1 << 24)
#define VIRGL_CAP_INDIRECT_INPUT_ADDR  (1 << 25)
#define VIRGL_CAP_COPY_TRANSFER        (1 << 26)
#define VIRGL_CAP_CLIP_HALFZ           (1 << 27)
#define VIRGL_CAP_APP_TWEAK_SUPPORT    (1 << 28)
#define VIRGL_CAP_BGRA_SRGB_IS_EMULATED  (1 << 29)

/* virgl bind flags - these are compatible with mesa 10.5 gallium.
 * but are fixed, no other should be passed to virgl either.
 */
#define VIRGL_BIND_DEPTH_STENCIL (1 << 0)
#define VIRGL_BIND_RENDER_TARGET (1 << 1)
#define VIRGL_BIND_SAMPLER_VIEW  (1 << 3)
#define VIRGL_BIND_VERTEX_BUFFER (1 << 4)
#define VIRGL_BIND_INDEX_BUFFER  (1 << 5)
#define VIRGL_BIND_CONSTANT_BUFFER (1 << 6)
#define VIRGL_BIND_DISPLAY_TARGET (1 << 7)
#define VIRGL_BIND_COMMAND_ARGS  (1 << 8)
#define VIRGL_BIND_STREAM_OUTPUT (1 << 11)
#define VIRGL_BIND_SHADER_BUFFER (1 << 14)
#define VIRGL_BIND_QUERY_BUFFER  (1 << 15)
#define VIRGL_BIND_CURSOR        (1 << 16)
#define VIRGL_BIND_CUSTOM        (1 << 17)
#define VIRGL_BIND_SCANOUT       (1 << 18)
/* Used for buffers that are backed by guest storage and
 * are only read by the host.
 */
#define VIRGL_BIND_STAGING       (1 << 19)
#define VIRGL_BIND_SHARED        (1 << 20)

#define VIRGL_BIND_PREFER_EMULATED_BGRA  (1 << 21)

#define VIRGL_BIND_LINEAR (1 << 22)

struct virgl_caps_bool_set1 {
        unsigned indep_blend_enable:1;
        unsigned indep_blend_func:1;
        unsigned cube_map_array:1;
        unsigned shader_stencil_export:1;
        unsigned conditional_render:1;
        unsigned start_instance:1;
        unsigned primitive_restart:1;
        unsigned blend_eq_sep:1;
        unsigned instanceid:1;
        unsigned vertex_element_instance_divisor:1;
        unsigned seamless_cube_map:1;
        unsigned occlusion_query:1;
        unsigned timer_query:1;
        unsigned streamout_pause_resume:1;
        unsigned texture_multisample:1;
        unsigned fragment_coord_conventions:1;
        unsigned depth_clip_disable:1;
        unsigned seamless_cube_map_per_texture:1;
        unsigned ubo:1;
        unsigned color_clamping:1; /* not in GL 3.1 core profile */
        unsigned poly_stipple:1; /* not in GL 3.1 core profile */
        unsigned mirror_clamp:1;
        unsigned texture_query_lod:1;
        unsigned has_fp64:1;
        unsigned has_tessellation_shaders:1;
        unsigned has_indirect_draw:1;
        unsigned has_sample_shading:1;
        unsigned has_cull:1;
        unsigned conditional_render_inverted:1;
        unsigned derivative_control:1;
        unsigned polygon_offset_clamp:1;
        unsigned transform_feedback_overflow_query:1;
        /* DO NOT ADD ANYMORE MEMBERS - need to add another 32-bit to v2 caps */
};

/* endless expansion capabilites - current gallium has 252 formats */
struct virgl_supported_format_mask {
        uint32_t bitmask[16];
};
/* capabilities set 2 - version 1 - 32-bit and float values */
struct virgl_caps_v1 {
        uint32_t max_version;
        struct virgl_supported_format_mask sampler;
        struct virgl_supported_format_mask render;
        struct virgl_supported_format_mask depthstencil;
        struct virgl_supported_format_mask vertexbuffer;
        struct virgl_caps_bool_set1 bset;
        uint32_t glsl_level;
        uint32_t max_texture_array_layers;
        uint32_t max_streamout_buffers;
        uint32_t max_dual_source_render_targets;
        uint32_t max_render_targets;
        uint32_t max_samples;
        uint32_t prim_mask;
        uint32_t max_tbo_size;
        uint32_t max_uniform_blocks;
        uint32_t max_viewports;
        uint32_t max_texture_gather_components;
};

/*
 * This struct should be growable when used in capset 2,
 * so we shouldn't have to add a v3 ever.
 */
struct virgl_caps_v2 {
        struct virgl_caps_v1 v1;
        float min_aliased_point_size;
        float max_aliased_point_size;
        float min_smooth_point_size;
        float max_smooth_point_size;
        float min_aliased_line_width;
        float max_aliased_line_width;
        float min_smooth_line_width;
        float max_smooth_line_width;
        float max_texture_lod_bias;
        uint32_t max_geom_output_vertices;
        uint32_t max_geom_total_output_components;
        uint32_t max_vertex_outputs;
        uint32_t max_vertex_attribs;
        uint32_t max_shader_patch_varyings;
        int32_t min_texel_offset;
        int32_t max_texel_offset;
        int32_t min_texture_gather_offset;
        int32_t max_texture_gather_offset;
        uint32_t texture_buffer_offset_alignment;
        uint32_t uniform_buffer_offset_alignment;
        uint32_t shader_buffer_offset_alignment;
        uint32_t capability_bits;
        uint32_t sample_locations[8];
        uint32_t max_vertex_attrib_stride;
        uint32_t max_shader_buffer_frag_compute;
        uint32_t max_shader_buffer_other_stages;
        uint32_t max_shader_image_frag_compute;
        uint32_t max_shader_image_other_stages;
        uint32_t max_image_samples;
        uint32_t max_compute_work_group_invocations;
        uint32_t max_compute_shared_memory_size;
        uint32_t max_compute_grid_size[3];
        uint32_t max_compute_block_size[3];
        uint32_t max_texture_2d_size;
        uint32_t max_texture_3d_size;
        uint32_t max_texture_cube_size;
        uint32_t max_combined_shader_buffers;
        uint32_t max_atomic_counters[6];
        uint32_t max_atomic_counter_buffers[6];
        uint32_t max_combined_atomic_counters;
        uint32_t max_combined_atomic_counter_buffers;
        uint32_t host_feature_check_version;
        struct virgl_supported_format_mask supported_readback_formats;
        struct virgl_supported_format_mask scanout;
};

union virgl_caps {
        uint32_t max_version;
        struct virgl_caps_v1 v1;
        struct virgl_caps_v2 v2;
};

#define VIRGL_RESOURCE_Y_0_TOP (1 << 0)
#endif
