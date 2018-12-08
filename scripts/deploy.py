import os
import sys
import subprocess
import atexit
import typing
import threading
from collections import defaultdict

MSG_BIN = os.path.join(os.path.dirname(os.path.abspath( __file__ )), "../build/msg_server")
APP_BIN = os.path.join(os.path.dirname(os.path.abspath( __file__ )), "../build/msg_app")

def deploy(ttfile='/tmp/tt.txt'):
    f = open(ttfile, 'r')
    lines = list(map(str.strip, f))

    msg_app_map: typing.DefaultDict[str, typing.List[str]] = defaultdict(list)
    msg_info: dict = dict()

    info_count = list(map(int, lines[0].split(' ')))[0]
    for i in range(info_count):
        name, msg_ip, msg_port, is_master, app_ip, app_port = lines[i + 1].split(' ')
        msg_info[name] = (msg_ip, int(msg_port), bool(is_master == '1'), app_ip, int(app_port))

    for idx, line in enumerate(lines):
        if line.startswith(': msg'):
            msg_node = line[2:]
            _, app_count, _ = lines[idx + 1].split(' ')
            msg_app_map[msg_node] = msg_app_map[msg_node]
            for i in range(idx + 2, idx + 2 + int(app_count)):
                msg_app_map[msg_node].append(lines[i])
    return msg_app_map, msg_info

def qemu(barrelfish_path):
    pass

def execute(msg_app_map):
    procs = []
    for msg_node, apps in msg_app_map.items():
        procs.append(subprocess.Popen(
            [MSG_BIN, msg_node], stdout=sys.stdout, stderr=subprocess.STDOUT))
        for app in apps:
            procs.append(subprocess.Popen(
                [APP_BIN, app], stdout=sys.stdout, stderr=subprocess.STDOUT))

    def close_all():
        for proc in procs:
            proc.kill()
    atexit.register(close_all)

    for proc in procs:
        proc.wait()


if __name__ == '__main__':
    ma, mi = deploy()
    tmp = {}
    if len(sys.argv) > 1:
        for arg in sys.argv[1:]:
            if arg not in ma:
                print("arg %s not valid" % arg)
                exit(-1)
            tmp[arg] = ma[arg]
    else:
        tmp = ma
    execute(tmp)
