#include <ipc_priv.h>
#include <sysdep.h>
#include <errno.h>
#include <sys/mman.h>
#include <android_sysvshm.h>

/* Detach shared memory segment starting at address specified by SHMADDR
   from the caller's data segment.  */

int shmdt(void const* shmaddr) {
    pthread_mutex_lock(&sysvshm_mutex);
    
    for (int i = 0; i < shmemory_count; i++) {
        if (shmemories[i].addr == shmaddr) {
            munmap(shmemories[i].addr, shmemories[i].size);
            shmemories[i].addr = NULL;
            if (shmemories[i].marked_for_delete) sysvshm_delete(i);
            break;
        }
    }    
    
    pthread_mutex_unlock(&sysvshm_mutex);
    return 0;
}
