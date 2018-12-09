#include <iostream>
#include <chrono>
#include <string>
#include <unistd.h>
using namespace std;
using namespace std::chrono;

void record()
{
    auto t1 = high_resolution_clock::now();
    char buffer[4096];
    while (cin.getline(buffer, 4096, '\n'))
    {
        auto t2 = high_resolution_clock::now();
        cout << duration_cast<microseconds>(t2 - t1).count() << endl;
        cout << buffer << endl;
    }
}

void replay()
{
    uint64_t cur_time = 0;
    uint64_t next_time;
    string line;
    while (cin >> next_time >> line)
    {
        usleep(next_time - cur_time);
        cout << line << endl;
        cur_time = next_time;
    }
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        goto fail;
    }
    if (string(argv[1]) == "record")
    {
        record();
    }
    else if (string(argv[1]) == "replay")
    {
        replay();
    }
    else
    {
        goto fail;
    }
    return 0;
fail:
    cout << "record or replay" << endl;
    return -1;
}
