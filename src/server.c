#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "app_server.h"
#include "rpc/udp.h"
#include "rpc/api.h"
#include "common.h"
#include <sys/shm.h> 
#include <sys/mman.h> 
#include <fcntl.h>
#include <errno.h> 
#include "mdl.h"
#include "ptp.h"
#include <pthread.h>

struct Channel* channels[1024];
int channel_fds[1024];

struct MsgProperty* prop;
struct MsgTopo* topo;

struct RpcCtrl* listen_ctrl;

struct ServerUserDefined
{
    struct RpcCtrl* ctrl;
    int ready;
    char ip[128];
    int port;
}*master_ud, *my_ud;

struct AppUserDefined
{
    int send_chan_idx, recv_chan_idx;
    enum {
        UNKONWN = 0,
        INITIALIZEING,
        READY
    } state;
};

struct MsgServer* my_server_info;
pthread_t master_ptp_thread;

struct Channel* get_channel(const char* app_name, enum ChannelDirection direction)
{
    for (int i = 0; i < sizeof(channels) / sizeof(channels[0]); i++)
    {
        struct Channel* chan = channels[i]; 
        if (!chan)
        {
            break;
        }
        if (strcmp(chan->app_name, app_name) == 0 && chan->direction == direction)
        {
            return chan;
        }
    }
    return NULL;
}

static void notify_all_app(void)
{
    for (int i = 0; i < prop->app_count; i++)
    {
        const char* app_name = prop->app_info[i].name;
        struct Channel* chan = get_channel(app_name, RECV);
        sprintf(chan->buffer, "Msg server %s command %s start\n", prop->my_name, app_name);
        chan->has_data = 1;
        // while (chan->has_data);
        // printf("app %s started\n", app_name);
    }
}

struct AppUserDefined* get_app_user_defined(const char* name)
{
    for (int i = 0; i < prop->app_count; i++)
    {
        struct MsgServerAppInfo* info  = &prop->app_info[i];
        if (strcmp(info->name, name) == 0)
        {
            CVT(struct AppUserDefined*, aud, info->user_defined);
            return aud;
        }
    }
    return NULL;
}

struct ServerUserDefined* get_server_user_defined(const char* name)
{
    for (int i = 0; i < topo->count; i++)
    {
        struct MsgServer* s = &topo->server[i];
        if (strcmp(s->name, name) == 0)
        {
            CVT(struct ServerUserDefined*, ud, s->user_defined);
            return ud;
        }
    }
    return NULL;
}

int is_all_ready(void)
{
    for (int i = 0; i < topo->count; i++)
    {
        struct MsgServer* s = &topo->server[i];
        if (strcmp(s->name, prop->my_name) != 0)
        {
            CVT(struct ServerUserDefined*, ud, s->user_defined);
            if (!ud->ready)
            {
                return 0;
            }
        }
    }
    return 1;
}

int handle_msg_server_slave()
{
    struct RpcHead* head = (struct RpcHead*)malloc(4096);
    int done = 0;

    udp_set_timeout(my_ud->ctrl, 1000);
    while(1)
    {
        CVT(struct RpcMsgServerReady*, rd, head);
        rd->done = 0;
        head->body_length = sizeof(*rd);
        strcpy(head->src_name, prop->my_name);
        udp_send(master_ud->ctrl, head);
        udp_recv(my_ud->ctrl, head);
        if (rd->done > 0)
        {
            break;
        }
    }
    printf("Slave: %s ready\n", prop->my_name);

    // notice master success
    while (1)
    {
        CVT(struct RpcMsgServerReady*, rd, head);
        udp_recv(my_ud->ctrl, head); // wait start message
        if (head->rpc_type == MSG_SERVER_START)
        {
            break;
        }
    }

    printf("Slave start work\n");
    return 0;
}

int handle_msg_server_master()
{
    struct RpcHead* head = (struct RpcHead*)malloc(4096);
    while (1)
    {
        if (is_all_ready())
        {
            break;
        }
        CVT(struct RpcMsgServerReady *, rd, head);
        head->body_length = 4096 - sizeof(*head);
        udp_recv(my_ud->ctrl, head);
        struct ServerUserDefined* ud = get_server_user_defined(head->src_name);
        rd->done++;
        udp_send(ud->ctrl, head);
        if (ud->ready == 0)
        {
            printf("Master: Slave %s ready\n", head->src_name);
        }
        ud->ready = 1;
    }

    for (int i = 0; i < topo->count; i++)
    {
        CVT(struct RpcMsgServerStart *, rd, head);
        head->body_length = sizeof(*rd);
        head->rpc_type = MSG_SERVER_START;
        strcpy(head->src_name, prop->my_name);
        struct MsgServer* s = &topo->server[i];
        CVT(struct ServerUserDefined*, ud, s->user_defined);
        if (ud == my_ud)
        {
            continue;
        }
        udp_send(ud->ctrl, head);
    }
    printf("Master start work\n");
    return 0;
}

