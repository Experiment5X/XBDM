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
    //std::string response;
    //console.SendCommand("autoinput user=0 bind queuelen=0", response);
    //cout << response << endl;

    console.StartAutomatingInput(0, ok);

    GamepadState gamepad = {0};
    gamepad.A = true;
    console.SendButton(0, gamepad, ok);

    console.StopAutomatingInput(0, ok);

    cout << "done" << endl;
    getchar();
    return 0;
}
