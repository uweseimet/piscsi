//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "protobuf/protobuf_util.h"
#include "shared/s2p_exceptions.h"

using namespace protobuf_util;

void TestSpecialDevice(const string &name)
{
    PbDeviceDefinition device;
    ParseParameters(device, name);
    EXPECT_EQ(name, GetParam(device, "file"));
    EXPECT_EQ("", GetParam(device, "interfaces"));
}

TEST(ProtobufUtil, ParseDeviceType)
{
    EXPECT_EQ(SCCD, ParseDeviceType("sccd"));
    EXPECT_EQ(SCDP, ParseDeviceType("scdp"));
    EXPECT_EQ(SCHD, ParseDeviceType("schd"));
    EXPECT_EQ(SCLP, ParseDeviceType("sclp"));
    EXPECT_EQ(SCMO, ParseDeviceType("scmo"));
    EXPECT_EQ(SCRM, ParseDeviceType("scrm"));
    EXPECT_EQ(SCHS, ParseDeviceType("schs"));
    EXPECT_EQ(SCTP, ParseDeviceType("sctp"));
    EXPECT_EQ(SCSG, ParseDeviceType("scsg"));

    EXPECT_EQ(SCCD, ParseDeviceType("c"));
    EXPECT_EQ(SCDP, ParseDeviceType("d"));
    EXPECT_EQ(SCHD, ParseDeviceType("h"));
    EXPECT_EQ(SCLP, ParseDeviceType("l"));
    EXPECT_EQ(SCMO, ParseDeviceType("m"));
    EXPECT_EQ(SCRM, ParseDeviceType("r"));
    EXPECT_EQ(SCHS, ParseDeviceType("s"));
    EXPECT_EQ(SCTP, ParseDeviceType("t"));

    EXPECT_EQ(UNDEFINED, ParseDeviceType(""));
    EXPECT_EQ(UNDEFINED, ParseDeviceType("xyz"));
}

TEST(ProtobufUtil, ParseCachingMode)
{
    EXPECT_EQ(DEFAULT, ParseCachingMode("default"));
    EXPECT_EQ(LINUX, ParseCachingMode("linux"));
    EXPECT_EQ(WRITE_THROUGH, ParseCachingMode("write_through"));
    EXPECT_EQ(WRITE_THROUGH, ParseCachingMode("write-through"));
    EXPECT_EQ(LINUX_OPTIMIZED, ParseCachingMode("linux_optimized"));
    EXPECT_EQ(LINUX_OPTIMIZED, ParseCachingMode("linux-optimized"));

    EXPECT_THROW(ParseCachingMode(""), ParserException);
    EXPECT_THROW(ParseCachingMode("xyz"), ParserException);
}

TEST(ProtobufUtil, GetSetParam)
{
    // The implementation is a function template, testing one possible T is sufficient
    PbCommand command;
    SetParam(command, "key", "value");
    EXPECT_EQ("value", GetParam(command, "key"));
    EXPECT_EQ("", GetParam(command, "xyz"));
    EXPECT_EQ("", GetParam(command, ""));
}

TEST(ProtobufUtil, ParseParameters)
{
    PbDeviceDefinition device1;
    ParseParameters(device1, "a=b:c=d:e");
    EXPECT_EQ("b", GetParam(device1, "a"));
    EXPECT_EQ("d", GetParam(device1, "c"));
    EXPECT_EQ("", GetParam(device1, "e"));

    // Old style parameter
    PbDeviceDefinition device2;
    ParseParameters(device2, "a");
    EXPECT_EQ("a", GetParam(device2, "file"));

    // Ensure that nothing breaks on an empty parameter list
    PbDeviceDefinition device3;
    ParseParameters(device3, "");

    TestSpecialDevice("daynaport");
    TestSpecialDevice("printer");
    TestSpecialDevice("services");
}

