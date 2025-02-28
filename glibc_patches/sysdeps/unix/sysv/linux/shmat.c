#include <ipc_priv.h>
#include <sysdep.h>
#include <errno.h>
#include <sys/mman.h>
#include <android_sysvshm.h>

/* Attach the shared memory segment associated with SHMID to the data
   segment of the calling process.  SHMADDR and SHMFLG determine how
   and where the segment is attached.  */

void* shmat(int shmid, void const* shmaddr, int shmflg)
{
    pthread_mutex_lock(&sysvshm_mutex);

    void* addr = NULL;
    int index = find_shmemory_index(shmid);
    if (index != -1) {
        if (shmemories[index].addr == NULL) {
            shmemories[index].addr = mmap(NULL, shmemories[index].size, PROT_READ | (shmflg == 0 ? PROT_WRITE : 0), MAP_SHARED, shmemories[index].fd, 0);
            if (shmemories[index].addr == MAP_FAILED) shmemories[index].addr = NULL;
        }
        addr = shmemories[index].addr;
    }
    
    pthread_mutex_unlock(&sysvshm_mutex);
    return addr ? addr : (void *)-1;
}
