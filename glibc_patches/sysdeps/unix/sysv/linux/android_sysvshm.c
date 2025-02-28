#include "android_sysvshm.h"

#define REQUEST_CODE_SHMGET 0
#define REQUEST_CODE_GET_FD 1
#define REQUEST_CODE_DELETE 2

/* based on https://github.com/pelya/android-shmem */

#define MIN_REQUEST_LENGTH 5

shmemory_t* shmemories = NULL;
int shmemory_count = 0;
int sysvshm_server_fd = -1;
pthread_mutex_t sysvshm_mutex = PTHREAD_MUTEX_INITIALIZER;

int find_shmemory_index(int shmid) {
    for (int i = 0; i < shmemory_count; i++) if (shmemories[i].id == shmid) return i;
    return -1;
}

void sysvshm_connect(void) {
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

void sysvshm_close(void) {
    if (sysvshm_server_fd >= 0) {
        close(sysvshm_server_fd);
        sysvshm_server_fd = -1;
    }
}

int sysvshm_shmget_request(size_t size) {
    if (sysvshm_server_fd < 0) return 0;
    
    char request_data[MIN_REQUEST_LENGTH];
    request_data[0] = REQUEST_CODE_SHMGET;
    *(int*)(request_data + 1) = size;
    
    int res = write(sysvshm_server_fd, request_data, sizeof(request_data));
    if (res < 0) return 0;
    
    int shmid;
    res = read(sysvshm_server_fd, &shmid, 4);
    return res == 4 ? shmid : 0;
}

int sysvshm_get_fd_request(int shmid) {
    if (sysvshm_server_fd < 0) return 0;
    
    char request_data[MIN_REQUEST_LENGTH];
    request_data[0] = REQUEST_CODE_GET_FD;
    *(int*)(request_data + 1) = shmid;
    
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

bool sysvshm_delete_request(int shmid) {
    if (sysvshm_server_fd < 0) return false;
    
    char request_data[MIN_REQUEST_LENGTH];
    request_data[0] = REQUEST_CODE_DELETE;
    *(int*)(request_data + 1) = shmid;
    
    int res = write(sysvshm_server_fd, request_data, sizeof(request_data));
    return res > 0 ? true : false;
}

void sysvshm_delete(int index) {
    sysvshm_connect();
    sysvshm_delete_request(shmemories[index].id);
    sysvshm_close();

    if (shmemories[index].fd >= 0) close(shmemories[index].fd);
    shmemory_count--;
    memmove(&shmemories[index], &shmemories[index+1], (shmemory_count - index) * sizeof(shmemory_t));
}