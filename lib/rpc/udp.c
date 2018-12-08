#include "rpc/udp.h"
#include "stdnetwork.h"
#include <arpa/inet.h>

static int get_sock(struct RpcCtrl* ctrl)
{
    return ((struct UdpRpcCtrl*)ctrl->ctrl)->sock;
}

static struct sockaddr_in* get_sockaddr(struct RpcCtrl* ctrl, int is_recv)
{
    CVT(struct UdpRpcCtrl*, uctrl, ctrl->ctrl);
    if (is_recv)
    {
        if (uctrl->is_server) 
        {
            return uctrl->sockaddr_sender;
        }
        else
        {
            return uctrl->sockaddr_bind;
        }
    }
    else
    {
        if (uctrl->is_server) 
        {
            return uctrl->sockaddr_sender;
        }
        else
        {
            return uctrl->sockaddr_bind;
        }
    }
}

int open_udp_sock(int32_t ip, int port, struct sockaddr_in** addr, int need_bind)
{
    int sock;
    struct sockaddr_in* bind_addr = (struct sockaddr_in*)calloc(1, sizeof(struct sockaddr_in));
    
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    
    if(sock == -1) {
        ERROR("ERROR creating socket!");
        exit(-1);
    }
    
    /* set details for socket to bind */
    bind_addr->sin_family = AF_INET;
    bind_addr->sin_addr.s_addr = ip;  /* bind to all interfaces */
    /* htons = host to network byte order, necessary for universal understanding by all machines */
    bind_addr->sin_port = htons(port);
    
    if (need_bind)
    {
        if(bind(sock, (struct sockaddr *) bind_addr, sizeof(*bind_addr)) < 0) {
            close_socket(sock);
            ERROR("ERROR binding!\n");
            exit(EXIT_FAILURE);
        } else {
            printf("Bound successfully!\n");
        }
    }
    /* bind socket */
    *addr = bind_addr;
    return sock;
}

static void udp_receive_packet(int sock, void *buffer, size_t bufsize, struct sockaddr_in *sender) {
    socklen_t senlen = sizeof(*sender);
    
    /* recv and log time */
    ssize_t bytes_recv = recvfrom(sock, buffer, bufsize, 0, (struct sockaddr *) sender, &senlen);

    /* check for error */
    // if(unlikely(bytes_recv < 0)) {
    //     close_socket(sock);
    //     ERROR("ERROR receiving!");
    //     exit(EXIT_FAILURE);
    // }
    
    return;
}

static void udp_send_packet(int sock, void *data, size_t data_size, struct sockaddr_in *receiver) {
    /* send and log time */
    ssize_t bytes_sent = sendto(sock, data, data_size, 0, (struct sockaddr *) receiver, sizeof(*receiver));

    /* check for error */
    if(unlikely(bytes_sent < 0)) {
        close_socket(sock);
        ERROR("ERROR sending!");
        exit(EXIT_FAILURE);
    }

    return;
}

struct RpcCtrl* udp_start_server(int port)
{
    struct RpcCtrl* ctrl = (struct RpcCtrl*)calloc(1, sizeof(struct RpcCtrl));
    struct UdpRpcCtrl* udp_ctrl = (struct UdpRpcCtrl*)malloc(sizeof(struct UdpRpcCtrl));
    udp_ctrl->sock = open_udp_sock(INADDR_ANY, port, &udp_ctrl->sockaddr_bind, 1);
    ctrl->ctrl = udp_ctrl;
    udp_ctrl->is_server = 1;
    udp_ctrl->sockaddr_sender = (struct sockaddr_in*)calloc(1, sizeof(*udp_ctrl->sockaddr_sender));

    // clear old datagrams
    struct sockaddr_in tmp;
    socklen_t senlen;
    char buffer[8192];
    set_socket_timeout(udp_ctrl->sock, 10);
    while (recvfrom(udp_ctrl->sock, buffer, 8192, 0, (struct sockaddr*)&tmp, &senlen) >= 0);
    set_socket_timeout(udp_ctrl->sock, 0);

    return ctrl;
}

struct RpcCtrl* udp_connect_to_server(const char* addr, int port)
{
    struct RpcCtrl* ctrl = (struct RpcCtrl*)calloc(1, sizeof(struct RpcCtrl));
    struct UdpRpcCtrl* udp_ctrl = (struct UdpRpcCtrl*)malloc(sizeof(struct UdpRpcCtrl));
    udp_ctrl->sock = open_udp_sock(inet_addr(addr), port, &udp_ctrl->sockaddr_bind, 0);
    ctrl->ctrl = udp_ctrl;
    udp_ctrl->is_server = 0;
    return ctrl;
}

void udp_send(struct RpcCtrl* ctrl, struct RpcHead* head)
{

    udp_send_packet(get_sock(ctrl), head, head->body_length + sizeof(struct RpcHead), get_sockaddr(ctrl, 0));
}

void udp_recv(struct RpcCtrl* ctrl, struct RpcHead* head)
{
    udp_receive_packet(get_sock(ctrl), head, head->body_length + sizeof(struct RpcHead), get_sockaddr(ctrl, 1));
}

void udp_set_timeout(struct RpcCtrl* ctrl, int timeout_msec)
{
    set_socket_timeout(get_sock(ctrl), timeout_msec);
}