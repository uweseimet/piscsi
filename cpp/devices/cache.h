//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include "generated/s2p_interface.pb.h"

using namespace std;
using namespace s2p_interface;

class Cache
{

public:

    explicit Cache(bool r) : raw(r)
    {
    }
    virtual ~Cache() = default;

    virtual bool ReadSector(span<uint8_t>, uint64_t) = 0;
    virtual bool WriteSector(span<const uint8_t>, uint64_t) = 0;

    virtual bool Flush() = 0;

    virtual bool Init()
    {
        // Nothing to do in the base class

        return true;
    }

    virtual vector<PbStatistics> GetStatistics(bool) const = 0;

    bool IsRawMode() const
    {
        return raw;
    }

protected:

    inline static const string READ_ERROR_COUNT = "read_error_count";
    inline static const string WRITE_ERROR_COUNT = "write_error_count";
    inline static const string CACHE_MISS_READ_COUNT = "cache_miss_read_count";
    inline static const string CACHE_MISS_WRITE_COUNT = "cache_miss_write_count";

private:

    // Raw mode for CD-ROMs
    bool raw;
};
