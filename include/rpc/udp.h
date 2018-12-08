#pragma once
#include "rpc/common.h"

struct UdpRpcCtrl
{
    int sock;
    struct sockaddr_in* sockaddr_bind, *sockaddr_sender;
    int is_server;
};

struct RpcCtrl* udp_start_server(int port);
struct RpcCtrl* udp_connect_to_server(const char* addr, int port);
void udp_send(struct RpcCtrl* ctrl, struct RpcHead* head);
void udp_recv(struct RpcCtrl* ctrl, struct RpcHead* head);
void udp_set_timeout(struct RpcCtrl* ctrl, int timeout_msec);

