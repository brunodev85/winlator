/**************************************************************************
 * 
 * Copyright 2008 VMware, Inc.
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

#include "util/u_debug.h"
#include "util/u_memory.h"
#include "tgsi_info.h"

#define NONE TGSI_OUTPUT_NONE
#define COMP TGSI_OUTPUT_COMPONENTWISE
#define REPL TGSI_OUTPUT_REPLICATE
#define CHAN TGSI_OUTPUT_CHAN_DEPENDENT
#define OTHR TGSI_OUTPUT_OTHER

static const struct tgsi_opcode_info opcode_info[TGSI_OPCODE_LAST] =
{
   { 1, 1, 0, 0, 0, 0, COMP, "ARL", TGSI_OPCODE_ARL },
   { 1, 1, 0, 0, 0, 0, COMP, "MOV", TGSI_OPCODE_MOV },
   { 1, 1, 0, 0, 0, 0, CHAN, "LIT", TGSI_OPCODE_LIT },
   { 1, 1, 0, 0, 0, 0, REPL, "RCP", TGSI_OPCODE_RCP },
   { 1, 1, 0, 0, 0, 0, REPL, "RSQ", TGSI_OPCODE_RSQ },
   { 1, 1, 0, 0, 0, 0, CHAN, "EXP", TGSI_OPCODE_EXP },
   { 1, 1, 0, 0, 0, 0, CHAN, "LOG", TGSI_OPCODE_LOG },
   { 1, 2, 0, 0, 0, 0, COMP, "MUL", TGSI_OPCODE_MUL },
   { 1, 2, 0, 0, 0, 0, COMP, "ADD", TGSI_OPCODE_ADD },
   { 1, 2, 0, 0, 0, 0, REPL, "DP3", TGSI_OPCODE_DP3 },
   { 1, 2, 0, 0, 0, 0, REPL, "DP4", TGSI_OPCODE_DP4 },
   { 1, 2, 0, 0, 0, 0, CHAN, "DST", TGSI_OPCODE_DST },
   { 1, 2, 0, 0, 0, 0, COMP, "MIN", TGSI_OPCODE_MIN },
   { 1, 2, 0, 0, 0, 0, COMP, "MAX", TGSI_OPCODE_MAX },
   { 1, 2, 0, 0, 0, 0, COMP, "SLT", TGSI_OPCODE_SLT },
   { 1, 2, 0, 0, 0, 0, COMP, "SGE", TGSI_OPCODE_SGE },
   { 1, 3, 0, 0, 0, 0, COMP, "MAD", TGSI_OPCODE_MAD },
   { 1, 2, 0, 0, 0, 0, COMP, "SUB", TGSI_OPCODE_SUB },
   { 1, 3, 0, 0, 0, 0, COMP, "LRP", TGSI_OPCODE_LRP },
   { 1, 3, 0, 0, 0, 0, COMP, "FMA", TGSI_OPCODE_FMA },
   { 1, 1, 0, 0, 0, 0, REPL, "SQRT", TGSI_OPCODE_SQRT },
   { 1, 3, 0, 0, 0, 0, REPL, "", 21 },      /* removed */
   { 0, 0, 0, 0, 0, 0, NONE, "", 22 },      /* removed */
   { 0, 0, 0, 0, 0, 0, NONE, "", 23 },      /* removed */
   { 1, 1, 0, 0, 0, 0, COMP, "FRC", TGSI_OPCODE_FRC },
   { 1, 3, 0, 0, 0, 0, COMP, "", 25 },      /* removed */
   { 1, 1, 0, 0, 0, 0, COMP, "FLR", TGSI_OPCODE_FLR },
   { 1, 1, 0, 0, 0, 0, COMP, "ROUND", TGSI_OPCODE_ROUND },
   { 1, 1, 0, 0, 0, 0, REPL, "EX2", TGSI_OPCODE_EX2 },
   { 1, 1, 0, 0, 0, 0, REPL, "LG2", TGSI_OPCODE_LG2 },
   { 1, 2, 0, 0, 0, 0, REPL, "POW", TGSI_OPCODE_POW },
   { 1, 2, 0, 0, 0, 0, COMP, "XPD", TGSI_OPCODE_XPD },
   { 0, 0, 0, 0, 0, 0, NONE, "", 32 },      /* removed */
   { 1, 1, 0, 0, 0, 0, COMP, "ABS", TGSI_OPCODE_ABS },
   { 0, 0, 0, 0, 0, 0, NONE, "", 34 },      /* removed */
   { 1, 2, 0, 0, 0, 0, REPL, "DPH", TGSI_OPCODE_DPH },
   { 1, 1, 0, 0, 0, 0, REPL, "COS", TGSI_OPCODE_COS },
   { 1, 1, 0, 0, 0, 0, COMP, "DDX", TGSI_OPCODE_DDX },
   { 1, 1, 0, 0, 0, 0, COMP, "DDY", TGSI_OPCODE_DDY },
   { 0, 0, 0, 0, 0, 0, NONE, "KILL", TGSI_OPCODE_KILL },
   { 1, 1, 0, 0, 0, 0, COMP, "PK2H", TGSI_OPCODE_PK2H },
   { 1, 1, 0, 0, 0, 0, COMP, "PK2US", TGSI_OPCODE_PK2US },
   { 1, 1, 0, 0, 0, 0, COMP, "PK4B", TGSI_OPCODE_PK4B },
   { 1, 1, 0, 0, 0, 0, COMP, "PK4UB", TGSI_OPCODE_PK4UB },
   { 0, 1, 0, 0, 0, 1, NONE, "", 44 },      /* removed */
   { 1, 2, 0, 0, 0, 0, COMP, "SEQ", TGSI_OPCODE_SEQ },
   { 0, 1, 0, 0, 0, 1, NONE, "", 46 },      /* removed */
   { 1, 2, 0, 0, 0, 0, COMP, "SGT", TGSI_OPCODE_SGT },
   { 1, 1, 0, 0, 0, 0, REPL, "SIN", TGSI_OPCODE_SIN },
   { 1, 2, 0, 0, 0, 0, COMP, "SLE", TGSI_OPCODE_SLE },
   { 1, 2, 0, 0, 0, 0, COMP, "SNE", TGSI_OPCODE_SNE },
   { 0, 1, 0, 0, 0, 1, NONE, "", 51 },      /* removed */
   { 1, 2, 1, 0, 0, 0, OTHR, "TEX", TGSI_OPCODE_TEX },
   { 1, 4, 1, 0, 0, 0, OTHR, "TXD", TGSI_OPCODE_TXD },
   { 1, 2, 1, 0, 0, 0, OTHR, "TXP", TGSI_OPCODE_TXP },
   { 1, 1, 0, 0, 0, 0, COMP, "UP2H", TGSI_OPCODE_UP2H },
   { 1, 1, 0, 0, 0, 0, COMP, "UP2US", TGSI_OPCODE_UP2US },
   { 1, 1, 0, 0, 0, 0, COMP, "UP4B", TGSI_OPCODE_UP4B },
   { 1, 1, 0, 0, 0, 0, COMP, "UP4UB", TGSI_OPCODE_UP4UB },
   { 0, 1, 0, 0, 0, 1, NONE, "", 59 },      /* removed */
   { 0, 1, 0, 0, 0, 1, NONE, "", 60 },      /* removed */
   { 1, 1, 0, 0, 0, 0, COMP, "ARR", TGSI_OPCODE_ARR },
   { 0, 1, 0, 0, 0, 1, NONE, "", 62 },      /* removed */
   { 0, 0, 0, 1, 0, 0, NONE, "CAL", TGSI_OPCODE_CAL },
   { 0, 0, 0, 0, 0, 0, NONE, "RET", TGSI_OPCODE_RET },
   { 1, 1, 0, 0, 0, 0, COMP, "SSG", TGSI_OPCODE_SSG },
   { 1, 3, 0, 0, 0, 0, COMP, "CMP", TGSI_OPCODE_CMP },
   { 1, 1, 0, 0, 0, 0, CHAN, "SCS", TGSI_OPCODE_SCS },
   { 1, 2, 1, 0, 0, 0, OTHR, "TXB", TGSI_OPCODE_TXB },
   { 1, 1, 0, 0, 0, 0, OTHR, "FBFETCH", TGSI_OPCODE_FBFETCH },
   { 1, 2, 0, 0, 0, 0, COMP, "DIV", TGSI_OPCODE_DIV },
   { 1, 2, 0, 0, 0, 0, REPL, "DP2", TGSI_OPCODE_DP2 },
   { 1, 2, 1, 0, 0, 0, OTHR, "TXL", TGSI_OPCODE_TXL },
   { 0, 0, 0, 0, 0, 0, NONE, "BRK", TGSI_OPCODE_BRK },
   { 0, 1, 0, 1, 0, 1, NONE, "IF", TGSI_OPCODE_IF },
   { 0, 1, 0, 1, 0, 1, NONE, "UIF", TGSI_OPCODE_UIF },
   { 0, 1, 0, 0, 0, 1, NONE, "", 76 },      /* removed */
   { 0, 0, 0, 1, 1, 1, NONE, "ELSE", TGSI_OPCODE_ELSE },
   { 0, 0, 0, 0, 1, 0, NONE, "ENDIF", TGSI_OPCODE_ENDIF },
   { 1, 1, 0, 0, 0, 0, COMP, "DDX_FINE", TGSI_OPCODE_DDX_FINE },
   { 1, 1, 0, 0, 0, 0, COMP, "DDY_FINE", TGSI_OPCODE_DDY_FINE },
   { 0, 0, 0, 0, 0, 0, NONE, "", 81 },      /* removed */
   { 0, 0, 0, 0, 0, 0, NONE, "", 82 },      /* removed */
   { 1, 1, 0, 0, 0, 0, COMP, "CEIL", TGSI_OPCODE_CEIL },
   { 1, 1, 0, 0, 0, 0, COMP, "I2F", TGSI_OPCODE_I2F },
   { 1, 1, 0, 0, 0, 0, COMP, "NOT", TGSI_OPCODE_NOT },
   { 1, 1, 0, 0, 0, 0, COMP, "TRUNC", TGSI_OPCODE_TRUNC },
   { 1, 2, 0, 0, 0, 0, COMP, "SHL", TGSI_OPCODE_SHL },
   { 0, 0, 0, 0, 0, 0, NONE, "", 88 },      /* removed */
   { 1, 2, 0, 0, 0, 0, COMP, "AND", TGSI_OPCODE_AND },
   { 1, 2, 0, 0, 0, 0, COMP, "OR", TGSI_OPCODE_OR },
   { 1, 2, 0, 0, 0, 0, COMP, "MOD", TGSI_OPCODE_MOD },
   { 1, 2, 0, 0, 0, 0, COMP, "XOR", TGSI_OPCODE_XOR },
   { 0, 0, 0, 0, 0, 0, COMP, "", 93 },      /* removed */
   { 1, 2, 1, 0, 0, 0, OTHR, "TXF", TGSI_OPCODE_TXF },
   { 1, 2, 1, 0, 0, 0, OTHR, "TXQ", TGSI_OPCODE_TXQ },
   { 0, 0, 0, 0, 0, 0, NONE, "CONT", TGSI_OPCODE_CONT },
   { 0, 1, 0, 0, 0, 0, NONE, "EMIT", TGSI_OPCODE_EMIT },
   { 0, 1, 0, 0, 0, 0, NONE, "ENDPRIM", TGSI_OPCODE_ENDPRIM },
   { 0, 0, 0, 1, 0, 1, NONE, "BGNLOOP", TGSI_OPCODE_BGNLOOP },
   { 0, 0, 0, 0, 0, 1, NONE, "BGNSUB", TGSI_OPCODE_BGNSUB },
   { 0, 0, 0, 1, 1, 0, NONE, "ENDLOOP", TGSI_OPCODE_ENDLOOP },
   { 0, 0, 0, 0, 1, 0, NONE, "ENDSUB", TGSI_OPCODE_ENDSUB },
   { 0, 0, 0, 0, 0, 0, OTHR, "", 103 },     /* removed */
   { 1, 1, 1, 0, 0, 0, OTHR, "TXQS", TGSI_OPCODE_TXQS },
   { 1, 1, 0, 0, 0, 0, OTHR, "RESQ", TGSI_OPCODE_RESQ },
   { 0, 0, 0, 0, 0, 0, NONE, "", 106 },     /* removed */
   { 0, 0, 0, 0, 0, 0, NONE, "NOP", TGSI_OPCODE_NOP },
   { 1, 2, 0, 0, 0, 0, COMP, "FSEQ", TGSI_OPCODE_FSEQ },
   { 1, 2, 0, 0, 0, 0, COMP, "FSGE", TGSI_OPCODE_FSGE },
   { 1, 2, 0, 0, 0, 0, COMP, "FSLT", TGSI_OPCODE_FSLT },
   { 1, 2, 0, 0, 0, 0, COMP, "FSNE", TGSI_OPCODE_FSNE },
   { 0, 1, 0, 0, 0, 0, OTHR, "MEMBAR", TGSI_OPCODE_MEMBAR },
   { 0, 1, 0, 0, 0, 0, NONE, "", 113 },     /* removed */
   { 0, 1, 0, 0, 0, 0, NONE, "", 114 },     /* removed */
   { 0, 1, 0, 0, 0, 0, NONE, "", 115 },     /* removed */
   { 0, 1, 0, 0, 0, 0, NONE, "KILL_IF", TGSI_OPCODE_KILL_IF },
   { 0, 0, 0, 0, 0, 0, NONE, "END", TGSI_OPCODE_END },
   { 1, 3, 0, 0, 0, 0, COMP, "DFMA", TGSI_OPCODE_DFMA },
   { 1, 1, 0, 0, 0, 0, COMP, "F2I", TGSI_OPCODE_F2I },
   { 1, 2, 0, 0, 0, 0, COMP, "IDIV", TGSI_OPCODE_IDIV },
   { 1, 2, 0, 0, 0, 0, COMP, "IMAX", TGSI_OPCODE_IMAX },
   { 1, 2, 0, 0, 0, 0, COMP, "IMIN", TGSI_OPCODE_IMIN },
   { 1, 1, 0, 0, 0, 0, COMP, "INEG", TGSI_OPCODE_INEG },
   { 1, 2, 0, 0, 0, 0, COMP, "ISGE", TGSI_OPCODE_ISGE },
   { 1, 2, 0, 0, 0, 0, COMP, "ISHR", TGSI_OPCODE_ISHR },
   { 1, 2, 0, 0, 0, 0, COMP, "ISLT", TGSI_OPCODE_ISLT },
   { 1, 1, 0, 0, 0, 0, COMP, "F2U", TGSI_OPCODE_F2U },
   { 1, 1, 0, 0, 0, 0, COMP, "U2F", TGSI_OPCODE_U2F },
   { 1, 2, 0, 0, 0, 0, COMP, "UADD", TGSI_OPCODE_UADD },
   { 1, 2, 0, 0, 0, 0, COMP, "UDIV", TGSI_OPCODE_UDIV },
   { 1, 3, 0, 0, 0, 0, COMP, "UMAD", TGSI_OPCODE_UMAD },
   { 1, 2, 0, 0, 0, 0, COMP, "UMAX", TGSI_OPCODE_UMAX },
   { 1, 2, 0, 0, 0, 0, COMP, "UMIN", TGSI_OPCODE_UMIN },
   { 1, 2, 0, 0, 0, 0, COMP, "UMOD", TGSI_OPCODE_UMOD },
   { 1, 2, 0, 0, 0, 0, COMP, "UMUL", TGSI_OPCODE_UMUL },
   { 1, 2, 0, 0, 0, 0, COMP, "USEQ", TGSI_OPCODE_USEQ },
   { 1, 2, 0, 0, 0, 0, COMP, "USGE", TGSI_OPCODE_USGE },
   { 1, 2, 0, 0, 0, 0, COMP, "USHR", TGSI_OPCODE_USHR },
   { 1, 2, 0, 0, 0, 0, COMP, "USLT", TGSI_OPCODE_USLT },
   { 1, 2, 0, 0, 0, 0, COMP, "USNE", TGSI_OPCODE_USNE },
   { 0, 1, 0, 0, 0, 0, NONE, "SWITCH", TGSI_OPCODE_SWITCH },
   { 0, 1, 0, 0, 0, 0, NONE, "CASE", TGSI_OPCODE_CASE },
   { 0, 0, 0, 0, 0, 0, NONE, "DEFAULT", TGSI_OPCODE_DEFAULT },
   { 0, 0, 0, 0, 0, 0, NONE, "ENDSWITCH", TGSI_OPCODE_ENDSWITCH },

   { 1, 3, 0, 0, 0, 0, OTHR, "SAMPLE",      TGSI_OPCODE_SAMPLE },
   { 1, 2, 0, 0, 0, 0, OTHR, "SAMPLE_I",    TGSI_OPCODE_SAMPLE_I },
   { 1, 3, 0, 0, 0, 0, OTHR, "SAMPLE_I_MS", TGSI_OPCODE_SAMPLE_I_MS },
   { 1, 4, 0, 0, 0, 0, OTHR, "SAMPLE_B",    TGSI_OPCODE_SAMPLE_B },
   { 1, 4, 0, 0, 0, 0, OTHR, "SAMPLE_C",    TGSI_OPCODE_SAMPLE_C },
   { 1, 4, 0, 0, 0, 0, OTHR, "SAMPLE_C_LZ", TGSI_OPCODE_SAMPLE_C_LZ },
   { 1, 5, 0, 0, 0, 0, OTHR, "SAMPLE_D",    TGSI_OPCODE_SAMPLE_D },
   { 1, 4, 0, 0, 0, 0, OTHR, "SAMPLE_L",    TGSI_OPCODE_SAMPLE_L },
   { 1, 3, 0, 0, 0, 0, OTHR, "GATHER4",     TGSI_OPCODE_GATHER4 },
   { 1, 2, 0, 0, 0, 0, OTHR, "SVIEWINFO",   TGSI_OPCODE_SVIEWINFO },
   { 1, 2, 0, 0, 0, 0, OTHR, "SAMPLE_POS",  TGSI_OPCODE_SAMPLE_POS },
   { 1, 2, 0, 0, 0, 0, OTHR, "SAMPLE_INFO", TGSI_OPCODE_SAMPLE_INFO },
   { 1, 1, 0, 0, 0, 0, COMP, "UARL", TGSI_OPCODE_UARL },
   { 1, 3, 0, 0, 0, 0, COMP, "UCMP", TGSI_OPCODE_UCMP },
   { 1, 1, 0, 0, 0, 0, COMP, "IABS", TGSI_OPCODE_IABS },
   { 1, 1, 0, 0, 0, 0, COMP, "ISSG", TGSI_OPCODE_ISSG },
   { 1, 2, 0, 0, 0, 0, OTHR, "LOAD", TGSI_OPCODE_LOAD },
   { 1, 2, 0, 0, 0, 0, OTHR, "STORE", TGSI_OPCODE_STORE },
   { 1, 0, 0, 0, 0, 0, OTHR, "", 163 },
   { 1, 0, 0, 0, 0, 0, OTHR, "", 164 },
   { 1, 0, 0, 0, 0, 0, OTHR, "", 165 },
   { 0, 0, 0, 0, 0, 0, OTHR, "BARRIER", TGSI_OPCODE_BARRIER },

   { 1, 3, 0, 0, 0, 0, OTHR, "ATOMUADD", TGSI_OPCODE_ATOMUADD },
   { 1, 3, 0, 0, 0, 0, OTHR, "ATOMXCHG", TGSI_OPCODE_ATOMXCHG },
   { 1, 4, 0, 0, 0, 0, OTHR, "ATOMCAS", TGSI_OPCODE_ATOMCAS },
   { 1, 3, 0, 0, 0, 0, OTHR, "ATOMAND", TGSI_OPCODE_ATOMAND },
   { 1, 3, 0, 0, 0, 0, OTHR, "ATOMOR", TGSI_OPCODE_ATOMOR },
   { 1, 3, 0, 0, 0, 0, OTHR, "ATOMXOR", TGSI_OPCODE_ATOMXOR },
   { 1, 3, 0, 0, 0, 0, OTHR, "ATOMUMIN", TGSI_OPCODE_ATOMUMIN },
   { 1, 3, 0, 0, 0, 0, OTHR, "ATOMUMAX", TGSI_OPCODE_ATOMUMAX },
   { 1, 3, 0, 0, 0, 0, OTHR, "ATOMIMIN", TGSI_OPCODE_ATOMIMIN },
   { 1, 3, 0, 0, 0, 0, OTHR, "ATOMIMAX", TGSI_OPCODE_ATOMIMAX },
   { 1, 3, 1, 0, 0, 0, OTHR, "TEX2", TGSI_OPCODE_TEX2 },
   { 1, 3, 1, 0, 0, 0, OTHR, "TXB2", TGSI_OPCODE_TXB2 },
   { 1, 3, 1, 0, 0, 0, OTHR, "TXL2", TGSI_OPCODE_TXL2 },
   { 1, 2, 0, 0, 0, 0, COMP, "IMUL_HI", TGSI_OPCODE_IMUL_HI },
   { 1, 2, 0, 0, 0, 0, COMP, "UMUL_HI", TGSI_OPCODE_UMUL_HI },
   { 1, 3, 1, 0, 0, 0, OTHR, "TG4", TGSI_OPCODE_TG4 },
   { 1, 2, 1, 0, 0, 0, OTHR, "LODQ", TGSI_OPCODE_LODQ },
   { 1, 3, 0, 0, 0, 0, COMP, "IBFE", TGSI_OPCODE_IBFE },
   { 1, 3, 0, 0, 0, 0, COMP, "UBFE", TGSI_OPCODE_UBFE },
   { 1, 4, 0, 0, 0, 0, COMP, "BFI", TGSI_OPCODE_BFI },
   { 1, 1, 0, 0, 0, 0, COMP, "BREV", TGSI_OPCODE_BREV },
   { 1, 1, 0, 0, 0, 0, COMP, "POPC", TGSI_OPCODE_POPC },
   { 1, 1, 0, 0, 0, 0, COMP, "LSB", TGSI_OPCODE_LSB },
   { 1, 1, 0, 0, 0, 0, COMP, "IMSB", TGSI_OPCODE_IMSB },
   { 1, 1, 0, 0, 0, 0, COMP, "UMSB", TGSI_OPCODE_UMSB },
   { 1, 1, 0, 0, 0, 0, OTHR, "INTERP_CENTROID", TGSI_OPCODE_INTERP_CENTROID },
   { 1, 2, 0, 0, 0, 0, OTHR, "INTERP_SAMPLE", TGSI_OPCODE_INTERP_SAMPLE },
   { 1, 2, 0, 0, 0, 0, OTHR, "INTERP_OFFSET", TGSI_OPCODE_INTERP_OFFSET },
   { 1, 1, 0, 0, 0, 0, COMP, "F2D", TGSI_OPCODE_F2D },
   { 1, 1, 0, 0, 0, 0, COMP, "D2F", TGSI_OPCODE_D2F },
   { 1, 1, 0, 0, 0, 0, COMP, "DABS", TGSI_OPCODE_DABS },
   { 1, 1, 0, 0, 0, 0, COMP, "DNEG", TGSI_OPCODE_DNEG },
   { 1, 2, 0, 0, 0, 0, COMP, "DADD", TGSI_OPCODE_DADD },
   { 1, 2, 0, 0, 0, 0, COMP, "DMUL", TGSI_OPCODE_DMUL },
   { 1, 2, 0, 0, 0, 0, COMP, "DMAX", TGSI_OPCODE_DMAX },
   { 1, 2, 0, 0, 0, 0, COMP, "DMIN", TGSI_OPCODE_DMIN },
   { 1, 2, 0, 0, 0, 0, COMP, "DSLT", TGSI_OPCODE_DSLT },
   { 1, 2, 0, 0, 0, 0, COMP, "DSGE", TGSI_OPCODE_DSGE },
   { 1, 2, 0, 0, 0, 0, COMP, "DSEQ", TGSI_OPCODE_DSEQ },
   { 1, 2, 0, 0, 0, 0, COMP, "DSNE", TGSI_OPCODE_DSNE },
   { 1, 1, 0, 0, 0, 0, COMP, "DRCP", TGSI_OPCODE_DRCP },
   { 1, 1, 0, 0 ,0, 0, COMP, "DSQRT", TGSI_OPCODE_DSQRT },
   { 1, 3, 0, 0 ,0, 0, COMP, "DMAD", TGSI_OPCODE_DMAD },
   { 1, 1, 0, 0, 0, 0, COMP, "DFRAC", TGSI_OPCODE_DFRAC},
   { 1, 2, 0, 0, 0, 0, COMP, "DLDEXP", TGSI_OPCODE_DLDEXP},
   { 2, 1, 0, 0, 0, 0, COMP, "DFRACEXP", TGSI_OPCODE_DFRACEXP},
   { 1, 1, 0, 0, 0, 0, COMP, "D2I", TGSI_OPCODE_D2I },
   { 1, 1, 0, 0, 0, 0, COMP, "I2D", TGSI_OPCODE_I2D },
   { 1, 1, 0, 0, 0, 0, COMP, "D2U", TGSI_OPCODE_D2U },
   { 1, 1, 0, 0, 0, 0, COMP, "U2D", TGSI_OPCODE_U2D },
   { 1, 1, 0, 0 ,0, 0, COMP, "DRSQ", TGSI_OPCODE_DRSQ },
   { 1, 1, 0, 0, 0, 0, COMP, "DTRUNC", TGSI_OPCODE_DTRUNC },
   { 1, 1, 0, 0, 0, 0, COMP, "DCEIL", TGSI_OPCODE_DCEIL },
   { 1, 1, 0, 0, 0, 0, COMP, "DFLR", TGSI_OPCODE_DFLR },
   { 1, 1, 0, 0, 0, 0, COMP, "DROUND", TGSI_OPCODE_DROUND },
   { 1, 1, 0, 0, 0, 0, COMP, "DSSG", TGSI_OPCODE_DSSG },
   { 1, 2, 0, 0, 0, 0, COMP, "DDIV", TGSI_OPCODE_DDIV },
   { 1, 0, 0, 0, 0, 0, OTHR, "CLOCK", TGSI_OPCODE_CLOCK },

   { 1, 1, 0, 0, 0, 0, COMP, "I64ABS", TGSI_OPCODE_I64ABS },
   { 1, 1, 0, 0, 0, 0, COMP, "I64NEG", TGSI_OPCODE_I64NEG },
   { 1, 1, 0, 0, 0, 0, COMP, "I64SSG", TGSI_OPCODE_I64SSG },
   { 1, 2, 0, 0, 0, 0, COMP, "I64SLT", TGSI_OPCODE_I64SLT },
   { 1, 2, 0, 0, 0, 0, COMP, "I64SGE", TGSI_OPCODE_I64SGE },
   { 1, 2, 0, 0, 0, 0, COMP, "I64MIN", TGSI_OPCODE_I64MIN },
   { 1, 2, 0, 0, 0, 0, COMP, "I64MAX", TGSI_OPCODE_I64MAX },
   { 1, 2, 0, 0, 0, 0, COMP, "I64SHR", TGSI_OPCODE_I64SHR },
   { 1, 2, 0, 0, 0, 0, COMP, "I64DIV", TGSI_OPCODE_I64DIV },
   { 1, 2, 0, 0, 0, 0, COMP, "I64MOD", TGSI_OPCODE_I64MOD },
   { 1, 1, 0, 0, 0, 0, COMP, "F2I64", TGSI_OPCODE_F2I64 },
   { 1, 1, 0, 0, 0, 0, COMP, "U2I64", TGSI_OPCODE_U2I64 },
   { 1, 1, 0, 0, 0, 0, COMP, "I2I64", TGSI_OPCODE_I2I64 },
   { 1, 1, 0, 0, 0, 0, COMP, "D2I64", TGSI_OPCODE_D2I64 },
   { 1, 1, 0, 0, 0, 0, COMP, "I642F", TGSI_OPCODE_I642F },
   { 1, 1, 0, 0, 0, 0, COMP, "I642D", TGSI_OPCODE_I642D },

   { 1, 2, 0, 0, 0, 0, COMP, "U64ADD", TGSI_OPCODE_U64ADD },
   { 1, 2, 0, 0, 0, 0, COMP, "U64MUL", TGSI_OPCODE_U64MUL },
   { 1, 2, 0, 0, 0, 0, COMP, "U64SEQ", TGSI_OPCODE_U64SEQ },
   { 1, 2, 0, 0, 0, 0, COMP, "U64SNE", TGSI_OPCODE_U64SNE },
   { 1, 2, 0, 0, 0, 0, COMP, "U64SLT", TGSI_OPCODE_U64SLT },
   { 1, 2, 0, 0, 0, 0, COMP, "U64SGE", TGSI_OPCODE_U64SGE },
   { 1, 2, 0, 0, 0, 0, COMP, "U64MIN", TGSI_OPCODE_U64MIN },
   { 1, 2, 0, 0, 0, 0, COMP, "U64MAX", TGSI_OPCODE_U64MAX },
   { 1, 2, 0, 0, 0, 0, COMP, "U64SHL", TGSI_OPCODE_U64SHL },
   { 1, 2, 0, 0, 0, 0, COMP, "U64SHR", TGSI_OPCODE_U64SHR },
   { 1, 2, 0, 0, 0, 0, COMP, "U64DIV", TGSI_OPCODE_U64DIV },
   { 1, 2, 0, 0, 0, 0, COMP, "U64MOD", TGSI_OPCODE_U64MOD },
   { 1, 1, 0, 0, 0, 0, COMP, "F2U64", TGSI_OPCODE_F2U64 },
   { 1, 1, 0, 0, 0, 0, COMP, "D2U64", TGSI_OPCODE_D2U64 },
   { 1, 1, 0, 0, 0, 0, COMP, "U642F", TGSI_OPCODE_U642F },
   { 1, 1, 0, 0, 0, 0, COMP, "U642D", TGSI_OPCODE_U642D }
};