int handle_app_start()
{
    if (prop->app_count == 0)
        return 0;
    struct RpcCtrl* ctrl = udp_start_server(my_server_info->app_port);
    struct RpcHead* head = (struct RpcHead*)malloc(4096);
    int shm_idx = 0;
    udp_set_timeout(ctrl, 0);
    while (1)
    {
        head->body_length = 4096 - sizeof(*head);
        udp_recv(ctrl, head);
        printf("app %s get\n", head->src_name);
        if (head->rpc_type == INIT)
        {
            CVT(struct RpcInit*, init, head);   
            init->done++;
            udp_send(ctrl, head);
            get_app_user_defined(head->src_name)->state = INITIALIZEING;
        }
        else if (head->rpc_type == CREATE_CHANNEL)
        {
            int idx = shm_idx++;
            CVT(struct RpcCreateChannel*, cc, head);   
            sprintf(cc->shm_filename, "shm_%d", idx);
            printf("Creating shm %s\n", cc->shm_filename);
            shm_unlink(cc->shm_filename);
            int fd = shm_open(cc->shm_filename, O_CREAT | O_RDWR, 0777);
            if (fd == -1) 
            {
                printf("shm open failed, %s\n", strerror(errno));
            }
            if (-1 == ftruncate(fd, sizeof(struct Channel)))
            {
                printf("Create shm failed\n");
            }
            channels[idx] = (struct Channel*)mmap(NULL, sizeof(struct Channel), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            channel_fds[idx] = fd;
            channels[idx]->direction = cc->direction;
            strcpy(channels[idx]->app_name, head->src_name);

            channels[idx]->has_data = 0;
            udp_send(ctrl, head);
            printf("Waiting channel succ\n");
            while (!channels[idx]->has_data);
            channels[idx]->has_data = 0;
            printf("Channel connect, data: %s\n", channels[idx]->buffer);

            struct AppUserDefined* aur = get_app_user_defined(head->src_name);
            if (cc->direction == RECV)
            {
                aur->recv_chan_idx = idx;
            }
            else
            {
                aur->send_chan_idx = idx;
            }
        }
        else if (head->rpc_type == INIT_DONE)
        {
            printf("app %s init done\n", head->src_name);
            udp_send(ctrl, head);
            struct AppUserDefined* aud = get_app_user_defined(head->src_name);
            if (!aud){
                printf("Cannot find app %s\n", head->src_name);
            }
            aud->state = READY;
            int all_ready = 1;
            for (int i = 0; i < prop->app_count; i++)
            {
                struct MsgServerAppInfo *info = &prop->app_info[i];
                CVT(struct AppUserDefined *, aud, info->user_defined);
                if (aud->state != READY) 
                {
                    printf("app %s not ready\n", info->name);
                    all_ready = 0;    
                }
            }
            if (all_ready)
            {
                break;
            }
        }
    }
    free(head);
    printf("app all ready\n");
    return 1;
}

struct Frame
{
    int id;
    char data[4096];
    int len;
};
#define FRAME_BUFFER_SIZE 256
static struct Frame frame_buffer[FRAME_BUFFER_SIZE];

static void init_frame_buffer()
{
    for (int i = 0; i < FRAME_BUFFER_SIZE; i++)
    {
        frame_buffer[i].id = -1;
    }
}

static struct Frame* insert_frame(int id, char* data, int len)
{
    for (int i = 0; i < FRAME_BUFFER_SIZE; i++)
    {
        struct Frame *f = &frame_buffer[i];
        if (f->id == -1 || f->id == id)
        {
            f->id = id;
            memcpy(f->data, data, len);
            f->len = len;
            return f;
        }
    }
    printf("Buffer full!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    exit(-1);
    return NULL;
}

static struct Frame* find_frame(int id)
{
    for (int i = 0; i < FRAME_BUFFER_SIZE; i++)
    {
        struct Frame *f = &frame_buffer[i];
        if (f->id == id)
        {
            return f;
        }
    }
    printf("not find frame %d !!!!!!!!!!!!!!!!!!!!!\n", id);
    exit(-1);
    return NULL;
}

static void run_mdl(void)
{
    struct RpcHead* head = (struct RpcHead*)malloc(4096);
    int cur_time = 0;
    init_frame_buffer();

    int t[2];
    uint64_t cur_clock_time;
    get_time(t);
    cur_clock_time = time_to_uint64(t);
    while (1)
    {
        printf("TIME_SLOT=%d %s ", cur_time, prop->my_name);
        for (int i = 0; i < prop->time_table_count; i++)
        {
            struct MsgTable* table = &prop->table[i];
            if (table->time_slot != cur_time)
            {
                continue;
            }

            // 是从msg server接收还是从app接收？
            struct AppUserDefined *aud = get_app_user_defined(table->target_name);
            struct ServerUserDefined *mud = get_server_user_defined(table->target_name);

            if (table->direction == RECV)
            {
                // 接受消息
                if (aud)
                {
                    struct Channel *chan = channels[aud->send_chan_idx];

                    // 等待该chan填充消息
                    wait_channel(chan, 300, 100);
                    if (!chan->has_data)
                    {
                        printf("Why %s still don't has data in its send chan?\n", chan->app_name);
                        exit(-1);
                    }
                    else
                    {
                        insert_frame(table->frame_id, chan->buffer, sizeof(chan->buffer));
                        chan->has_data = 0;
                    }
                    printf("RECV_FROM=%s:%d ", chan->app_name, table->frame_id);
                } 
                else if (mud)
                {
                    head->body_length = 4096 - sizeof(*head);
                    udp_recv(my_ud->ctrl, head);
                    CVT(struct RpcMsgServerData*, d, head);
                    insert_frame(table->frame_id, d->data, sizeof(d->data));
                    printf("RECV_FROM=%s:%d ", head->src_name, table->frame_id);
                }
                else
                {
                    printf("both aud src and mud src is NULL \n");
                    exit(-1);
                }
            }
            else
            {
                struct Frame* frame = find_frame(table->frame_id);
                // 发送消息
                if (aud)
                {
                    struct Channel *chan = channels[aud->recv_chan_idx];
                    if (chan->has_data)
                    {
                        printf("Why %s still has data in its send chan?\n", chan->app_name);
                        exit(-1);
                    }
                    else
                    {
                        memcpy(chan->buffer, frame->data, frame->len);
                        chan->has_data = 1;
                    }
                    printf("SEND_TO=%s ", chan->app_name);
                } 
                else if (mud)
                {
                    CVT(struct RpcMsgServerData*, d, head);
                    memcpy(d->data, frame->data, frame->len);
                    head->rpc_type = MSG_SERVER_DATA;
                    head->body_length = sizeof(*d);
                    strcpy(head->src_name, prop->my_name);
                    printf("SEND_TO=%s ", table->target_name);
                    udp_send(mud->ctrl, head);
                }
                else
                {
                    printf("both aud dest and mud dest is NULL \n");
                    exit(-1);
                }
            }
        }

        cur_time++;

        if (1 && cur_time == prop->peroid && my_ud != master_ud)
        {
            int i;
            for (i = 0; i < topo->count; i++)
            {
                if (strcmp(topo->server[i].name, prop->my_name) == 0)
                {
                    break;
                }
            }
            ptp_slave(master_ud->ip, master_ud->port + 20000 + i);
        }
        printf("\n");

        cur_time %= prop->peroid;

        cur_clock_time += TIME_SLOT_LEN_MS * 1000000ULL;
        int t2[2];
        get_time(t2);
        uint64_t now = time_to_uint64(t2);
        // 计算刚才的耗时
        uint64_t diff_nano = cur_clock_time - now;
        usleep(diff_nano / 1000);
        // 到这里过去了整一秒钟
    }
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Please provide name\n");
        return -1;
    }
    prop = read_msg_property(default_path, argv[1]);
    topo = read_msg_topo(default_path);
    for (int i = 0; i < topo->count; i++)
    {
        ALLOC(struct ServerUserDefined, ud);
        if (strcmp(topo->server[i].name, prop->my_name) == 0)
        {
            my_server_info = &topo->server[i];
            ud->ctrl = udp_start_server(topo->server[i].port);
            my_ud = ud;
        }
        else
        {
            ud->ctrl = udp_connect_to_server(topo->server[i].ip_addr, topo->server[i].port);
        }
        topo->server[i].user_defined = ud;
        if (topo->server[i].is_master)
        {
            master_ud = ud;
        }
        strcpy(ud->ip, topo->server[i].ip_addr);
        ud->port =  topo->server[i].port;
    }

    printf("my peroid: %d\n", prop->peroid);

    for (int i = 0; i < prop->app_count; i++)
    {
        ALLOC(struct AppUserDefined, aud);
        prop->app_info[i].user_defined = aud;
    }

    handle_app_start();

    if (master_ud == my_ud)
    {
        // start ptp service
        if (master_ud == my_ud)
        {
            for (int i = 0; i < topo->count; i++)
            {
                if (strcmp(topo->server[i].name, prop->my_name) == 0)
                {
                    continue;
                }
                pthread_create(&master_ptp_thread, NULL, master_thread, (void*)(my_ud->port + 20000 + i));
            }
        }
        handle_msg_server_master();
    }
    else
    {
        handle_msg_server_slave();
    }

    notify_all_app();

    run_mdl();

    return 0;
}
