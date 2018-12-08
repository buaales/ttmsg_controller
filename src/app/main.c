#include <stdio.h>
#include <string.h>

#include "rpc/udp.h"
#include "rpc/api.h"
#include "common.h"

#include <sys/shm.h> 
#include <sys/mman.h> 
#include <fcntl.h>
#include <errno.h> 

#include "mdl.h"

struct RpcCtrl* ctrl = NULL;

struct Channel* channels[2];
int channel_fds[2];
int channel_idx = 0;
int recv_idx, send_idx;

#define RECV_CHAN (channels[recv_idx])
#define SEND_CHAN (channels[send_idx])

struct AppProperty* prop;
struct MsgTopo* topo;

static void init(struct RpcHead* head)
{
    CVT(struct RpcInit*, init, head);
    udp_set_timeout(ctrl, 1000);
    int done = 0;
    while (1)
    {
        strcpy(head->src_name, prop->my_name);
        init->head.rpc_type = INIT;
        init->head.body_length = sizeof(struct RpcInit);
        init->done = done++;
        udp_send(ctrl, head);
        udp_recv(ctrl, head);
        if (init->done == done)
        {
            printf("Init done\n");
            break;
        }
    }
}

static void init_done(struct RpcHead* head)
{
    CVT(struct RpcInitDone*, id, head);
    strcpy(head->src_name, prop->my_name);
    id->head.rpc_type = INIT_DONE;
    id->head.body_length = sizeof(struct RpcInitDone);
    printf("Sending %s \n", prop->my_name);
    udp_send(ctrl, head);
    udp_recv(ctrl, head);
}

static void wait_for_start()
{
    while (!RECV_CHAN->has_data) {
        usleep(400);
    }
    // printf("app start with info: %s\n", RECV_CHAN->buffer);
    RECV_CHAN->has_data = 0;
}

static struct Channel* create_channel(struct RpcHead* head, enum ChannelDirection direction)
{
    printf("Sending cc %s \n", prop->my_name);
    CVT(struct RpcCreateChannel*, cc, head);
    strcpy(head->src_name, prop->my_name);
    cc->head.rpc_type = CREATE_CHANNEL;
    cc->head.body_length = sizeof(struct RpcCreateChannel);
    cc->direction = direction;
    udp_send(ctrl, head);
    udp_recv(ctrl, head);

    printf("Channel get name: %s\n", cc->shm_filename);
    ALLOC(struct Channel, chan);
    int idx = channel_idx++;

    int fd = shm_open(cc->shm_filename, O_CREAT | O_RDWR, 0777);
    channels[idx] = (struct Channel*)mmap(NULL, sizeof(struct Channel), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    channel_fds[idx] = fd;
    sprintf(channels[idx]->buffer, "fuck %d", idx);
    channels[idx]->has_data = 1;
    channels[idx]->direction = direction;
    strcpy(channels[idx]->app_name, prop->my_name);

    if (direction == RECV)
    {
        recv_idx = idx;
    }
    else
    {
        send_idx = idx;
    }

    return chan;
}

static void run_mdl(void)
{
    struct RpcHead* head = (struct RpcHead*)malloc(4096);
    int cur_time = 0;
    int time_slot = 0;
    while (1)
    {
        int start_time[2], end_time[2];
        get_time(start_time);
        printf("TIME_SLOT=%d %s checking\n", time_slot,prop->my_name);
        if (cur_time == prop->send_offset)
        {
            if (SEND_CHAN->has_data)
            {
                printf("msg server down\n");
                exit(-1);
            }
            sprintf(SEND_CHAN->buffer, "I'm a message sent by %s", prop->my_name);
            printf("\033[1m\033[41;37mTIME_SLOT=%d %s SEND=%s\033[0m\n", time_slot, prop->my_name,  SEND_CHAN->buffer);
            SEND_CHAN->has_data = 1;
        }
        wait_channel(RECV_CHAN, 200, 50);
        if (RECV_CHAN->has_data)
        {
            printf("\033[1m\033[44;37mTIME_SLOT=%d %s RECV=%s\033[0m\n", time_slot, prop->my_name,  RECV_CHAN->buffer);
            RECV_CHAN->has_data = 0;
        }

        cur_time++;
        time_slot++;
        get_time(end_time);
        // 计算刚才的耗时
        uint64_t diff_nano = (end_time[1] - start_time[1]) + (end_time[0] -  start_time[0]) * 1000000000ULL;
        usleep((TIME_SLOT_LEN_MS * 1000ULL * 1000ULL - diff_nano) / 1000);
        // 到这里过去了整一秒钟
        cur_time %= prop->peroid;
        time_slot %= topo->global_period;
    }
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        printf("Please provide name\n");
        return -1;
    }
    prop = read_app_property(default_path, argv[1]);
    topo = read_msg_topo(default_path);
    if (!prop)
    {
        printf("Cannot find %s\n", argv[1]);
        return -1;
    }

    for (int i = 0; i < topo->count; i++)
    {
        if (strcmp(topo->server[i].name, prop->msg_server_name) == 0)
        {
            ctrl = udp_connect_to_server(topo->server[i].app_ip_addr, topo->server[i].app_port);
        }
    }
    if (!ctrl)
    {
        printf("Cannot connect to msg server\n");
        return -1;
    }
    struct RpcHead* head = (struct RpcHead*)malloc(4096);
    init(head);
    create_channel(head, RECV);
    create_channel(head, SEND);
    init_done(head);
    wait_for_start();

    run_mdl();
    return 0;
}