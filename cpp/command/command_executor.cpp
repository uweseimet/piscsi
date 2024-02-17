//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <sstream>
#include <spdlog/spdlog.h>
#include "base/device_factory.h"
#include "shared/localizer.h"
#include "shared/shared_exceptions.h"
#include "protobuf/protobuf_util.h"
#include "protobuf/command_context.h"
#include "devices/disk.h"
#include "command_executor.h"

using namespace spdlog;
using namespace protobuf_util;
using namespace s2p_util;

bool CommandExecutor::ProcessDeviceCmd(const CommandContext &context, const PbDeviceDefinition &pb_device, bool dryRun)
{
    info((dryRun ? "Validating: " : "Executing: ") + PrintCommand(context.GetCommand(), pb_device));

    const int id = pb_device.id();
    const int lun = pb_device.unit();

    if (!ValidateIdAndLun(context, id, lun)) {
        return false;
    }

    const PbOperation operation = context.GetCommand().operation();

    // For all commands except ATTACH the device and LUN must exist
    if (operation != ATTACH && !VerifyExistingIdAndLun(context, id, lun)) {
        return false;
    }

    auto device = controller_factory->GetDeviceForIdAndLun(id, lun);

    if (!ValidateOperationAgainstDevice(context, *device, operation)) {
        return false;
    }

    switch (operation) {
    case START:
        return Start(*device, dryRun);

    case STOP:
        return Stop(*device, dryRun);

    case ATTACH:
        return Attach(context, pb_device, dryRun);

    case DETACH:
        return Detach(context, *device, dryRun);

    case INSERT:
        return Insert(context, pb_device, device, dryRun);

    case EJECT:
        return Eject(*device, dryRun);

    case PROTECT:
        return Protect(*device, dryRun);

    case UNPROTECT:
        return Unprotect(*device, dryRun);
        break;

    default:
        return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION, to_string(operation));
    }

    return true;
}

bool CommandExecutor::ProcessCmd(const CommandContext &context)
{
    const PbCommand &command = context.GetCommand();

    // Handle commands that are not device-specific
    switch (command.operation()) {
    case DETACH_ALL:
        DetachAll();
        return context.ReturnSuccessStatus();

    case RESERVE_IDS: {
        if (const string error = SetReservedIds(GetParam(command, "ids")); !error.empty()) {
            return context.ReturnErrorStatus(error);
        }
        else {
            PropertyHandler::Instance().AddProperty("reserved_ids", Join(reserved_ids, ","));
            return context.ReturnSuccessStatus();
        }
    }

    case CHECK_AUTHENTICATION:
    case NO_OPERATION:
        // Do nothing, just log
        trace("Received %s command", PbOperation_Name(command.operation()));
        return context.ReturnSuccessStatus();

    default:
        // This is a device-specific command handled below
        break;
    }

    // Remember the list of reserved files during the dry run
    const auto &reserved_files = StorageDevice::GetReservedFiles();
    const bool isDryRunError = ranges::find_if_not(command.devices(), [&](const auto &device)
        {   return ProcessDeviceCmd(context, device, true);}) != command.devices().end();
    StorageDevice::SetReservedFiles(reserved_files);

    if (isDryRunError) {
        return false;
    }

    if (const string error = EnsureLun0(command); !error.empty()) {
        return context.ReturnErrorStatus(error);
    }

    if (ranges::find_if_not(command.devices(), [&](const auto &device)
        {   return ProcessDeviceCmd(context, device, false);}) != command.devices().end()) {
        return false;
    }

    // ATTACH and DETACH are special cases because they return the current device list
    return command.operation() == ATTACH || command.operation() == DETACH ? true : context.ReturnSuccessStatus();
}

bool CommandExecutor::Start(PrimaryDevice &device, bool dryRun) const
{
    if (!dryRun) {
        info("Start requested for {}", GetIdentifier(device));

        if (!device.Start()) {
            warn("Starting {} failed", GetIdentifier(device));
        }
    }

    return true;
}

bool CommandExecutor::Stop(PrimaryDevice &device, bool dryRun) const
{
    if (!dryRun) {
        info("Stop requested for {}", GetIdentifier(device));

        device.Stop();

        device.SetStatus(sense_key::no_sense, asc::no_additional_sense_information);
    }

    return true;
}

