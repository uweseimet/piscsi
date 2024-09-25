//---------------------------------------------------------------------------
//
// SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "storage_device.h"
#include "base/memory_util.h"
#include <unistd.h>
#include "shared/s2p_exceptions.h"

using namespace filesystem;
using namespace memory_util;

StorageDevice::StorageDevice(PbDeviceType type, scsi_level level, int lun, bool supports_mode_select,
    bool supports_save_parameters, const unordered_set<uint32_t> &s)
: ModePageDevice(type, level, lun, supports_mode_select, supports_save_parameters), supported_block_sizes(s)
{
    SupportsFile(true);
    SetStoppable(true);
}

bool StorageDevice::Init(const param_map &params)
{
    ModePageDevice::Init(params);

    AddCommand(scsi_command::cmd_start_stop, [this]
        {
            StartStopUnit();
        });
    AddCommand(scsi_command::cmd_prevent_allow_medium_removal, [this]
        {
            PreventAllowMediumRemoval();
        });

    return true;
}

void StorageDevice::CleanUp()
{
    UnreserveFile();

    ModePageDevice::CleanUp();
}

void StorageDevice::Dispatch(scsi_command cmd)
{
    // Media changes must be reported on the next access, i.e. not only for TEST UNIT READY
    if (cmd != scsi_command::cmd_inquiry && cmd != scsi_command::cmd_request_sense && IsMediumChanged()) {
        assert(IsRemovable());

        SetMediumChanged(false);

        throw scsi_exception(sense_key::unit_attention, asc::not_ready_to_ready_change);
    }

    ModePageDevice::Dispatch(cmd);
}

void StorageDevice::StartStopUnit()
{
    const bool start = GetController()->GetCdbByte(4) & 0x01;
    const bool load = GetController()->GetCdbByte(4) & 0x02;

    if (load) {
        LogTrace(start ? "Loading medium" : "Ejecting medium");
    }
    else {
        LogTrace(start ? "Starting unit" : "Stopping unit");

        SetStopped(!start);
    }

    if (!start) {
        // Look at the eject bit and eject if necessary
        if (load) {
            if (IsLocked()) {
                // Cannot be ejected because it is locked
                throw scsi_exception(sense_key::illegal_request, asc::load_or_eject_failed);
            }

            // Eject
            if (!Eject(false)) {
                throw scsi_exception(sense_key::illegal_request, asc::load_or_eject_failed);
            }
        }
        else {
            FlushCache();
        }
    }
    else if (load && !last_filename.empty()) {
        SetFilename (last_filename);
        if (!ReserveFile()) {
            last_filename.clear();
            throw scsi_exception(sense_key::illegal_request, asc::load_or_eject_failed);
        }

        SetMediumChanged(true);
    }

    StatusPhase();
}

void StorageDevice::PreventAllowMediumRemoval()
{
    CheckReady();

    const bool lock = GetController()->GetCdbByte(4) & 0x01;

    LogTrace(lock ? "Locking medium" : "Unlocking medium");

    SetLocked(lock);

    StatusPhase();
}

bool StorageDevice::Eject(bool force)
{
    const bool status = ModePageDevice::Eject(force);
    if (status) {
        FlushCache();

        last_filename = GetFilename();

        // The image file for this device is not in use anymore
        UnreserveFile();

        block_read_count = 0;
        block_write_count = 0;
    }

    return status;
}

