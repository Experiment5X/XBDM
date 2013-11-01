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
    //console.SendCommand("getfile name=\"DEVKIT:\\settings.ini\"", response);
    //BYTE buffer[0x400] = {0};
   // console.RecieveBinary(buffer, 0x400, false);

    cout << console.GetFeatures(ok) << endl;

    cout << "done" << endl;

    getchar();
    return 0;
}