const struct tgsi_opcode_info *
tgsi_get_opcode_info( uint opcode )
{
   static boolean firsttime = 1;

   if (firsttime) {
      unsigned i;
      firsttime = 0;
      for (i = 0; i < Elements(opcode_info); i++)
         assert(opcode_info[i].opcode == i);
   }

   if (opcode < TGSI_OPCODE_LAST)
      return &opcode_info[opcode];

   assert( 0 );
   return NULL;
}


const char *
tgsi_get_opcode_name( uint opcode )
{
   const struct tgsi_opcode_info *info = tgsi_get_opcode_info(opcode);
   return info->mnemonic;
}


const char *
tgsi_get_processor_name( uint processor )
{
   switch (processor) {
   case TGSI_PROCESSOR_VERTEX:
      return "vertex shader";
   case TGSI_PROCESSOR_FRAGMENT:
      return "fragment shader";
   case TGSI_PROCESSOR_GEOMETRY:
      return "geometry shader";
   case TGSI_PROCESSOR_TESS_CTRL:
      return "tessellation control shader";
   case TGSI_PROCESSOR_TESS_EVAL:
      return "tessellation evaluation shader";
   default:
      return "unknown shader type!";
   }
}

/**
 * Infer the type (of the dst) of the opcode.
 *
 * MOV and UCMP is special so return VOID
 */
