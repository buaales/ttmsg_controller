#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "mdl.h"
// read app property from file

static int goto_my_part(FILE *f, const char *app_name)
{
    char line[1024];
    while (fgets(line, sizeof(line), f))
    {
        line[strlen(line) - 1] = 0;
        if (strncmp(line, ":", 1) == 0)
        {
            char *name = line + 2;
            if (strcmp(name, app_name) == 0)
            {
                return 1;
            }
        }
    }
    return 0;
}

struct MsgProperty *read_msg_property(const char *path, const char *app_name)
{
    FILE *fp = fopen(path, "r");
    int succ = goto_my_part(fp, app_name);
    if (!succ)
    {
        printf("No %s mdl\n", app_name);
        return NULL;
    }
    ALLOC(struct MsgProperty, p);
    strcpy(p->my_name, app_name);
    fscanf(fp, "%d %d %d", &p->peroid, &p->app_count, &p->time_table_count);
    for (int i = 0; i < p->app_count; i++)
    {
        fscanf(fp, "%s", p->app_info[i].name);
    }

    for (int i = 0; i < p->time_table_count; i++)
    {
        char direct[16];
        memset(direct, 0, sizeof(direct));
        fscanf(fp, "%s %s %d %d", direct, p->table[i].target_name, &p->table[i].frame_id, &p->table[i].time_slot);
        if (strcmp(direct, "send") == 0)
        {
            p->table[i].direction = SEND;
        }
        else
        {
            p->table[i].direction = RECV;
        }
    }
    fclose(fp);
    return p;
}

struct AppProperty *read_app_property(const char *path, const char *app_name)
{
    FILE *fp = fopen(path, "r");
    int succ = goto_my_part(fp, app_name);
    if (!succ)
    {
        printf("No %s mdl\n", app_name);
        return NULL;
    }
    ALLOC(struct AppProperty, p);
    strcpy(p->my_name, app_name);
    fscanf(fp, "%d %d %s", &p->send_offset, &p->peroid, p->msg_server_name);
    fclose(fp);
    return p;
}

struct MsgTopo *read_msg_topo(const char *path)
{
    FILE *fp = fopen(path, "r");
    ALLOC(struct MsgTopo, p);
    fscanf(fp, "%d %d", &p->count, &p->global_period);
    for (int i = 0; i < p->count; i++)
    {
        fscanf(fp, "%s %s %d %d %s %d", p->server[i].name, p->server[i].ip_addr, &p->server[i].port, &p->server[i].is_master,
        p->server[i].app_ip_addr, &p->server[i].app_port);
    }
    fclose(fp);
    return p;
}