TEST(ProtobufUtil, SetCommandParams)
{
    PbCommand command1;

    EXPECT_TRUE(SetCommandParams(command1, "").empty());

    EXPECT_TRUE(SetCommandParams(command1, "file").empty());
    EXPECT_EQ("", GetParam(command1, "folder_pattern"));
    EXPECT_EQ("file", GetParam(command1, "file_pattern"));

    PbCommand command2;
    EXPECT_TRUE(SetCommandParams(command2, ":file").empty());
    EXPECT_EQ("", GetParam(command2, "folder_pattern"));
    EXPECT_EQ("file", GetParam(command2, "file_pattern"));

    PbCommand command3;
    EXPECT_TRUE(SetCommandParams(command3, "file:").empty());
    EXPECT_EQ("file", GetParam(command3, "file_pattern"));
    EXPECT_EQ("", GetParam(command3, "folder_pattern"));

    PbCommand command4;
    EXPECT_TRUE(SetCommandParams(command4, "folder:file").empty());
    EXPECT_EQ("folder", GetParam(command4, "folder_pattern"));
    EXPECT_EQ("file", GetParam(command4, "file_pattern"));

    PbCommand command5;
    EXPECT_TRUE(SetCommandParams(command5, "folder:file:").empty());
    EXPECT_EQ("folder", GetParam(command5, "folder_pattern"));
    EXPECT_EQ("file", GetParam(command5, "file_pattern"));

    PbCommand command6;
    EXPECT_TRUE(SetCommandParams(command6, "folder:file:operations").empty());
    EXPECT_EQ("folder", GetParam(command6, "folder_pattern"));
    EXPECT_EQ("file", GetParam(command6, "file_pattern"));
    EXPECT_EQ("operations", GetParam(command6, "operations"));

    PbCommand command7;
    EXPECT_TRUE(SetCommandParams(command7, "folder:file:operations:unparsed").empty());
    EXPECT_EQ("folder", GetParam(command7, "folder_pattern"));
    EXPECT_EQ("file", GetParam(command7, "file_pattern"));
    EXPECT_EQ("operations:unparsed", GetParam(command7, "operations"));

    PbCommand command_generic;
    EXPECT_TRUE(SetCommandParams(command_generic, "operations=mapping_info:folder_pattern=pattern").empty());
    EXPECT_EQ("mapping_info", GetParam(command_generic, "operations"));
    EXPECT_EQ("pattern", GetParam(command_generic, "folder_pattern"));
}

TEST(ProtobufUtil, SetFromGenericParams)
{
    PbCommand command1;
    EXPECT_TRUE(SetFromGenericParams(command1, "operations=mapping_info:folder_pattern=pattern").empty());
    EXPECT_EQ("mapping_info", GetParam(command1, "operations"));
    EXPECT_EQ("pattern", GetParam(command1, "folder_pattern"));

    PbCommand command2;
    EXPECT_FALSE(SetFromGenericParams(command2, "=mapping_info").empty());

    PbCommand command3;
    EXPECT_FALSE(SetFromGenericParams(command3, "=").empty());
}

TEST(ProtobufUtil, GetLunMax)
{
    EXPECT_EQ(32, GetLunMax(SCHD));
    EXPECT_EQ(2, GetLunMax(SAHD));
}

TEST(ProtobufUtil, ListDevices)
{
    vector<PbDevice> devices;

    EXPECT_FALSE(ListDevices(devices).empty());

    PbDevice device;
    device.set_type(SCHD);
    devices.emplace_back(device);
    device.set_type(SCDP);
    devices.emplace_back(device);
    device.set_type(SCHS);
    devices.emplace_back(device);
    device.set_type(SCLP);
    devices.emplace_back(device);
    const string device_list = ListDevices(devices);
    EXPECT_FALSE(device_list.empty());
}

