#ifndef __XBDM_H__
#define __XBDM_H__

#define XBDMAPI

#include <iostream>
#include <sstream>
#include <vector>
#include <functional>
#include <algorithm>
#include <cctype>
#include "XbdmDefinitions.h"

using std::string;

// because fuck you microsoft
#define _WIN32_WINNT 0x0501

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <Ws2tcpip.h>
#else
    // worry about this later, maybe use winnames.h from velocity
    #define ZeroMemory(buff, len) memset(buff, 0, len)
#endif

namespace XBDM
{
    class DevConsole
    {
    public:
        DevConsole(std::string consoleIp);

        bool OpenConnection();
        bool CloseConnection();

        bool SendBinary(const BYTE *buffer, DWORD length);
        bool ReceiveBinary(BYTE *buffer, DWORD length);
        bool SendCommand(string command, string &response);
        bool SendCommand(string command, string &response, ResponseStatus &status);

        bool                    IsHddEnabled(bool &ok, bool forceResend = false);
        DWORD                   GetDebugMemorySize(bool &ok, bool forceResend = false);
        std::string             GetType(bool &ok, bool forceResend = false);
        std::string             GetFeatures(bool &ok, bool forceResend = false);
        std::string             GetDebugName(bool &ok, bool forceResend = false);
        std::string             GetPlatform(bool &ok, bool forceResend = false);
        std::string             GetMotherboard(bool &ok, bool forceResend = false);
        std::string             GetBaseKernelVersion(bool &ok, bool forceResend = false);
        std::string             GetKernelVersion(bool &ok, bool forceResend = false);
        std::string             GetRecoveryVersion(bool &ok, bool forceResend = false);
        DebugMemStatus          GetDebugMemoryStatus(bool &ok, bool forceResend = false);
        DumpMode                GetDumpMode(bool &ok, bool forceResend = false);
        DumpSettings            GetDumpSettings(bool &ok, bool forceResend = false);
        SystemInformation       GetSystemInformation(bool &ok, bool forceResend = false);
        std::vector<Drive>      GetDrives(bool &ok, bool forceResend = false);
        std::vector<FileEntry>  GetDirectoryContents(std::string directory, bool &ok);
        std::vector<Module>     GetLoadedModules(bool &ok, bool forceResend = false);

    public:
        SOCKET          xsocket;
        bool            connected;
        std::string     ip;

        // cached properties
        DWORD                       debugMemSize;
        std::string                 type;
        std::string                 features;
        std::string                 debugName;
        DebugMemStatus              debugMemStatus;
        DumpMode                    dumpMode;
        DumpSettings                dumpSettings;
        SystemInformation           systemInformation;
        std::vector<Drive>          drives;
        std::vector<Module>         loadedModules;

        // i would use std::regex in these 2 functions, but it's not fully implemented in mingw yet
        DWORD       GetIntegerProperty(std::string &response, std::string propertyName, bool &ok, bool hex = false, bool update = false);
        std::string GetStringProperty(std::string &response, std::string propertyName, bool &ok, bool update = false);
        std::string GetEnumProperty(std::string &response, std::string propertyName, bool &ok);
    };
};

#endif
