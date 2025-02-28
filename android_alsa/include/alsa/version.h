/*
 *  version.h
 */

#define SND_LIB_MAJOR		1 /**< major number of library version */
#define SND_LIB_MINOR		2 /**< minor number of library version */
#define SND_LIB_SUBMINOR	13 /**< subminor number of library version */
#define SND_LIB_EXTRAVER	1000000 /**< extra version number, used mainly for betas */
/** library version */
#define SND_LIB_VER(maj, min, sub) (((maj)<<16)|((min)<<8)|(sub))
#define SND_LIB_VERSION SND_LIB_VER(SND_LIB_MAJOR, SND_LIB_MINOR, SND_LIB_SUBMINOR)
/** library version (string) */
#define SND_LIB_VERSION_STR	"1.2.13"