static inline enum tgsi_opcode_type
tgsi_opcode_infer_type( uint opcode )
{
   switch (opcode) {
   case TGSI_OPCODE_MOV:
   case TGSI_OPCODE_UCMP:
      return TGSI_TYPE_UNTYPED;
   case TGSI_OPCODE_NOT:
   case TGSI_OPCODE_SHL:
   case TGSI_OPCODE_AND:
   case TGSI_OPCODE_OR:
   case TGSI_OPCODE_XOR:
   case TGSI_OPCODE_TXQ:
   case TGSI_OPCODE_TXQS:
   case TGSI_OPCODE_F2U:
   case TGSI_OPCODE_UDIV:
   case TGSI_OPCODE_UMAD:
   case TGSI_OPCODE_UMAX:
   case TGSI_OPCODE_UMIN:
   case TGSI_OPCODE_UMOD:
   case TGSI_OPCODE_UMUL:
   case TGSI_OPCODE_USEQ:
   case TGSI_OPCODE_USGE:
   case TGSI_OPCODE_USHR:
   case TGSI_OPCODE_USLT:
   case TGSI_OPCODE_USNE:
   case TGSI_OPCODE_SVIEWINFO:
   case TGSI_OPCODE_UMUL_HI:
   case TGSI_OPCODE_UBFE:
   case TGSI_OPCODE_BFI:
   case TGSI_OPCODE_BREV:
   case TGSI_OPCODE_D2U:
   case TGSI_OPCODE_CLOCK:
      return TGSI_TYPE_UNSIGNED;
   case TGSI_OPCODE_ARL:
   case TGSI_OPCODE_ARR:
   case TGSI_OPCODE_MOD:
   case TGSI_OPCODE_F2I:
   case TGSI_OPCODE_FSEQ:
   case TGSI_OPCODE_FSGE:
   case TGSI_OPCODE_FSLT:
   case TGSI_OPCODE_FSNE:
   case TGSI_OPCODE_IDIV:
   case TGSI_OPCODE_IMAX:
   case TGSI_OPCODE_IMIN:
   case TGSI_OPCODE_INEG:
   case TGSI_OPCODE_ISGE:
   case TGSI_OPCODE_ISHR:
   case TGSI_OPCODE_ISLT:
   case TGSI_OPCODE_UADD:
   case TGSI_OPCODE_UARL:
   case TGSI_OPCODE_IABS:
   case TGSI_OPCODE_ISSG:
   case TGSI_OPCODE_IMUL_HI:
   case TGSI_OPCODE_IBFE:
   case TGSI_OPCODE_IMSB:
   case TGSI_OPCODE_DSEQ:
   case TGSI_OPCODE_DSGE:
   case TGSI_OPCODE_DSLT:
   case TGSI_OPCODE_DSNE:
   case TGSI_OPCODE_D2I:
   case TGSI_OPCODE_LSB:
   case TGSI_OPCODE_POPC:
   case TGSI_OPCODE_UMSB:
   case TGSI_OPCODE_U64SEQ:
   case TGSI_OPCODE_U64SNE:
   case TGSI_OPCODE_U64SLT:
   case TGSI_OPCODE_U64SGE:
   case TGSI_OPCODE_I64SLT:
   case TGSI_OPCODE_I64SGE:
      return TGSI_TYPE_SIGNED;
   case TGSI_OPCODE_DADD:
   case TGSI_OPCODE_DABS:
   case TGSI_OPCODE_DFMA:
   case TGSI_OPCODE_DNEG:
   case TGSI_OPCODE_DMUL:
   case TGSI_OPCODE_DMAX:
   case TGSI_OPCODE_DMIN:
   case TGSI_OPCODE_DRCP:
   case TGSI_OPCODE_DSQRT:
   case TGSI_OPCODE_DMAD:
   case TGSI_OPCODE_DLDEXP:
   case TGSI_OPCODE_DFRACEXP:
   case TGSI_OPCODE_DFRAC:
   case TGSI_OPCODE_DRSQ:
   case TGSI_OPCODE_DTRUNC:
   case TGSI_OPCODE_DCEIL:
   case TGSI_OPCODE_DFLR:
   case TGSI_OPCODE_DROUND:
   case TGSI_OPCODE_DSSG:
   case TGSI_OPCODE_DDIV:
   case TGSI_OPCODE_F2D:
   case TGSI_OPCODE_I2D:
   case TGSI_OPCODE_U2D:
   case TGSI_OPCODE_U642D:
   case TGSI_OPCODE_I642D:
      return TGSI_TYPE_DOUBLE;
   case TGSI_OPCODE_U64MAX:
   case TGSI_OPCODE_U64MIN:
   case TGSI_OPCODE_U64ADD:
   case TGSI_OPCODE_U64MUL:
   case TGSI_OPCODE_U64DIV:
   case TGSI_OPCODE_U64MOD:
   case TGSI_OPCODE_U64SHL:
   case TGSI_OPCODE_U64SHR:
   case TGSI_OPCODE_F2U64:
   case TGSI_OPCODE_D2U64:
      return TGSI_TYPE_UNSIGNED64;
   case TGSI_OPCODE_I64MAX:
   case TGSI_OPCODE_I64MIN:
   case TGSI_OPCODE_I64ABS:
   case TGSI_OPCODE_I64SSG:
   case TGSI_OPCODE_I64NEG:
   case TGSI_OPCODE_I64SHR:
   case TGSI_OPCODE_I64DIV:
   case TGSI_OPCODE_I64MOD:
   case TGSI_OPCODE_F2I64:
   case TGSI_OPCODE_U2I64:
   case TGSI_OPCODE_I2I64:
   case TGSI_OPCODE_D2I64:
      return TGSI_TYPE_SIGNED64;
   default:
      return TGSI_TYPE_FLOAT;
   }
}