bool CommandExecutor::Eject(PrimaryDevice &device, bool dryRun) const
{
    if (!dryRun) {
        info("Eject requested for {}", GetIdentifier(device));

        if (!device.Eject(true)) {
            warn("Ejecting {} failed", GetIdentifier(device));
        }
    }

    return true;
}

bool CommandExecutor::Protect(PrimaryDevice &device, bool dryRun) const
{
    if (!dryRun) {
        info("Write protection requested for {}", GetIdentifier(device));

        device.SetProtected(true);
    }

    return true;
}

bool CommandExecutor::Unprotect(PrimaryDevice &device, bool dryRun) const
{
    if (!dryRun) {
        info("Write unprotection requested for {}", GetIdentifier(device));

        device.SetProtected(false);
    }

    return true;
}

bool CommandExecutor::Attach(const CommandContext &context, const PbDeviceDefinition &pb_device, bool dryRun)
{
    const int id = pb_device.id();
    const int lun = pb_device.unit();

    if (lun >= ControllerFactory::GetScsiLunMax()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_INVALID_LUN, to_string(lun),
            to_string(ControllerFactory::GetScsiLunMax()));
    }

    if (controller_factory->HasDeviceForIdAndLun(id, lun)) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_DUPLICATE_ID, to_string(id), to_string(lun));
    }

    if (reserved_ids.contains(id)) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_RESERVED_ID, to_string(id));
    }

    const string filename = GetParam(pb_device, "file");

    const PbDeviceType type = pb_device.type();

    auto device = CreateDevice(context, type, lun, filename);
    if (!device) {
        return false;
    }

    const PbCachingMode caching_mode =
        pb_device.caching_mode() == PbCachingMode::DEFAULT ? PbCachingMode::PISCSI : pb_device.caching_mode();
    if (caching_mode == PbCachingMode::DEFAULT) {
        // The requested caching mode is not available for this device type
        return false;
    }

    if (!SetScsiLevel(context, device, pb_device.scsi_level())) {
        return false;
    }

    // If no filename was provided the medium is considered not inserted
    device->SetRemoved(device->SupportsFile() ? filename.empty() : false);

    if (!SetProductData(context, pb_device, *device)) {
        return false;
    }

    if (!SetSectorSize(context, device, pb_device.block_size())) {
        return false;
    }

#ifdef BUILD_DISK
    const auto storage_device = dynamic_pointer_cast<StorageDevice>(device);
    if (device->SupportsFile()) {
        // The caching mode must be set before the file is accessed
        if (const auto disk = dynamic_pointer_cast<Disk>(device)) {
            disk->SetCachingMode(caching_mode);
        }

        // Only with removable media drives, CD and MO the medium (=file) may be inserted later
        if (!device->IsRemovable() && filename.empty()) {
            return context.ReturnLocalizedError(LocalizationKey::ERROR_DEVICE_MISSING_FILENAME,
                fmt::format("{0} {1}:{2}", device->GetTypeString(), id, lun));
        }

        if (!ValidateImageFile(context, *storage_device, filename)) {
            return false;
        }
    }
#endif

    // Only non read-only devices support protect/unprotect
    // This operation must not be executed before Open() because Open() overrides some settings.
    if (device->IsProtectable() && !device->IsReadOnly()) {
        device->SetProtected(pb_device.protected_());
    }

    // Stop the dry run here, before actually attaching
    if (dryRun) {
        return true;
    }

    param_map params = { pb_device.params().begin(), pb_device.params().end() };
    if (!device->SupportsFile()) {
        // Legacy clients like scsictl might have sent both "file" and "interfaces"
        params.erase("file");
    }

    if (!device->Init(params)) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_INITIALIZATION,
            fmt::format("{0} {1}:{2}", PbDeviceType_Name(device->GetType()), id, lun));
    }

    if (!controller_factory->AttachToController(bus, id, device)) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_CONTROLLER);
    }

#ifdef BUILD_DISK
    if (storage_device && !storage_device->IsRemoved()) {
        storage_device->ReserveFile();
    }
#endif

    SetUpDeviceProperties(context, device);

    string msg = "Attached ";
    if (device->IsReadOnly()) {
        msg += "read-only ";
    }
    else if (device->IsProtectable() && device->IsProtected()) {
        msg += "protected ";
    }
    msg += GetIdentifier(*device);
    info(msg);

    return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
