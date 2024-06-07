#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <linux/kthread.h>
#include <linux/eventpoll.h>
#include <linux/fdtable.h>
#include <linux/slab.h>
// Modified epoll header
#include <linux/fs.h>

#define SERVER_PORT 8848
#define CONTEXT_MAX_THREAD 32
#define NO_FAIL(reason, err_str, finally) \
  if(unlikely(ret < 0)) {err_str = reason; goto finally;}

static struct thread_context {
    struct socket *server_socket;
    struct task_struct *thread;
} thread_contexts[CONTEXT_MAX_THREAD];

static int num_threads = 1;
module_param(num_threads, int, 0644);
MODULE_PARM_DESC(num_threads, "Number of threads");

static struct socket* create_server_socket(void) {
    int ret;
    const char *err_msg;
    struct sockaddr_in server_addr;
    int optval = 1;
    sockptr_t koptval = KERNEL_SOCKPTR(&optval);
    struct socket *server_socket;

    ret = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &server_socket);
    NO_FAIL("Failed to create socket", err_msg, done);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    ret = sock_setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, koptval, sizeof(optval));
    NO_FAIL("sock_setsockopt(SO_REUSEADDR)", err_msg, done);
    ret = sock_setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, koptval, sizeof(optval));
    NO_FAIL("sock_setsockopt(SO_REUSEPORT)", err_msg, done);

    ret = kernel_bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    NO_FAIL("kernel_bind", err_msg, done);

    ret = kernel_listen(server_socket, 1024);
    NO_FAIL("kernel_listen", err_msg, done);

    return server_socket;

done:
    pr_err("%s", err_msg);
    if(server_socket) sock_release(server_socket);
    return NULL;
}


static int make_fd_and_file(struct socket *sock) {
    struct file *file;
    int fd;
    file = sock_alloc_file(sock, 0 /* NONBLOCK? */, NULL);
    if(unlikely(IS_ERR(file))) return -1;
    fd = get_unused_fd_flags(0);
    if(unlikely(fd < 0)) {
        fput(file);
        return fd;
    }
    fd_install(fd, file);
    return fd;
}


static int update_event(int epfd, int ep_ctl_flag, __poll_t ep_event_flag, /// Epoll.
                        struct socket *sockets[], int fd, struct socket *sock) { /// Sockets.
    int ret;
    struct epoll_event event;
    bool fd_is_ready = (ep_ctl_flag != EPOLL_CTL_ADD);

    if(!fd_is_ready) fd = make_fd_and_file(sock);
    if(unlikely(fd < 0)) {
        pr_warn("fd cannot allocate: %d\n", fd);
        return fd;
    }
    event.data = fd;
    event.events = ep_event_flag;
    ret = do_epoll_ctl(epfd, ep_ctl_flag, fd, &event, false /* true for io_uring only */);
    if(unlikely(ret < 0)) {
        pr_warn("do_epoll_ctl: %d\n", ret);
        return ret;
    }
    if(!fd_is_ready) sockets[fd] = sock;
    return fd;
}


static void dump_event(struct epoll_event *e) {
    bool epollin  = e->events & EPOLLIN;
    bool epollout = e->events & EPOLLOUT;
    bool epollhup = e->events & EPOLLHUP;
    bool epollerr = e->events & EPOLLERR;
    __u64 data = e->data;
    pr_info("dump: %d%d%d%d %llu\n", epollin, epollout, epollhup, epollerr, data);
}


static void event_loop(int epfd, struct epoll_event *events, const int nevents,
                       int server_fd, struct socket *server_socket, struct socket *sockets[],
                       char *drop_buffer, const size_t DROP_BUFFER) {
    int ret;

    __poll_t next_event;
    int client_fd;
    struct socket *client_socket;

    char *response =
        "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, world!";
    struct kvec vec;
    struct msghdr msg;

    for(struct epoll_event *e = &events[0]; e != &events[nevents]; e++) {
        // dump_event(e);
        if(e->data == server_fd) {
            kernel_accept(server_socket, &client_socket, 0);
            update_event(epfd, EPOLL_CTL_ADD, EPOLLIN | EPOLLHUP, sockets, -1, client_socket);
        } else {
            next_event = e->events;
            client_fd = e->data;
            client_socket = sockets[client_fd];
            if(e->events & EPOLLIN) {
                memset(&msg, 0, sizeof(msg));
                vec.iov_base = drop_buffer;
                vec.iov_len = DROP_BUFFER;
                ret = kernel_recvmsg(client_socket, &msg, &vec, 1, vec.iov_len, 0);
                // Fast check: Maybe a FIN packet and nothing is buffered (!EPOLLOUT).
                if(ret == 0 && e->events == EPOLLIN) {
                    e->events = EPOLLHUP;
                // May be an RST packet.
                } else if(unlikely(ret < 0)) {
                    pr_warn("kernel_recvmsg: %d\n", ret);
                    if(ret != -EINTR) e->events = EPOLLHUP;
                // Slower path, call (do_)epoll_ctl().
                } else {
                    next_event &= ~EPOLLIN;
                    next_event |= EPOLLOUT;
                }
            }
            if(e->events & EPOLLOUT) {
                memset(&msg, 0, sizeof(msg));
                vec.iov_base = response;
                vec.iov_len = strlen(response);

                ret = kernel_sendmsg(client_socket, &msg, &vec, 1, vec.iov_len);
                if(ret < 0) {
                    pr_warn("kernel_sendmsg: %d\n", ret);
                    if(ret != -EINTR) e->events = EPOLLHUP;
                } else {
                    next_event &= ~EPOLLOUT;
                    next_event |= EPOLLIN;
                }
            }
            if(e->events & EPOLLHUP && !(e->events & EPOLLIN)) {
                next_event = EPOLLHUP;
                ret = update_event(epfd, EPOLL_CTL_DEL, 0, sockets, client_fd, client_socket);
                if(unlikely(ret < 0)) pr_warn("update_event[HUP]: %d\n", ret);
                sock_release(client_socket);
            }
            if(likely(e->events != EPOLLHUP)) {
                ret = update_event(epfd, EPOLL_CTL_MOD, next_event,
                                    sockets, client_fd, client_socket);
                if(unlikely(ret < 0)) pr_warn("update_event[~HUP]: %d\n", ret);
            }
        }
    }
}

