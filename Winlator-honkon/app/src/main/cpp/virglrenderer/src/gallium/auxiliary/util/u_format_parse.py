
'''
/**************************************************************************
 *
 * Copyright 2009 VMware, Inc.
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
'''


from __future__ import division


VOID, UNSIGNED, SIGNED, FIXED, FLOAT = range(5)

SWIZZLE_X, SWIZZLE_Y, SWIZZLE_Z, SWIZZLE_W, SWIZZLE_0, SWIZZLE_1, SWIZZLE_NONE, = range(7)

PLAIN = 'plain'

RGB = 'rgb'
SRGB = 'srgb'
YUV = 'yuv'
ZS = 'zs'


def is_pot(x):
   return (x & (x - 1)) == 0


VERY_LARGE = 99999999999999999999999


class Channel:
    '''Describe the channel of a color channel.'''
    
    def __init__(self, type, norm, pure, size, name = ''):
        self.type = type
        self.norm = norm
        self.pure = pure
        self.size = size
        self.sign = type in (SIGNED, FIXED, FLOAT)
        self.name = name

    def __str__(self):
        s = str(self.type)
        if self.norm:
            s += 'n'
        if self.pure:
            s += 'p'
        s += str(self.size)
        return s

    def __eq__(self, other):
        if other is None:
            return False

        return self.type == other.type and self.norm == other.norm and self.pure == other.pure and self.size == other.size

    def __ne__(self, other):
        return not self == other

    def max(self):
        '''Maximum representable number.'''
        if self.type == FLOAT:
            return VERY_LARGE
        if self.type == FIXED:
            return (1 << (self.size // 2)) - 1
        if self.norm:
            return 1
        if self.type == UNSIGNED:
            return (1 << self.size) - 1
        if self.type == SIGNED:
            return (1 << (self.size - 1)) - 1
        assert False
    
    def min(self):
        '''Minimum representable number.'''
        if self.type == FLOAT:
            return -VERY_LARGE
        if self.type == FIXED:
            return -(1 << (self.size // 2))
        if self.type == UNSIGNED:
            return 0
        if self.norm:
            return -1
        if self.type == SIGNED:
            return -(1 << (self.size - 1))
        assert False


class Format:
    '''Describe a pixel format.'''

    def __init__(self, name, layout, block_width, block_height, le_channels, le_swizzles, be_channels, be_swizzles, colorspace):
        self.name = name
        self.layout = layout
        self.block_width = block_width
        self.block_height = block_height
        self.le_channels = le_channels
        self.le_swizzles = le_swizzles
        self.be_channels = be_channels
        self.be_swizzles = be_swizzles
        self.name = name
        self.colorspace = colorspace

    def __str__(self):
        return self.name

    def short_name(self):
        '''Make up a short norm for a format, suitable to be used as suffix in
        function names.'''

        name = self.name
        if name.startswith('PIPE_FORMAT_'):
            name = name[len('PIPE_FORMAT_'):]
        name = name.lower()
        return name

    def block_size(self):
        size = 0
        for channel in self.le_channels:
            size += channel.size
        return size

    def nr_channels(self):
        nr_channels = 0
        for channel in self.le_channels:
            if channel.size:
                nr_channels += 1
        return nr_channels

    def array_element(self):
        if self.layout != PLAIN:
            return None
        ref_channel = self.le_channels[0]
        if ref_channel.type == VOID:
           ref_channel = self.le_channels[1]
        for channel in self.le_channels:
            if channel.size and (channel.size != ref_channel.size or channel.size % 8):
                return None
            if channel.type != VOID:
                if channel.type != ref_channel.type:
                    return None
                if channel.norm != ref_channel.norm:
                    return None
                if channel.pure != ref_channel.pure:
                    return None
        return ref_channel

    def is_array(self):
        return self.array_element() != None

    def is_mixed(self):
        if self.layout != PLAIN:
            return False
        ref_channel = self.le_channels[0]
        if ref_channel.type == VOID:
           ref_channel = self.le_channels[1]
        for channel in self.le_channels[1:]:
            if channel.type != VOID:
                if channel.type != ref_channel.type:
                    return True
                if channel.norm != ref_channel.norm:
                    return True
                if channel.pure != ref_channel.pure:
                    return True
        return False

    def is_pot(self):
        return is_pot(self.block_size())

    def is_int(self):
        if self.layout != PLAIN:
            return False
        for channel in self.le_channels:
            if channel.type not in (VOID, UNSIGNED, SIGNED):
                return False
        return True

    def is_float(self):
        if self.layout != PLAIN:
            return False
        for channel in self.le_channels:
            if channel.type not in (VOID, FLOAT):
                return False
        return True

    def is_bitmask(self):
        if self.layout != PLAIN:
            return False
        if self.block_size() not in (8, 16, 32):
            return False
        for channel in self.le_channels:
            if channel.type not in (VOID, UNSIGNED, SIGNED):
                return False
        return True

    def is_pure_color(self):
        if self.layout != PLAIN or self.colorspace == ZS:
            return False
        pures = [channel.pure
                 for channel in self.le_channels
                 if channel.type != VOID]
        for x in pures:
           assert x == pures[0]
        return pures[0]

    def channel_type(self):
        types = [channel.type
                 for channel in self.le_channels
                 if channel.type != VOID]
        for x in types:
           assert x == types[0]
        return types[0]

    def is_pure_signed(self):
        return self.is_pure_color() and self.channel_type() == SIGNED

    def is_pure_unsigned(self):
        return self.is_pure_color() and self.channel_type() == UNSIGNED

    def has_channel(self, id):
        return self.le_swizzles[id] != SWIZZLE_NONE

    def has_depth(self):
        return self.colorspace == ZS and self.has_channel(0)

    def has_stencil(self):
        return self.colorspace == ZS and self.has_channel(1)

    def stride(self):
        return self.block_size()/8


_type_parse_map = {
    '':  VOID,
    'x': VOID,
    'u': UNSIGNED,
    's': SIGNED,
    'h': FIXED,
    'f': FLOAT,
}

_swizzle_parse_map = {
    'x': SWIZZLE_X,
    'y': SWIZZLE_Y,
    'z': SWIZZLE_Z,
    'w': SWIZZLE_W,
    '0': SWIZZLE_0,
    '1': SWIZZLE_1,
    '_': SWIZZLE_NONE,
}

def _parse_channels(fields, layout, colorspace, swizzles):
    if layout == PLAIN:
        names = ['']*4
        if colorspace in (RGB, SRGB):
            for i in range(4):
                swizzle = swizzles[i]
                if swizzle < 4:
                    names[swizzle] += 'rgba'[i]
        elif colorspace == ZS:
            for i in range(4):
                swizzle = swizzles[i]
                if swizzle < 4:
                    names[swizzle] += 'zs'[i]
        else:
            assert False
        for i in range(4):
            if names[i] == '':
                names[i] = 'x'
    else:
        names = ['x', 'y', 'z', 'w']

    channels = []
    for i in range(0, 4):
        field = fields[i]
        if field:
            type = _type_parse_map[field[0]]
            if field[1] == 'n':
                norm = True
                pure = False
                size = int(field[2:])
            elif field[1] == 'p':
                pure = True
                norm = False
                size = int(field[2:])
            else:
                norm = False
                pure = False
                size = int(field[1:])
        else:
            type = VOID
            norm = False
            pure = False
            size = 0
        channel = Channel(type, norm, pure, size, names[i])
        channels.append(channel)

    return channels

def parse(filename):
    '''Parse the format description in CSV format in terms of the
    Channel and Format classes above.'''

    stream = open(filename)
    formats = []
    for line in stream:
        try:
            comment = line.index('#')
        except ValueError:
            pass
        else:
            line = line[:comment]
        line = line.strip()
        if not line:
            continue

        fields = [field.strip() for field in line.split(',')]
        if len (fields) == 10:
           fields += fields[4:9]
        assert len (fields) == 15
        
        name = fields[0]
        layout = fields[1]
        block_width, block_height = map(int, fields[2:4])
        colorspace = fields[9]

        le_swizzles = [_swizzle_parse_map[swizzle] for swizzle in fields[8]]
        le_channels = _parse_channels(fields[4:8], layout, colorspace, le_swizzles)

        be_swizzles = [_swizzle_parse_map[swizzle] for swizzle in fields[14]]
        be_channels = _parse_channels(fields[10:14], layout, colorspace, be_swizzles)

        le_shift = 0
        for channel in le_channels:
            channel.shift = le_shift
            le_shift += channel.size

        be_shift = 0
        for channel in be_channels[3::-1]:
            channel.shift = be_shift
            be_shift += channel.size

        assert le_shift == be_shift
        for i in range(4):
            assert (le_swizzles[i] != SWIZZLE_NONE) == (be_swizzles[i] != SWIZZLE_NONE)

        format = Format(name, layout, block_width, block_height, le_channels, le_swizzles, be_channels, be_swizzles, colorspace)
        formats.append(format)
    return formats

