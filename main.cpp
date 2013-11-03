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
    //console.SendCommand("walkmem", response, 0x400, 0);
    //cout << response << endl;

    console.RebootToCurrentTitle();
    cout << "rebooting..." << endl;

    getchar();
    return 0;
}
