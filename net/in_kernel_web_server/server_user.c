#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <assert.h>

// https://elixir.bootlin.com/linux/v6.4.8/source/include/linux/compiler.h#L76
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define pr_info(...) pr_warn(__VA_ARGS__)
#define pr_warn(...) pr_err(__VA_ARGS__)
#define pr_err(...) fprintf(stderr, __VA_ARGS__)

#define SERVER_PORT 8848
#define CONTEXT_MAX_THREAD 32
#define NO_FAIL(reason, err_str, finally) \
  if(unlikely(ret < 0)) {err_str = reason; goto finally;}
#define USERSPACE_UNSED

static struct thread_context {
    int server_fd;
    pthread_t thread;
} thread_contexts[CONTEXT_MAX_THREAD];

struct socket_context {
    void *_placeholder_;
    size_t _r_n;
    size_t responses;
};

// 1st argument.
static int num_threads = 1;
// 2nd argument. Set any non-zero number to enable zerocopy feature.
// https://www.kernel.org/doc/html/v6.4/networking/msg_zerocopy.html
static int zerocopy_flag = 0;

static int create_server_socket(void) {
    int ret;
    int fd = -1;
    const char *err_msg;
    struct sockaddr_in server_addr;
    int optval = 1;

    ret = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    NO_FAIL("socket", err_msg, done);
    fd = ret;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    NO_FAIL("setsockopt(SO_REUSEADDR)", err_msg, done);
    ret = setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
    NO_FAIL("setsockopt(SO_REUSEPORT)", err_msg, done);

    ret = bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    NO_FAIL("bind", err_msg, done);

    ret = listen(fd, 1024);
    NO_FAIL("listen", err_msg, done);

    return fd;

done:
    pr_err("%s: %d, %d\n", err_msg, ret, errno);
    if(~fd) close(fd);
    return ret;
}


static int make_fd_and_file(int fd) {
    return fd;
}


static int update_event(int epfd, int ep_ctl_flag, uint32_t ep_event_flag, /// Epoll.
                        struct socket_context context[], int fd) { /// Sockets.
    int ret;
    struct epoll_event event;
    bool fd_is_ready = (ep_ctl_flag != EPOLL_CTL_ADD);

    if(!fd_is_ready) fd = make_fd_and_file(fd);
    if(unlikely(fd < 0)) {
        pr_warn("fd cannot allocate: %d\n", fd);
        return fd;
    }
    event.data.fd = fd;
    event.events = ep_event_flag;
    ret = epoll_ctl(epfd, ep_ctl_flag, fd, &event);
    if(unlikely(ret < 0)) {
        pr_warn("epoll_ctl: %d\n", ret);
        return ret;
    }
    if(!fd_is_ready) context[fd]._placeholder_ = (void*)1;
    return fd;
}


static void dump_event(struct epoll_event *e) {
    bool epollin  = e->events & EPOLLIN;
    bool epollout = e->events & EPOLLOUT;
    bool epollhup = e->events & EPOLLHUP;
    bool epollerr = e->events & EPOLLERR;
    int data = e->data.fd;
    pr_info("dump: %d%d%d%d %d\n", epollin, epollout, epollhup, epollerr, data);
}


static int wrk_parse(struct socket_context *context, const char *buffer, int nread) {
    int _r_n = context->_r_n;
    int requests = 0;
    for(const char *c = buffer; c != buffer + nread; c++) {
        if(*c == '\r' || *c == '\n') {
            // `wrk` must send three \r\n per request by default.
            if(++_r_n == 6) ++requests, _r_n = 0;
        }
    }
    context->_r_n = _r_n;
    // 1:1 response to request.
    context->responses += requests;
    return requests;
}


