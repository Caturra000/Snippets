#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <linux/kthread.h>

#define SERVER_PORT 8848
#define NO_FAIL(reason, str, finally) \
 if(ret < 0) {err_msg = reason; goto finally;}

static struct socket *server_socket = NULL;
static struct task_struct *thread_st;

static int create_server_socket(void) {
    struct sockaddr_in server_addr;
    int ret;
    int optval = 1;
    sockptr_t koptval = KERNEL_SOCKPTR(&optval);
    char *err_msg;

    ret = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &server_socket);
    NO_FAIL("Failed to create socket\n", err_msg, err_routine);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_PORT);

    ret = sock_setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, koptval, sizeof(optval));
    NO_FAIL("Failed to set SO_REUSEADDR\n", err_msg, err_routine);

    ret = kernel_bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    NO_FAIL("Failed to bind socket\n", err_msg, err_routine);

    ret = kernel_listen(server_socket, 5);
    NO_FAIL("Failed to listen on socket\n", err_msg, err_routine);

    return 0;

err_routine:
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

    allow_signal(SIGKILL);

    while(!kthread_should_stop()) {
        ret = sock_create_lite(PF_INET, SOCK_STREAM, IPPROTO_TCP, &conn_socket);
        NO_FAIL("Failed to create connection socket\n", err_msg, err_routine);

        ret = kernel_accept(server_socket, &conn_socket, 0);
        NO_FAIL("Failed to accept connection\n", err_msg, err_routine);

        memset(&msg, 0, sizeof(msg));
        vec.iov_base = response;
        vec.iov_len = strlen(response);

        ret = kernel_sendmsg(conn_socket, &msg, &vec, 1, vec.iov_len);
        NO_FAIL("Failed to send response\n", err_msg, err_routine);

        sock_release(conn_socket);
    }

    return 0;

err_routine:
    printk(KERN_ERR "%s", err_msg);
    if(conn_socket) sock_release(conn_socket);
    return ret;
}

// static void reactor() {
//     int epfd = do_epoll_create();
//     struct timespec64 to;
//     do_epoll_wait(epfd, NULL, 1, get_timespec64(&to, 1));
// }

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
