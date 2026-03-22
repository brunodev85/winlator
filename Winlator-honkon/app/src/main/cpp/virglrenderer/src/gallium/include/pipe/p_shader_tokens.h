/**************************************************************************
 * 
 * Copyright 2008 VMware, Inc.
 * Copyright 2009-2010 VMware, Inc.
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

#ifndef P_SHADER_TOKENS_H
#define P_SHADER_TOKENS_H

#ifdef __cplusplus
extern "C" {
#endif


struct tgsi_header
{
   unsigned HeaderSize : 8;
   unsigned BodySize   : 24;
};

#define TGSI_PROCESSOR_FRAGMENT  0
#define TGSI_PROCESSOR_VERTEX    1
#define TGSI_PROCESSOR_GEOMETRY  2
#define TGSI_PROCESSOR_TESS_CTRL 3
#define TGSI_PROCESSOR_TESS_EVAL 4
#define TGSI_PROCESSOR_COMPUTE   5

struct tgsi_processor
{
   unsigned Processor  : 4;  /* TGSI_PROCESSOR_ */
   unsigned Padding    : 28;
};

#define TGSI_TOKEN_TYPE_DECLARATION    0
#define TGSI_TOKEN_TYPE_IMMEDIATE      1
#define TGSI_TOKEN_TYPE_INSTRUCTION    2
#define TGSI_TOKEN_TYPE_PROPERTY       3

struct tgsi_token
{
   unsigned Type       : 4;  /**< TGSI_TOKEN_TYPE_x */
   unsigned NrTokens   : 8;  /**< UINT */
   unsigned Padding    : 20;
};

enum tgsi_file_type {
   TGSI_FILE_NULL                =0,
   TGSI_FILE_CONSTANT            =1,
   TGSI_FILE_INPUT               =2,
   TGSI_FILE_OUTPUT              =3,
   TGSI_FILE_TEMPORARY           =4,
   TGSI_FILE_SAMPLER             =5,
   TGSI_FILE_ADDRESS             =6,
   TGSI_FILE_IMMEDIATE           =7,
   TGSI_FILE_PREDICATE           =8,
   TGSI_FILE_SYSTEM_VALUE        =9,
   TGSI_FILE_IMAGE               =10,
   TGSI_FILE_SAMPLER_VIEW        =11,
   TGSI_FILE_BUFFER,
   TGSI_FILE_MEMORY,
   TGSI_FILE_HW_ATOMIC,
   TGSI_FILE_COUNT      /**< how many TGSI_FILE_ types */
};


#define TGSI_WRITEMASK_NONE     0x00
#define TGSI_WRITEMASK_X        0x01
#define TGSI_WRITEMASK_Y        0x02
#define TGSI_WRITEMASK_XY       0x03
#define TGSI_WRITEMASK_Z        0x04
#define TGSI_WRITEMASK_XZ       0x05
#define TGSI_WRITEMASK_YZ       0x06
#define TGSI_WRITEMASK_XYZ      0x07
#define TGSI_WRITEMASK_W        0x08
#define TGSI_WRITEMASK_XW       0x09
#define TGSI_WRITEMASK_YW       0x0A
#define TGSI_WRITEMASK_XYW      0x0B
#define TGSI_WRITEMASK_ZW       0x0C
#define TGSI_WRITEMASK_XZW      0x0D
#define TGSI_WRITEMASK_YZW      0x0E
#define TGSI_WRITEMASK_XYZW     0x0F

#define TGSI_INTERPOLATE_CONSTANT      0
#define TGSI_INTERPOLATE_LINEAR        1
#define TGSI_INTERPOLATE_PERSPECTIVE   2
#define TGSI_INTERPOLATE_COLOR         3 /* special color case for smooth/flat */
#define TGSI_INTERPOLATE_COUNT         4

#define TGSI_INTERPOLATE_LOC_CENTER    0
#define TGSI_INTERPOLATE_LOC_CENTROID  1
#define TGSI_INTERPOLATE_LOC_SAMPLE    2
#define TGSI_INTERPOLATE_LOC_COUNT     3

#define TGSI_CYLINDRICAL_WRAP_X (1 << 0)
#define TGSI_CYLINDRICAL_WRAP_Y (1 << 1)
#define TGSI_CYLINDRICAL_WRAP_Z (1 << 2)
#define TGSI_CYLINDRICAL_WRAP_W (1 << 3)

