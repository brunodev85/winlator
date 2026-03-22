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
#ifndef VIRGL_SERVER_PROTOCOL_H
#define VIRGL_SERVER_PROTOCOL_H

#define VCMD_CREATE_RENDERER 1
#define VCMD_GET_CAPS 2
#define VCMD_RESOURCE_CREATE 3
#define VCMD_RESOURCE_DESTROY 4
#define VCMD_TRANSFER_GET 5
#define VCMD_TRANSFER_PUT 6
#define VCMD_SUBMIT_CMD 7
#define VCMD_RESOURCE_BUSY_WAIT 8
#define VCMD_FLUSH_FRONTBUFFER 9

#define VCMD_BUSY_WAIT_FLAG_WAIT 1

#endif
