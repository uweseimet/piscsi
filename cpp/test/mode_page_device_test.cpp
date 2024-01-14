//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/shared_exceptions.h"
#include "devices/mode_page_device.h"

using namespace std;

TEST(ModePageDeviceTest, SupportsSaveParameters)
{
    MockModePageDevice device;

    EXPECT_FALSE(device.SupportsSaveParameters()) << "Wrong default value";
    device.SupportsSaveParameters(true);
    EXPECT_TRUE(device.SupportsSaveParameters());
    device.SupportsSaveParameters(false);
    EXPECT_FALSE(device.SupportsSaveParameters());
}

TEST(ModePageDeviceTest, AddModePages)
{
    vector<int> cdb(6);
    vector<uint8_t> buf(512);
    MockModePageDevice device;

    // Page 0
    cdb[2] = 0x00;
    EXPECT_THAT([&] {device.AddModePages(cdb, buf, 0, 12, 255);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    << "Data were returned for non-existing mode page 0";

    // All pages, non changeable
    cdb[2] = 0x3f;
    EXPECT_EQ(0, device.AddModePages(cdb, buf, 0, 0, 255));
    EXPECT_EQ(3, device.AddModePages(cdb, buf, 0, 3, 255));
    EXPECT_THAT([&] {device.AddModePages(cdb, buf, 0, 12, -1);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb)))) << "Maximum size was ignored";

    // All pages, changeable
    cdb[2] = 0x7f;
    EXPECT_EQ(0, device.AddModePages(cdb, buf, 0, 0, 255));
    EXPECT_EQ(3, device.AddModePages(cdb, buf, 0, 3, 255));
    EXPECT_THAT([&] {device.AddModePages(cdb, buf, 0, 12, -1);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb)))) << "Maximum size was ignored";
}

TEST(ModePageDeviceTest, Page0)
{
    vector<int> cdb(6);
    vector<uint8_t> buf(512);
    MockPage0ModePageDevice device;

    cdb[2] = 0x3f;
    EXPECT_EQ(0, device.AddModePages(cdb, buf, 0, 0, 255));
    EXPECT_EQ(1, device.AddModePages(cdb, buf, 0, 1, 255));
}

TEST(ModePageDeviceTest, AddVendorModePages)
{
    map<int, vector<byte>> pages;
    MockModePageDevice device;

    device.AddVendorModePages(pages, 0x3f, false);
    EXPECT_TRUE(pages.empty()) << "Unexpected default vendor mode page";
    device.AddVendorModePages(pages, 0x3f, true);
    EXPECT_TRUE(pages.empty()) << "Unexpected default vendor mode page";
}

TEST(ModePageDeviceTest, ModeSense6)
{
    auto controller = make_shared<MockAbstractController>(0);
    auto device = make_shared<NiceMock<MockModePageDevice>>();
    EXPECT_TRUE(device->Init( { }));

    controller->AddDevice(device);

    EXPECT_CALL(*controller, DataIn());
    device->Dispatch(scsi_command::cmd_mode_sense6);
}

TEST(ModePageDeviceTest, ModeSense10)
{
    auto controller = make_shared<MockAbstractController>(0);
    auto device = make_shared<NiceMock<MockModePageDevice>>();
    EXPECT_TRUE(device->Init( { }));

    controller->AddDevice(device);

    EXPECT_CALL(*controller, DataIn());
    device->Dispatch(scsi_command::cmd_mode_sense10);
}

TEST(ModePageDeviceTest, ModeSelect)
{
    MockModePageDevice device;
    vector<int> cmd;
    vector<uint8_t> buf;

    EXPECT_THAT([&] {device.ModeSelect(scsi_command::cmd_mode_select6, cmd, buf, 0);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    << "Unexpected MODE SELECT(6) default implementation";
    EXPECT_THAT([&] {device.ModeSelect(scsi_command::cmd_mode_select10, cmd, buf, 0);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    << "Unexpected MODE SELECT(10) default implementation";
}

TEST(ModePageDeviceTest, ModeSelect6)
{
    auto controller = make_shared<MockAbstractController>(0);
    auto device = make_shared<MockModePageDevice>();
    EXPECT_TRUE(device->Init( { }));

    controller->AddDevice(device);

    EXPECT_CALL(*controller, DataOut());
    device->Dispatch(scsi_command::cmd_mode_select6);

    controller->SetCmdByte(1, 0x01);
    EXPECT_THAT([&] {device->Dispatch(scsi_command::cmd_mode_select6);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    << "Saving parameters is not supported by base class";
}

TEST(ModePageDeviceTest, ModeSelect10)
{
    auto controller = make_shared<MockAbstractController>(0);
    auto device = make_shared<MockModePageDevice>();
    EXPECT_TRUE(device->Init( { }));

    controller->AddDevice(device);

    EXPECT_CALL(*controller, DataOut());
    device->Dispatch(scsi_command::cmd_mode_select10);

    controller->SetCmdByte(1, 0x01);
    EXPECT_THAT([&] {device->Dispatch(scsi_command::cmd_mode_select10);}, Throws<scsi_exception>(AllOf(
                Property(&scsi_exception::get_sense_key, sense_key::illegal_request),
                Property(&scsi_exception::get_asc, asc::invalid_field_in_cdb))))
    << "Saving parameters is not supported for by base class";
}