enum tgsi_memory_type {
   TGSI_MEMORY_TYPE_GLOBAL,         /* OpenCL global              */
   TGSI_MEMORY_TYPE_SHARED,         /* OpenCL local / GLSL shared */
   TGSI_MEMORY_TYPE_PRIVATE,        /* OpenCL private             */
   TGSI_MEMORY_TYPE_INPUT,          /* OpenCL kernel input params */
   TGSI_MEMORY_TYPE_COUNT,
};

struct tgsi_declaration
{
   unsigned Type        : 4;  /**< TGSI_TOKEN_TYPE_DECLARATION */
   unsigned NrTokens    : 8;  /**< UINT */
   unsigned File        : 4;  /**< one of TGSI_FILE_x */
   unsigned UsageMask   : 4;  /**< bitmask of TGSI_WRITEMASK_x flags */
   unsigned Dimension   : 1;  /**< any extra dimension info? */
   unsigned Semantic    : 1;  /**< BOOL, any semantic info? */
   unsigned Interpolate : 1;  /**< any interpolation info? */
   unsigned Invariant   : 1;  /**< invariant optimization? */
   unsigned Local       : 1;  /**< optimize as subroutine local variable? */
   unsigned Array       : 1;  /**< extra array info? */
   unsigned Atomic      : 1;  /**< atomic only? for TGSI_FILE_BUFFER */
   unsigned MemType     : 2;  /**< TGSI_MEMORY_TYPE_x for TGSI_FILE_MEMORY */
   unsigned Padding     : 3;
};

struct tgsi_declaration_range
{
   unsigned First   : 16; /**< UINT */
   unsigned Last    : 16; /**< UINT */
};

struct tgsi_declaration_dimension
{
   unsigned Index2D:16; /**< UINT */
   unsigned Padding:16;
};

struct tgsi_declaration_interp
{
   unsigned Interpolate : 4;   /**< one of TGSI_INTERPOLATE_x */
   unsigned Location    : 2;   /**< one of TGSI_INTERPOLATE_LOC_x */
   unsigned CylindricalWrap:4; /**< TGSI_CYLINDRICAL_WRAP_x flags */
   unsigned Padding     : 22;
};

#define TGSI_SEMANTIC_POSITION   0
#define TGSI_SEMANTIC_COLOR      1
#define TGSI_SEMANTIC_BCOLOR     2  /**< back-face color */
#define TGSI_SEMANTIC_FOG        3
#define TGSI_SEMANTIC_PSIZE      4
#define TGSI_SEMANTIC_GENERIC    5
#define TGSI_SEMANTIC_NORMAL     6
#define TGSI_SEMANTIC_FACE       7
#define TGSI_SEMANTIC_EDGEFLAG   8
#define TGSI_SEMANTIC_PRIMID     9
#define TGSI_SEMANTIC_INSTANCEID 10 /**< doesn't include start_instance */
#define TGSI_SEMANTIC_VERTEXID   11
#define TGSI_SEMANTIC_STENCIL    12
#define TGSI_SEMANTIC_CLIPDIST   13
#define TGSI_SEMANTIC_CLIPVERTEX 14
#define TGSI_SEMANTIC_GRID_SIZE  15 /**< grid size in blocks */
#define TGSI_SEMANTIC_BLOCK_ID   16 /**< id of the current block */
#define TGSI_SEMANTIC_BLOCK_SIZE 17 /**< block size in threads */
#define TGSI_SEMANTIC_THREAD_ID  18 /**< block-relative id of the current thread */
#define TGSI_SEMANTIC_TEXCOORD   19 /**< texture or sprite coordinates */
#define TGSI_SEMANTIC_PCOORD     20 /**< point sprite coordinate */
#define TGSI_SEMANTIC_VIEWPORT_INDEX 21 /**< viewport index */
#define TGSI_SEMANTIC_LAYER      22 /**< layer (rendertarget index) */
#define TGSI_SEMANTIC_CULLDIST   23
#define TGSI_SEMANTIC_SAMPLEID   24
#define TGSI_SEMANTIC_SAMPLEPOS  25
#define TGSI_SEMANTIC_SAMPLEMASK 26
#define TGSI_SEMANTIC_INVOCATIONID 27
#define TGSI_SEMANTIC_VERTEXID_NOBASE 28
#define TGSI_SEMANTIC_BASEVERTEX 29
#define TGSI_SEMANTIC_PATCH      30 /**< generic per-patch semantic */
#define TGSI_SEMANTIC_TESSCOORD  31 /**< coordinate being processed by tess */
#define TGSI_SEMANTIC_TESSOUTER  32 /**< outer tessellation levels */
#define TGSI_SEMANTIC_TESSINNER  33 /**< inner tessellation levels */
#define TGSI_SEMANTIC_VERTICESIN 34 /**< number of input vertices */
#define TGSI_SEMANTIC_HELPER_INVOCATION 35 /**< current invocation is helper */
#define TGSI_SEMANTIC_COUNT      36 /**< number of semantic values */