void StorageDevice::ModeSelect(cdb_t cdb, span<const uint8_t> buf, int length)
{
    const auto cmd = static_cast<scsi_command>(cdb[0]);
    assert(cmd == scsi_command::cmd_mode_select6 || cmd == scsi_command::cmd_mode_select10);

    // PF
    if (!(cdb[1] & 0x10)) {
        // Vendor-specific parameters (SCSI-1) are not supported.
        // Do not report an error in order to support Apple's HD SC Setup.
        return;
    }

    // The page data are optional
    if (!length) {
        return;
    }

    int size = GetBlockSizeInBytes();

    int offset = EvaluateBlockDescriptors(cmd, span(buf.data(), length), size);
    length -= offset;

    // Set up the available pages in order to check for the right page size below
    map<int, vector<byte>> pages;
    SetUpModePages(pages, 0x3f, true);
    for (const auto& [p, data] : PropertyHandler::Instance().GetCustomModePages(GetVendor(), GetProduct())) {
        if (data.empty()) {
            pages.erase(p);
        }
        else {
            pages[p] = data;
        }
    }

    // Parse the pages
    while (length > 0) {
        const int page_code = buf[offset];

        const auto &it = pages.find(page_code);
        if (it == pages.end()) {
            throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
        }

        // Page 0 can contain anything and can have any length
        if (!page_code) {
            break;
        }

        if (length < 2) {
            throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
        }

        // The page size field does not count itself and the page code field
        const size_t page_size = buf[offset + 1] + 2;

        // The page size in the parameters must match the actual page size, otherwise report
        // INVALID FIELD IN PARAMETER LIST (SCSI-2 8.2.8).
        if (it->second.size() != page_size || page_size > static_cast<size_t>(length)) {
            throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
        }

        switch (page_code) {
        // Read-write/Verify error recovery and caching pages
        case 0x01:
            case 0x07:
            case 0x08:
            // Simply ignore the requested changes in the error handling or caching, they are not relevant for SCSI2Pi
            break;

            // Format device page
        case 0x03:
            // With this page the sector size for a subsequent FORMAT can be selected, but only a few devices
            // support this, e.g. FUJITSU M2624S.
            // We are fine as long as the permanent current block size remains unchanged.
            VerifyBlockSizeChange(GetInt16(buf, offset + 12), false);
            break;

        default:
            throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
        }

        length -= page_size;
        offset += page_size;
    }

    ChangeBlockSize(size);
}

int StorageDevice::EvaluateBlockDescriptors(scsi_command cmd, span<const uint8_t> buf, int &size) const
{
    assert(cmd == scsi_command::cmd_mode_select6 || cmd == scsi_command::cmd_mode_select10);

    const size_t required_length = cmd == scsi_command::cmd_mode_select10 ? 8 : 4;
    if (buf.size() < required_length) {
        throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
    }

    const size_t descriptor_length = cmd == scsi_command::cmd_mode_select10 ? GetInt16(buf, 6) : buf[3];
    if (buf.size() < descriptor_length + required_length) {
        throw scsi_exception(sense_key::illegal_request, asc::parameter_list_length_error);
    }

    // Check for temporary block size change in first block descriptor
    if (descriptor_length && buf.size() >= required_length + 8) {
        size = VerifyBlockSizeChange(GetInt16(buf, static_cast<int>(required_length) + 6), true);
    }

    return static_cast<int>(descriptor_length + required_length);
}

int StorageDevice::VerifyBlockSizeChange(int requested_size, bool temporary) const
{
    if (requested_size == static_cast<int>(GetBlockSizeInBytes())) {
        return requested_size;
    }

    // Simple consistency check
    if (requested_size && !(requested_size % 4)) {
        if (temporary) {
            return requested_size;
        }
        else {
            LogWarn(fmt::format(
                "Block size change from {0} to {1} bytes requested. Configure the block size in the s2p settings.",
                GetBlockSizeInBytes(), requested_size));
        }
    }

    throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
}

void StorageDevice::ChangeBlockSize(uint32_t new_size)
{
    if (!GetSupportedBlockSizes().contains(new_size) && new_size % 4) {
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_parameter_list);
    }

    const auto current_size = GetBlockSizeInBytes();
    if (new_size != current_size) {
        const uint64_t capacity = current_size * GetBlockCount();
        SetBlockSizeInBytes(new_size);
        SetBlockCount(static_cast<uint32_t>(capacity / new_size));

        LogTrace(fmt::format("Changed block size from {0} to {1} bytes", current_size, new_size));
    }
}

bool StorageDevice::SetBlockSizeInBytes(uint32_t size)
{
    if (!supported_block_sizes.contains(size) && configured_block_size != size) {
        return false;
    }

    block_size = size;

    return true;
}

bool StorageDevice::SetConfiguredBlockSize(uint32_t configured_size)
{
    if (!configured_size || configured_size % 4
        || (!supported_block_sizes.contains(configured_size) && GetType() != SCHD)) {
        return false;
    }

    configured_block_size = configured_size;

    return true;
}

void StorageDevice::ValidateFile()
{
    if (!blocks) {
        throw io_exception("Device has 0 blocks");
    }

    if (GetFileSize() > 2LL * 1024 * 1024 * 1024 * 1024) {
        throw io_exception("Image files > 2 TiB are not supported");
    }

    // TODO Check for duplicate handling of these properties (-> CommandExecutor)
    if (IsReadOnlyFile()) {
        // Permanently write-protected
        SetReadOnly(true);
        SetProtectable(false);
        SetProtected(false);
    }

    SetStopped(false);
    SetRemoved(false);
    SetLocked(false);
    SetReady(true);
}