bool CommandExecutor::Insert(const CommandContext &context, const PbDeviceDefinition &pb_device,
    const shared_ptr<PrimaryDevice> &device, bool dryRun) const
{
    if (!device->SupportsFile()) {
        return false;
    }

    if (!device->IsRemoved()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_EJECT_REQUIRED);
    }

    if (!pb_device.vendor().empty() || !pb_device.product().empty() || !pb_device.revision().empty()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_DEVICE_NAME_UPDATE);
    }

    const string filename = GetParam(pb_device, "file");
    if (filename.empty()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_DEVICE_MISSING_FILENAME, GetIdentifier(*device));
    }

    // Stop the dry run here, before modifying the device
    if (dryRun) {
        return true;
    }

    info("Insert " + string(pb_device.protected_() ? "protected " : "") + "file '" + filename + "' requested into "
        + GetIdentifier(*device));

    if (!SetSectorSize(context, device, pb_device.block_size())) {
        return false;
    }

#ifdef BUILD_DISK
    auto storage_device = dynamic_pointer_cast<StorageDevice>(device);
    if (!ValidateImageFile(context, *storage_device, filename)) {
        return false;
    }

    storage_device->SetProtected(pb_device.protected_());
    storage_device->ReserveFile();
    storage_device->SetMediumChanged(true);
#endif

    return true;
}
#pragma GCC diagnostic pop

bool CommandExecutor::Detach(const CommandContext &context, PrimaryDevice &device, bool dryRun) const
{
    auto controller = controller_factory->FindController(device.GetId());
    if (!controller) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_DETACH);
    }

    // LUN 0 can only be detached if there is no other LUN anymore
    if (!device.GetLun() && controller->GetLunCount() > 1) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_LUN0);
    }

    if (!dryRun) {
        // Remember some device data before the device data become invalid on removal
        const string identifier = GetIdentifier(device);
        const int id = device.GetId();
        const int lun = device.GetLun();

        if (!controller->RemoveDevice(device)) {
            return context.ReturnLocalizedError(LocalizationKey::ERROR_DETACH);
        }

        // If no LUN is left also delete the controller
        if (!controller->GetLunCount() && !controller_factory->DeleteController(*controller)) {
            return context.ReturnLocalizedError(LocalizationKey::ERROR_DETACH);
        }

        // Consider both potential identifiers if the LUN is 0
        PropertyHandler::Instance().RemoveProperties(fmt::format("device.{}.", identifier));
        if (!lun) {
            PropertyHandler::Instance().RemoveProperties(fmt::format("device.{}.", id));
        }

        info("Detached " + identifier);
    }

    return true;
}

void CommandExecutor::DetachAll() const
{
    if (controller_factory->DeleteAllControllers()) {
        PropertyHandler::Instance().RemoveProperties("device.");

        info("Detached all devices");
    }
}

void CommandExecutor::SetUpDeviceProperties(const CommandContext &context, shared_ptr<PrimaryDevice> device)
{
    const string &identifier = fmt::format("device.{0}:{1}.", device->GetId(), device->GetLun());
    PropertyHandler::Instance().AddProperty(identifier + "type", device->GetTypeString());
    PropertyHandler::Instance().AddProperty(identifier + "product",
        device->GetVendor() + ":" + device->GetProduct() + ":" + device->GetRevision());
    const auto disk = dynamic_pointer_cast<Disk>(device);
    if (disk && disk->GetConfiguredSectorSize()) {
        PropertyHandler::Instance().AddProperty(identifier + "block_size", to_string(disk->GetConfiguredSectorSize()));

    }
    if (disk && !disk->GetFilename().empty()) {
        if (string filename = disk->GetFilename(); filename.starts_with(context.GetDefaultFolder())) {
            filename = filename.substr(context.GetDefaultFolder().length() + 1);
        }
        else {
            PropertyHandler::Instance().AddProperty(identifier + "params", filename);
        }
    }
    else if (!device->GetParams().empty()) {
        vector<string> p;
        for (const auto& [param, value] : device->GetParams()) {
            p.emplace_back(param + "=" + value);
        }
        PropertyHandler::Instance().AddProperty(identifier + "params", Join(p, ":"));
    }
}