struct tgsi_declaration_semantic
{
   unsigned Name           : 8;  /**< one of TGSI_SEMANTIC_x */
   unsigned Index          : 16; /**< UINT */
   unsigned StreamX        : 2; /**< vertex stream (for GS output) */
   unsigned StreamY        : 2;
   unsigned StreamZ        : 2;
   unsigned StreamW        : 2;
};

struct tgsi_declaration_image {
   unsigned Resource    : 8; /**< one of TGSI_TEXTURE_ */
   unsigned Raw         : 1;
   unsigned Writable    : 1;
   unsigned Format      : 10; /**< one of PIPE_FORMAT_ */
   unsigned Padding     : 12;
};

enum tgsi_return_type {
   TGSI_RETURN_TYPE_UNORM = 0,
   TGSI_RETURN_TYPE_SNORM,
   TGSI_RETURN_TYPE_SINT,
   TGSI_RETURN_TYPE_UINT,
   TGSI_RETURN_TYPE_FLOAT,
   TGSI_RETURN_TYPE_COUNT
};

struct tgsi_declaration_sampler_view {
   unsigned Resource    : 8; /**< one of TGSI_TEXTURE_ */
   unsigned ReturnTypeX : 6; /**< one of enum tgsi_return_type */
   unsigned ReturnTypeY : 6; /**< one of enum tgsi_return_type */
   unsigned ReturnTypeZ : 6; /**< one of enum tgsi_return_type */
   unsigned ReturnTypeW : 6; /**< one of enum tgsi_return_type */
};

struct tgsi_declaration_array {
   unsigned ArrayID : 10;
   unsigned Padding : 22;
};

/*
 * Special resources that don't need to be declared.  They map to the
 * GLOBAL/LOCAL/PRIVATE/INPUT compute memory spaces.
 */
#define TGSI_RESOURCE_GLOBAL	0x7fff
#define TGSI_RESOURCE_LOCAL	0x7ffe
#define TGSI_RESOURCE_PRIVATE	0x7ffd
#define TGSI_RESOURCE_INPUT	0x7ffc

#define TGSI_IMM_FLOAT32   0
#define TGSI_IMM_UINT32    1
#define TGSI_IMM_INT32     2
#define TGSI_IMM_FLOAT64   3

struct tgsi_immediate
{
   unsigned Type       : 4;  /**< TGSI_TOKEN_TYPE_IMMEDIATE */
   unsigned NrTokens   : 14; /**< UINT */
   unsigned DataType   : 4;  /**< one of TGSI_IMM_x */
   unsigned Padding    : 10;
};

union tgsi_immediate_data
{
   float Float;
   unsigned Uint;
   int Int;
};

#define TGSI_PROPERTY_GS_INPUT_PRIM          0
#define TGSI_PROPERTY_GS_OUTPUT_PRIM         1
#define TGSI_PROPERTY_GS_MAX_OUTPUT_VERTICES 2
#define TGSI_PROPERTY_FS_COORD_ORIGIN        3
#define TGSI_PROPERTY_FS_COORD_PIXEL_CENTER  4
#define TGSI_PROPERTY_FS_COLOR0_WRITES_ALL_CBUFS 5
#define TGSI_PROPERTY_FS_DEPTH_LAYOUT        6
#define TGSI_PROPERTY_VS_PROHIBIT_UCPS       7
#define TGSI_PROPERTY_GS_INVOCATIONS         8
#define TGSI_PROPERTY_VS_WINDOW_SPACE_POSITION 9
#define TGSI_PROPERTY_TCS_VERTICES_OUT       10
#define TGSI_PROPERTY_TES_PRIM_MODE          11
#define TGSI_PROPERTY_TES_SPACING            12
#define TGSI_PROPERTY_TES_VERTEX_ORDER_CW    13
#define TGSI_PROPERTY_TES_POINT_MODE         14
#define TGSI_PROPERTY_NUM_CLIPDIST_ENABLED   15
#define TGSI_PROPERTY_NUM_CULLDIST_ENABLED   16
#define TGSI_PROPERTY_FS_EARLY_DEPTH_STENCIL 17
#define TGSI_PROPERTY_FS_POST_DEPTH_COVERAGE 18
#define TGSI_PROPERTY_NEXT_SHADER            19
#define TGSI_PROPERTY_CS_FIXED_BLOCK_WIDTH   20
#define TGSI_PROPERTY_CS_FIXED_BLOCK_HEIGHT  21
#define TGSI_PROPERTY_CS_FIXED_BLOCK_DEPTH   22
#define TGSI_PROPERTY_MUL_ZERO_WINS          23
#define TGSI_PROPERTY_COUNT                  24