/*
 * infer the source type of a TGSI opcode.
 */
enum tgsi_opcode_type
tgsi_opcode_infer_src_type( uint opcode )
{
   switch (opcode) {
   case TGSI_OPCODE_UIF:
   case TGSI_OPCODE_TXF:
   case TGSI_OPCODE_U2F:
   case TGSI_OPCODE_U2D:
   case TGSI_OPCODE_UADD:
   case TGSI_OPCODE_SWITCH:
   case TGSI_OPCODE_CASE:
   case TGSI_OPCODE_SAMPLE_I:
   case TGSI_OPCODE_SAMPLE_I_MS:
   case TGSI_OPCODE_UMUL_HI:
   case TGSI_OPCODE_UMSB:
   case TGSI_OPCODE_U2I64:
   case TGSI_OPCODE_MEMBAR:
      return TGSI_TYPE_UNSIGNED;
   case TGSI_OPCODE_IMUL_HI:
   case TGSI_OPCODE_I2F:
   case TGSI_OPCODE_I2D:
   case TGSI_OPCODE_I2I64:
      return TGSI_TYPE_SIGNED;
   case TGSI_OPCODE_ARL:
   case TGSI_OPCODE_ARR:
   case TGSI_OPCODE_F2D:
   case TGSI_OPCODE_F2I:
   case TGSI_OPCODE_F2U:
   case TGSI_OPCODE_FSEQ:
   case TGSI_OPCODE_FSGE:
   case TGSI_OPCODE_FSLT:
   case TGSI_OPCODE_FSNE:
   case TGSI_OPCODE_UCMP:
   case TGSI_OPCODE_F2U64:
   case TGSI_OPCODE_F2I64:
      return TGSI_TYPE_FLOAT;
   case TGSI_OPCODE_D2F:
   case TGSI_OPCODE_D2U:
   case TGSI_OPCODE_D2I:
   case TGSI_OPCODE_DSEQ:
   case TGSI_OPCODE_DSGE:
   case TGSI_OPCODE_DSLT:
   case TGSI_OPCODE_DSNE:
   case TGSI_OPCODE_D2U64:
   case TGSI_OPCODE_D2I64:
      return TGSI_TYPE_DOUBLE;
   case TGSI_OPCODE_U64SEQ:
   case TGSI_OPCODE_U64SNE:
   case TGSI_OPCODE_U64SLT:
   case TGSI_OPCODE_U64SGE:
   case TGSI_OPCODE_U642F:
   case TGSI_OPCODE_U642D:
      return TGSI_TYPE_UNSIGNED64;
   case TGSI_OPCODE_I64SLT:
   case TGSI_OPCODE_I64SGE:
   case TGSI_OPCODE_I642F:
   case TGSI_OPCODE_I642D:
      return TGSI_TYPE_SIGNED64;
   default:
      return tgsi_opcode_infer_type(opcode);
   }
}

/*
 * infer the destination type of a TGSI opcode.
 */
enum tgsi_opcode_type
tgsi_opcode_infer_dst_type( uint opcode )
{
   return tgsi_opcode_infer_type(opcode);
}
