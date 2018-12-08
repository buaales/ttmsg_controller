#pragma once
#include <stdlib.h>

enum RpcType
{
    INIT,
    CREATE_CHANNEL,
    INIT_DONE,

    MSG_SERVER_READY,
    MSG_SERVER_START,
    MSG_SERVER_DATA,
};

struct RpcHead
{
    enum RpcType rpc_type;
    size_t body_length;
    char src_name[128];
};

struct RpcCtrl
{
    enum {
        UDP,
        BARRELFISH,
        MEMSHARED,
    } type;
    void *ctrl;
};
