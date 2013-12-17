#include <stdio.h>
#include <fstream>
#include <iostream>
#include "Xbdm.h"

using namespace XBDM;
using namespace std;

int main(void)
{
    DevConsole console("192.168.1.16");
    if (!console.OpenConnection())
    {
        cout << "Couldn't connect!" << endl;
        return 0;
    }
    console.ColdReboot();

    cout << "Done" << endl;
    return 0;
}
