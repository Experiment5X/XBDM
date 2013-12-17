#ifndef XBDMDEFINITIONS_H
#define XBDMDEFINITIONS_H

#include <iostream>
#include <exception>
#include <vector>
#include <string>

#ifdef _WIN32
    #include <windows.h>
#else
    typedef uint8_t     BYTE;
    typedef int16_t     SHORT;
    typedef uint16_t    WORD;
    typedef uint32_t    DWORD;
    typedef uint64_t    UINT64;

#endif

#define FILETIME_TO_TIMET(time) ((time_t)(time / 10000000L - 11644473600L))

namespace XBDM
{
    enum class ResponseStatus
    {
        OK = 200,
        Multiline = 202,
        Binary = 203,
        ReadyToAcceptData = 204,
        Error = 405
    };

    enum class DebugMemStatus
    {
        NoAdditionalMemory = 0,
        AdditionalMemoryDisabled = 1,
        AdditionalMemoryEnabled = 2,

        Undefined = 0xFF
    };

    // behavior when exception occurs (while not debugging)
    enum class DumpMode
    {
        Smart = 0,          // Debug if xbdm can, dumps if kd required
        Enabled = 1,        // Always dump
        Disabled = 2,       // Always debug

        Undefined = 0xFF
    };

    enum class DumpReport
    {
        Prompt = 0,
        Always = 1,
        Never = 2,

        Undefined = 0xFF
    };

    enum class DumpDestination
    {
        Local = 0,
        Remote = 1,

        Undefined = 0xFF
    };

    enum class DumpFormat
    {
        FullHeap = 0,
        PartialHeap = 1,    // memory scan
        NoHeap = 2,         // just threads and exception info
        Retail = 3,         // a retail dump - NYI

        Undefined = 0xFF
    };

    enum class MemoryRegionFlags
    {
        Execute = 0x10,
        ExecuteRead = 0x20,
        ExecuteReadWrite = 0x40,
        ExecuteWriteCopy = 0x80,
        Guard = 0x100,
        NoAccess = 1,
        NoCache = 0x200,
        ReadOnly = 2,
        ReadWrite = 4,
        UserReadOnly = 0x1000,
        UserReadWrite = 0x2000,
        WriteCombine = 0x400,
        WriteCopy = 8,

        Undefined = 0xFF
    };

    struct MemoryRegion
    {
        DWORD baseAddress;
        DWORD size;
        std::string protection;
    };

    struct DumpSettings
    {
        DumpReport report;
        DumpDestination destination;
        DumpFormat format;

        std::string path;
    };

    struct Drive
    {
        std::string name;
        UINT64 freeBytesAvailable;
        UINT64 totalBytes;
        UINT64 totalFreeBytes;
        std::string friendlyName;
    };

    struct SystemInformation
    {
        bool hddEnabled;
        std::string platform;       // mobo codename?
        std::string system;         // mobo
        std::string baseKernelVersion;
        std::string kernelVersion;
        std::string recoveryVersion;
    };

    struct FileEntry
    {
        std::string name;
        UINT64 size;
        time_t creationTime;
        time_t modifiedTime;

        bool directory;
    };

    struct ModuleSection
    {
        std::string name;
        DWORD baseAddress;
        DWORD size;
        DWORD index;
        DWORD flags;
    };

    struct Module
    {
        std::string name;
        DWORD baseAddress;
        DWORD size;
        DWORD checksum;
        DWORD timestamp;
        DWORD dataAddress;      // probably the code start address
        DWORD dataSize;
        DWORD threadId;
        DWORD originalSize;     // no idea

        std::vector<ModuleSection> sections;
    };

    struct LocalDirent
    {
        std::string name;
        bool directory;
    };

    struct Thread
    {
        DWORD id;
        DWORD suspendCount;
        DWORD priority;
        DWORD tlsBaseAddress;
        DWORD baseAddress;
        DWORD startAddress;
        DWORD stackBaseAddress;
        DWORD stackLimitAddress;
        time_t creationTime;
        DWORD stackSlackSpace;
        DWORD nameAddress;
        DWORD nameLength;
        WORD currentProcessor;
        DWORD lastError;
    };

    struct GamepadState
    {
        // all of these buttons have a pressed and non pressed state
        bool dpadUP         : 1;
        bool dpadDOWN       : 1;
        bool dpadLEFT       : 1;
        bool dpadRIGHT      : 1;
        bool start          : 1;
        bool back           : 1;
        bool leftThumb      : 1;
        bool rightThumb     : 1;
        bool leftBumper     : 1;
        bool rightBumper    : 1;
        bool centerButton   : 1;
        bool buttonBind     : 1;
        bool A              : 1;
        bool B              : 1;
        bool X              : 1;
        bool Y              : 1;

        // the amount of pressure put on the trigger
        BYTE leftTrigger;
        BYTE rightTrigger;

        // Each of the thumbstick axis members is a signed
        // value describing the position of the thumbstick.
        // A value of 0 is centered. Negative values signify down
        // or to the left. Positive values signify up or to the right.
        SHORT leftStickX;
        SHORT leftStickY;
        SHORT rightStickX;
        SHORT rightStickY;
    };
};

#endif // XBDMDEFINITIONS_H
