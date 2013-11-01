#include "xbdm.h"

using namespace std;
using namespace XBDM;

XBDM::DevConsole::DevConsole(std::string consoleIp) :
    ip(consoleIp), connected(false), debugMemStatus(DebugMemStatus::Undefined), debugMemSize(0xFFFFFFFF),
    dumpMode(DumpMode::Undefined), dumpSettings({ DumpReport::Undefined })
{

}

bool XBDM::DevConsole::OpenConnection()
{
    // The windows code for this function was taken off
    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms737591(v=vs.85).aspx
    // and simply modified to connect to a devkit

    WSADATA wsaData;
    xsocket = INVALID_SOCKET;
    struct addrinfo *result = NULL,
                    *ptr = NULL,
                    hints;
    int iResult;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
        return false;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(ip.c_str(), "730", &hints, &result);
    if (iResult != 0)
    {
        WSACleanup();
        return false;
    }

    // Attempt to connect to an address until one succeeds
    for(ptr = result; ptr != NULL; ptr = ptr->ai_next)
    {
        // Create a socket for connecting to server
        xsocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (xsocket == INVALID_SOCKET)
        {
            WSACleanup();
            return false;
        }

        // Connect to console
        iResult = connect(xsocket, ptr->ai_addr, (int)ptr->ai_addrlen);
        if (iResult == SOCKET_ERROR)
        {
            closesocket(xsocket);
            xsocket = INVALID_SOCKET;
            continue;
        }
        break;
    }

    // make sure that the connection succeeded
    freeaddrinfo(result);
    if (xsocket == INVALID_SOCKET)
    {
        WSACleanup();
        return false;
    }

    // read the crap placed on teh queue, should be "201- connected"
    BYTE buffer[0x80] = {0};
    RecieveBinary(buffer, 0x80);
    DWORD cmp = strcmp((char*)buffer, "201- connected\r\n");

    return connected = (cmp == 0);
}

bool XBDM::DevConsole::CloseConnection()
{
    std::string response;
    SendCommand("bye", response);

    shutdown(xsocket, SD_SEND);
    closesocket(xsocket);
    WSACleanup();
    return true;
}

bool XBDM::DevConsole::SendBinary(const BYTE *buffer, DWORD length)
{
    int iResult = send(xsocket, (char*)buffer, length, 0);
    if (iResult == SOCKET_ERROR)
    {
        closesocket(xsocket);
        WSACleanup();
        return false;
    }
    return true;
}

bool XBDM::DevConsole::RecieveBinary(BYTE *buffer, DWORD length, bool text)
{
    // for text, we're going to take characters off the queue until we hit a null-terminator
    if (text)
    {
        recv(xsocket, (char*)buffer, length, MSG_PEEK);

        DWORD lenToGet = 0;
        do
        {
            if (buffer[lenToGet] == 0)
                break;
            lenToGet++;
        }
        while (lenToGet < length);

        // now actually read the bytes off the queue
        ZeroMemory(buffer, length);
        recv(xsocket, (char*)buffer, lenToGet, 0);
    }
    else
    {
        int bytesRecieved = recv(xsocket, (char*)buffer, length, 0);
        return bytesRecieved == length;
    }
}

bool XBDM::DevConsole::SendCommand(string command, string &response, DWORD responseLength)
{
    ResponseStatus status;
    return SendCommand(command, response, status, responseLength);
}