bool StorageDevice::ReserveFile() const
{
    if (filename.empty() || reserved_files.contains(filename.string())) {
        return false;
    }

    reserved_files[filename.string()] = { GetId(), GetLun() };

    return true;
}

void StorageDevice::UnreserveFile()
{
    reserved_files.erase(filename.string());

    filename.clear();
}

id_set StorageDevice::GetIdsForReservedFile(const string &file)
{
    if (const auto &it = reserved_files.find(file); it != reserved_files.end()) {
        return {it->second.first, it->second.second};
    }

    return {-1, -1};
}

bool StorageDevice::FileExists(string_view file)
{
    return exists(path(file));
}

bool StorageDevice::IsReadOnlyFile() const
{
    return access(filename.c_str(), W_OK);
}

off_t StorageDevice::GetFileSize() const
{
    try {
        return file_size(filename);
    }
    catch (const filesystem_error &e) {
        throw io_exception("Can't get size of '" + filename.string() + "': " + e.what());
    }
}

int StorageDevice::ModeSense6(cdb_t cdb, vector<uint8_t> &buf) const
{
    const auto length = static_cast<int>(min(buf.size(), static_cast<size_t>(cdb[4])));
    fill_n(buf.begin(), length, 0);

    // DEVICE SPECIFIC PARAMETER
    if (IsProtected()) {
        buf[2] = 0x80;
    }

    // Basic information
    int size = 4;

    // Add block descriptor if DBD is 0, only if ready
    if (!(cdb[1] & 0x08) && IsReady()) {
        // Mode parameter header, block descriptor length
        buf[3] = 0x08;

        // Short LBA mode parameter block descriptor (number of blocks and block length)
        SetInt32(buf, 4, static_cast<uint32_t>(blocks));
        SetInt32(buf, 8, block_size);

        size = 12;
    }

    size = AddModePages(cdb, buf, size, length, 255);

    // The size field does not count itself
    buf[0] = (uint8_t)(size - 1);

    return size;
}

int StorageDevice::ModeSense10(cdb_t cdb, vector<uint8_t> &buf) const
{
    const auto length = static_cast<int>(min(buf.size(), static_cast<size_t>(GetInt16(cdb, 7))));
    fill_n(buf.begin(), length, 0);

    // DEVICE SPECIFIC PARAMETER
    if (IsProtected()) {
        buf[3] = 0x80;
    }

    // Basic Information
    int size = 8;

    // Add block descriptor if DBD is 0, only if ready
    if (!(cdb[1] & 0x08) && IsReady()) {
        // Check LLBAA for short or long block descriptor
        if (!(cdb[1] & 0x10) || blocks <= 0xffffffff) {
            // Mode parameter header, block descriptor length
            buf[7] = 0x08;

            // Short LBA mode parameter block descriptor (number of blocks and block length)
            SetInt32(buf, 8, static_cast<uint32_t>(blocks));
            SetInt32(buf, 12, block_size);

            size = 16;
        }
        else {
            // Mode parameter header, LONGLBA
            buf[4] = 0x01;

            // Mode parameter header, block descriptor length
            buf[7] = 0x10;

            // Long LBA mode parameter block descriptor (number of blocks and block length)
            SetInt64(buf, 8, blocks);
            SetInt32(buf, 20, block_size);

            size = 24;
        }
    }

    size = AddModePages(cdb, buf, size, length, 65535);

    // The size fields do not count themselves
    SetInt16(buf, 0, size - 2);

    return size;
}

vector<PbStatistics> StorageDevice::GetStatistics() const
{
    vector<PbStatistics> statistics = ModePageDevice::GetStatistics();

    PbStatistics s;
    s.set_id(GetId());
    s.set_unit(GetLun());

    s.set_category(PbStatisticsCategory::CATEGORY_INFO);

    s.set_key(BLOCK_READ_COUNT);
    s.set_value(block_read_count);
    statistics.push_back(s);

    if (!IsReadOnly()) {
        s.set_key(BLOCK_WRITE_COUNT);
        s.set_value(block_write_count);
        statistics.push_back(s);
    }

    return statistics;
}

