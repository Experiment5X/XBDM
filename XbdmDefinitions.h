#ifndef XBDMDEFINITIONS_H
#define XBDMDEFINITIONS_H

#include <iostream>
#include <exception>
#include <vector>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace XBDM
{
    enum class ResponseStatus
    {
        OK = 200,
        Multiline = 202,
        Binary = 203,
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
        UINT64 creationTime;
        UINT64 modifiedTime;

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
        FILETIME creationTime;
        DWORD stackSlackSpace;
        DWORD nameAddress;
        DWORD nameLength;
        WORD currentProcessor;
        DWORD lastError;
    };

};

#endif // XBDMDEFINITIONS_H