static void event_loop(int epfd, struct epoll_event *events, const int nevents,
                       int server_fd, struct socket_context sockets[],
                       struct msghdr *read_msg, struct msghdr * write_msg,
                       const int MAX_RESPONSES) {
    int ret;

    uint32_t next_event;
    uint32_t current_event;
    int client_fd;
    struct socket_context *client_context;
    const char *read_buffer;

    int requests;
    int responses;

    for(struct epoll_event *e = &events[0]; e != &events[nevents]; e++) {
        if(e->data.fd == server_fd) {
            client_fd = accept(server_fd, NULL, NULL);
            update_event(epfd, EPOLL_CTL_ADD, EPOLLIN | EPOLLHUP, sockets, client_fd);
        } else {
            current_event = e->events;
            next_event = e->events;
            client_fd = e->data.fd;
            client_context = &sockets[client_fd];
            if(e->events & EPOLLIN) {
                ret = recvmsg(client_fd, read_msg, 0);
                // Fast check: Maybe a FIN packet and nothing is buffered (!EPOLLOUT).
                if(ret == 0 && e->events == EPOLLIN) {
                    e->events = EPOLLHUP;
                // May be an RST packet.
                } else if(unlikely(ret < 0)) {
                    if(errno != EINTR) e->events = EPOLLHUP;
                // Slower path, may call (do_)epoll_ctl().
                } else {
                    read_buffer = read_msg->msg_iov->iov_base;
                    requests = wrk_parse(client_context, read_buffer, ret);
                    // Keep reading if there is no complete request.
                    // Otherwise disable EPOLLIN.
                    // FIXME. always enable? Cost more "syscall"s?
                    if(requests) next_event &= ~EPOLLIN;
                    // There are some pending responses to be send.
                    if(client_context->responses) next_event |= EPOLLOUT;
                }
            }
            if(e->events & EPOLLOUT) {
                assert(client_context->responses != 0);
                responses = client_context->responses;
                if(responses >= MAX_RESPONSES) {
                    responses = MAX_RESPONSES - 1;
                }
                // >= 0
                client_context->responses -= responses;
                write_msg->msg_iovlen = responses;

                ret = sendmsg(client_fd, write_msg, zerocopy_flag);
                if(ret < 0) {
                    pr_warn("kernel_sendmsg: %d, %d\n", ret, errno);
                    if(errno != EINTR) e->events = EPOLLHUP;
                } else {
                    if(!client_context->responses) next_event &= ~EPOLLOUT;
                    next_event |= EPOLLIN;
                }
            }
            if((e->events & EPOLLHUP) && !(e->events & EPOLLIN)) {
                ret = update_event(epfd, EPOLL_CTL_DEL, 0, sockets, client_fd);
                if(unlikely(ret < 0)) pr_warn("update_event[HUP]: %d, %d\n", ret, errno);
                close(client_fd);
                memset(client_context, 0, sizeof (struct socket_context));
            }
            // Not necessary to compare the current event,
            // but avoid duplicate syscall.
            if(e->events != EPOLLHUP && current_event != next_event) {
                ret = update_event(epfd, EPOLL_CTL_MOD, next_event,
                                    sockets, client_fd);
                if(unlikely(ret < 0)) pr_warn("update_event[~HUP]: %d, %d\n", ret, errno);
            }
        }
    }
}


