//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
// A device implementing mandatory SCSI primary commands, to be used for subclassing
//
//---------------------------------------------------------------------------

#pragma once

#include <functional>
#include "interfaces/scsi_primary_commands.h"
#include "controllers/abstract_controller.h"
#include "shared/s2p_exceptions.h"
#include "s2p_defs.h"
#include "device.h"

class PrimaryDevice : public ScsiPrimaryCommands, public Device
{
    friend class AbstractController;
    friend class PageHandler;

    using command = function<void()>;

public:

    ~PrimaryDevice() override = default;

    bool Init();
    virtual bool SetUp() = 0;
    virtual void CleanUp()
    {
        // Override if cleanup work is required for a derived device
    }

    virtual void Dispatch(scsi_command);

    auto GetController() const
    {
        return controller;
    }

    scsi_level GetScsiLevel() const
    {
        return level;
    }
    bool SetScsiLevel(scsi_level);

    enum sense_key GetSenseKey() const
    {
        return sense_key;
    }
    enum asc GetAsc() const
    {
        return asc;
    }
    void SetStatus(sense_key, asc);
    void ResetStatus();

    int GetId() const override;

    int GetDelayAfterBytes() const
    {
        return delay_after_bytes;
    }

    bool CheckReservation(int) const;
    void DiscardReservation();

    void Reset();

    virtual int ReadData(data_in_t)
    {
        // Devices that implement a DATA IN phase have to override this method

        return 0;
    }

    // For DATA OUT phase, except for MODE SELECT
    virtual void WriteData(cdb_t, data_out_t, int, int) = 0;

    virtual void ModeSelect(cdb_t, data_out_t, int, int)
    {
        // There is no default implementation of MODE SELECT
        throw scsi_exception(sense_key::illegal_request, asc::invalid_field_in_cdb);
    }

    virtual void FlushCache()
    {
        // Devices with a cache have to override this method
    }

    // Devices providing statistics have to override this method
    virtual vector<PbStatistics> GetStatistics() const
    {
        return vector<PbStatistics>();
    }

protected:

    PrimaryDevice(PbDeviceType type, int lun, int delay = SEND_NO_DELAY)
    : Device(type, lun), delay_after_bytes(delay)
    {
    }

    void AddCommand(scsi_command, const command&);

    vector<uint8_t> HandleInquiry(device_type, bool) const;
    virtual vector<uint8_t> InquiryInternal() const = 0;
    void CheckReady();

    void Inquiry() override;
    void RequestSense() override;


    void SendDiagnostic() override;
    void ReserveUnit() override;
    void ReleaseUnit() override;

    virtual int ModeSense6(cdb_t, data_in_t) const
    {
        // Nothing to do in base class
        return 0;
    }
    virtual int ModeSense10(cdb_t, data_in_t) const
    {
        // Nothing to do in base class
        return 0;
    }
    virtual void SetUpModePages(map<int, vector<byte>>&, int, bool) const
    {
        // Nothing to do in base class
    }

    void SetFilemark();
    void SetEom(ascq);
    void SetIli();
    void SetInformation(int64_t);

    void StatusPhase() const;
    void DataInPhase(int) const;
    void DataOutPhase(int) const;

    auto GetCdbByte(int index) const
    {
        return controller->GetCdb()[index];
    }
    auto GetCdbInt16(int index) const
    {
        return memory_util::GetInt16(controller->GetCdb(), index);
    }
    auto GetCdbInt24(int index) const
    {
        return memory_util::GetInt24(controller->GetCdb(), index);
    }
    auto GetCdbInt32(int index) const
    {
        return memory_util::GetInt32(controller->GetCdb(), index);
    }
    auto GetCdbInt64(int index) const
    {
        return memory_util::GetInt64(controller->GetCdb(), index);
    }

    void LogTrace(const string &s) const
    {
        device_logger.Trace(s);
    }
    void LogDebug(const string &s) const
    {
        device_logger.Debug(s);
    }
    void LogInfo(const string &s) const
    {
        device_logger.Info(s);
    }
    void LogWarn(const string &s) const
    {
        device_logger.Warn(s);
    }
    void LogError(const string &s) const
    {
        device_logger.Error(s);
    }

private:

    static constexpr int NOT_RESERVED = -2;

    void SetController(AbstractController*);

    void TestUnitReady() override;
    void ReportLuns() override;

    vector<byte> HandleRequestSense() const;

    DeviceLogger device_logger;

    scsi_level level = scsi_level::scsi_2;

    enum sense_key sense_key = sense_key::no_sense;
    enum asc asc = asc::no_additional_sense_information;

    bool valid = false;
    bool filemark = false;
    ascq eom = ascq::none;
    bool ili = false;
    int32_t information = 0;

    // Owned by the controller factory
    AbstractController *controller = nullptr;

    array<command, 256> commands = { };

    // Number of bytes during a transfer after which to delay for the Mac DaynaPort driver
    int delay_after_bytes = SEND_NO_DELAY;

    int reserving_initiator = NOT_RESERVED;
};
