/**************************************************************************
 * 
 * Copyright 2007 VMware, Inc.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL VMWARE AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

#ifndef PIPE_DEFINES_H
#define PIPE_DEFINES_H

#include "p_compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Gallium error codes.
 *
 * - A zero value always means success.
 * - A negative value always means failure.
 * - The meaning of a positive value is function dependent.
 */
enum pipe_error {
   PIPE_OK = 0,
   PIPE_ERROR = -1,    /**< Generic error */
   PIPE_ERROR_BAD_INPUT = -2,
   PIPE_ERROR_OUT_OF_MEMORY = -3,
   PIPE_ERROR_RETRY = -4
   /* TODO */
};


#define PIPE_BLENDFACTOR_ONE                 0x1
#define PIPE_BLENDFACTOR_SRC_COLOR           0x2
#define PIPE_BLENDFACTOR_SRC_ALPHA           0x3
#define PIPE_BLENDFACTOR_DST_ALPHA           0x4
#define PIPE_BLENDFACTOR_DST_COLOR           0x5
#define PIPE_BLENDFACTOR_SRC_ALPHA_SATURATE  0x6
#define PIPE_BLENDFACTOR_CONST_COLOR         0x7
#define PIPE_BLENDFACTOR_CONST_ALPHA         0x8
#define PIPE_BLENDFACTOR_SRC1_COLOR          0x9
#define PIPE_BLENDFACTOR_SRC1_ALPHA          0x0A
#define PIPE_BLENDFACTOR_ZERO                0x11
#define PIPE_BLENDFACTOR_INV_SRC_COLOR       0x12
#define PIPE_BLENDFACTOR_INV_SRC_ALPHA       0x13
#define PIPE_BLENDFACTOR_INV_DST_ALPHA       0x14
#define PIPE_BLENDFACTOR_INV_DST_COLOR       0x15
#define PIPE_BLENDFACTOR_INV_CONST_COLOR     0x17
#define PIPE_BLENDFACTOR_INV_CONST_ALPHA     0x18
#define PIPE_BLENDFACTOR_INV_SRC1_COLOR      0x19
#define PIPE_BLENDFACTOR_INV_SRC1_ALPHA      0x1A

#define PIPE_BLEND_ADD               0
#define PIPE_BLEND_SUBTRACT          1
#define PIPE_BLEND_REVERSE_SUBTRACT  2
#define PIPE_BLEND_MIN               3
#define PIPE_BLEND_MAX               4


enum pipe_logicop {
   PIPE_LOGICOP_CLEAR,
   PIPE_LOGICOP_NOR,
   PIPE_LOGICOP_AND_INVERTED,
   PIPE_LOGICOP_COPY_INVERTED,
   PIPE_LOGICOP_AND_REVERSE,
   PIPE_LOGICOP_INVERT,
   PIPE_LOGICOP_XOR,
   PIPE_LOGICOP_NAND,
   PIPE_LOGICOP_AND,
   PIPE_LOGICOP_EQUIV,
   PIPE_LOGICOP_NOOP,
   PIPE_LOGICOP_OR_INVERTED,
   PIPE_LOGICOP_COPY,
   PIPE_LOGICOP_OR_REVERSE,
   PIPE_LOGICOP_OR,
   PIPE_LOGICOP_SET,
};

#define PIPE_MASK_R  0x1
#define PIPE_MASK_G  0x2
#define PIPE_MASK_B  0x4
#define PIPE_MASK_A  0x8
#define PIPE_MASK_RGBA 0xf
#define PIPE_MASK_Z  0x10
#define PIPE_MASK_S  0x20
#define PIPE_MASK_ZS 0x30
#define PIPE_MASK_RGBAZS (PIPE_MASK_RGBA|PIPE_MASK_ZS)


/**
 * Inequality functions.  Used for depth test, stencil compare, alpha
 * test, shadow compare, etc.
 */
#define PIPE_FUNC_NEVER    0
#define PIPE_FUNC_LESS     1
#define PIPE_FUNC_EQUAL    2
#define PIPE_FUNC_LEQUAL   3
#define PIPE_FUNC_GREATER  4
#define PIPE_FUNC_NOTEQUAL 5
#define PIPE_FUNC_GEQUAL   6
#define PIPE_FUNC_ALWAYS   7

/** Polygon fill mode */
#define PIPE_POLYGON_MODE_FILL  0
#define PIPE_POLYGON_MODE_LINE  1
#define PIPE_POLYGON_MODE_POINT 2

/** Polygon face specification, eg for culling */
#define PIPE_FACE_NONE           0
#define PIPE_FACE_FRONT          1
#define PIPE_FACE_BACK           2
#define PIPE_FACE_FRONT_AND_BACK (PIPE_FACE_FRONT | PIPE_FACE_BACK)