TEST(ProtobufUtil, SetProductData)
{
    PbDeviceDefinition device;

    SetProductData(device, "");
    EXPECT_EQ("", device.vendor());
    EXPECT_EQ("", device.product());
    EXPECT_EQ("", device.revision());

    SetProductData(device, "vendor");
    EXPECT_EQ("vendor", device.vendor());
    EXPECT_EQ("", device.product());
    EXPECT_EQ("", device.revision());

    SetProductData(device, "vendor:product");
    EXPECT_EQ("vendor", device.vendor());
    EXPECT_EQ("product", device.product());
    EXPECT_EQ("", device.revision());

    SetProductData(device, "vendor:product:revision");
    EXPECT_EQ("vendor", device.vendor());
    EXPECT_EQ("product", device.product());
    EXPECT_EQ("revision", device.revision());
}

TEST(ProtobufUtil, SetIdAndLun)
{
    PbDeviceDefinition device;

    EXPECT_NE("", SetIdAndLun(device, ""));
    EXPECT_EQ("", SetIdAndLun(device, "1"));
    EXPECT_EQ(1, device.id());
    EXPECT_EQ("", SetIdAndLun(device, "2:0"));
    EXPECT_EQ(2, device.id());
    EXPECT_EQ(0, device.unit());
}

TEST(ProtobufUtil, SerializeMessage)
{
    PbResult result;

    const int fd = open("/dev/null", O_WRONLY);
    ASSERT_NE(-1, fd);
    SerializeMessage(fd, result);
    close(fd);
    EXPECT_THROW(SerializeMessage(-1, result), IoException)<< "Writing a message must fail";
}

TEST(ProtobufUtil, DeserializeMessage)
{
    PbResult result;
    vector<byte> buf(1);

    int fd = open("/dev/null", O_RDONLY);
    ASSERT_NE(-1, fd);
    EXPECT_THROW(DeserializeMessage(fd, result), IoException)<< "Reading the message header must fail";
    close(fd);

    auto [fd1, filename1] = OpenTempFile();
    // Data size -1
    buf = { byte { 0xff }, byte { 0xff }, byte { 0xff }, byte { 0xff } };
    EXPECT_EQ((ssize_t )buf.size(), write(fd1, buf.data(), buf.size()));
    close(fd1);
    fd1 = open(filename1.c_str(), O_RDONLY);
    ASSERT_NE(-1, fd1);
    EXPECT_THROW(DeserializeMessage(fd1, result), IoException)<< "Invalid header was not rejected";

    auto [fd2, filename2] = OpenTempFile();
    // Data size 2
    buf = { byte { 0x02 }, byte { 0x00 }, byte { 0x00 }, byte { 0x00 } };
    EXPECT_EQ((ssize_t )buf.size(), write(fd2, buf.data(), buf.size()));
    close(fd2);
    fd2 = open(filename2.c_str(), O_RDONLY);
    EXPECT_NE(-1, fd2);
    EXPECT_THROW(DeserializeMessage(fd2, result), IoException)<< "Invalid data were not rejected";
}

TEST(ProtobufUtil, SerializeDeserializeMessage)
{
    PbResult result;
    result.set_status(true);

    auto [fd, filename] = OpenTempFile();
    ASSERT_NE(-1, fd);
    SerializeMessage(fd, result);
    close(fd);

    result.set_status(false);
    fd = open(filename.c_str(), O_RDONLY);
    ASSERT_NE(-1, fd);
    DeserializeMessage(fd, result);
    close(fd);

    EXPECT_TRUE(result.status());
}

TEST(ProtobufUtil, ReadBytes)
{
    vector<byte> buf1(1);
    vector<byte> buf2;

    int fd = open("/dev/null", O_RDONLY);
    ASSERT_NE(-1, fd);
    EXPECT_EQ(0U, ReadBytes(fd, buf1));
    EXPECT_EQ(0U, ReadBytes(fd, buf2));
    close(fd);

    fd = open("/dev/zero", O_RDONLY);
    ASSERT_NE(-1, fd);
    EXPECT_EQ(1U, ReadBytes(fd, buf1));
    EXPECT_EQ(0U, ReadBytes(fd, buf2));
    close(fd);
}
