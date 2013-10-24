#include <stdio.h>
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
    console.SendCommand("systeminfo", response, 0x4000);
    cout << response << endl;

    SystemInformation info = console.GetSystemInformation(ok);

    console.CloseConnection();

    getchar();
    return 0;
}

