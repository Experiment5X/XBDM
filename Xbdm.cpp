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

        // set timeouts
        struct timeval tv = { 5, 0 };
        setsockopt(xsocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));
        //setsockopt(xsocket, SOL_SOCKET, SO_SNDTIMEO, (char*)&tv, sizeof(struct timeval));

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
    RecieveTextBuffer(buffer, 0x80);
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

void XBDM::DevConsole::ResetConnection()
{
    CloseConnection();
    OpenConnection();
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

bool XBDM::DevConsole::RecieveBinary(BYTE *buffer, DWORD length, DWORD &bytesRecieved)
{
    return recvTimeout(xsocket, (char*)buffer, length, 0, bytesRecieved);
}

bool XBDM::DevConsole::RecieveTextBuffer(BYTE *buffer, DWORD length)
{
    DWORD bytesReceived;
    if (!recvTimeout(xsocket, (char*)buffer, length, MSG_PEEK, bytesReceived))
        return false;

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
    if (!recvTimeout(xsocket, (char*)buffer, lenToGet, 0, bytesReceived))
        return false;
    return true;
}

bool XBDM::DevConsole::SendCommand(string command, string &response, DWORD responseLength, DWORD statusLength)
{
    ResponseStatus status;
    return SendCommand(command, response, status, responseLength, statusLength);
}

bool XBDM::DevConsole::SendCommand(string command, string &response, ResponseStatus &status, DWORD responseLength, DWORD statusLength)
{
    response = "";

    // send the command to the devkit
    command += "\r\n";
    if (!SendBinary((BYTE*)command.c_str(), command.length()))
        return false;

    // weeeeeeellllll, i guess the xbox needs some time to compile the response
    Sleep(20);

    return RecieveResponse(response, status, responseLength, statusLength);
}

bool XBDM::DevConsole::RecieveResponse(string &response, ResponseStatus &status, DWORD responseLength, DWORD statusLength)
{
    // get the response from the console, first just read the status
    unique_ptr<BYTE[]> buffer(new BYTE[responseLength]);
    ZeroMemory(buffer.get(), responseLength);

    // read the status
    DWORD bytesRecieved;
    if (!RecieveBinary(buffer.get(), 5, bytesRecieved))
        return false;

    // get the status from the response
    int statusInt;
    istringstream(string((char*)buffer.get()).substr(0, 3)) >> statusInt;
    status = (ResponseStatus)statusInt;

    // parse the response
    switch ((ResponseStatus)statusInt)
    {
        case ResponseStatus::OK:
        case ResponseStatus::ReadyToAcceptData:
            if (!RecieveTextBuffer(buffer.get(), responseLength))
                return false;
            response = std::string((char*)buffer.get());
            break;

        case ResponseStatus::Multiline:
            // THEY ARE SO INCONSISTENT! holy shit, i don't understand
            // when requesting threads, it says "thread info follows" instead of the
            // normal multiine response thing, it makes no sense
            // read off "mulitline response follows"
            if (!RecieveBinary(buffer.get(), (statusLength == -1) ? 0x1C : statusLength, bytesRecieved))
                return false;
            ZeroMemory(buffer.get(), responseLength);

            // the end of the response always contains "\r\n."
            while (response.find("\r\n.") == std::string::npos)
            {
                ZeroMemory(buffer.get(), 0x400);

                if (!RecieveTextBuffer(buffer.get(), 0x400))
                    return false;
                response += std::string((char*)buffer.get(), 0x400);
            }
            break;

        case ResponseStatus::Binary:
            // read off "binary response follows"
            if (!RecieveBinary(buffer.get(), (statusLength == -1) ? 0x19 : statusLength, bytesRecieved))
                return false;

            // let the caller deal with reading the stream
            break;

        case ResponseStatus::Error:
            if (!RecieveTextBuffer(buffer.get(), responseLength))
                return false;
            response = std::string((char*)buffer.get());

            break;
        default:
            return false;
    }

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
    DWORD bytesRecieved;
    std::unique_ptr<BYTE[]> imageBuffer(new BYTE[size]);
    RecieveBinary(imageBuffer.get(), size, bytesRecieved);

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
    DWORD bytesRecieved;
    while (length >= 0x400)
    {
        // there's always these 2 bytes at the beginning, i think they're flags
        // not gonna worry about them now, if there are issues then there
        // might be some status flag here, i see in their code they do something
        // with the flag 0x8000, i think they might quit early? i dunno
        RecieveBinary(buffer.get() + offset, 2, bytesRecieved);

        // read the memory from the console
        RecieveBinary(buffer.get() + offset, 0x400, bytesRecieved);
        length -= 0x400;
        offset += 0x400;
    }
    if (length > 0)
    {
        // read off those flags again
        RecieveBinary(buffer.get() + offset, 2, bytesRecieved);

        RecieveBinary(buffer.get() + offset, length, bytesRecieved);
        length -= length;
    }

    return buffer;
}

void XBDM::DevConsole::ReceiveFile(string remotePath, string localPath, bool &ok)
{
    // send the command to get the file, only reading the '203 - binary response follows' or whatever
    std::string response = "";
    SendCommand("getfile name=\"" + remotePath + "\"", response, 0x1E);

    // read the file length
    BYTE temp[4];
    DWORD bytesRecieved;
    if (!RecieveBinary(temp, 4, bytesRecieved))
    {
        ok = false;
        return;
    }

    // it's in little endian on the xbox, i have no idea what
    // endian the system will be that this runs on, so we gotta swap it
    DWORD fileLength = ((DWORD)temp[3] << 24) | ((DWORD)temp[2] << 16) | ((DWORD)temp[1] << 8) | (DWORD)temp[0];

    // create the file on the local machine to write to
    fstream outFile(localPath.c_str(), ios_base::out | ios_base::trunc | ios_base::binary);

    // read the file in 0x10000 byte chunks
    BYTE *fileBuffer = new BYTE[0x10000];
    while (fileLength > 0)
    {
        // read, at most 0x10000 bytes, off the queue
        if (!RecieveBinary(fileBuffer, ((fileLength >= 0x10000) ? 0x10000 : fileLength), bytesRecieved))
        {
            ok = false;
            delete[] fileBuffer;
            return;
        }
        outFile.write((char*)fileBuffer, bytesRecieved);

        fileLength -= bytesRecieved;
    }

    // cleanup, everyone's gotta do their share
    outFile.close();
    delete[] fileBuffer;

    ok = true;
}

void XBDM::DevConsole::ReceiveDirectory(string remoteDirectory, string localLocation, bool &ok)
{
    // get the contents of the directory requested
    vector<FileEntry> dirContents = GetDirectoryContents(remoteDirectory, ok);
    if (!ok)
        return;

    // create the folder on the local file system
    string requestedDirectoryName = remoteDirectory.substr(remoteDirectory.rfind('\\', remoteDirectory.size() - 2) + 1, remoteDirectory.rfind('\\'));
#ifdef __WIN32
    ok = CreateDirectoryA((localLocation + requestedDirectoryName).c_str(), NULL);
    if (!ok)
        return;
#endif

    // get all of the dirents from the console
    localLocation += requestedDirectoryName;
    for (FileEntry dirent : dirContents)
    {
        // if there are folders inside the folder requested, then we need to recursively get them
        if (dirent.directory)
        {
            ReceiveDirectory(remoteDirectory + dirent.name + "\\", localLocation, ok);
            if (!ok)
                return;
        }
        else
        {
            ReceiveFile(remoteDirectory + dirent.name, localLocation + dirent.name, ok);
            if (!ok)
                return;
        }
    }
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

void XBDM::DevConsole::RebootToXShell()
{
    std::string response;
    SendCommand("magicboot", response);
}

void XBDM::DevConsole::RebootToCurrentTitle()
{
    bool ok;
    LaunchXEX(GetActiveTitle(ok));
}

void XBDM::DevConsole::ColdReboot()
{
    std::string response;
    SendCommand("magicboot  COLD", response);
}

void XBDM::DevConsole::LaunchXEX(string xexPath)
{
    // for whatever reason the xbox needs the directory that the xex is in
    std::string directory = xexPath.substr(0, xexPath.find_last_of('\\') + 1);

    std::string response;
    SendCommand("magicboot title=\"" + xexPath + "\" directory=\"" + directory + "\"", response);
}

void XBDM::DevConsole::StartAutomatingInput(DWORD userIndex, bool &ok, DWORD queueLen)
{
    // validate the user index
    if (userIndex > 3)
    {
        ok = false;
        return;
    }

    // get the message to send to the user
    char message[50] = {0};
    snprintf(message, 50, "autoinput user=%d bind queuelen=%d", userIndex, queueLen);

    // tell the console to start with the input
    std::string response;
    SendCommand(std::string(message), response);
}

void XBDM::DevConsole::StopAutomatingInput(DWORD userIndex, bool &ok)
{
    // validate the user index
    if (userIndex > 3)
    {
        ok = false;
        return;
    }

    // get the message to send to the user
    char message[30];
    snprintf(message, 30, "autoinput user=%d unbind", userIndex);

    // tell the console to start with the input
    std::string response;
    SendCommand(std::string(message), response);
}

void XBDM::DevConsole::AddGamepadToQueue(DWORD userIndex, GamepadState gamepad, bool &ok)
{
    // validate the user index
    if (userIndex > 3)
    {
        ok = false;
        return;
    }

    // get the command to send to the xbox  we'll always
    // send one at a time because I don't think that they
    // can handle more than one at a time
    char command[60] = {0};
    snprintf(command, 60, "autoinput user=%d queuepackets count=1 timearray countarray", userIndex);

    std::string response;
    ResponseStatus status;
    SendCommand(std::string(command), response, status);

    // make sure that the console processed the request properly
    if (status != ResponseStatus::ReadyToAcceptData)
    {
        ok = false;
        return;
    }

    // the xbox sends 2 null bytes following both the gamepad structure and the
    // other data it needs. If this is true for all of the sending of binary data
    // then ill change the function, but for now ill just do it here. I think those
    // 2 bytes might be flags, not sure though
    BYTE temp[14] = {0};
    memcpy(temp, &gamepad, sizeof(GamepadState));
    SendBinary((BYTE*)temp, sizeof(GamepadState) + 2);

    // this is the time array and count array, which the xbox seems to pay no attention to
    // it says that it's the amount of time that the button is down, but it doesn't work
    DWORD args[3] = { 1, 1, 0};
    SendBinary((BYTE*)args, 10);

    // the console will send back the amount of gamepads queued
    RecieveResponse(response, status);

    response = "";
}

void XBDM::DevConsole::SendGamepads(DWORD userIndex, std::vector<GamepadState> &gamepads, bool &ok)
{
    // send all of the gamepads to the console
    for (GamepadState g : gamepads)
    {
        // tell the console to stop recieving input from the controller
        StartAutomatingInput(userIndex, ok, gamepads.size());

        AddGamepadToQueue(userIndex, g, ok);
        ResetConnection();

        // return control to the physical controller
        StopAutomatingInput(userIndex, ok);
    }
}

void XBDM::DevConsole::ClearGamepadQueue(DWORD userIndex, bool &ok)
{
    // validate the user index
    if (userIndex > 3)
    {
        ok = false;
        return;
    }

    char command[40];
    snprintf(command, 40, "autoinput user=%d clearqueue", userIndex);

    std::string response;
    SendCommand(std::string(command), response);
}

void XBDM::DevConsole::RenameFile(string oldName, string newName, bool &ok)
{
    string command = "RENAME NAME=\"" + oldName + "\"" + " NEWNAME=\"" + newName + "\"";
    string response;

    ok = SendCommand(command, response);
}

void XBDM::DevConsole::MakeDirectory(string path, bool &ok)
{
    string response;
    ok = SendCommand("mkdir name=\"" + path + "\"", response);
}

void XBDM::DevConsole::SendFile(string localPath, string remotePath, bool &ok)
{
    // open the file for reading, and it could be anything so: binary
    ifstream localFile(localPath.c_str(), ios_base::in | ios_base::binary);

    // get the length of the file
    localFile.seekg(0, ios_base::end);
    DWORD fileLength = localFile.tellg();

    // format the command to send to the console
    stringstream command;
    command << "sendfile name=\"" << remotePath << "\" ";
    command << "length=0x" << std::hex << fileLength;

    string response;
    ResponseStatus status;
    SendCommand(command.str(), response, status);

    if (status != ResponseStatus::ReadyToAcceptData)
    {
        ok = false;
        return;
    }

    // seek back to the beginning of the file
    localFile.seekg(0, ios_base::beg);

    // send it in chunks, this way it can handle huge files
    BYTE *fileBuffer = new BYTE[0x10000];
    while (fileLength >= 0x10000)
    {
        localFile.read((char*)fileBuffer, 0x10000);
        SendBinary(fileBuffer, 0x10000);

        fileLength -= 0x10000;
    }
    if (fileLength != 0)
    {
        localFile.read((char*)fileBuffer, fileLength);
        SendBinary(fileBuffer, fileLength);
    }

    delete[] fileBuffer;
    return;
}

void XBDM::DevConsole::DeleteDirectory(string path, bool &ok)
{
    DeleteDirent(path, true, ok);
}

void XBDM::DevConsole::SetMemory(DWORD address, const BYTE *buffer, DWORD length, bool &ok)
{
    while (length > 0)
    {
        // the xbox can only receive 128 bytes at once
        DWORD bytesToSend = (length > 128) ? 128 : length;

        // build the command to send
        stringstream command;
        command << "setmem ";
        command << "addr=0x" << std::hex << address << " ";
        command << "data=";

        for (int i = 0; i < bytesToSend; i++)
            command << std::hex << (DWORD)buffer[i];

        // send the command
        string response;
        ResponseStatus status;
        SendCommand(command.str(), response, status);

        // if the status isn't 200, then you can't write to that location
        if (status != ResponseStatus::OK)
        {
            ok = false;
            return;
        }

        // update our values for the next iteration
        buffer += bytesToSend;
        address += bytesToSend;
        length -= bytesToSend;
    }

    ok = true;
}

void XBDM::DevConsole::DeleteDirent(string path, bool isDirectory, bool &ok)
{
    string response;
    ok = SendCommand("delete name=\"" + path + "\"" + ((isDirectory) ? " DIR" : ""), response);
}

void XBDM::DevConsole::DeleteFile(string path, bool &ok)
{
    DeleteDirent(path, false, ok);
}

void XBDM::DevConsole::MoveFile(string oldName, string newName, bool &ok)
{
    RenameFile(oldName, newName, ok);
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
        // clear the existing drives
        drives.erase(drives.begin(), drives.end());

        std::string response;
        SendCommand("drivelist", response);

        // get all of the drive names from the response
        bool ok;
        do
        {
            std::string driveName = GetStringProperty(response, "drivename", ok, true);

            if (ok)
            {
                Drive d = { driveName, 0, 0, 0 };
                drives.push_back(d);
            }
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

            // get the friendly name for volume, these are from neighborhood
            if (d.name == "DEVKIT" || d.name == "E")
                d.friendlyName = "Game Development Volume (" + d.name + ":)";
            else if (d.name == "HDD")
                d.friendlyName = "Retail Hard Drive Emulation (" + d.name + ":)";
            else if (d.name == "Y")
                d.friendlyName = "Xbox360 Dashboard Volume (" + d.name + ":)";
            else if (d.name == "Z")
                d.friendlyName = "Devkit Drive (" + d.name + ":)";
            else
                d.friendlyName = "Volume (" + d.name + ":)";
        }
    }

    ok = true;
    return drives;
}

std::vector<FileEntry> XBDM::DevConsole::GetDirectoryContents(string directory, bool &ok)
{
    std::string response;
    ok = SendCommand("dirlist name=\"" + directory + "\"", response);

    std::vector<FileEntry> toReturn;
    if (!ok || response.find("file not found\r\n") == 0)
    {
        ok = false;
        return toReturn;
    }

    do
    {
        FileEntry entry;

        entry.name = GetStringProperty(response, "name", ok, true);
        entry.size = ((UINT64)GetIntegerProperty(response, "sizehi", ok, true, true) << 32) | GetIntegerProperty(response, "sizelo", ok, true, true);
        entry.creationTime = FILETIME_TO_TIMET(((UINT64)GetIntegerProperty(response, "createhi", ok, true, true) << 32) | GetIntegerProperty(response, "createlo", ok, true, true));
        entry.modifiedTime = FILETIME_TO_TIMET(((UINT64)GetIntegerProperty(response, "changehi", ok, true, true) << 32) | GetIntegerProperty(response, "changelo", ok, true, true));
        entry.directory = response.find(" directory") == 0;

        if (ok)
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
        // clear the existing modules
        loadedModules.erase(loadedModules.begin(), loadedModules.end());

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

std::vector<Thread> XBDM::DevConsole::GetThreads(bool &ok, bool forceResend)
{
    if (threads.size() == 0 || forceResend)
    {
        // clear the existing threads
        threads.erase(threads.begin(), threads.end());

        std::string response;
        SendCommand("threads", response);

        // parse all of the thread IDs from the response
        std::vector<DWORD> threadIDs;
        bool isFirst = true;
        while (response.length() > 1)
        {
            // split the string up by the delimeter \r\n
            std::string curID = response.substr(0, response.find("\r\n"));
            response = response.substr(response.find("\r\n") + 2);

            // we have to skip the first one, it's not a thread
            // i have no idea what it actually is though
            if (isFirst)
            {
                isFirst = false;
                continue;
            }

            // convert the string ID to a DWORD
            DWORD dwId;
            istringstream(curID) >> dwId;
            threadIDs.push_back(dwId);
        }

        // request information about all of the threads
        for (DWORD id : threadIDs)
        {
            char command[0x50] = {0};
            snprintf(command, 0x50, "threadinfo thread=0x%0.8x", id);

            // request the thread info from the console
            SendCommand(std::string(command), response, 0x400, 21);

            // parse the response
            Thread threadInfo;
            threadInfo.id = id;
            threadInfo.suspendCount =                   GetIntegerProperty(response, "suspend", ok);
            threadInfo.priority =                       GetIntegerProperty(response, "priority", ok);
            threadInfo.tlsBaseAddress =                 GetIntegerProperty(response, "tlsbase", ok, true);
            threadInfo.baseAddress =                    GetIntegerProperty(response, "base", ok, true);
            threadInfo.tlsBaseAddress =                 GetIntegerProperty(response, "limit", ok, true);
            threadInfo.tlsBaseAddress =                 GetIntegerProperty(response, "slack", ok, true);

            UINT64 creationTime = 0;
            creationTime |=                             GetIntegerProperty(response, "createhi", ok, true);
            creationTime |=                             GetIntegerProperty(response, "createlo", ok, true);
            threadInfo.creationTime =                   FILETIME_TO_TIMET(creationTime);

            threadInfo.nameAddress =                    GetIntegerProperty(response, "nameaddr", ok, true);
            threadInfo.nameLength =                     GetIntegerProperty(response, "namelen", ok, true);
            threadInfo.currentProcessor =               GetIntegerProperty(response, "proc", ok, true);
            threadInfo.lastError =                      GetIntegerProperty(response, "proc", ok, true);

            threads.push_back(threadInfo);
        }
    }

    return threads;
}

std::vector<MemoryRegion> XBDM::DevConsole::GetMemoryRegions(bool &ok, bool forceResend)
{
    if (memoryRegions.size() == 0 || forceResend)
    {
        // clear the existing memory regions
        memoryRegions.erase(memoryRegions.begin(), memoryRegions.end());

        std::string response;
        SendCommand("walkmem", response, 0x400, 37);

        // parse all of the memory regions
        ok = true;
        while (ok)
        {
            MemoryRegion region;
            region.baseAddress =    GetIntegerProperty(response, "base", ok, true, true);
            region.size =           GetIntegerProperty(response, "size", ok, true, true);
            region.protection =     MemoryRegionFlagsToString(GetIntegerProperty(response, "protect", ok, true, true));

            if (ok)
                memoryRegions.push_back(region);
        }
    }

    return memoryRegions;
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

string XBDM::DevConsole::GetActiveTitle(bool &ok, bool forceResend)
{
    if (activeTitle == "" || forceResend)
    {
        std::string response;
        SendCommand("xbeinfo running", response);

        activeTitle = GetStringProperty(response, "name", ok);
    }

    return activeTitle;
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

string XBDM::DevConsole::MemoryRegionFlagsToString(DWORD flags)
{
    const std::string flagStrings[] =
    {
        "No Access",
        "Read Only",
        "Read Write",
        "Write Copy",
        "Execute",
        "Execute Read",
        "Execute Read Write",
        "Execute Write Copy",
        "Guard",
        "No Cache",
        "Write Combine",
        "",
        "User Read Only",
        "User Read Write"
    };

    // doubles aren't precices, that's why i need to do the rounding
    return flagStrings[(int)((log(flags) / log(2)) + .5)];
}

bool XBDM::DevConsole::recvTimeout(SOCKET s, char *buf, int len, int flags, DWORD &bytesReceived)
{
    bytesReceived = recv(s, buf, len, flags);
    if (bytesReceived == SOCKET_ERROR)
        return false;
    return true;
}