/** Stencil ops */
#define PIPE_STENCIL_OP_KEEP       0
#define PIPE_STENCIL_OP_ZERO       1
#define PIPE_STENCIL_OP_REPLACE    2
#define PIPE_STENCIL_OP_INCR       3
#define PIPE_STENCIL_OP_DECR       4
#define PIPE_STENCIL_OP_INCR_WRAP  5
#define PIPE_STENCIL_OP_DECR_WRAP  6
#define PIPE_STENCIL_OP_INVERT     7

/** Texture types.
 * See the documentation for info on PIPE_TEXTURE_RECT vs PIPE_TEXTURE_2D */
enum pipe_texture_target {
   PIPE_BUFFER           = 0,
   PIPE_TEXTURE_1D       = 1,
   PIPE_TEXTURE_2D       = 2,
   PIPE_TEXTURE_3D       = 3,
   PIPE_TEXTURE_CUBE     = 4,
   PIPE_TEXTURE_RECT     = 5,
   PIPE_TEXTURE_1D_ARRAY = 6,
   PIPE_TEXTURE_2D_ARRAY = 7,
   PIPE_TEXTURE_CUBE_ARRAY = 8,
   PIPE_MAX_TEXTURE_TYPES
};

#define PIPE_TEX_FACE_POS_X 0
#define PIPE_TEX_FACE_NEG_X 1
#define PIPE_TEX_FACE_POS_Y 2
#define PIPE_TEX_FACE_NEG_Y 3
#define PIPE_TEX_FACE_POS_Z 4
#define PIPE_TEX_FACE_NEG_Z 5
#define PIPE_TEX_FACE_MAX   6

#define PIPE_TEX_WRAP_REPEAT                   0
#define PIPE_TEX_WRAP_CLAMP                    1
#define PIPE_TEX_WRAP_CLAMP_TO_EDGE            2
#define PIPE_TEX_WRAP_CLAMP_TO_BORDER          3
#define PIPE_TEX_WRAP_MIRROR_REPEAT            4
#define PIPE_TEX_WRAP_MIRROR_CLAMP             5
#define PIPE_TEX_WRAP_MIRROR_CLAMP_TO_EDGE     6
#define PIPE_TEX_WRAP_MIRROR_CLAMP_TO_BORDER   7

/* Between mipmaps, ie mipfilter
 */
#define PIPE_TEX_MIPFILTER_NEAREST  0
#define PIPE_TEX_MIPFILTER_LINEAR   1
#define PIPE_TEX_MIPFILTER_NONE     2

/* Within a mipmap, ie min/mag filter 
 */
#define PIPE_TEX_FILTER_NEAREST      0
#define PIPE_TEX_FILTER_LINEAR       1

#define PIPE_TEX_COMPARE_NONE          0
#define PIPE_TEX_COMPARE_R_TO_TEXTURE  1

/**
 * Clear buffer bits
 */
#define PIPE_CLEAR_DEPTH        (1 << 0)
#define PIPE_CLEAR_STENCIL      (1 << 1)
#define PIPE_CLEAR_COLOR0       (1 << 2)
#define PIPE_CLEAR_COLOR1       (1 << 3)
#define PIPE_CLEAR_COLOR2       (1 << 4)
#define PIPE_CLEAR_COLOR3       (1 << 5)
#define PIPE_CLEAR_COLOR4       (1 << 6)
#define PIPE_CLEAR_COLOR5       (1 << 7)
#define PIPE_CLEAR_COLOR6       (1 << 8)
#define PIPE_CLEAR_COLOR7       (1 << 9)
/** Combined flags */
/** All color buffers currently bound */
#define PIPE_CLEAR_COLOR        (PIPE_CLEAR_COLOR0 | PIPE_CLEAR_COLOR1 | \
                                 PIPE_CLEAR_COLOR2 | PIPE_CLEAR_COLOR3 | \
                                 PIPE_CLEAR_COLOR4 | PIPE_CLEAR_COLOR5 | \
                                 PIPE_CLEAR_COLOR6 | PIPE_CLEAR_COLOR7)
#define PIPE_CLEAR_DEPTHSTENCIL (PIPE_CLEAR_DEPTH | PIPE_CLEAR_STENCIL)

/**
 * Transfer object usage flags
 */
enum pipe_transfer_usage {
   /**
    * Resource contents read back (or accessed directly) at transfer
    * create time.
    */
   PIPE_TRANSFER_READ = (1 << 0),
   
   /**
    * Resource contents will be written back at transfer_unmap
    * time (or modified as a result of being accessed directly).
    */
   PIPE_TRANSFER_WRITE = (1 << 1),

   /**
    * Read/modify/write
    */
   PIPE_TRANSFER_READ_WRITE = PIPE_TRANSFER_READ | PIPE_TRANSFER_WRITE,

