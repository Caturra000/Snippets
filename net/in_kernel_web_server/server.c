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
#define NO_FAIL(reason, err_str, finally) \
 if(unlikely(ret < 0)) {err_str = reason; goto finally;}

static struct socket *server_socket = NULL;
static struct task_struct *thread_st;

static int create_server_socket(void) {
    struct sockaddr_in server_addr;
    int ret;
    int optval = 1;
    sockptr_t koptval = KERNEL_SOCKPTR(&optval);
    char *err_msg;

    ret = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &server_socket);
    NO_FAIL("Failed to create socket", err_msg, done);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    ret = sock_setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, koptval, sizeof(optval));
    NO_FAIL("Failed to set SO_REUSEADDR", err_msg, done);

    ret = kernel_bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    NO_FAIL("Failed to bind socket", err_msg, done);

    ret = kernel_listen(server_socket, 1024);
    NO_FAIL("Failed to listen on socket", err_msg, done);

    return 0;

done:
    printk(KERN_ERR "%s", err_msg);
    if(server_socket) sock_release(server_socket);
    return ret;
}

static int server_thread(void *data) {
    int ret;
    struct socket *conn_socket = NULL;
    char *response =
        "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, world!";
    struct kvec vec;
    struct msghdr msg;
    char *err_msg;
    int epfd = -1;
    const int EVENTS = 1024;
    int nevents;
    struct epoll_event *events;
    struct epoll_event accept_event;
    struct file *file;
    int fd;

    events = kmalloc(EVENTS * sizeof(struct epoll_event), GFP_KERNEL);

    ret = do_epoll_create(0);
    NO_FAIL("Failed to create epoll instance", err_msg, done);
    epfd = ret;

    file = sock_alloc_file(server_socket, 0 /* NONBLOCK? */, NULL);
    if(IS_ERR(file)) {
        ret = -1;
        NO_FAIL("Failed to allocate file", err_msg, done);
    }
    ret = get_unused_fd_flags(0);
    NO_FAIL("Failed to get fd", err_msg, done);
    fd = ret;
    fd_install(fd, file);

    memset(&accept_event, 0, sizeof(struct epoll_event));
    memcpy(&accept_event.data, &server_socket, sizeof(struct socket *));
    accept_event.events = EPOLLIN;
    ret = do_epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &accept_event, false /* true for io_uring only */);
    NO_FAIL("Failed to register acceptor event", err_msg, done);

    allow_signal(SIGKILL);

    while(!kthread_should_stop()) {
        ret = do_epoll_wait(epfd, &events[0], EVENTS, NULL /* wait_ms == -1 */);
        NO_FAIL("Failed to wait epoll events", err_msg, done);
        nevents = ret;
        if(nevents > 0) {
            ret = kernel_accept(server_socket, &conn_socket, 0);
            NO_FAIL("Failed to accept connection", err_msg, done);

            memset(&msg, 0, sizeof(msg));
            vec.iov_base = response;
            vec.iov_len = strlen(response);

            ret = kernel_sendmsg(conn_socket, &msg, &vec, 1, vec.iov_len);
            NO_FAIL("Failed to send response", err_msg, done);

            sock_release(conn_socket);
            conn_socket = NULL;
        }
    }

done:
    if(ret < 0) printk(KERN_ERR "%s: %d\n", err_msg, ret);
    if(~epfd) close_fd(epfd);
    if(events) kfree(events);
    if(conn_socket) sock_release(conn_socket);
    thread_st = NULL;
    return ret;
}

static int __init simple_web_server_init(void) {
    int ret;

    ret = create_server_socket();
    if(ret < 0) {
        return ret;
    }

    thread_st = kthread_run(server_thread, NULL, "in_kernel_web_server");
    printk(KERN_INFO "worker thread id: %d\n", thread_st->pid);

    if(IS_ERR(thread_st)) {
        printk(KERN_ERR "Failed to create thread\n");
        return PTR_ERR(thread_st);
    }

    printk(KERN_INFO "Simple Web Server Initialized\n");

    return 0;
}

static void __exit simple_web_server_exit(void) {
    if(thread_st) {
        send_sig(SIGKILL, thread_st, 1);
        kthread_stop(thread_st);
    }

    if(server_socket) {
        sock_release(server_socket);
    }

    printk(KERN_INFO "Simple Web Server Exited\n");
}

module_init(simple_web_server_init);
module_exit(simple_web_server_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Caturra");
MODULE_DESCRIPTION("Simple In-Kernel Web Server");
