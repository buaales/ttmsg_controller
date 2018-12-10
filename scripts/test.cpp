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
    char buffer[8192];
    while (1)
    {
        if (cin.eof())
        {
            return;
        }
        cin >> next_time;
        getchar();
        usleep((next_time - cur_time));
        cur_time = next_time;

        cin.getline(buffer, sizeof(buffer), '\n');
        cout << buffer << endl;
    }
}

int main(int argc, char* argv[])
{
    if (argc == 2 && string(argv[1]) == "record")
    {
        record();
    }
    else
    {
        replay();
    }
    return 0;
}
