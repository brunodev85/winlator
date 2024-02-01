#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>

#include "sys/shm.h"

#define REQUEST_CODE_SHMGET 0
#define REQUEST_CODE_GET_FD 1
#define REQUEST_CODE_DELETE 2

#define MIN_REQUEST_LENGTH 5
#define ROUND_UP(N, S) ((((N) + (S) - 1) / (S)) * (S))

/* based on https://github.com/pelya/android-shmem */

typedef struct {
    int id;
    void* addr;
    int fd;
    size_t size;
    char marked_for_delete;
} shmemory_t;

static shmemory_t* shmemories = NULL;
static int shmemory_count = 0;
static int sysvshm_server_fd = -1;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static int find_shmemory_index(int shmid) {
    for (int i = 0; i < shmemory_count; i++) if (shmemories[i].id == shmid) return i;
    return -1;
}

static void sysvshm_connect() {
    if (sysvshm_server_fd >= 0) return;
    char* path = getenv("ANDROID_SYSVSHM_SERVER");
    
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;
    
    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_LOCAL;
    
    strncpy(server_addr.sun_path, path, sizeof(server_addr.sun_path) - 1);
    
    int res;
    do {
        res = 0;
        if (connect(fd, (struct sockaddr*)&server_addr, sizeof(struct sockaddr_un)) < 0) res = -errno;
    } 
    while (res == -EINTR);        
    
    if (res < 0) {
        close(fd);
        return;
    }

    sysvshm_server_fd = fd;    
}

static void sysvshm_close() {
    if (sysvshm_server_fd >= 0) {
        close(sysvshm_server_fd);
        sysvshm_server_fd = -1;
    }
}

static int sysvshm_shmget_request(size_t size) {
    if (sysvshm_server_fd < 0) return 0;
    
    char request_data[MIN_REQUEST_LENGTH];
    request_data[0] = REQUEST_CODE_SHMGET;
    memcpy(request_data + 1, &size, 4);
    
    int res = write(sysvshm_server_fd, request_data, sizeof(request_data));
    if (res < 0) return 0;
    
    int shmid;
    res = read(sysvshm_server_fd, &shmid, 4);
    return res == 4 ? shmid : 0;
}

static int sysvshm_get_fd_request(int shmid) {
    if (sysvshm_server_fd < 0) return 0;
    
    char request_data[MIN_REQUEST_LENGTH];
    request_data[0] = REQUEST_CODE_GET_FD;
    memcpy(request_data + 1, &shmid, 4);
    
    int res = write(sysvshm_server_fd, request_data, sizeof(request_data));
    if (res < 0) return -1;
    
    char zero = 0;
    struct iovec iovmsg = {.iov_base = &zero, .iov_len = 1};
    struct {
        struct cmsghdr align;
        int fds[1];
    } ctrlmsg;

    struct msghdr msg = {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = &iovmsg,
        .msg_iovlen = 1,
        .msg_flags = 0,
        .msg_control = &ctrlmsg,
        .msg_controllen = sizeof(struct cmsghdr) + sizeof(int)
    };

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = msg.msg_controllen;
    ((int*)CMSG_DATA(cmsg))[0] = -1;

    recvmsg(sysvshm_server_fd, &msg, 0);
    return ((int*)CMSG_DATA(cmsg))[0];
}

static void sysvshm_delete_request(int shmid) {
    if (sysvshm_server_fd < 0) return;
    
    char request_data[MIN_REQUEST_LENGTH];
    request_data[0] = REQUEST_CODE_DELETE;
    memcpy(request_data + 1, &shmid, 4);
    
    write(sysvshm_server_fd, request_data, sizeof(request_data));
}

static void sysvshm_delete(int index) {
    sysvshm_connect();
    sysvshm_delete_request(shmemories[index].id);
    sysvshm_close();

    if (shmemories[index].fd >= 0) close(shmemories[index].fd);
    shmemory_count--;
    memmove(&shmemories[index], &shmemories[index+1], (shmemory_count - index) * sizeof(shmemory_t));
}

int shmget(key_t key, size_t size, int flags) {
    if (key != IPC_PRIVATE) return -1;
    
    pthread_mutex_lock(&mutex);
        
    sysvshm_connect();
    int shmid = sysvshm_shmget_request(size);
    if (shmid == 0) {
        sysvshm_close();
        pthread_mutex_unlock(&mutex);
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
        pthread_mutex_unlock(&mutex);
        return -1;
    }
    
    pthread_mutex_unlock(&mutex);
    return shmid;
}

void* shmat(int shmid, const void* shmaddr, int shmflg) {
    pthread_mutex_lock(&mutex);

    void* addr;
    int index = find_shmemory_index(shmid);
    if (index != -1) {
        if (shmemories[index].addr == NULL) {
            shmemories[index].addr = mmap(NULL, shmemories[index].size, PROT_READ | (shmflg == 0 ? PROT_WRITE : 0), MAP_SHARED, shmemories[index].fd, 0);
            if (shmemories[index].addr == MAP_FAILED) shmemories[index].addr = NULL;
        }
        addr = shmemories[index].addr;
    }
    
    pthread_mutex_unlock(&mutex);
    return addr ? addr : (void *)-1;
}

int shmdt(const void* shmaddr) {
    pthread_mutex_lock(&mutex);
    
    for (int i = 0; i < shmemory_count; i++) {
        if (shmemories[i].addr == shmaddr) {
            munmap(shmemories[i].addr, shmemories[i].size);
            shmemories[i].addr = NULL;
            if (shmemories[i].marked_for_delete) sysvshm_delete(i);
            break;
        }
    }    
    
    pthread_mutex_unlock(&mutex);
    return 0;
}

int shmctl(int shmid, int cmd, struct shmid_ds* buf) {
    if (cmd == IPC_RMID) {
        pthread_mutex_lock(&mutex);
        
        int index = find_shmemory_index(shmid);
        if (index != -1) {
            if (shmemories[index].addr) {
                shmemories[index].marked_for_delete = 1;
            } 
            else sysvshm_delete(index);                
        }        
        
        pthread_mutex_unlock(&mutex);
        return 0;
    } 
    else if (cmd == IPC_STAT) {
        pthread_mutex_lock(&mutex);
        
        int index = find_shmemory_index(shmid);
        if (!buf || index == -1) {
            pthread_mutex_unlock(&mutex);
            return -1;
        }
        
        memset(buf, 0, sizeof(struct shmid_ds));
        buf->shm_segsz = shmemories[index].size;
        buf->shm_nattch = 1;
        buf->shm_perm.__key = IPC_PRIVATE;
        buf->shm_perm.uid = geteuid();
        buf->shm_perm.gid = getegid();
        buf->shm_perm.cuid = geteuid();
        buf->shm_perm.cgid = getegid();
        buf->shm_perm.mode = 0666;
        buf->shm_perm.__seq = 1;
        
        pthread_mutex_unlock(&mutex);
        return 0;
    }
    return -1;
}