//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <filesystem>
#include <gtest/gtest.h>
#include "test/test_shared.h"
#include "base/property_handler.h"

using namespace testing;

PropertyHandler SetUpProperties(const string &properties, const property_map &cmd_properties = { })
{
    auto &property_handler = PropertyHandler::Instance();
    auto [fd, filename] = OpenTempFile();
    write(fd, properties.data(), properties.size());
    close(fd);
    property_handler.Init(filename, cmd_properties);
    remove(filename);

    return property_handler;
}

TEST(PropertyHandlerTest, Init)
{
    const string &properties1 =
        R"(key1=value1
key2=value2
)";
    const string &properties2 =
        R"(key
)";

    auto property_handler = PropertyHandler::Instance();
    property_handler.Init("", { });
    EXPECT_THROW(property_handler.Init("non_existing_file", { }), parser_exception);

    property_map cmd_properties;
    cmd_properties["key1"] = "value2";
    property_handler = SetUpProperties(properties1, cmd_properties);
    EXPECT_EQ("value2", property_handler.GetProperty("key1"));
    EXPECT_EQ("value2", property_handler.GetProperty("key2"));

    EXPECT_THROW(SetUpProperties(properties2), parser_exception);
}

TEST(PropertyHandlerTest, GetProperty)
{
    const string &properties =
        R"(key1=value1
key2=value2
#key3=value3
)";

    const auto &property_handler = SetUpProperties(properties);

    EXPECT_TRUE(property_handler.GetProperty("key").empty());
    EXPECT_TRUE(property_handler.GetProperty("key3").empty());
    EXPECT_EQ("value1", property_handler.GetProperty("key1"));
    EXPECT_EQ("value2", property_handler.GetProperty("key2"));
}

TEST(PropertyHandlerTest, GetCustomModePages)
{
    const string &properties1 =
        R"(mode_page.0.VENDOR=0010020304ff
mode_page.2.VENDOR:PRODUCT=02:01:B0
mode_page.3.VENDOR:PRODUCT=

mode_page.1._:PRODUCT2=
#mode_page.4.VENDOR=040101
)";

    const string &properties2 =
        R"(mode_page.1.VENDOR:PRODUCT=03:02:ef:ff
)";

    const string &properties3 =
        R"(mode_page.1.VENDOR:PRODUCT=01:03:fe:ff
)";

    const string &properties4 =
        R"(mode_page.63.VENDOR:PRODUCT=3f:01:ff
)";

    const string &properties5 =
        R"(mode_page.2.VENDOR:PRODUCT=02:1:ff
)";

    auto property_handler = SetUpProperties(properties1);

    const auto &mode_pages = property_handler.GetCustomModePages("VENDOR", "PRODUCT");
    EXPECT_EQ(3, mode_pages.size());
    auto value = mode_pages.at(0);
    EXPECT_EQ(6, value.size());
    EXPECT_EQ(byte { 0x00 }, value[0]);
    EXPECT_EQ(byte { 0x10 }, value[1]);
    EXPECT_EQ(byte { 0x02 }, value[2]);
    EXPECT_EQ(byte { 0x03 }, value[3]);
    EXPECT_EQ(byte { 0x04 }, value[4]);
    EXPECT_EQ(byte { 0xff }, value[5]);
    value = mode_pages.at(2);
    EXPECT_EQ(3, value.size());
    EXPECT_EQ(byte { 0x02 }, value[0]);
    EXPECT_EQ(byte { 0x01 }, value[1]);
    EXPECT_EQ(byte { 0xb0 }, value[2]);
    value = mode_pages.at(3);
    EXPECT_TRUE(value.empty());

    property_handler = SetUpProperties(properties2);
    EXPECT_TRUE(property_handler.GetCustomModePages("VENDOR", "PRODUCT").empty())
    << "Mode page codes are inconsistent";

    property_handler = SetUpProperties(properties3);
    EXPECT_TRUE(property_handler.GetCustomModePages("VENDOR", "PRODUCT").empty())
    << "Mode page length field is wrong";

    property_handler = SetUpProperties(properties4);
    EXPECT_TRUE(property_handler.GetCustomModePages("VENDOR", "PRODUCT").empty()) << "Mode page code is invalid";

    property_handler = SetUpProperties(properties5);
    EXPECT_TRUE(property_handler.GetCustomModePages("VENDOR", "PRODUCT").empty()) << "Mode page data format is invalid";
}