string CommandExecutor::SetReservedIds(string_view ids)
{
    set<int> ids_to_reserve;
    stringstream ss(ids.data());
    string id;
    while (getline(ss, id, ',')) {
        int res_id;
        if (!GetAsUnsignedInt(id, res_id) || res_id > 7) {
            return "Invalid ID " + id;
        }

        if (controller_factory->HasController(res_id)) {
            return "ID " + id + " is currently in use";
        }

        ids_to_reserve.insert(res_id);
    }

    reserved_ids = { ids_to_reserve.begin(), ids_to_reserve.end() };

    if (!ids_to_reserve.empty()) {
        info("Reserved ID(s) set to {}", Join(ids_to_reserve));
    }
    else {
        info("Cleared reserved ID(s)");
    }

    return "";
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
bool CommandExecutor::ValidateImageFile(const CommandContext &context, StorageDevice &storage_device,
    const string &filename) const
{
#ifdef BUILD_DISK
    if (filename.empty()) {
        return true;
    }

    if (!CheckForReservedFile(context, filename)) {
        return false;
    }

    storage_device.SetFilename(filename);

    if (!StorageDevice::FileExists(filename)) {
        // If the file does not exist search for it in the default image folder
        const string effective_filename = context.GetDefaultFolder() + "/" + filename;

        if (!CheckForReservedFile(context, effective_filename)) {
            return false;
        }

        storage_device.SetFilename(effective_filename);
    }

    try {
        storage_device.Open();
    }
    catch (const io_exception&) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_FILE_OPEN, storage_device.GetFilename());
    }
#endif

    return true;
}
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
bool CommandExecutor::CheckForReservedFile(const CommandContext &context, const string &filename)
{
#ifdef BUILD_DISK
    if (const auto [id, lun] = StorageDevice::GetIdsForReservedFile(filename); id != -1) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_IMAGE_IN_USE, filename,
            to_string(id) + ":" + to_string(lun));
    }
#endif

    return true;
}
#pragma GCC diagnostic pop

string CommandExecutor::PrintCommand(const PbCommand &command, const PbDeviceDefinition &pb_device) const
{
    const map<string, string, less<>> params = { command.params().begin(), command.params().end() };

    ostringstream s;
    s << "operation=" << PbOperation_Name(command.operation());

    if (!params.empty()) {
        s << ", command parameters=";
        bool isFirst = true;
        for (const auto& [key, value] : params) {
            if (!isFirst) {
                s << ", ";
            }
            isFirst = false;
            string v = key != "token" ? value : "???";
            s << "'" << key << "=" << v << "'";
        }
    }

    s << ", device=" << pb_device.id() << ":" << pb_device.unit();

    if (pb_device.type() != PbDeviceType::UNDEFINED) {
        s << ", type=" << PbDeviceType_Name(pb_device.type());
    }

    if (pb_device.params_size()) {
        s << ", device parameters=";
        bool isFirst = true;
        for (const auto& [key, value] : pb_device.params()) {
            if (!isFirst) {
                s << ":";
            }
            isFirst = false;
            s << "'" << key << "=" << value << "'";
        }
    }

    if (!pb_device.vendor().empty()) {
        s << ", vendor='" << pb_device.vendor();
    }
    if (!pb_device.product().empty()) {
        s << "', product='" << pb_device.product();
    }
    if (!pb_device.revision().empty()) {
        s << "', revision='" << pb_device.revision();
    }

    if (pb_device.block_size()) {
        s << "', block size=" << pb_device.block_size();
    }

    if (pb_device.caching_mode() != PbCachingMode::DEFAULT) {
        s << ", caching mode=" << PbCachingMode_Name(pb_device.caching_mode());
    }

    return s.str();
}

string CommandExecutor::EnsureLun0(const PbCommand &command) const
{
    // Mapping of available LUNs (bit vector) to devices
    unordered_map<int32_t, int32_t> luns;

    // Collect LUN bit vectors of new devices
    for (const auto &device : command.devices()) {
        luns[device.id()] |= 1 << device.unit();
    }

    // Collect LUN bit vectors of existing devices
    for (const auto &device : GetAllDevices()) {
        luns[device->GetId()] |= 1 << device->GetLun();
    }

    const auto &it = ranges::find_if_not(luns, [](const auto &l) {return l.second & 0x01;});
    return it == luns.end() ? "" : "LUN 0 is missing for device ID " + to_string((*it).first);
}

bool CommandExecutor::VerifyExistingIdAndLun(const CommandContext &context, int id, int lun) const
{
    if (!controller_factory->HasController(id)) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_NON_EXISTING_DEVICE, to_string(id));
    }

    if (!controller_factory->HasDeviceForIdAndLun(id, lun)) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_NON_EXISTING_UNIT, to_string(id), to_string(lun));
    }

    return true;
}