   /** 
    * The transfer should map the texture storage directly. The driver may
    * return NULL if that isn't possible, and the state tracker needs to cope
    * with that and use an alternative path without this flag.
    *
    * E.g. the state tracker could have a simpler path which maps textures and
    * does read/modify/write cycles on them directly, and a more complicated
    * path which uses minimal read and write transfers.
    */
   PIPE_TRANSFER_MAP_DIRECTLY = (1 << 2),

   /**
    * Discards the memory within the mapped region.
    *
    * It should not be used with PIPE_TRANSFER_READ.
    *
    * See also:
    * - OpenGL's ARB_map_buffer_range extension, MAP_INVALIDATE_RANGE_BIT flag.
    */
   PIPE_TRANSFER_DISCARD_RANGE = (1 << 8),

   /**
    * Fail if the resource cannot be mapped immediately.
    *
    * See also:
    * - Direct3D's D3DLOCK_DONOTWAIT flag.
    * - Mesa3D's MESA_MAP_NOWAIT_BIT flag.
    * - WDDM's D3DDDICB_LOCKFLAGS.DonotWait flag.
    */
   PIPE_TRANSFER_DONTBLOCK = (1 << 9),

   /**
    * Do not attempt to synchronize pending operations on the resource when mapping.
    *
    * It should not be used with PIPE_TRANSFER_READ.
    *
    * See also:
    * - OpenGL's ARB_map_buffer_range extension, MAP_UNSYNCHRONIZED_BIT flag.
    * - Direct3D's D3DLOCK_NOOVERWRITE flag.
    * - WDDM's D3DDDICB_LOCKFLAGS.IgnoreSync flag.
    */
   PIPE_TRANSFER_UNSYNCHRONIZED = (1 << 10),

   /**
    * Written ranges will be notified later with
    * pipe_context::transfer_flush_region.
    *
    * It should not be used with PIPE_TRANSFER_READ.
    *
    * See also:
    * - pipe_context::transfer_flush_region
    * - OpenGL's ARB_map_buffer_range extension, MAP_FLUSH_EXPLICIT_BIT flag.
    */
   PIPE_TRANSFER_FLUSH_EXPLICIT = (1 << 11),

   /**
    * Discards all memory backing the resource.
    *
    * It should not be used with PIPE_TRANSFER_READ.
    *
    * This is equivalent to:
    * - OpenGL's ARB_map_buffer_range extension, MAP_INVALIDATE_BUFFER_BIT
    * - BufferData(NULL) on a GL buffer
    * - Direct3D's D3DLOCK_DISCARD flag.
    * - WDDM's D3DDDICB_LOCKFLAGS.Discard flag.
    * - D3D10 DDI's D3D10_DDI_MAP_WRITE_DISCARD flag
    * - D3D10's D3D10_MAP_WRITE_DISCARD flag.
    */
   PIPE_TRANSFER_DISCARD_WHOLE_RESOURCE = (1 << 12),

   /**
    * Allows the resource to be used for rendering while mapped.
    *
    * PIPE_RESOURCE_FLAG_MAP_PERSISTENT must be set when creating
    * the resource.
    *
    * If COHERENT is not set, memory_barrier(PIPE_BARRIER_MAPPED_BUFFER)
    * must be called to ensure the device can see what the CPU has written.
    */
   PIPE_TRANSFER_PERSISTENT = (1 << 13),

   /**
    * If PERSISTENT is set, this ensures any writes done by the device are
    * immediately visible to the CPU and vice versa.
    *
    * PIPE_RESOURCE_FLAG_MAP_COHERENT must be set when creating
    * the resource.
    */
   PIPE_TRANSFER_COHERENT = (1 << 14)
};

/**
 * Flags for the flush function.
 */
enum pipe_flush_flags {
   PIPE_FLUSH_END_OF_FRAME = (1 << 0)
};

/**
 * Flags for pipe_context::memory_barrier.
 */
#define PIPE_BARRIER_MAPPED_BUFFER     (1 << 0)
#define PIPE_BARRIER_SHADER_BUFFER     (1 << 1)
#define PIPE_BARRIER_QUERY_BUFFER      (1 << 2)
#define PIPE_BARRIER_VERTEX_BUFFER     (1 << 3)
#define PIPE_BARRIER_INDEX_BUFFER      (1 << 4)
#define PIPE_BARRIER_CONSTANT_BUFFER   (1 << 5)
#define PIPE_BARRIER_INDIRECT_BUFFER   (1 << 6)
#define PIPE_BARRIER_TEXTURE           (1 << 7)
#define PIPE_BARRIER_IMAGE             (1 << 8)
#define PIPE_BARRIER_FRAMEBUFFER       (1 << 9)
#define PIPE_BARRIER_STREAMOUT_BUFFER  (1 << 10)
#define PIPE_BARRIER_GLOBAL_BUFFER     (1 << 11)
#define PIPE_BARRIER_ALL               ((1 << 12) - 1)

/**
 * Flags for pipe_context::texture_barrier.
 */
#define PIPE_TEXTURE_BARRIER_SAMPLER      (1 << 0)
#define PIPE_TEXTURE_BARRIER_FRAMEBUFFER  (1 << 1)

