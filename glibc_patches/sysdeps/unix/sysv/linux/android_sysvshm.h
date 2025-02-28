#ifndef __ANDROID_SYSVSHM
#define __ANDROID_SYSVSHM

#include <stdio.h>
#include <stdbool.h> 
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

/* based on https://github.com/pelya/android-shmem */

typedef struct {
    int id;
    void* addr;
    int fd;
    size_t size;
    char marked_for_delete;
} shmemory_t;

extern shmemory_t* shmemories;
extern int shmemory_count;
extern int sysvshm_server_fd;
extern pthread_mutex_t sysvshm_mutex;

extern int find_shmemory_index(int shmid);
extern void sysvshm_connect(void);
extern void sysvshm_close(void);
extern int sysvshm_shmget_request(size_t size);
extern int sysvshm_get_fd_request(int shmid);
extern bool sysvshm_delete_request(int shmid);
extern void sysvshm_delete(int index);

#endif