shared_ptr<PrimaryDevice> CommandExecutor::CreateDevice(const CommandContext &context, const PbDeviceType type,
    int lun, const string &filename) const
{
    auto device = DeviceFactory::Instance().CreateDevice(type, lun, filename);
    if (!device) {
        if (type == UNDEFINED) {
            context.ReturnLocalizedError(LocalizationKey::ERROR_MISSING_DEVICE_TYPE, filename);
        }
        else {
            context.ReturnLocalizedError(LocalizationKey::ERROR_UNKNOWN_DEVICE_TYPE, PbDeviceType_Name(type));
        }

        return nullptr;
    }

    // Some device types must be unique
    if (UNIQUE_DEVICE_TYPES.contains(device->GetType())) {
        for (const auto &d : GetAllDevices()) {
            if (d->GetType() == device->GetType()) {
                context.ReturnLocalizedError(LocalizationKey::ERROR_UNIQUE_DEVICE_TYPE,
                    PbDeviceType_Name(device->GetType()));
                return nullptr;
            }
        }
    }

    return device;
}

bool CommandExecutor::SetScsiLevel(const CommandContext &context, shared_ptr<PrimaryDevice> device, int level) const
{
    if (level && !device->SetScsiLevel(static_cast<scsi_level>(level))) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_SCSI_LEVEL, to_string(level));
    }

    return true;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
bool CommandExecutor::SetSectorSize(const CommandContext &context, shared_ptr<PrimaryDevice> device,
    int sector_size) const
{
#ifdef BUILD_DISK
    if (sector_size) {
        const auto disk = dynamic_pointer_cast<Disk>(device);
        if (disk && disk->IsSectorSizeConfigurable()) {
            if (!disk->SetConfiguredSectorSize(sector_size)) {
                return context.ReturnLocalizedError(LocalizationKey::ERROR_BLOCK_SIZE, to_string(sector_size));
            }
        }
        else {
            return context.ReturnLocalizedError(LocalizationKey::ERROR_BLOCK_SIZE_NOT_CONFIGURABLE,
                device->GetTypeString());
        }
    }
#endif

    return true;
}
#pragma GCC diagnostic pop

bool CommandExecutor::ValidateOperationAgainstDevice(const CommandContext &context, const PrimaryDevice &device,
    PbOperation operation)
{
    if ((operation == START || operation == STOP) && !device.IsStoppable()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION_DENIED_STOPPABLE,
            PbOperation_Name(operation),
            device.GetTypeString());
    }

    if ((operation == INSERT || operation == EJECT) && !device.IsRemovable()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION_DENIED_REMOVABLE,
            PbOperation_Name(operation),
            device.GetTypeString());
    }

    if ((operation == PROTECT || operation == UNPROTECT) && !device.IsProtectable()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION_DENIED_PROTECTABLE,
            PbOperation_Name(operation),
            device.GetTypeString());
    }

    if ((operation == PROTECT || operation == UNPROTECT) && !device.IsReady()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_OPERATION_DENIED_READY, PbOperation_Name(operation),
            device.GetTypeString());
    }

    return true;
}

bool CommandExecutor::ValidateIdAndLun(const CommandContext &context, int id, int lun)
{
    if (id < 0) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_MISSING_DEVICE_ID);
    }
    if (id >= 8) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_INVALID_ID, to_string(id));
    }
    if (lun < 0 || lun >= ControllerFactory::GetLunMax()) {
        return context.ReturnLocalizedError(LocalizationKey::ERROR_INVALID_LUN, to_string(lun),
            to_string(ControllerFactory::GetLunMax() - 1));
    }

    return true;
}

bool CommandExecutor::SetProductData(const CommandContext &context, const PbDeviceDefinition &pb_device,
    PrimaryDevice &device)
{
    try {
        if (!pb_device.vendor().empty()) {
            device.SetVendor(pb_device.vendor());
        }
        if (!pb_device.product().empty()) {
            device.SetProduct(pb_device.product());
        }
        if (!pb_device.revision().empty()) {
            device.SetRevision(pb_device.revision());
        }
    }
    catch (const invalid_argument &e) {
        return context.ReturnErrorStatus(e.what());
    }

    return true;
}