/*
 * Resource binding flags -- state tracker must specify in advance all
 * the ways a resource might be used.
 */
#define PIPE_BIND_DEPTH_STENCIL        (1 << 0) /* create_surface */
#define PIPE_BIND_RENDER_TARGET        (1 << 1) /* create_surface */
#define PIPE_BIND_BLENDABLE            (1 << 2) /* create_surface */
#define PIPE_BIND_SAMPLER_VIEW         (1 << 3) /* create_sampler_view */
#define PIPE_BIND_VERTEX_BUFFER        (1 << 4) /* set_vertex_buffers */
#define PIPE_BIND_INDEX_BUFFER         (1 << 5) /* draw_elements */
#define PIPE_BIND_CONSTANT_BUFFER      (1 << 6) /* set_constant_buffer */
#define PIPE_BIND_DISPLAY_TARGET       (1 << 8) /* flush_front_buffer */
#define PIPE_BIND_TRANSFER_WRITE       (1 << 9) /* transfer_map */
#define PIPE_BIND_TRANSFER_READ        (1 << 10) /* transfer_map */
#define PIPE_BIND_STREAM_OUTPUT        (1 << 11) /* set_stream_output_buffers */
#define PIPE_BIND_CURSOR               (1 << 16) /* mouse cursor */
#define PIPE_BIND_CUSTOM               (1 << 17) /* state-tracker/winsys usages */
#define PIPE_BIND_GLOBAL               (1 << 18) /* set_global_binding */
#define PIPE_BIND_SHADER_RESOURCE      (1 << 19) /* set_shader_resources */
#define PIPE_BIND_COMPUTE_RESOURCE     (1 << 20) /* set_compute_resources */
#define PIPE_BIND_COMMAND_ARGS_BUFFER  (1 << 21) /* pipe_draw_info.indirect */
#define PIPE_BIND_QUERY_BUFFER         (1 << 22) /* get_query_result_resource */

/* The first two flags above were previously part of the amorphous
 * TEXTURE_USAGE, most of which are now descriptions of the ways a
 * particular texture can be bound to the gallium pipeline.  The two flags
 * below do not fit within that and probably need to be migrated to some
 * other place.
 *
 * It seems like scanout is used by the Xorg state tracker to ask for
 * a texture suitable for actual scanout (hence the name), which
 * implies extra layout constraints on some hardware.  It may also
 * have some special meaning regarding mouse cursor images.
 *
 * The shared flag is quite underspecified, but certainly isn't a
 * binding flag - it seems more like a message to the winsys to create
 * a shareable allocation.
 * 
 * The third flag has been added to be able to force textures to be created
 * in linear mode (no tiling).
 */
#define PIPE_BIND_SCANOUT     (1 << 14) /*  */
#define PIPE_BIND_SHARED      (1 << 15) /* get_texture_handle ??? */
#define PIPE_BIND_LINEAR      (1 << 21)


/* Flags for the driver about resource behaviour:
 */
#define PIPE_RESOURCE_FLAG_MAP_PERSISTENT (1 << 0)
#define PIPE_RESOURCE_FLAG_MAP_COHERENT   (1 << 1)
#define PIPE_RESOURCE_FLAG_DRV_PRIV    (1 << 16) /* driver/winsys private */
#define PIPE_RESOURCE_FLAG_ST_PRIV     (1 << 24) /* state-tracker/winsys private */

/* Hint about the expected lifecycle of a resource.
 * Sorted according to GPU vs CPU access.
 */
#define PIPE_USAGE_DEFAULT        0 /* fast GPU access */
#define PIPE_USAGE_IMMUTABLE      1 /* fast GPU access, immutable */
#define PIPE_USAGE_DYNAMIC        2 /* uploaded data is used multiple times */
#define PIPE_USAGE_STREAM         3 /* uploaded data is used once */
#define PIPE_USAGE_STAGING        4 /* fast CPU access */


/**
 * Shaders
 */
#define PIPE_SHADER_VERTEX   0
#define PIPE_SHADER_FRAGMENT 1
#define PIPE_SHADER_GEOMETRY 2
#define PIPE_SHADER_TESS_CTRL 3
#define PIPE_SHADER_TESS_EVAL 4
#define PIPE_SHADER_COMPUTE  5
#define PIPE_SHADER_TYPES    6


/**
 * Primitive types:
 */
#define PIPE_PRIM_POINTS                    0
#define PIPE_PRIM_LINES                     1
#define PIPE_PRIM_LINE_LOOP                 2
#define PIPE_PRIM_LINE_STRIP                3
#define PIPE_PRIM_TRIANGLES                 4
#define PIPE_PRIM_TRIANGLE_STRIP            5
#define PIPE_PRIM_TRIANGLE_FAN              6
#define PIPE_PRIM_QUADS                     7
#define PIPE_PRIM_QUAD_STRIP                8
#define PIPE_PRIM_POLYGON                   9
#define PIPE_PRIM_LINES_ADJACENCY          10
#define PIPE_PRIM_LINE_STRIP_ADJACENCY     11
#define PIPE_PRIM_TRIANGLES_ADJACENCY      12
#define PIPE_PRIM_TRIANGLE_STRIP_ADJACENCY 13
#define PIPE_PRIM_PATCHES                  14
#define PIPE_PRIM_MAX                      15


