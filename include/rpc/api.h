#pragma once
#include "rpc/common.h"
#include <unistd.h>

enum ChannelDirection
{
    SEND,
    RECV,
};

struct Channel
{
    char app_name[128];
    enum ChannelDirection direction;
    char buffer[4096];
    volatile int has_data;
};

struct RpcInit
{
    struct RpcHead head;
    int done;
};

struct RpcCreateChannel
{
    struct RpcHead head;
    enum ChannelDirection direction;
    char shm_filename[1024];
};

struct RpcInitDone
{
    struct RpcHead head;
};

struct RpcMsgServerReady
{
    struct RpcHead head;
    int done;
};

struct RpcMsgServerStart
{
    struct RpcHead head;
};

struct RpcMsgServerData
{
    struct RpcHead head;
    char data[4096];
};

static void wait_channel(struct Channel* chan, int max_wait_ms, int max_test_count)
{
    int i = max_test_count;
    while (!chan->has_data && i--)
    {
        usleep((max_wait_ms * 1000) / max_test_count);
    }
}
