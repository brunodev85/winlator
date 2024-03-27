#ifndef UTIL_BOX_INLINES_H
#define UTIL_BOX_INLINES_H

#include "pipe/p_state.h"

static inline
void u_box_1d( unsigned x,
	       unsigned w,
	       struct pipe_box *box )
{
   box->x = x;
   box->y = 0;
   box->z = 0;
   box->width = w;
   box->height = 1;
   box->depth = 1;
}

static inline
void u_box_2d( unsigned x,
	       unsigned y,
	       unsigned w,
	       unsigned h,
	       struct pipe_box *box )
{
   box->x = x;
   box->y = y;
   box->z = 0;
   box->width = w;
   box->height = h;
   box->depth = 1;
}

static inline
void u_box_origin_2d( unsigned w,
		      unsigned h,
		      struct pipe_box *box )
{
   box->x = 0;
   box->y = 0;
   box->z = 0;
   box->width = w;
   box->height = h;
   box->depth = 1;
}

static inline
void u_box_2d_zslice( unsigned x,
		      unsigned y,
		      unsigned z,
		      unsigned w,
		      unsigned h,
		      struct pipe_box *box )
{
   box->x = x;
   box->y = y;
   box->z = z;
   box->width = w;
   box->height = h;
   box->depth = 1;
}

static inline
void u_box_3d( unsigned x,
	       unsigned y,
	       unsigned z,
	       unsigned w,
	       unsigned h,
	       unsigned d,
	       struct pipe_box *box )
{
   box->x = x;
   box->y = y;
   box->z = z;
   box->width = w;
   box->height = h;
   box->depth = d;
}

#endif