/**
 * Tessellator spacing types
 */
#define PIPE_TESS_SPACING_FRACTIONAL_ODD    0
#define PIPE_TESS_SPACING_FRACTIONAL_EVEN   1
#define PIPE_TESS_SPACING_EQUAL             2

/**
 * Query object types
 */
#define PIPE_QUERY_OCCLUSION_COUNTER     0
#define PIPE_QUERY_OCCLUSION_PREDICATE   1
#define PIPE_QUERY_TIMESTAMP             2
#define PIPE_QUERY_TIMESTAMP_DISJOINT    3
#define PIPE_QUERY_TIME_ELAPSED          4
#define PIPE_QUERY_PRIMITIVES_GENERATED  5
#define PIPE_QUERY_PRIMITIVES_EMITTED    6
#define PIPE_QUERY_SO_STATISTICS         7
#define PIPE_QUERY_SO_OVERFLOW_PREDICATE 8
#define PIPE_QUERY_GPU_FINISHED          9
#define PIPE_QUERY_PIPELINE_STATISTICS  10
#define PIPE_QUERY_OCCLUSION_PREDICATE_CONSERVATIVE 11
#define PIPE_QUERY_SO_OVERFLOW_ANY_PREDICATE 12
#define PIPE_QUERY_TYPES                13

/* start of driver queries,
 * see pipe_screen::get_driver_query_info */
#define PIPE_QUERY_DRIVER_SPECIFIC     256


/**
 * Conditional rendering modes
 */
#define PIPE_RENDER_COND_WAIT              0
#define PIPE_RENDER_COND_NO_WAIT           1
#define PIPE_RENDER_COND_BY_REGION_WAIT    2
#define PIPE_RENDER_COND_BY_REGION_NO_WAIT 3


/**
 * Point sprite coord modes
 */
#define PIPE_SPRITE_COORD_UPPER_LEFT 0
#define PIPE_SPRITE_COORD_LOWER_LEFT 1


/**
 * Texture swizzles
 */
#define PIPE_SWIZZLE_RED   0
#define PIPE_SWIZZLE_GREEN 1
#define PIPE_SWIZZLE_BLUE  2
#define PIPE_SWIZZLE_ALPHA 3
#define PIPE_SWIZZLE_ZERO  4
#define PIPE_SWIZZLE_ONE   5


#define PIPE_TIMEOUT_INFINITE 0xffffffffffffffffull

/**
 * pipe_image_view access flags.
 */
#define PIPE_IMAGE_ACCESS_READ       (1 << 0)
#define PIPE_IMAGE_ACCESS_WRITE      (1 << 1)
#define PIPE_IMAGE_ACCESS_READ_WRITE (PIPE_IMAGE_ACCESS_READ | \
                                      PIPE_IMAGE_ACCESS_WRITE)

/**
 * Implementation capabilities/limits which are queried through
 * pipe_screen::get_param()
 */