struct tgsi_property {
   unsigned Type         : 4;  /**< TGSI_TOKEN_TYPE_PROPERTY */
   unsigned NrTokens     : 8;  /**< UINT */
   unsigned PropertyName : 8;  /**< one of TGSI_PROPERTY */
   unsigned Padding      : 12;
};

#define TGSI_FS_COORD_ORIGIN_UPPER_LEFT 0
#define TGSI_FS_COORD_ORIGIN_LOWER_LEFT 1

#define TGSI_FS_COORD_PIXEL_CENTER_HALF_INTEGER 0
#define TGSI_FS_COORD_PIXEL_CENTER_INTEGER 1

#define TGSI_FS_DEPTH_LAYOUT_NONE         0
#define TGSI_FS_DEPTH_LAYOUT_ANY          1
#define TGSI_FS_DEPTH_LAYOUT_GREATER      2
#define TGSI_FS_DEPTH_LAYOUT_LESS         3
#define TGSI_FS_DEPTH_LAYOUT_UNCHANGED    4


struct tgsi_property_data {
   unsigned Data;
};

/* TGSI opcodes.  
 * 
 * For more information on semantics of opcodes and
 * which APIs are known to use which opcodes, see
 * gallium/docs/source/tgsi.rst
 */
/* VIRGLRENDERER specific - DON'T SYNC WITH MESA
 * OR REMOVE OPCODES - FILL in and REWRITE tgsi_info
 * accordingly.
 */
#define TGSI_OPCODE_ARL                 0
#define TGSI_OPCODE_MOV                 1
#define TGSI_OPCODE_LIT                 2
#define TGSI_OPCODE_RCP                 3
#define TGSI_OPCODE_RSQ                 4
#define TGSI_OPCODE_EXP                 5
#define TGSI_OPCODE_LOG                 6
#define TGSI_OPCODE_MUL                 7
#define TGSI_OPCODE_ADD                 8
#define TGSI_OPCODE_DP3                 9
#define TGSI_OPCODE_DP4                 10
#define TGSI_OPCODE_DST                 11
#define TGSI_OPCODE_MIN                 12
#define TGSI_OPCODE_MAX                 13
#define TGSI_OPCODE_SLT                 14
#define TGSI_OPCODE_SGE                 15
#define TGSI_OPCODE_MAD                 16
#define TGSI_OPCODE_SUB                 17
#define TGSI_OPCODE_LRP                 18
#define TGSI_OPCODE_FMA                 19
#define TGSI_OPCODE_SQRT                20
                                /* gap */
#define TGSI_OPCODE_FRC                 24
                                /* gap */
#define TGSI_OPCODE_FLR                 26
#define TGSI_OPCODE_ROUND               27
#define TGSI_OPCODE_EX2                 28
#define TGSI_OPCODE_LG2                 29
#define TGSI_OPCODE_POW                 30
#define TGSI_OPCODE_XPD                 31
                                /* gap */
#define TGSI_OPCODE_ABS                 33
                                /* gap */
#define TGSI_OPCODE_DPH                 35
#define TGSI_OPCODE_COS                 36
#define TGSI_OPCODE_DDX                 37
#define TGSI_OPCODE_DDY                 38
#define TGSI_OPCODE_KILL                39 /* unconditional */
#define TGSI_OPCODE_PK2H                40
#define TGSI_OPCODE_PK2US               41
#define TGSI_OPCODE_PK4B                42
#define TGSI_OPCODE_PK4UB               43
                                /* gap */
#define TGSI_OPCODE_SEQ                 45
                                /* gap */
#define TGSI_OPCODE_SGT                 47
#define TGSI_OPCODE_SIN                 48
#define TGSI_OPCODE_SLE                 49
#define TGSI_OPCODE_SNE                 50
                                /* gap */
#define TGSI_OPCODE_TEX                 52
#define TGSI_OPCODE_TXD                 53
#define TGSI_OPCODE_TXP                 54
#define TGSI_OPCODE_UP2H                55
#define TGSI_OPCODE_UP2US               56
#define TGSI_OPCODE_UP4B                57
#define TGSI_OPCODE_UP4UB               58
                                /* gap */
#define TGSI_OPCODE_ARR                 61
                                /* gap */
