#include "rpc/api.h"

struct MsgServerAppInfo
{
    char name[128];
    void* user_defined;
};

struct MsgTable
{
    enum ChannelDirection direction;
    char target_name[128];
    int time_slot;
    int frame_id;
};

struct MsgProperty
{
    int peroid;
    int app_count, time_table_count;
    char my_name[128];
    struct MsgServerAppInfo app_info[1024];
    struct MsgTable table[1024];
};

struct AppProperty
{
    int peroid;
    int send_offset;
    char my_name[128];
    char msg_server_name[128];
};

struct MsgServer
{
    char name[128];
    char ip_addr[128];
    int port;
    int is_master;
    void *user_defined;

    char app_ip_addr[128];
    int app_port;
};

struct MsgTopo
{
    int count;
    int global_period;
    struct MsgServer server[1024];
};

struct MsgProperty *read_msg_property(const char *path, const char *app_name);
struct AppProperty *read_app_property(const char *path, const char *app_name);
struct MsgTopo *read_msg_topo(const char *path);

static const char *default_path = "/tmp/tt.txt";