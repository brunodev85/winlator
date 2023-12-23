#include <jni.h>
#include <sys/epoll.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/eventfd.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <jni.h>
#include <android/log.h>

#define printf(...) __android_log_print(ANDROID_LOG_DEBUG, "System.out", __VA_ARGS__);
#define MAX_EVENTS 10
#define MAX_FDS 32

struct epoll_event events[MAX_EVENTS];

JNIEXPORT jint JNICALL
Java_com_winlator_xconnector_XConnectorEpoll_createAFUnixSocket(JNIEnv *env, jobject obj,
                                                                jstring path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sun_family = AF_LOCAL;

    const char *pathPtr = (*env)->GetStringUTFChars(env, path, 0);

    int addrLength = sizeof(sa_family_t) + strlen(pathPtr);
    strncpy(serverAddr.sun_path, pathPtr, sizeof(serverAddr.sun_path) - 1);

    (*env)->ReleaseStringUTFChars(env, path, pathPtr);

    unlink(serverAddr.sun_path);
    if (bind(fd, (struct sockaddr*) &serverAddr, addrLength) < 0) goto error;
    if (listen(fd, MAX_EVENTS) < 0) goto error;

    return fd;
    error:
    close(fd);
    return -1;
}

JNIEXPORT jint JNICALL
Java_com_winlator_xconnector_XConnectorEpoll_createEpollFd(JNIEnv *env, jobject obj) {
    return epoll_create(MAX_EVENTS);
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XConnectorEpoll_closeFd(JNIEnv *env, jobject obj, jint fd) {
    close(fd);
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_xconnector_XConnectorEpoll_doEpollIndefinitely(JNIEnv *env, jobject obj,
                                                                 jint epollFd, jint serverFd,
                                                                 jboolean addClientToEpoll) {
    jclass cls = (*env)->GetObjectClass(env, obj);
    jmethodID handleNewConnection = (*env)->GetMethodID(env, cls, "handleNewConnection", "(I)V");
    jmethodID handleExistingConnection = (*env)->GetMethodID(env, cls, "handleExistingConnection", "(I)V");

    int numFds = epoll_wait(epollFd, events, MAX_EVENTS, -1);
    for (int i = 0; i < numFds; i++) {
        if (events[i].data.fd == serverFd) {
            int clientFd = accept(serverFd, NULL, NULL);
            if (clientFd >= 0) {
                if (addClientToEpoll) {
                    struct epoll_event event;
                    event.data.fd = clientFd;
                    event.events = EPOLLIN;

                    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, clientFd, &event) >= 0) {
                        (*env)->CallVoidMethod(env, obj, handleNewConnection, clientFd);
                    }
                }
                else (*env)->CallVoidMethod(env, obj, handleNewConnection, clientFd);
            }
        }
        else if (events[i].events & EPOLLIN) {
            (*env)->CallVoidMethod(env, obj, handleExistingConnection, events[i].data.fd);
        }
    }

    return numFds >= 0;
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_xconnector_XConnectorEpoll_addFdToEpoll(JNIEnv *env, jobject obj,
                                                          jint epollFd,
                                                          jint fd) {
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if (epoll_ctl(epollFd, EPOLL_CTL_ADD, fd, &event) < 0) return JNI_FALSE;
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_winlator_xconnector_XConnectorEpoll_removeFdFromEpoll(JNIEnv *env, jobject obj,
                                                               jint epollFd, jint fd) {
    epoll_ctl(epollFd, EPOLL_CTL_DEL, fd, NULL);
}

JNIEXPORT jint JNICALL
Java_com_winlator_xconnector_ClientSocket_read(JNIEnv *env, jobject obj, jint fd, jobject data,
                                               jint offset, jint length) {
    char *dataAddr = (*env)->GetDirectBufferAddress(env, data);
    return read(fd, dataAddr + offset, length);
}

JNIEXPORT jint JNICALL
Java_com_winlator_xconnector_ClientSocket_write(JNIEnv *env, jobject obj, jint fd, jobject data,
                                                jint length) {
    char *dataAddr = (*env)->GetDirectBufferAddress(env, data);
    return write(fd, dataAddr, length);
}

JNIEXPORT jint JNICALL
Java_com_winlator_xconnector_XConnectorEpoll_createEventFd(JNIEnv *env, jobject obj) {
    return eventfd(0, EFD_NONBLOCK);
}

JNIEXPORT jint JNICALL
Java_com_winlator_xconnector_ClientSocket_recvAncillaryMsg(JNIEnv *env, jobject obj, jint clientFd, jobject data,
                                                           jint offset, jint length) {
    char *dataAddr = (*env)->GetDirectBufferAddress(env, data);

    struct iovec iovmsg = {.iov_base = dataAddr + offset, .iov_len = length};
    struct {
        struct cmsghdr align;
        int fds[MAX_FDS];
    } ctrlmsg;

    struct msghdr msg = {
        .msg_name = NULL,
        .msg_namelen = 0,
        .msg_iov = &iovmsg,
        .msg_iovlen = 1,
        .msg_control = &ctrlmsg,
        .msg_controllen = sizeof(struct cmsghdr) + MAX_FDS * sizeof(int)
    };

    int size = recvmsg(clientFd, &msg, 0);

    if (size >= 0) {
        struct cmsghdr *cmsg;
        for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
                int numFds = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
                if (numFds > 0) {
                    jclass cls = (*env)->GetObjectClass(env, obj);
                    jmethodID addAncillaryFd = (*env)->GetMethodID(env, cls, "addAncillaryFd", "(I)V");
                    for (int i = 0; i < numFds; i++) {
                        int ancillaryFd = ((int*)CMSG_DATA(cmsg))[i];
                        (*env)->CallVoidMethod(env, obj, addAncillaryFd, ancillaryFd);
                    }
                }
            }
        }
    }
    return size;
}

JNIEXPORT jint JNICALL
Java_com_winlator_xconnector_ClientSocket_sendAncillaryMsg(JNIEnv *env, jobject obj, jint clientFd,
                                                           jobject data, jint length, jint ancillaryFd) {
    char *dataAddr = (*env)->GetDirectBufferAddress(env, data);

    struct iovec iovmsg = {.iov_base = dataAddr, .iov_len = length};
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
    ((int*)CMSG_DATA(cmsg))[0] = ancillaryFd;

    return sendmsg(clientFd, &msg, 0);
}

JNIEXPORT jboolean JNICALL
Java_com_winlator_xconnector_XConnectorEpoll_waitForSocketRead(JNIEnv *env, jobject obj, jint clientFd, jint shutdownFd) {
    struct pollfd pfds[2];
    pfds[0].fd = clientFd;
    pfds[0].events = POLLIN;

    pfds[1].fd = shutdownFd;
    pfds[1].events = POLLIN;

    int res = poll(pfds, 2, -1);
    if (res < 0 || (pfds[1].revents & POLLIN)) return JNI_FALSE;

    if (pfds[0].revents & POLLIN) {
        jclass cls = (*env)->GetObjectClass(env, obj);
        jmethodID handleExistingConnection = (*env)->GetMethodID(env, cls, "handleExistingConnection", "(I)V");
        (*env)->CallVoidMethod(env, obj, handleExistingConnection, clientFd);
    }
    return JNI_TRUE;
}