#define TGSI_OPCODE_CAL                 63
#define TGSI_OPCODE_RET                 64
#define TGSI_OPCODE_SSG                 65 /* SGN */
#define TGSI_OPCODE_CMP                 66
#define TGSI_OPCODE_SCS                 67
#define TGSI_OPCODE_TXB                 68
#define TGSI_OPCODE_FBFETCH             69
#define TGSI_OPCODE_DIV                 70
#define TGSI_OPCODE_DP2                 71
#define TGSI_OPCODE_TXL                 72
#define TGSI_OPCODE_BRK                 73
#define TGSI_OPCODE_IF                  74
#define TGSI_OPCODE_UIF                 75
#define TGSI_OPCODE_ELSE                77
#define TGSI_OPCODE_ENDIF               78

#define TGSI_OPCODE_DDX_FINE            79
#define TGSI_OPCODE_DDY_FINE            80
                                /* gap */
#define TGSI_OPCODE_CEIL                83
#define TGSI_OPCODE_I2F                 84
#define TGSI_OPCODE_NOT                 85
#define TGSI_OPCODE_TRUNC               86
#define TGSI_OPCODE_SHL                 87
                                /* gap */
#define TGSI_OPCODE_AND                 89
#define TGSI_OPCODE_OR                  90
#define TGSI_OPCODE_MOD                 91
#define TGSI_OPCODE_XOR                 92
                                /* gap */
#define TGSI_OPCODE_TXF                 94
#define TGSI_OPCODE_TXQ                 95
#define TGSI_OPCODE_CONT                96
#define TGSI_OPCODE_EMIT                97
#define TGSI_OPCODE_ENDPRIM             98
#define TGSI_OPCODE_BGNLOOP             99
#define TGSI_OPCODE_BGNSUB              100
#define TGSI_OPCODE_ENDLOOP             101
#define TGSI_OPCODE_ENDSUB              102
                                /* gap */
#define TGSI_OPCODE_TXQS                104
#define TGSI_OPCODE_RESQ                105
                                /* gap */
#define TGSI_OPCODE_NOP                 107

#define TGSI_OPCODE_FSEQ                108
#define TGSI_OPCODE_FSGE                109
#define TGSI_OPCODE_FSLT                110
#define TGSI_OPCODE_FSNE                111

#define TGSI_OPCODE_MEMBAR              112
                                /* gap */
#define TGSI_OPCODE_KILL_IF             116  /* conditional kill */
#define TGSI_OPCODE_END                 117  /* aka HALT */
#define TGSI_OPCODE_DFMA                118
#define TGSI_OPCODE_F2I                 119
#define TGSI_OPCODE_IDIV                120
#define TGSI_OPCODE_IMAX                121
#define TGSI_OPCODE_IMIN                122
#define TGSI_OPCODE_INEG                123
#define TGSI_OPCODE_ISGE                124
#define TGSI_OPCODE_ISHR                125
#define TGSI_OPCODE_ISLT                126
#define TGSI_OPCODE_F2U                 127
#define TGSI_OPCODE_U2F                 128
#define TGSI_OPCODE_UADD                129
#define TGSI_OPCODE_UDIV                130
#define TGSI_OPCODE_UMAD                131
#define TGSI_OPCODE_UMAX                132
#define TGSI_OPCODE_UMIN                133
#define TGSI_OPCODE_UMOD                134
#define TGSI_OPCODE_UMUL                135
#define TGSI_OPCODE_USEQ                136
#define TGSI_OPCODE_USGE                137
#define TGSI_OPCODE_USHR                138
#define TGSI_OPCODE_USLT                139
#define TGSI_OPCODE_USNE                140
#define TGSI_OPCODE_SWITCH              141
#define TGSI_OPCODE_CASE                142
#define TGSI_OPCODE_DEFAULT             143
#define TGSI_OPCODE_ENDSWITCH           144

/* resource related opcodes */
#define TGSI_OPCODE_SAMPLE              145
#define TGSI_OPCODE_SAMPLE_I            146
#define TGSI_OPCODE_SAMPLE_I_MS         147
#define TGSI_OPCODE_SAMPLE_B            148
#define TGSI_OPCODE_SAMPLE_C            149
#define TGSI_OPCODE_SAMPLE_C_LZ         150
#define TGSI_OPCODE_SAMPLE_D            151
#define TGSI_OPCODE_SAMPLE_L            152
#define TGSI_OPCODE_GATHER4             153
#define TGSI_OPCODE_SVIEWINFO           154
#define TGSI_OPCODE_SAMPLE_POS          155
#define TGSI_OPCODE_SAMPLE_INFO         156

