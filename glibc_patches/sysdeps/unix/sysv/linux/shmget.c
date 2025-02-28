#include <sys/msg.h>
#include <stddef.h>
#include <ipc_priv.h>
#include <sysdep.h>
#include <android_sysvshm.h>

#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))

/* Return an identifier for an shared memory segment of at least size SIZE
   which is associated with KEY.  */

int shmget(key_t key, size_t size, int flags) {
    if (key != IPC_PRIVATE) return -1;
    
    pthread_mutex_lock(&sysvshm_mutex);
        
    sysvshm_connect();
    int shmid = sysvshm_shmget_request(size);
    if (shmid == 0) {
        sysvshm_close();
        pthread_mutex_unlock(&sysvshm_mutex);
        return -1;
    }
    
    size = ROUND_UP(size, getpagesize());
    int index = shmemory_count;
    shmemory_count++;
    shmemories = realloc(shmemories, shmemory_count * sizeof(shmemory_t));
    shmemories[index].size = size;
    shmemories[index].fd = sysvshm_get_fd_request(shmid);
    shmemories[index].addr = NULL;
    shmemories[index].id = shmid;
    shmemories[index].marked_for_delete = 0;
    
    sysvshm_close();
    
    if (shmemories[index].fd < 0) {
        shmemory_count--;
        shmemories = realloc(shmemories, shmemory_count * sizeof(shmemory_t));
        pthread_mutex_unlock(&sysvshm_mutex);
        return -1;
    }
    
    pthread_mutex_unlock(&sysvshm_mutex);
    return shmid;
}