enum pipe_cap {
   PIPE_CAP_NPOT_TEXTURES = 1,
   PIPE_CAP_TWO_SIDED_STENCIL = 2,
   PIPE_CAP_MAX_DUAL_SOURCE_RENDER_TARGETS = 4,
   PIPE_CAP_ANISOTROPIC_FILTER = 5,
   PIPE_CAP_POINT_SPRITE = 6,
   PIPE_CAP_MAX_RENDER_TARGETS = 7,
   PIPE_CAP_OCCLUSION_QUERY = 8,
   PIPE_CAP_QUERY_TIME_ELAPSED = 9,
   PIPE_CAP_TEXTURE_SHADOW_MAP = 10,
   PIPE_CAP_TEXTURE_SWIZZLE = 11,
   PIPE_CAP_MAX_TEXTURE_2D_LEVELS = 12,
   PIPE_CAP_MAX_TEXTURE_3D_LEVELS = 13,
   PIPE_CAP_MAX_TEXTURE_CUBE_LEVELS = 14,
   PIPE_CAP_TEXTURE_MIRROR_CLAMP = 25,
   PIPE_CAP_BLEND_EQUATION_SEPARATE = 28,
   PIPE_CAP_SM3 = 29,  /*< Shader Model, supported */
   PIPE_CAP_MAX_STREAM_OUTPUT_BUFFERS = 30,
   PIPE_CAP_PRIMITIVE_RESTART = 31,
   /** blend enables and write masks per rendertarget */
   PIPE_CAP_INDEP_BLEND_ENABLE = 33,
   /** different blend funcs per rendertarget */
   PIPE_CAP_INDEP_BLEND_FUNC = 34,
   PIPE_CAP_MAX_TEXTURE_ARRAY_LAYERS = 36,
   PIPE_CAP_TGSI_FS_COORD_ORIGIN_UPPER_LEFT = 37,
   PIPE_CAP_TGSI_FS_COORD_ORIGIN_LOWER_LEFT = 38,
   PIPE_CAP_TGSI_FS_COORD_PIXEL_CENTER_HALF_INTEGER = 39,
   PIPE_CAP_TGSI_FS_COORD_PIXEL_CENTER_INTEGER = 40,
   PIPE_CAP_DEPTH_CLIP_DISABLE = 41,
   PIPE_CAP_SHADER_STENCIL_EXPORT = 42,
   PIPE_CAP_TGSI_INSTANCEID = 43,
   PIPE_CAP_VERTEX_ELEMENT_INSTANCE_DIVISOR = 44,
   PIPE_CAP_FRAGMENT_COLOR_CLAMPED = 45,
   PIPE_CAP_MIXED_COLORBUFFER_FORMATS = 46,
   PIPE_CAP_SEAMLESS_CUBE_MAP = 47,
   PIPE_CAP_SEAMLESS_CUBE_MAP_PER_TEXTURE = 48,
   PIPE_CAP_MIN_TEXEL_OFFSET = 50,
   PIPE_CAP_MAX_TEXEL_OFFSET = 51,
   PIPE_CAP_CONDITIONAL_RENDER = 52,
   PIPE_CAP_TEXTURE_BARRIER = 53,
   PIPE_CAP_MAX_STREAM_OUTPUT_SEPARATE_COMPONENTS = 55,
   PIPE_CAP_MAX_STREAM_OUTPUT_INTERLEAVED_COMPONENTS = 56,
   PIPE_CAP_STREAM_OUTPUT_PAUSE_RESUME = 57,
   PIPE_CAP_TGSI_CAN_COMPACT_CONSTANTS = 59, /* temporary */
   PIPE_CAP_VERTEX_COLOR_UNCLAMPED = 60,
   PIPE_CAP_VERTEX_COLOR_CLAMPED = 61,
   PIPE_CAP_GLSL_FEATURE_LEVEL = 62,
   PIPE_CAP_QUADS_FOLLOW_PROVOKING_VERTEX_CONVENTION = 63,
   PIPE_CAP_USER_VERTEX_BUFFERS = 64,
   PIPE_CAP_VERTEX_BUFFER_OFFSET_4BYTE_ALIGNED_ONLY = 65,
   PIPE_CAP_VERTEX_BUFFER_STRIDE_4BYTE_ALIGNED_ONLY = 66,
   PIPE_CAP_VERTEX_ELEMENT_SRC_OFFSET_4BYTE_ALIGNED_ONLY = 67,
   PIPE_CAP_COMPUTE = 68,
   PIPE_CAP_USER_INDEX_BUFFERS = 69,
   PIPE_CAP_USER_CONSTANT_BUFFERS = 70,
   PIPE_CAP_CONSTANT_BUFFER_OFFSET_ALIGNMENT = 71,
   PIPE_CAP_START_INSTANCE = 72,
   PIPE_CAP_QUERY_TIMESTAMP = 73,
   PIPE_CAP_TEXTURE_MULTISAMPLE = 74,
   PIPE_CAP_MIN_MAP_BUFFER_ALIGNMENT = 75,
   PIPE_CAP_CUBE_MAP_ARRAY = 76,
   PIPE_CAP_TEXTURE_BUFFER_OBJECTS = 77,
   PIPE_CAP_TEXTURE_BUFFER_OFFSET_ALIGNMENT = 78,
   PIPE_CAP_TGSI_TEXCOORD = 79,
   PIPE_CAP_PREFER_BLIT_BASED_TEXTURE_TRANSFER = 80,
   PIPE_CAP_QUERY_PIPELINE_STATISTICS = 81,
   PIPE_CAP_TEXTURE_BORDER_COLOR_QUIRK = 82,
   PIPE_CAP_MAX_TEXTURE_BUFFER_SIZE = 83,
   PIPE_CAP_MAX_VIEWPORTS = 84,
   PIPE_CAP_ENDIANNESS = 85,
   PIPE_CAP_MIXED_FRAMEBUFFER_SIZES = 86,
   PIPE_CAP_TGSI_VS_LAYER_VIEWPORT = 87,
   PIPE_CAP_MAX_GEOMETRY_OUTPUT_VERTICES = 88,
   PIPE_CAP_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS = 89,
   PIPE_CAP_MAX_TEXTURE_GATHER_COMPONENTS = 90,
   PIPE_CAP_TEXTURE_GATHER_SM5 = 91,
   PIPE_CAP_BUFFER_MAP_PERSISTENT_COHERENT = 92,
   PIPE_CAP_FAKE_SW_MSAA = 93,
   PIPE_CAP_TEXTURE_QUERY_LOD = 94,
   PIPE_CAP_MIN_TEXTURE_GATHER_OFFSET = 95,
   PIPE_CAP_MAX_TEXTURE_GATHER_OFFSET = 96,
   PIPE_CAP_SAMPLE_SHADING = 97,
   PIPE_CAP_TEXTURE_GATHER_OFFSETS = 98,
   PIPE_CAP_TGSI_VS_WINDOW_SPACE_POSITION = 99,
   PIPE_CAP_MAX_VERTEX_STREAMS = 100,
   PIPE_CAP_DRAW_INDIRECT = 101,
   PIPE_CAP_TGSI_FS_FINE_DERIVATIVE = 102,
   PIPE_CAP_VENDOR_ID = 103,
   PIPE_CAP_DEVICE_ID = 104,
   PIPE_CAP_ACCELERATED = 105,
   PIPE_CAP_VIDEO_MEMORY = 106,
   PIPE_CAP_UMA = 107,
   PIPE_CAP_CONDITIONAL_RENDER_INVERTED = 108,
   PIPE_CAP_MAX_VERTEX_ATTRIB_STRIDE = 109,
   PIPE_CAP_SAMPLER_VIEW_TARGET = 110,
   PIPE_CAP_CLIP_HALFZ = 111,
   PIPE_CAP_VERTEXID_NOBASE = 112,
   PIPE_CAP_POLYGON_OFFSET_CLAMP = 113,
};

