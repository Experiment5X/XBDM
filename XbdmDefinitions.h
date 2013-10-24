#ifndef XBDMDEFINITIONS_H
#define XBDMDEFINITIONS_H

#include <iostream>
#include <exception>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace XBDM
{
    enum class ResponseStatus
    {
        OK = 200,
        MultilineResponse = 202
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

    class XbdmException : public std::exception
    {
    public:
        virtual const char* what() const throw()
        {
            return "XBDM: An error occurred while connecting to the console.\n";
        }
    };
};

#endif // XBDMDEFINITIONS_H
