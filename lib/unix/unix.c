// low level
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int emulate_processor_id = -1;

int get_processor_id(void)
{
    return emulate_processor_id;
}

void init_compat_layer(int argc, char* argv[])
{
    // 需要指定processor号
    int ch;
    while ((ch = getopt(argc, argv, "p:")) != -1)
    {
        switch(ch)
        {
            case 'p':
            emulate_processor_id = atoi(optarg);
            break;
            case '?':
            printf("Unknown option: %c\n", (char)optopt);
            exit(-1);
        }
    }
    if (emulate_processor_id < 0)
    {
        printf("Unix version should assign -p processor_id\n");
        exit(-1);
    }
}

void _ll_init_server()
{

}

void _ll_init_done()
{

}