#define PIPE_QUIRK_TEXTURE_BORDER_COLOR_SWIZZLE_NV50 (1 << 0)
#define PIPE_QUIRK_TEXTURE_BORDER_COLOR_SWIZZLE_R600 (1 << 1)

enum pipe_endian {
   PIPE_ENDIAN_LITTLE = 0,
   PIPE_ENDIAN_BIG = 1,
#if defined(PIPE_ARCH_LITTLE_ENDIAN)
   PIPE_ENDIAN_NATIVE = PIPE_ENDIAN_LITTLE
#elif defined(PIPE_ARCH_BIG_ENDIAN)
   PIPE_ENDIAN_NATIVE = PIPE_ENDIAN_BIG
#endif
};

/**
 * Implementation limits which are queried through
 * pipe_screen::get_paramf()
 */
enum pipe_capf
{
   PIPE_CAPF_MAX_LINE_WIDTH,
   PIPE_CAPF_MAX_LINE_WIDTH_AA,
   PIPE_CAPF_MAX_POINT_WIDTH,
   PIPE_CAPF_MAX_POINT_WIDTH_AA,
   PIPE_CAPF_MAX_TEXTURE_ANISOTROPY,
   PIPE_CAPF_MAX_TEXTURE_LOD_BIAS,
   PIPE_CAPF_GUARD_BAND_LEFT,
   PIPE_CAPF_GUARD_BAND_TOP,
   PIPE_CAPF_GUARD_BAND_RIGHT,
   PIPE_CAPF_GUARD_BAND_BOTTOM
};

/* Shader caps not specific to any single stage */
enum pipe_shader_cap
{
   PIPE_SHADER_CAP_MAX_INSTRUCTIONS, /* if 0, it means the stage is unsupported */
   PIPE_SHADER_CAP_MAX_ALU_INSTRUCTIONS,
   PIPE_SHADER_CAP_MAX_TEX_INSTRUCTIONS,
   PIPE_SHADER_CAP_MAX_TEX_INDIRECTIONS,
   PIPE_SHADER_CAP_MAX_CONTROL_FLOW_DEPTH,
   PIPE_SHADER_CAP_MAX_INPUTS,
   PIPE_SHADER_CAP_MAX_OUTPUTS,
   PIPE_SHADER_CAP_MAX_CONST_BUFFER_SIZE,
   PIPE_SHADER_CAP_MAX_CONST_BUFFERS,
   PIPE_SHADER_CAP_MAX_TEMPS,
   PIPE_SHADER_CAP_MAX_PREDS,
   /* boolean caps */
   PIPE_SHADER_CAP_TGSI_CONT_SUPPORTED,
   PIPE_SHADER_CAP_INDIRECT_INPUT_ADDR,
   PIPE_SHADER_CAP_INDIRECT_OUTPUT_ADDR,
   PIPE_SHADER_CAP_INDIRECT_TEMP_ADDR,
   PIPE_SHADER_CAP_INDIRECT_CONST_ADDR,
   PIPE_SHADER_CAP_SUBROUTINES, /* BGNSUB, ENDSUB, CAL, RET */
   PIPE_SHADER_CAP_INTEGERS,
   PIPE_SHADER_CAP_MAX_TEXTURE_SAMPLERS,
   PIPE_SHADER_CAP_PREFERRED_IR,
   PIPE_SHADER_CAP_TGSI_SQRT_SUPPORTED,
   PIPE_SHADER_CAP_MAX_SAMPLER_VIEWS,
   PIPE_SHADER_CAP_DOUBLES
};