#define TGSI_OPCODE_UARL                157
#define TGSI_OPCODE_UCMP                158
#define TGSI_OPCODE_IABS                159
#define TGSI_OPCODE_ISSG                160

#define TGSI_OPCODE_LOAD                161
#define TGSI_OPCODE_STORE               162

                                /* gap */
#define TGSI_OPCODE_BARRIER             166

#define TGSI_OPCODE_ATOMUADD            167
#define TGSI_OPCODE_ATOMXCHG            168
#define TGSI_OPCODE_ATOMCAS             169
#define TGSI_OPCODE_ATOMAND             170
#define TGSI_OPCODE_ATOMOR              171
#define TGSI_OPCODE_ATOMXOR             172
#define TGSI_OPCODE_ATOMUMIN            173
#define TGSI_OPCODE_ATOMUMAX            174
#define TGSI_OPCODE_ATOMIMIN            175
#define TGSI_OPCODE_ATOMIMAX            176

/* to be used for shadow cube map compares */
#define TGSI_OPCODE_TEX2                177
#define TGSI_OPCODE_TXB2                178
#define TGSI_OPCODE_TXL2                179

#define TGSI_OPCODE_IMUL_HI             180
#define TGSI_OPCODE_UMUL_HI             181

#define TGSI_OPCODE_TG4                 182

#define TGSI_OPCODE_LODQ                183

#define TGSI_OPCODE_IBFE                184
#define TGSI_OPCODE_UBFE                185
#define TGSI_OPCODE_BFI                 186
#define TGSI_OPCODE_BREV                187
#define TGSI_OPCODE_POPC                188
#define TGSI_OPCODE_LSB                 189
#define TGSI_OPCODE_IMSB                190
#define TGSI_OPCODE_UMSB                191

#define TGSI_OPCODE_INTERP_CENTROID     192
#define TGSI_OPCODE_INTERP_SAMPLE       193
#define TGSI_OPCODE_INTERP_OFFSET       194

/* sm5 marked opcodes are supported in D3D11 optionally - also DMOV, DMOVC */
#define TGSI_OPCODE_F2D                 195 /* SM5 */
#define TGSI_OPCODE_D2F                 196
#define TGSI_OPCODE_DABS                197
#define TGSI_OPCODE_DNEG                198 /* SM5 */
#define TGSI_OPCODE_DADD                199 /* SM5 */
#define TGSI_OPCODE_DMUL                200 /* SM5 */
#define TGSI_OPCODE_DMAX                201 /* SM5 */
#define TGSI_OPCODE_DMIN                202 /* SM5 */
#define TGSI_OPCODE_DSLT                203 /* SM5 */
#define TGSI_OPCODE_DSGE                204 /* SM5 */
#define TGSI_OPCODE_DSEQ                205 /* SM5 */
#define TGSI_OPCODE_DSNE                206 /* SM5 */
#define TGSI_OPCODE_DRCP                207 /* eg, cayman */
#define TGSI_OPCODE_DSQRT               208 /* eg, cayman also has DRSQ */
#define TGSI_OPCODE_DMAD                209
#define TGSI_OPCODE_DFRAC               210 /* eg, cayman */
#define TGSI_OPCODE_DLDEXP              211 /* eg, cayman */
#define TGSI_OPCODE_DFRACEXP            212 /* eg, cayman */
#define TGSI_OPCODE_D2I                 213
#define TGSI_OPCODE_I2D                 214
#define TGSI_OPCODE_D2U                 215
#define TGSI_OPCODE_U2D                 216
#define TGSI_OPCODE_DRSQ                217 /* eg, cayman also has DRSQ */
#define TGSI_OPCODE_DTRUNC              218 /* nvc0 */
#define TGSI_OPCODE_DCEIL               219 /* nvc0 */
#define TGSI_OPCODE_DFLR                220 /* nvc0 */
#define TGSI_OPCODE_DROUND              221 /* nvc0 */
#define TGSI_OPCODE_DSSG                222
#define TGSI_OPCODE_DDIV                223
#define TGSI_OPCODE_CLOCK               224

/* opcodes for ARB_gpu_shader_int64 */
#define TGSI_OPCODE_I64ABS              225
#define TGSI_OPCODE_I64NEG              226
#define TGSI_OPCODE_I64SSG              227
#define TGSI_OPCODE_I64SLT              228
#define TGSI_OPCODE_I64SGE              229
#define TGSI_OPCODE_I64MIN              230
#define TGSI_OPCODE_I64MAX              231
#define TGSI_OPCODE_I64SHR              232
#define TGSI_OPCODE_I64DIV              233
#define TGSI_OPCODE_I64MOD              234
#define TGSI_OPCODE_F2I64               235
#define TGSI_OPCODE_U2I64               236
#define TGSI_OPCODE_I2I64               237
#define TGSI_OPCODE_D2I64               238
#define TGSI_OPCODE_I642F               239
#define TGSI_OPCODE_I642D               240

