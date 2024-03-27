//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "memory_util.h"

int memory_util::GetInt24(span<const int> buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 2);

    return (buf[offset] << 16) | (buf[offset + 1] << 8) | buf[offset + 2];
}

uint32_t memory_util::GetInt32(span<const int> buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 3);

    return (static_cast<uint32_t>(buf[offset]) << 24) | (static_cast<uint32_t>(buf[offset + 1]) << 16) |
        (static_cast<uint32_t>(buf[offset + 2]) << 8) | static_cast<uint32_t>(buf[offset + 3]);
}

uint64_t memory_util::GetInt64(span<const int> buf, int offset)
{
    assert(buf.size() > static_cast<size_t>(offset) + 7);

    return (static_cast<uint64_t>(buf[offset]) << 56) | (static_cast<uint64_t>(buf[offset + 1]) << 48) |
        (static_cast<uint64_t>(buf[offset + 2]) << 40) | (static_cast<uint64_t>(buf[offset + 3]) << 32) |
        (static_cast<uint64_t>(buf[offset + 4]) << 24) | (static_cast<uint64_t>(buf[offset + 5]) << 16) |
        (static_cast<uint64_t>(buf[offset + 6]) << 8) | static_cast<uint64_t>(buf[offset + 7]);
}

void memory_util::SetInt64(vector<uint8_t> &buf, int offset, uint64_t value)
{
    assert(buf.size() > static_cast<size_t>(offset) + 7);

    buf[offset] = static_cast<uint8_t>(value >> 56);
    buf[offset + 1] = static_cast<uint8_t>(value >> 48);
    buf[offset + 2] = static_cast<uint8_t>(value >> 40);
    buf[offset + 3] = static_cast<uint8_t>(value >> 32);
    buf[offset + 4] = static_cast<uint8_t>(value >> 24);
    buf[offset + 5] = static_cast<uint8_t>(value >> 16);
    buf[offset + 6] = static_cast<uint8_t>(value >> 8);
    buf[offset + 7] = static_cast<uint8_t>(value);
}