bool XBDM::DevConsole::SendCommand(string command, string &response, ResponseStatus &status, DWORD responseLength)
{
    // send the command to the devkit
    command += "\r\n";
    if (!SendBinary((BYTE*)command.c_str(), command.length()))
        return false;

    // weeeeeeellllll, i guess the xbox needs some time to compile the response
    Sleep(20);

    // get the response from the console, first just read the status
    BYTE *buffer = new BYTE[responseLength];
    ZeroMemory(buffer, responseLength);
    RecieveBinary(buffer, 5, false);

    // get the status from the response
    int statusInt;
    istringstream(string((char*)buffer).substr(0, 3)) >> statusInt;
    status = (ResponseStatus)statusInt;

    // parse the response
    switch ((ResponseStatus)statusInt)
    {
        case ResponseStatus::OK:
            RecieveBinary(buffer, responseLength);
            response = std::string((char*)buffer);
            break;
        case ResponseStatus::Multiline:
            // read off "mulitline response follows"
            RecieveBinary(buffer, 0x1C, false);
            ZeroMemory(buffer, responseLength);

            // the end of the response always contains "\r\n."
            while (response.find("\r\n.") == std::string::npos)
            {
                ZeroMemory(buffer, 0x400);

                RecieveBinary(buffer, 0x400);
                response += std::string((char*)buffer);
            }

            break;
        case ResponseStatus::Binary:
            // read off "mulitline response follows"
            RecieveBinary(buffer, 0x19, false);

            // let the caller deal with reading the stream
            break;
    }

    delete[] buffer;

    // trim off the leading and trailing whitespace
    response.erase(response.begin(), std::find_if(response.begin(), response.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    response.erase(std::find_if(response.rbegin(), response.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), response.end());

    return true;
}

bool XBDM::DevConsole::IsHddEnabled(bool &ok, bool forceResend)
{
    SystemInformation info = GetSystemInformation(ok, forceResend);
    return info.hddEnabled;
}

std::unique_ptr<BYTE[]> XBDM::DevConsole::GetScreenshot(bool &ok)
{
    std::string response;
    SendCommand("screenshot", response);

    DWORD size = GetIntegerProperty(response, "framebuffersize", ok, true);

    // get the screenshot from the console
    std::unique_ptr<BYTE[]> imageBuffer(new BYTE[size]);
    RecieveBinary(imageBuffer.get(), size, false);

    return imageBuffer;
}

std::unique_ptr<BYTE[]> XBDM::DevConsole::GetMemory(DWORD address, DWORD length, bool &ok)
{
    // format the command to send to the console
    char command[0x30] = {0};
    snprintf(command, 0x30, "getmemex addr=0x%08x length=0x%08x", address, length);

    std::string response;
    SendCommand(std::string(command), response);

    // allocate memory for the buffer, better be safe with these new fancy safe pointers
    std::unique_ptr<BYTE[]> buffer(new BYTE[length]);

    // we have to read the memory in 1kb, 0x400 byte, chunks
    DWORD offset = 0;
    while (length >= 0x400)
    {
        // there's always these 2 bytes at the beginning, i think they're flags
        // not gonna worry about them now, if there are issues then there
        // might be some status flag here, i see in their code they do something
        // with the flag 0x8000, i think they might quit early? i dunno
        RecieveBinary(buffer.get() + offset, 2, false);

        // read the memory from the console
        RecieveBinary(buffer.get() + offset, 0x400, false);
        length -= 0x400;
        offset += 0x400;
    }
    if (length > 0)
    {
        // read off those flags again
        RecieveBinary(buffer.get() + offset, 2, false);

        RecieveBinary(buffer.get() + offset, length, false);
        length -= length;
    }

    return buffer;
}

void XBDM::DevConsole::GetFile(string remotePath, string localPath, bool &ok)
{
    // send the command to get the file, only reading the '203 - binary response follows' or whatever
    std::string response = "";
    SendCommand("getfile name=\"" + remotePath + "\"", response, 0x1E);

    // read the file length
    BYTE temp[4];
    RecieveBinary(temp, 4, false);

    // it's in little endian on the xbox, i have no idea what
    // endian the system will be that this runs on, so we gotta swap it
    DWORD fileLength = ((DWORD)temp[3] << 24) | ((DWORD)temp[2] << 16) | ((DWORD)temp[1] << 8) | (DWORD)temp[0];

    // create the file on the local machine to write to
    fstream outFile(localPath.c_str(), ios_base::out | ios_base::trunc | ios_base::binary);

    // read the file in 0x10000 byte chunks
    BYTE *fileBuffer = new BYTE[0x10000];
    while (fileLength >= 0x10000)
    {
        RecieveBinary(fileBuffer, 0x10000, false);
        outFile.write((char*)fileBuffer, 0x10000);

        fileLength -= 0x10000;
    }
    if (fileLength != 0)
    {
        RecieveBinary(fileBuffer, fileLength, false);
        outFile.write((char*)fileBuffer, fileLength);
    }

    // cleanup, everyone's gotta do their share
    outFile.close();
    delete[] fileBuffer;
}

void XBDM::DevConsole::DumpMemory(DWORD address, DWORD length, string dumpPath)
{
    // read the memory from the console
    bool ok;
    auto memory = GetMemory(address, length, ok);

    // write the memory to the file the caller specified
    fstream dumpFile(dumpPath.c_str(), ios_base::out | ios_base::binary | ios_base::trunc);
    dumpFile.write((char*)memory.get(), length);
    dumpFile.close();
}

DWORD XBDM::DevConsole::GetDebugMemorySize(bool &ok, bool forceResend)
{
    if (debugMemSize == 0xFFFFFFFF || forceResend)
    {
        std::string response;
        SendCommand("debugmemsize", response);

        bool ok;
        debugMemSize = GetIntegerProperty(response, "debugmemsize", ok, true);
    }

    ok = true;
    return debugMemSize;
}

XBDM::DebugMemStatus XBDM::DevConsole::GetDebugMemoryStatus(bool &ok, bool forceResend)
{
    if (debugMemStatus == DebugMemStatus::Undefined || forceResend)
    {
        ResponseStatus status;
        std::string temp;
        ok = SendCommand("consolemem", temp, status);

        if (status != ResponseStatus::OK)
            ok = false;

        // get the actual status from the response
        // it's formated like this: consolemem=0xVALUE
        int memStatus;
        istringstream(temp.substr(13)) >> std::hex >> memStatus;
        debugMemStatus = (DebugMemStatus)memStatus;
    }

    ok = true;
    return debugMemStatus;
}

DumpMode XBDM::DevConsole::GetDumpMode(bool &ok, bool forceResend)
{
    if (dumpMode == DumpMode::Undefined || forceResend)
    {
        std::string response;
        SendCommand("dumpmode", response);

        if (response == "smart")
            dumpMode = DumpMode::Smart;
        else if (response == "enabled")
            dumpMode = DumpMode::Enabled;
        else if (response == "disabled")
            dumpMode = DumpMode::Disabled;
        else
        {
            ok = false;
            return DumpMode::Undefined;
        }
    }

    ok = true;
    return dumpMode;
}

DumpSettings XBDM::DevConsole::GetDumpSettings(bool &ok, bool forceResend)
{
    if (dumpSettings.report == DumpReport::Undefined || forceResend)
    {
        bool ok;
        std::string response;
        SendCommand("dumpsettings", response);

        // get the report
        std::string report = GetEnumProperty(response, "rpt", ok);
        if (report == "prompt")
            dumpSettings.report = DumpReport::Prompt;
        else if (report == "always")
            dumpSettings.report = DumpReport::Always;
        else if (report == "never")
            dumpSettings.report = DumpReport::Never;
        else
        {
            ok = false;
            return dumpSettings;
        }

        // get the destination
        std::string destination = GetEnumProperty(response, "dst", ok);
        if (destination == "local")
            dumpSettings.destination = DumpDestination::Local;
        else if (destination == "remote")
            dumpSettings.destination = DumpDestination::Remote;
        else
        {
            ok = false;
            return dumpSettings;
        }

        // get the report
        std::string format = GetEnumProperty(response, "fmt", ok);
        if (format == "full")
            dumpSettings.format = DumpFormat::FullHeap;
        else if (format == "partial")
            dumpSettings.format = DumpFormat::PartialHeap;
        else if (format == "noheap")
            dumpSettings.format = DumpFormat::NoHeap;
        else if (format == "retail")
            dumpSettings.format = DumpFormat::Retail;
        else
        {
            ok = false;
            return dumpSettings;
        }

        dumpSettings.path = GetStringProperty(response, "path", ok);
    }

    return dumpSettings;
}

SystemInformation XBDM::DevConsole::GetSystemInformation(bool &ok, bool forceResend)
{
    if (systemInformation.platform == "" || forceResend)
    {
        bool ok;
        std::string response;
        SendCommand("systeminfo", response);

        // parse through the response
        systemInformation.hddEnabled =          GetEnumProperty(response, "HDD", ok) == "Enabled";
        systemInformation.platform =            GetEnumProperty(response, "Platform", ok);
        systemInformation.system =              GetEnumProperty(response, "System", ok);
        systemInformation.baseKernelVersion =   GetEnumProperty(response, "BaseKrnl", ok);
        systemInformation.kernelVersion =       GetEnumProperty(response, "Krnl", ok);
        systemInformation.recoveryVersion =     GetEnumProperty(response, "XDK", ok);
    }

    return systemInformation;
}

std::vector<Drive> XBDM::DevConsole::GetDrives(bool &ok, bool forceResend)
{
    if (drives.size() == 0 || forceResend)
    {
        std::string response;
        SendCommand("drivelist", response);

        // get all of the drive names from the response
        bool ok;
        do
        {
            std::string driveName = GetStringProperty(response, "drivename", ok, true);

            Drive d = { driveName, 0, 0, 0 };
            drives.push_back(d);
        }
        while (ok);

        // now that all of the drive names are loaded, let's get the drive size information
        for (Drive &d : drives)
        {
            SendCommand("drivefreespace name=\"" + d.name + ":\\\"", response);

            d.freeBytesAvailable =  ((UINT64)GetIntegerProperty(response, "freetocallerhi", ok, true) << 32) |
                                     (UINT64)GetIntegerProperty(response, "freetocallerlo", ok, true);
            d.totalBytes =          ((UINT64)GetIntegerProperty(response, "totalbyteshi", ok, true) << 32) |
                                     (UINT64)GetIntegerProperty(response, "totalbyteslo", ok, true);
            d.totalFreeBytes =      ((UINT64)GetIntegerProperty(response, "totalfreebyteshi", ok, true) << 32) |
                                     (UINT64)GetIntegerProperty(response, "totalfreebyteslo", ok, true);
        }
    }

    ok = true;
    return drives;
}

std::vector<FileEntry> XBDM::DevConsole::GetDirectoryContents(string directory, bool &ok)
{
    std::string response;
    SendCommand("dirlist name=\"" + directory + "\"", response);

    std::vector<FileEntry> toReturn;
    if (response.find("file not found\r\n") == 0)
    {
        ok = false;
        return toReturn;
    }

    do
    {
        FileEntry entry;

        entry.name = GetStringProperty(response, "name", ok, true);
        entry.size = ((UINT64)GetIntegerProperty(response, "sizehi", ok, true, true) << 32) | GetIntegerProperty(response, "sizelo", ok, true, true);
        entry.creationTime = ((UINT64)GetIntegerProperty(response, "createhi", ok, true, true) << 32) | GetIntegerProperty(response, "createlo", ok, true, true);
        entry.modifiedTime = ((UINT64)GetIntegerProperty(response, "changehi", ok, true, true) << 32) | GetIntegerProperty(response, "changelo", ok, true, true);
        entry.directory = response.find(" directory") == 0;

        toReturn.push_back(entry);
    }
    while (ok);

    ok = true;
    return toReturn;
}

std::vector<Module> XBDM::DevConsole::GetLoadedModules(bool &ok, bool forceResend)
{
    if (loadedModules.size() == 0 || forceResend)
    {
        std::string response;
        SendCommand("modules", response);

        do
        {
            Module m;

            m.name =            GetStringProperty(response, "name", ok, true);
            m.baseAddress =     GetIntegerProperty(response, "base", ok, true, true);
            m.size =            GetIntegerProperty(response, "size", ok, true, true);
            m.checksum =        GetIntegerProperty(response, "check", ok, true, true);
            m.timestamp =       GetIntegerProperty(response, "timestamp", ok, true, true);
            m.dataAddress =     GetIntegerProperty(response, "pdata", ok, true, true);
            m.dataSize =        GetIntegerProperty(response, "psize", ok, true, true);
            m.threadId =        GetIntegerProperty(response, "thread", ok, true, true);
            m.originalSize =    GetIntegerProperty(response, "osize", ok, true, true);

            if (ok)
                loadedModules.push_back(m);
        }
        while (ok);

        // now let's request all of the sections for each of the module
        for (Module &m : loadedModules)
        {
            std::string response;
            SendCommand("modsections name=\"" + m.name + "\"", response);

            do
            {
                ModuleSection section;

                section.name =              GetStringProperty(response, "name", ok, true);
                section.baseAddress =       GetIntegerProperty(response, "base", ok, true, true);
                section.size =              GetIntegerProperty(response, "size", ok, true, true);
                section.index =             GetIntegerProperty(response, "index", ok, false, true);
                section.flags =             GetIntegerProperty(response, "flags", ok, false, true);

                if (ok)
                    m.sections.push_back(section);
            }
            while (ok);
        }
    }

    return loadedModules;
}

string XBDM::DevConsole::GetFeatures(bool &ok, bool forceResend)
{
    if (features == "" || forceResend)
    {
        ResponseStatus status;
        ok = SendCommand("consolefeatures", features, status);

        if (status != ResponseStatus::OK)
        {
            ok = false;
            return "";
        }
    }

    ok = true;
    return features;
}

string XBDM::DevConsole::GetDebugName(bool &ok, bool forceResend)
{
    if (debugName == "" || forceResend)
    {
        SendCommand("dbgname", debugName);
    }
    return debugName;
}

string XBDM::DevConsole::GetPlatform(bool &ok, bool forceResend)
{
    SystemInformation info = GetSystemInformation(ok, forceResend);
    return info.platform;
}

string XBDM::DevConsole::GetMotherboard(bool &ok, bool forceResend)
{
    SystemInformation info = GetSystemInformation(ok, forceResend);
    return info.system;
}

string XBDM::DevConsole::GetBaseKernelVersion(bool &ok, bool forceResend)
{
    SystemInformation info = GetSystemInformation(ok, forceResend);
    return info.baseKernelVersion;
}

string XBDM::DevConsole::GetKernelVersion(bool &ok, bool forceResend)
{
    SystemInformation info = GetSystemInformation(ok, forceResend);
    return info.kernelVersion;
}

string XBDM::DevConsole::GetRecoveryVersion(bool &ok, bool forceResend)
{
    SystemInformation info = GetSystemInformation(ok, forceResend);
    return info.recoveryVersion;
}

string XBDM::DevConsole::GetType(bool &ok, bool forceResend)
{
    if (type == "" || forceResend)
    {
        ResponseStatus status;
        ok = SendCommand("consoletype", type, status);

        if (status != ResponseStatus::OK)
        {
            ok = false;
            return "";
        }
    }

    ok = true;
    return type;
}

DWORD XBDM::DevConsole::GetIntegerProperty(string &response, string propertyName, bool &ok, bool hex, bool update)
{
    // all of the properties are like this: NAME=VALUE
    int startIndex = response.find(propertyName) + propertyName.size() + 1;
    int spaceIndex = response.find(' ', startIndex);
    int crIndex = response.find('\r', startIndex);
    int endIndex = (spaceIndex != -1 && spaceIndex < crIndex) ? spaceIndex : crIndex;

    if (response.find(propertyName) == string::npos)
    {
        ok = false;
        return 0;
    }

    DWORD toReturn;
    if (hex)
        istringstream(response.substr(startIndex, endIndex - startIndex)) >> std::hex >> toReturn;
    else
        istringstream(response.substr(startIndex, endIndex - startIndex)) >> toReturn;

    if (update)
        response = response.substr(endIndex);
    ok = true;
    return toReturn;
}

string XBDM::DevConsole::GetStringProperty(string &response, string propertyName, bool &ok, bool update)
{
    // all string properties are like this: NAME="VALUE"
    int startIndex = response.find(propertyName) + propertyName.size() + 2;
    int endIndex = response.find('"', startIndex);

    if (response.find(propertyName) == string::npos)
    {
        ok = false;
        return "";
    }

    std::string toReturn = response.substr(startIndex, endIndex - startIndex);

    if (update)
        response = response.substr(endIndex);
    ok = true;
    return toReturn;
}

string XBDM::DevConsole::GetEnumProperty(string &response, string propertyName, bool &ok)
{
    // all of the properties are like this: NAME=VALUE
    int startIndex = response.find(propertyName) + propertyName.size() + 1;
    int spaceIndex = response.find(' ', startIndex);
    int crIndex = response.find('\r', startIndex);
    int endIndex = (spaceIndex != -1 && spaceIndex < crIndex) ? spaceIndex : crIndex;

    if (response.find(propertyName) == string::npos)
    {
        ok = false;
        return "";
    }

    return response.substr(startIndex, endIndex - startIndex);
}











