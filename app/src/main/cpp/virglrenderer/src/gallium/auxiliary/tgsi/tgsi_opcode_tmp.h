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
#ifndef OP12_TEX
#define OP12_TEX(a) OP12(a)
#endif

#ifndef OP14_TEX
#define OP14_TEX(a) OP14(a)
#endif

#ifndef OP12_SAMPLE
#define OP12_SAMPLE(a) OP12(a)
#endif

#ifndef OP13_SAMPLE
#define OP13_SAMPLE(a) OP13(a)
#endif

#ifndef OP14_SAMPLE
#define OP14_SAMPLE(a) OP14(a)
#endif

#ifndef OP15_SAMPLE
#define OP15_SAMPLE(a) OP15(a)
#endif

#ifndef OP00_LBL
#define OP00_LBL(a) OP00(a)
#endif

#ifndef OP01_LBL
#define OP01_LBL(a) OP01(a)
#endif

OP11(ARL)
OP11(MOV)
OP11(LIT)
OP11(RCP)
OP11(RSQ)
OP11(EXP)
OP11(LOG)
OP12(MUL)
OP12(ADD)
OP12(DP3)
OP12(DP4)
OP12(DST)
OP12(MIN)
OP12(MAX)
OP12(SLT)
OP12(SGE)
OP13(MAD)
OP12(SUB)
OP13(LRP)
OP11(SQRT)
OP11(FRC)
OP11(FLR)
OP11(ROUND)
OP11(EX2)
OP11(LG2)
OP12(POW)
OP12(XPD)
OP11(ABS)
OP12(DPH)
OP11(COS)
OP11(DDX)
OP11(DDY)
OP00(KILL)
OP11(PK2H)
OP11(PK2US)
OP11(PK4B)
OP11(PK4UB)
OP12(SEQ)
OP12(SGT)
OP11(SIN)
OP12(SLE)
OP12(SNE)
OP12_TEX(TEX)
OP14_TEX(TXD)
OP12_TEX(TXP)
OP11(UP2H)
OP11(UP2US)
OP11(UP4B)
OP11(UP4UB)
OP11(ARR)
OP00_LBL(CAL)
OP00(RET)
OP11(SSG)
OP13(CMP)
OP11(SCS)
OP12_TEX(TXB)
OP12(DIV)
OP12(DP2)
OP12_TEX(TXL)
OP00(BRK)
OP01_LBL(IF)
OP01_LBL(UIF)
OP00_LBL(ELSE)
OP00(ENDIF)
OP11(CEIL)
OP11(I2F)
OP11(NOT)
OP11(TRUNC)
OP12(SHL)
OP12(AND)
OP12(OR)
OP12(MOD)
OP12(XOR)
OP12_TEX(TXF)
OP12_TEX(TXQ)
OP00(CONT)
OP01(EMIT)
OP01(ENDPRIM)
OP00_LBL(BGNLOOP)
OP00(BGNSUB)
OP00_LBL(ENDLOOP)
OP00(ENDSUB)
OP00(NOP)
OP01(KILL_IF)
OP00(END)
OP11(F2I)
OP12(FSEQ)
OP12(FSGE)
OP12(FSLT)
OP12(FSNE)
OP12(IDIV)
OP12(IMAX)
OP12(IMIN)
OP11(INEG)
OP12(ISGE)
OP12(ISHR)
OP12(ISLT)
OP11(F2U)
OP11(U2F)
OP12(UADD)
OP12(UDIV)
OP13(UMAD)
OP12(UMAX)
OP12(UMIN)
OP12(UMOD)
OP12(UMUL)
OP12(USEQ)
OP12(USGE)
OP12(USHR)
OP12(USLT)
OP12(USNE)
OP01(SWITCH)
OP01(CASE)
OP00(DEFAULT)
OP00(ENDSWITCH)

OP13_SAMPLE(SAMPLE)
OP12_SAMPLE(SAMPLE_I)
OP13_SAMPLE(SAMPLE_I_MS)
OP14_SAMPLE(SAMPLE_B)
OP14_SAMPLE(SAMPLE_C)
OP14_SAMPLE(SAMPLE_C_LZ)
OP15_SAMPLE(SAMPLE_D)
OP14_SAMPLE(SAMPLE_L)
OP13_SAMPLE(GATHER4)
OP12(SVIEWINFO)
OP13(SAMPLE_POS)
OP12(SAMPLE_INFO)
OP11(UARL)

OP13(UCMP)

OP12(IMUL_HI)
OP12(UMUL_HI)

#undef OP00
#undef OP01
#undef OP10
#undef OP11
#undef OP12
#undef OP13

#ifdef OP14
#undef OP14
#endif

#ifdef OP15
#undef OP15
#endif

#undef OP00_LBL
#undef OP01_LBL

#undef OP12_TEX
#undef OP14_TEX

#undef OP12_SAMPLE
#undef OP13_SAMPLE
#undef OP14_SAMPLE
#undef OP15_SAMPLE
