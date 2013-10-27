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
    console.SendCommand("modules", response);
    cout << response << endl;

    console.CloseConnection();

    getchar();
    return 0;
}