static void* server_thread(void *data) {
    /// Control flows.

    int ret;
    const char *err_msg;
    struct thread_context *context = data;

    /// Sockets.

    int server_fd = context->server_fd;
    // Limited by fd size. 1024 is enough for test.
    // Usage: sockets[fd].socket = socket_ptr.
    const size_t SOCKETS = 1024;
    struct socket_context *sockets = NULL;

    /// Buffers.

    const size_t READ_BUFFER = 4096;
    char *read_buffer = NULL;
    char *response_content =
        "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, world!";
    const int response_content_len = strlen(response_content);
    const int MAX_RESPONSES = 32;
    struct iovec request_vec;
    struct iovec response_vec[32] = {
        [0 ... 31] = {
            .iov_base = response_content,
            .iov_len = response_content_len,
        }
    };
    struct msghdr read_msg = {
        .msg_iov = &request_vec,
        .msg_iovlen = 1,
    };
    struct msghdr write_msg = {
        .msg_iov = response_vec,
        // .msg_iovlen =  /* Modified in event_loop(). */
    };

    /// Epoll.

    int epfd = -1;
    const size_t EVENTS = 1024;
    int nevents;
    struct epoll_event *events = NULL;

    sockets = malloc(SOCKETS * sizeof(struct socket_context));
    memset(sockets, 0, SOCKETS * sizeof(struct socket_context));
    events = malloc(EVENTS * sizeof(struct epoll_event));
    read_buffer = malloc(READ_BUFFER);
    ret = (sockets && events && read_buffer) ? 0 : -ENOMEM;
    NO_FAIL("kmalloc[s|e|d]", err_msg, done);
    request_vec.iov_base = read_buffer;
    request_vec.iov_len = READ_BUFFER;

    /////////////////////////////////////////////////////////////////////////////////

    // Debug only.
    (void)dump_event;

    ret = epoll_create(1);
    NO_FAIL("epoll_create", err_msg, done);
    epfd = ret;

    ret = update_event(epfd, EPOLL_CTL_ADD, EPOLLIN, sockets, server_fd);
    NO_FAIL("update_event", err_msg, done);
    server_fd = ret;

    // FIXME. NO check flag.
    while(true) {
        ret = epoll_wait(epfd, &events[0], EVENTS, -1);
        NO_FAIL("epoll_wait", err_msg, done);
        nevents = ret;
        event_loop(epfd, events, nevents, // Epoll
                   server_fd, sockets, // Socket
                   &read_msg, &write_msg, // Iterators
                   MAX_RESPONSES);
    }

done:
    if(ret < 0) pr_err("%s: %d, %d\n", err_msg, ret, errno);
    if(~epfd) close(epfd);
    if(events) free(events);
    if(read_buffer) free(read_buffer);
    // Server is included.
    if(sockets) {
        for(size_t i = 0; i < SOCKETS; i++) {
            if(sockets[i]._placeholder_) close(i);
        }
        free(sockets);
    }
    return NULL;
}

static int each_server_init(struct thread_context *context) {
    int ret;
    context->server_fd = create_server_socket();
    if(!context->server_fd) {
        return -1;
    }

    ret = pthread_create(&context->thread, NULL, server_thread, context);

    if(ret < 0) {
        pr_err("Failed to create thread\n");
        return ret;
    }

    pr_info("worker pthread id: %lu\n", context->thread);
    return 0;
}

static void each_server_exit(struct thread_context *context) {
    pthread_cancel(context->thread);
    pthread_join(context->thread, NULL);
}


static int simple_web_server_init(void) {
    int threads = num_threads;
    if(threads >= CONTEXT_MAX_THREAD || threads < 1) {
        pr_err("num_threads < (CONTEXT_MAX_THREAD=32)\n");
        return -1;
    }
    for(int i = 0; i < threads; ++i) {
        if(each_server_init(&thread_contexts[i])) {
            pr_err("Boot failed\n");
            for(--i; ~i; i--) {
                each_server_exit(&thread_contexts[i]);
            }
            return -1;
        }
    }
    pr_info("Simple Web Server Initialized\n");
    return 0;
}


static void simple_web_server_exit(void) {
    struct thread_context *context;
    int threads = num_threads;
    for(context = &thread_contexts[0]; threads--; context++) {
        each_server_exit(context);
    }
    pr_info("Simple Web Server Exited\n");
}


int main(int argc, char *argv[]) {
    num_threads = argc > 1 ? atoi(argv[1]) : 1;
    zerocopy_flag = argc > 2 ? atoi(argv[2]) : 0;
    zerocopy_flag = zerocopy_flag ? MSG_ZEROCOPY : 0;
    if(num_threads < 1 || num_threads >= CONTEXT_MAX_THREAD) {
        pr_err("num_threads < (CONTEXT_MAX_THREAD=32)\n");
        return 1;
    }
    if(zerocopy_flag == MSG_ZEROCOPY) {
        pr_info("Enable MSG_ZEROCOPY.\n");
    }
    simple_web_server_init();
    // Press any key...
    getchar();
    simple_web_server_exit();
}
