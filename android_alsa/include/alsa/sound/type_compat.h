#ifndef __SOUND_TYPE_COMPAT_H
#define __SOUND_TYPE_COMPAT_H

#ifndef DOC_HIDDEN
#include <stdint.h>
#if defined(__linux__)
#include <linux/types.h>
#else
typedef uint8_t __u8;
typedef uint16_t __u16;
typedef uint32_t __u32;
typedef uint64_t __u64;
typedef int8_t __s8;
typedef int16_t __s16;
typedef int32_t __s32;
typedef int64_t __s64;

#if defined(__sun)
#include <sys/byteorder.h>
#define __cpu_to_le32 LE_32(x)
#define __cpu_to_be32 BE_32(x)
#define __cpu_to_le16 LE_16(x)
#define __cpu_to_be16 BE_16(x)
#else
#include <sys/endian.h>
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define __cpu_to_le32(x) (x)
#define __cpu_to_be32(x) bswap_32(x)
#define __cpu_to_le16(x) (x)
#define __cpu_to_be16(x) bswap_16(x)
#else
#define __cpu_to_le32(x) bswap_32(x)
#define __cpu_to_be32(x) (x)
#define __cpu_to_le16(x) bswap_16(x)
#define __cpu_to_be16(x) (x)
#endif
#endif

#define __le32_to_cpu __cpu_to_le32
#define __be32_to_cpu __cpu_to_be32
#define __le16_to_cpu __cpu_to_le16
#define __be16_to_cpu __cpu_to_be16
#endif

#ifndef __kernel_long_t
#define __kernel_long_t long
#endif

#ifndef __user
#define __user
#endif

#ifndef __packed
#define __packed __attribute__((__packed__))
#endif

#endif /* DOC_HIDDEN */

#endif /* __SOUND_TYPE_COMPAT_H */
