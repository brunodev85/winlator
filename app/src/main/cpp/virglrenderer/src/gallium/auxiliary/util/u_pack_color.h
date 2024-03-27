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

/**
 * @file
 * Functions to produce packed colors/Z from floats.
 */


#ifndef U_PACK_COLOR_H
#define U_PACK_COLOR_H


#include "pipe/p_compiler.h"
#include "pipe/p_format.h"
#include "util/u_debug.h"
#include "util/u_format.h"
#include "util/u_math.h"


/**
 * Helper union for packing pixel values.
 * Will often contain values in formats which are too complex to be described
 * in simple terms, hence might just effectively contain a number of bytes.
 * Must be big enough to hold data for all formats (currently 256 bits).
 */
union util_color {
   ubyte ub;
   ushort us;
   uint ui[4];
   ushort h[4]; /* half float */
   float f[4];
   double d[4];
};

/**
 * Pack 4 ubytes into a 4-byte word
 */
static inline unsigned
pack_ub4(ubyte b0, ubyte b1, ubyte b2, ubyte b3)
{
   return ((((unsigned int)b0) << 0) |
	   (((unsigned int)b1) << 8) |
	   (((unsigned int)b2) << 16) |
	   (((unsigned int)b3) << 24));
}


/**
 * Pack/convert 4 floats into one 4-byte word.
 */
static inline unsigned
pack_ui32_float4(float a, float b, float c, float d)
{
   return pack_ub4( float_to_ubyte(a),
		    float_to_ubyte(b),
		    float_to_ubyte(c),
		    float_to_ubyte(d) );
}



#endif /* U_PACK_COLOR_H */
