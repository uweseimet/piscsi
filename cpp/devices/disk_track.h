//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
// Copyright (C) 2014-2020 GIMONS
//
// XM6i
//   Copyright (C) 2010-2015 isaki@NetBSD.org
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <string>
#include <vector>
#include "shared/s2p_defs.h"

using namespace std;

class DiskTrack
{

public:

    DiskTrack() = default;
    ~DiskTrack();
    DiskTrack(DiskTrack&) = delete;
    DiskTrack& operator=(const DiskTrack&) = delete;

private:

    friend class DiskCache;

    void Init(int, int, int);
    bool Load(const string&, uint64_t&);
    bool Save(const string&, uint64_t&);

    int ReadSector(data_in_t, int) const;
    int WriteSector(data_out_t, int);

    int GetTrack() const
    {
        return track_number;
    }

    int track_number = 0;

    // 8 = 256, 9 = 512, 10 = 1024, 11 = 2048, 12 = 4096
    int shift_count = 0;

    // < 256
    int sector_count = 0;

    uint8_t *buffer = nullptr;

    uint32_t buffer_size = 0;

    bool is_initialized = false;

    bool is_modified = false;

    // Do not use bool here in order to avoid special rules for vector<bool>
    vector<uint8_t> modified_flags;
};
