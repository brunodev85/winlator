#ifndef _SYS_SHM_H
#define _SYS_SHM_H	1

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The following System V style IPC functions implement a shared memory
   facility.  The definition is found in XPG4.2.  */

/* Mode bits for `msgget', `semget', and `shmget'.  */
#define IPC_CREAT	01000		/* Create key if key does not exist. */
#define IPC_EXCL	02000		/* Fail if key exists.  */
#define IPC_NOWAIT	04000		/* Return error on wait.  */

/* Control commands for `msgctl', `semctl', and `shmctl'.  */
#define IPC_RMID	0		/* Remove identifier.  */
#define IPC_SET		1		/* Set `ipc_perm' options.  */
#define IPC_STAT	2		/* Get `ipc_perm' options.  */
#define IPC_INFO	3		/* See ipcs.  */

/* Special key values.  */
#define IPC_PRIVATE	((key_t) 0)	/* Private key.  */

/* Permission flag for shmget.  */
#define SHM_R		0400		/* or S_IRUGO from <linux/stat.h> */
#define SHM_W		0200		/* or S_IWUGO from <linux/stat.h> */

/* Flags for `shmat'.  */
#define SHM_RDONLY	010000		/* attach read-only else read-write */
#define SHM_RND		020000		/* round attach address to SHMLBA */
#define SHM_REMAP	040000		/* take-over region on attach */
#define SHM_EXEC	0100000		/* execution access */

/* Commands for `shmctl'.  */
#define SHM_LOCK	11		/* lock segment (root only) */
#define SHM_UNLOCK	12		/* unlock segment (root only) */

/* Type to count number of attaches.  */
typedef unsigned long int shmatt_t;
typedef long int __long_time_t; /* to keep compatibility to Debian Wheezy armhf */
/*
typedef int32_t __key_t;
typedef __key_t key_t;
typedef uint32_t __uid_t;
typedef uint32_t __gid_t;
*/

/* Data structure used to pass permission information to IPC operations.  */
struct debian_ipc_perm /* We cannot use Android version, because there are no padding fields */
  {
    key_t __key;			/* Key.  */
    __uid_t uid;			/* Owner's user ID.  */
    __gid_t gid;			/* Owner's group ID.  */
    __uid_t cuid;			/* Creator's user ID.  */
    __gid_t cgid;			/* Creator's group ID.  */
    unsigned short int mode;		/* Read/write permission.  */
    unsigned short int __pad1;
    unsigned short int __seq;		/* Sequence number.  */
    unsigned short int __pad2;
    unsigned long int __unused1;
    unsigned long int __unused2;
  };

/* Data structure describing a shared memory segment.  */
struct shmid_ds
  {
    struct debian_ipc_perm shm_perm;		/* operation permission struct */
    size_t shm_segsz;			/* size of segment in bytes */
    __long_time_t shm_atime;			/* time of last shmat() */
    unsigned long int __unused1;
    __long_time_t shm_dtime;			/* time of last shmdt() */
    unsigned long int __unused2;
    __long_time_t shm_ctime;			/* time of last change by shmctl() */
    unsigned long int __unused3;
    __pid_t shm_cpid;			/* pid of creator */
    __pid_t shm_lpid;			/* pid of last shmop */
    shmatt_t shm_nattch;		/* number of current attaches */
    unsigned long int __unused4;
    unsigned long int __unused5;
  };

/* Shared memory control operation.  */
extern int shmctl (int __shmid, int __cmd, struct shmid_ds *__buf);

/* Get shared memory segment.  */
extern int shmget (key_t __key, size_t __size, int __shmflg);

/* Attach shared memory segment.  */
extern void *shmat (int __shmid, const void *__shmaddr, int __shmflg);

/* Detach shared memory segment.  */
extern int shmdt (const void *__shmaddr);

#ifdef __cplusplus
}
#endif

#endif /* sys/shm.h */