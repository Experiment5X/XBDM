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
    console.SendCommand("modsections name=\"xboxkrnl.exe\"", response);
    cout << response << endl;

    std::vector<Module> loadedModules = console.GetLoadedModules(ok);
    for (const Module &m : loadedModules)
    {
        cout << m.name << endl;
        for (ModuleSection sect : m.sections)
        {
            cout << "\t" << sect.name << "\t0x" << std::hex << sect.baseAddress << endl;
        }
    }

    console.CloseConnection();

    getchar();
    return 0;
}
