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
    bool ok;
    std::string response;
    //console.SendCommand("getmemex addr=0x80000000 length=0x00000400", response);

    //BYTE buffer[0x400] = {0};
    //console.RecieveBinary(buffer, 0x800, false);

    console.DumpMemory(0x80000000, 0x900, "C:\\Users\\Adam\\Desktop\\dump.bin");
    cout << "done" << endl;

    getchar();
    return 0;
}
