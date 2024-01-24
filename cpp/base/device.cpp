//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <spdlog/spdlog.h>
#include <cassert>
#include <stdexcept>
#include "shared/s2p_version.h"
#include "device.h"

using namespace spdlog;
using namespace std;

Device::Device(PbDeviceType type, int lun) : type(type), lun(lun)
{
    revision = fmt::format("{0:02}{1:02}", s2p_major_version, s2p_minor_version);
}

void Device::Reset()
{
    locked = false;
    attn = false;
    reset = false;
}

void Device::SetProtected(bool b)
{
    if (protectable && !read_only) {
        write_protected = b;
    }
}

void Device::SetVendor(const string &v)
{
    if (v.empty() || v.length() > 8) {
        throw invalid_argument("Vendor '" + v + "' must have between 1 and 8 characters");
    }

    vendor = v;
}

void Device::SetProduct(const string &p, bool force)
{
    if (p.empty() || p.length() > 16) {
        throw invalid_argument("Product '" + p + "' must have between 1 and 16 characters");
    }

    // Changing vital product data is not SCSI compliant
    if (!product.empty() && !force) {
        return;
    }

    product = p;
}

void Device::SetRevision(const string &r)
{
    if (r.empty() || r.length() > 4) {
        throw invalid_argument("Revision '" + r + "' must have between 1 and 4 characters");
    }

    revision = r;
}

string Device::GetPaddedName() const
{
    return fmt::format("{0:8}{1:16}{2:4}", vendor, product, revision);
}

string Device::GetParam(const string &key) const
{
    const auto &it = params.find(key);
    return it == params.end() ? "" : it->second;
}

void Device::SetParams(const param_map &set_params)
{
    params = GetDefaultParams();

    // Devices with image file support implicitly support the "file" parameter
    if (SupportsFile()) {
        params["file"] = "";
    }

    for (const auto& [key, value] : set_params) {
        // It is assumed that there are default parameters for all supported parameters
        if (params.contains(key)) {
            params[key] = value;
        }
        else {
            warn("Ignored unknown parameter '" + key + "'");
        }
    }
}

bool Device::Start()
{
    if (!ready) {
        return false;
    }

    stopped = false;

    return true;
}

void Device::Stop()
{
    ready = false;
    attn = false;
    stopped = true;

    status_code = 0;
}

bool Device::Eject(bool force)
{
    if (!ready || !removable) {
        return false;
    }

    // Must be unlocked if there is no force flag
    if (!force && locked) {
        return false;
    }

    ready = false;
    attn = false;
    removed = true;
    write_protected = false;
    locked = false;
    stopped = true;

    return true;
}