#define TGSI_OPCODE_U64ADD              241
#define TGSI_OPCODE_U64MUL              242
#define TGSI_OPCODE_U64SEQ              243
#define TGSI_OPCODE_U64SNE              244
#define TGSI_OPCODE_U64SLT              245
#define TGSI_OPCODE_U64SGE              246
#define TGSI_OPCODE_U64MIN              247
#define TGSI_OPCODE_U64MAX              248
#define TGSI_OPCODE_U64SHL              249
#define TGSI_OPCODE_U64SHR              250
#define TGSI_OPCODE_U64DIV              251
#define TGSI_OPCODE_U64MOD              252
#define TGSI_OPCODE_F2U64               253
#define TGSI_OPCODE_D2U64               254
#define TGSI_OPCODE_U642F               255
#define TGSI_OPCODE_U642D               256

#define TGSI_OPCODE_LAST                257

/**
 * Opcode is the operation code to execute. A given operation defines the
 * semantics how the source registers (if any) are interpreted and what is
 * written to the destination registers (if any) as a result of execution.
 *
 * NumDstRegs and NumSrcRegs is the number of destination and source registers,
 * respectively. For a given operation code, those numbers are fixed and are
 * present here only for convenience.
 *
 * Saturate controls how are final results in destination registers modified.
 */

/*
 * VIRGLRENDERER specific -
 * we no long keep this in sync with mesa, we had to increase the NrTokens
 * as mesa can remove old opcodes, but the renderer cannot.
 */
struct tgsi_instruction
{
   unsigned Type       : 4;  /* TGSI_TOKEN_TYPE_INSTRUCTION */
   unsigned NrTokens   : 9;  /* UINT */
   unsigned Opcode     : 8;  /* TGSI_OPCODE_ */
   unsigned Saturate   : 1;  /* BOOL */
   unsigned NumDstRegs : 2;  /* UINT */
   unsigned NumSrcRegs : 4;  /* UINT */
   unsigned Label      : 1;
   unsigned Texture    : 1;
   unsigned Memory     : 1;
   unsigned Precise    : 1;
};

/*
 * If tgsi_instruction::Label is TRUE, tgsi_instruction_label follows.
 *
 * If tgsi_instruction::Texture is TRUE, tgsi_instruction_texture follows.
 *   if texture instruction has a number of offsets,
 *   then tgsi_instruction::Texture::NumOffset of tgsi_texture_offset follow.
 * 
 * Then, tgsi_instruction::NumDstRegs of tgsi_dst_register follow.
 * 
 * Then, tgsi_instruction::NumSrcRegs of tgsi_src_register follow.
 *
 * tgsi_instruction::NrTokens contains the total number of words that make the
 * instruction, including the instruction word.
 */

#define TGSI_SWIZZLE_X      0
#define TGSI_SWIZZLE_Y      1
#define TGSI_SWIZZLE_Z      2
#define TGSI_SWIZZLE_W      3

struct tgsi_instruction_label
{
   unsigned Label    : 24;   /* UINT */
   unsigned Padding  : 8;
};

#define TGSI_TEXTURE_BUFFER         0
#define TGSI_TEXTURE_1D             1
#define TGSI_TEXTURE_2D             2
#define TGSI_TEXTURE_3D             3
#define TGSI_TEXTURE_CUBE           4
#define TGSI_TEXTURE_RECT           5
#define TGSI_TEXTURE_SHADOW1D       6
#define TGSI_TEXTURE_SHADOW2D       7
#define TGSI_TEXTURE_SHADOWRECT     8
#define TGSI_TEXTURE_1D_ARRAY       9
#define TGSI_TEXTURE_2D_ARRAY       10
#define TGSI_TEXTURE_SHADOW1D_ARRAY 11
#define TGSI_TEXTURE_SHADOW2D_ARRAY 12
#define TGSI_TEXTURE_SHADOWCUBE     13
#define TGSI_TEXTURE_2D_MSAA        14
#define TGSI_TEXTURE_2D_ARRAY_MSAA  15
#define TGSI_TEXTURE_CUBE_ARRAY     16
#define TGSI_TEXTURE_SHADOWCUBE_ARRAY 17
#define TGSI_TEXTURE_UNKNOWN        18
#define TGSI_TEXTURE_COUNT          19

