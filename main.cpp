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
    console.SendCommand("screenshot", response);
    cout << response << endl;

    //console.CloseConnection();

   /* auto buffer = console.GetScreenshot(ok);

    fstream dump("C:\\Users\\Adam\\Desktop\\dump.bin", ios_base::out);
    dump.write((char*)buffer.get(), 0x00195000);
    dump.close();

    cout << "success";*/

    getchar();
    return 0;
}
