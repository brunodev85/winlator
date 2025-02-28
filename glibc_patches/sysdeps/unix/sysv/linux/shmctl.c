#include <stdarg.h>
#include <ipc_priv.h>
#include <sysdep.h>
#include <shlib-compat.h>
#include <errno.h>
#include <asm-generic/ipcbuf.h>
#include <asm-generic/shmbuf.h>
#include <android_sysvshm.h>

#ifndef shmid_ds
# define shmid_ds shmid64_ds
#endif

int shmctl(int shmid, int cmd, struct shmid_ds *buf) {
    if (cmd == IPC_RMID) {
        pthread_mutex_lock(&sysvshm_mutex);
        
        int index = find_shmemory_index(shmid);
        if (index != -1) {
            if (shmemories[index].addr) {
                shmemories[index].marked_for_delete = 1;
            } 
            else sysvshm_delete(index);                
        }        
        
        pthread_mutex_unlock(&sysvshm_mutex);
        return 0;
    } 
    else if (cmd == IPC_STAT) {
        pthread_mutex_lock(&sysvshm_mutex);
        
        int index = find_shmemory_index(shmid);
        if (!buf || index == -1) {
            pthread_mutex_unlock(&sysvshm_mutex);
            return -1;
        }
        
        memset(buf, 0, sizeof(struct shmid_ds));
        buf->shm_segsz = shmemories[index].size;
        buf->shm_nattch = 1;
        buf->shm_perm.key = IPC_PRIVATE;
        buf->shm_perm.uid = geteuid();
        buf->shm_perm.gid = getegid();
        buf->shm_perm.cuid = geteuid();
        buf->shm_perm.cgid = getegid();
        buf->shm_perm.mode = 0666;
        buf->shm_perm.seq = 1;
        
        pthread_mutex_unlock(&sysvshm_mutex);
        return 0;
    }
    return -1;
}

weak_alias (shmctl, __shmctl64)