struct tgsi_instruction_texture
{
   unsigned Texture  : 8;    /* TGSI_TEXTURE_ */
   unsigned NumOffsets : 4;
   unsigned Padding : 20;
};

/* for texture offsets in GLSL and DirectX.
 * Generally these always come from TGSI_FILE_IMMEDIATE,
 * however DX11 appears to have the capability to do
 * non-constant texture offsets.
 */
struct tgsi_texture_offset
{
   int      Index    : 16;
   unsigned File     : 4;  /**< one of TGSI_FILE_x */
   unsigned SwizzleX : 2;  /* TGSI_SWIZZLE_x */
   unsigned SwizzleY : 2;  /* TGSI_SWIZZLE_x */
   unsigned SwizzleZ : 2;  /* TGSI_SWIZZLE_x */
   unsigned Padding  : 6;
};

/**
 * File specifies the register array to access.
 *
 * Index specifies the element number of a register in the register file.
 *
 * If Indirect is TRUE, Index should be offset by the X component of the indirect
 * register that follows. The register can be now fetched into local storage
 * for further processing.
 *
 * If Negate is TRUE, all components of the fetched register are negated.
 *
 * The fetched register components are swizzled according to SwizzleX, SwizzleY,
 * SwizzleZ and SwizzleW.
 *
 */

struct tgsi_src_register
{
   unsigned File        : 4;  /* TGSI_FILE_ */
   unsigned Indirect    : 1;  /* BOOL */
   unsigned Dimension   : 1;  /* BOOL */
   int      Index       : 16; /* SINT */
   unsigned SwizzleX    : 2;  /* TGSI_SWIZZLE_ */
   unsigned SwizzleY    : 2;  /* TGSI_SWIZZLE_ */
   unsigned SwizzleZ    : 2;  /* TGSI_SWIZZLE_ */
   unsigned SwizzleW    : 2;  /* TGSI_SWIZZLE_ */
   unsigned Absolute    : 1;    /* BOOL */
   unsigned Negate      : 1;    /* BOOL */
};

/**
 * If tgsi_src_register::Indirect is TRUE, tgsi_ind_register follows.
 *
 * File, Index and Swizzle are handled the same as in tgsi_src_register.
 *
 * If ArrayID is zero the whole register file might be indirectly addressed,
 * if not only the Declaration with this ArrayID is accessed by this operand.
 *
 */

struct tgsi_ind_register
{
   unsigned File    : 4;  /* TGSI_FILE_ */
   int      Index   : 16; /* SINT */
   unsigned Swizzle : 2;  /* TGSI_SWIZZLE_ */
   unsigned ArrayID : 10; /* UINT */
};

/**
 * If tgsi_src_register::Dimension is TRUE, tgsi_dimension follows.
 */

struct tgsi_dimension
{
   unsigned Indirect    : 1;  /* BOOL */
   unsigned Dimension   : 1;  /* BOOL */
   unsigned Padding     : 14;
   int      Index       : 16; /* SINT */
};

struct tgsi_dst_register
{
   unsigned File        : 4;  /* TGSI_FILE_ */
   unsigned WriteMask   : 4;  /* TGSI_WRITEMASK_ */
   unsigned Indirect    : 1;  /* BOOL */
   unsigned Dimension   : 1;  /* BOOL */
   int      Index       : 16; /* SINT */
   unsigned Padding     : 6;
};

#define TGSI_MEMORY_COHERENT (1 << 0)
#define TGSI_MEMORY_RESTRICT (1 << 1)
#define TGSI_MEMORY_VOLATILE (1 << 2)

/**
 * Specifies the type of memory access to do for the LOAD/STORE instruction.
 */
struct tgsi_instruction_memory
{
   unsigned Qualifier : 3;  /* TGSI_MEMORY_ */
   unsigned Texture   : 8;  /* only for images: TGSI_TEXTURE_ */
   unsigned Format    : 10; /* only for images: PIPE_FORMAT_ */
   unsigned Padding   : 11;
};

#define TGSI_MEMBAR_SHADER_BUFFER (1 << 0)
#define TGSI_MEMBAR_ATOMIC_BUFFER (1 << 1)
#define TGSI_MEMBAR_SHADER_IMAGE  (1 << 2)
#define TGSI_MEMBAR_SHARED        (1 << 3)
#define TGSI_MEMBAR_THREAD_GROUP  (1 << 4)

#ifdef __cplusplus
}
#endif

#endif /* P_SHADER_TOKENS_H */