/**
 * Shader intermediate representation.
 */
enum pipe_shader_ir
{
   PIPE_SHADER_IR_TGSI,
   PIPE_SHADER_IR_LLVM,
   PIPE_SHADER_IR_NATIVE
};

/**
 * Compute-specific implementation capability.  They can be queried
 * using pipe_screen::get_compute_param.
 */
enum pipe_compute_cap
{
   PIPE_COMPUTE_CAP_IR_TARGET,
   PIPE_COMPUTE_CAP_GRID_DIMENSION,
   PIPE_COMPUTE_CAP_MAX_GRID_SIZE,
   PIPE_COMPUTE_CAP_MAX_BLOCK_SIZE,
   PIPE_COMPUTE_CAP_MAX_THREADS_PER_BLOCK,
   PIPE_COMPUTE_CAP_MAX_GLOBAL_SIZE,
   PIPE_COMPUTE_CAP_MAX_LOCAL_SIZE,
   PIPE_COMPUTE_CAP_MAX_PRIVATE_SIZE,
   PIPE_COMPUTE_CAP_MAX_INPUT_SIZE,
   PIPE_COMPUTE_CAP_MAX_MEM_ALLOC_SIZE,
   PIPE_COMPUTE_CAP_MAX_CLOCK_FREQUENCY,
   PIPE_COMPUTE_CAP_MAX_COMPUTE_UNITS,
   PIPE_COMPUTE_CAP_IMAGES_SUPPORTED
};

/**
 * Composite query types
 */

/**
 * Query result for PIPE_QUERY_SO_STATISTICS.
 */
struct pipe_query_data_so_statistics
{
   uint64_t num_primitives_written;
   uint64_t primitives_storage_needed;
};

/**
 * Query result for PIPE_QUERY_TIMESTAMP_DISJOINT.
 */
struct pipe_query_data_timestamp_disjoint
{
   uint64_t frequency;
   boolean  disjoint;
};

/**
 * Query result for PIPE_QUERY_PIPELINE_STATISTICS.
 */
struct pipe_query_data_pipeline_statistics
{
   uint64_t ia_vertices;    /**< Num vertices read by the vertex fetcher. */
   uint64_t ia_primitives;  /**< Num primitives read by the vertex fetcher. */
   uint64_t vs_invocations; /**< Num vertex shader invocations. */
   uint64_t gs_invocations; /**< Num geometry shader invocations. */
   uint64_t gs_primitives;  /**< Num primitives output by a geometry shader. */
   uint64_t c_invocations;  /**< Num primitives sent to the rasterizer. */
   uint64_t c_primitives;   /**< Num primitives that were rendered. */
   uint64_t ps_invocations; /**< Num pixel shader invocations. */
   uint64_t hs_invocations; /**< Num hull shader invocations. */
   uint64_t ds_invocations; /**< Num domain shader invocations. */
   uint64_t cs_invocations; /**< Num compute shader invocations. */
};

/**
 * Query result (returned by pipe_context::get_query_result).
 */
union pipe_query_result
{
   /* PIPE_QUERY_OCCLUSION_PREDICATE */
   /* PIPE_QUERY_SO_OVERFLOW_PREDICATE */
   /* PIPE_QUERY_GPU_FINISHED */
   boolean b;

   /* PIPE_QUERY_OCCLUSION_COUNTER */
   /* PIPE_QUERY_TIMESTAMP */
   /* PIPE_QUERY_TIME_ELAPSED */
   /* PIPE_QUERY_PRIMITIVES_GENERATED */
   /* PIPE_QUERY_PRIMITIVES_EMITTED */
   uint64_t u64;

   /* PIPE_QUERY_SO_STATISTICS */
   struct pipe_query_data_so_statistics so_statistics;

   /* PIPE_QUERY_TIMESTAMP_DISJOINT */
   struct pipe_query_data_timestamp_disjoint timestamp_disjoint;

   /* PIPE_QUERY_PIPELINE_STATISTICS */
   struct pipe_query_data_pipeline_statistics pipeline_statistics;
};

enum pipe_query_value_type
{
   PIPE_QUERY_TYPE_I32,
   PIPE_QUERY_TYPE_U32,
   PIPE_QUERY_TYPE_I64,
   PIPE_QUERY_TYPE_U64,
};

union pipe_color_union
{
   float f[4];
   int i[4];
   unsigned int ui[4];
};

struct pipe_driver_query_info
{
   const char *name;
   unsigned query_type; /* PIPE_QUERY_DRIVER_SPECIFIC + i */
   uint64_t max_value; /* max value that can be returned */
   boolean uses_byte_units; /* whether the result is in bytes */
};

#ifdef __cplusplus
}
#endif

#endif