static int server_thread(void *data) {
    /// Control flows.

    int ret;
    const char *err_msg;
    struct thread_context *context = data;

    /// Sockets.

    int server_fd;
    struct socket *server_socket = context->server_socket;
    // Limited by fd size. 1024 is enough for test.
    // Usage: sockets[fd] = socket_ptr.
    const size_t SOCKETS = 1024;
    struct socket **sockets;

    /// Buffers.

    const size_t DROP_BUFFER = 1024;
    char *drop_buffer;

    /// Epoll.

    int epfd = -1;
    const size_t EVENTS = 1024;
    int nevents;
    struct epoll_event *events;

    sockets = kmalloc_array(SOCKETS, sizeof(struct socket*), GFP_KERNEL);
    events = kmalloc_array(EVENTS, sizeof(struct epoll_event), GFP_KERNEL);
    drop_buffer = kmalloc(DROP_BUFFER, GFP_KERNEL);
    ret = (sockets && events && drop_buffer) ? 0 : -ENOMEM;
    NO_FAIL("kmalloc[s|e|d]", err_msg, done);

    // Debug only.
    (void)dump_event;

    allow_signal(SIGKILL);

    ret = do_epoll_create(0);
    NO_FAIL("do_epoll_create", err_msg, done);
    epfd = ret;

    ret = update_event(epfd, EPOLL_CTL_ADD, EPOLLIN, sockets, -1, server_socket);
    NO_FAIL("update_event", err_msg, done);
    server_fd = ret;

    while(!kthread_should_stop()) {
        ret = do_epoll_wait(epfd, &events[0], EVENTS, NULL /* wait_ms == -1 */);
        NO_FAIL("do_epoll_wait", err_msg, done);
        nevents = ret;
        event_loop(epfd, events, nevents,
                   server_fd, server_socket, sockets,
                   drop_buffer, DROP_BUFFER);
    }

done:
    if(ret < 0) pr_err("%s: %d\n", err_msg, ret);
    if(~epfd) close_fd(epfd);
    if(events) kfree(events);
    if(drop_buffer) kfree(drop_buffer);
    if(sockets) kfree(sockets);
    context->thread = NULL;
    // TODO record max_fd and destroy.
    return ret;
}

static int each_server_init(struct thread_context *context) {
    context->server_socket = create_server_socket();
    if(!context->server_socket) {
        return -1;
    }

    context->thread = kthread_run(server_thread, context, "in_kernel_web_server");
    pr_info("worker thread id: %d\n", context->thread->pid);

    if(IS_ERR(context->thread)) {
        pr_err("Failed to create thread\n");
        return PTR_ERR(context->thread);
    }

    return 0;
}

static void each_server_exit(struct thread_context *context) {
    struct task_struct *thread = context->thread;
    struct socket *socket = context->server_socket;
    if(thread) {
        send_sig(SIGKILL, thread, 1);
        kthread_stop(thread);
    }
    if(socket) {
        sock_release(socket);
    }
}


static int __init simple_web_server_init(void) {
    int threads = num_threads;
    if(threads >= CONTEXT_MAX_THREAD || threads < 1) {
        pr_err("num_threads < (CONTEXT_MAX_THREAD=32)\n");
        return -1;
    }
    for(int i = 0; i < threads; ++i) {
        if(each_server_init(&thread_contexts[i])) return -1;
    }
    pr_info("Simple Web Server Initialized\n");
    return 0;
}


static void __exit simple_web_server_exit(void) {
    struct thread_context *context;
    int threads = num_threads;
    for(context = &thread_contexts[0]; threads--; context++) {
        each_server_exit(context);
    }
    pr_info("Simple Web Server Exited\n");
}


module_init(simple_web_server_init);
module_exit(simple_web_server_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Caturra");
MODULE_DESCRIPTION("Simple In-Kernel Web Server");
