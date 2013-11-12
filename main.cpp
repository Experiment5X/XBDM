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
   // std::string response;
   // console.SendCommand("sendfile name=\"DEVKIT:\\testing\\boothole\" size=0x34", response);
    //cout << response << endl;

    BYTE buffer[0x1000];
    memset(buffer, 0xAA, 0x1000);

    console.SetMemory(0x800CCFB4, buffer, 0x1000, ok);
    console.CloseConnection();


    cout << "done" << endl;
    getchar();
    return 0;
}
