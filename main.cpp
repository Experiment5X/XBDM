#include <stdio.h>
#include <fstream>
#include "Xbdm.h"

using namespace XBDM;
using namespace std;

int main(void)
{
    DevConsole console("169.254.66.42");
    if (!console.OpenConnection())
    {
        cout << "Couldn't connect to console." << endl;
        return -1;
    }
    char command[0x50];
    snprintf(command, 0x50, "threadinfo thread=0x%0.8x", 92);

    bool ok;
    std::string response;
    //console.SendCommand(std::string(command), response, 0x400, 21);
    //cout << response << endl;

    console.DumpMemory(0x80000000, 0x900, "C:\\Users\\Adam\\Desktop\\memdump.bin");


    getchar();
    return 0;
}
