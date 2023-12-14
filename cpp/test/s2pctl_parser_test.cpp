//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include "s2pctl/s2pctl_parser.h"

TEST(S2pCtlParserTest, ParseOperation)
{
    S2pCtlParser parser;

    EXPECT_EQ(ATTACH, parser.ParseOperation("A"));
    EXPECT_EQ(ATTACH, parser.ParseOperation("a"));
    EXPECT_EQ(DETACH, parser.ParseOperation("d"));
    EXPECT_EQ(INSERT, parser.ParseOperation("i"));
    EXPECT_EQ(EJECT, parser.ParseOperation("e"));
    EXPECT_EQ(PROTECT, parser.ParseOperation("p"));
    EXPECT_EQ(UNPROTECT, parser.ParseOperation("u"));
    EXPECT_EQ(NO_OPERATION, parser.ParseOperation(""));
    EXPECT_EQ(NO_OPERATION, parser.ParseOperation("xyz"));
}

TEST(S2pCtlParserTest, ParseType)
{
    S2pCtlParser parser;

    EXPECT_EQ(SCCD, parser.ParseType("sccd"));
    EXPECT_EQ(SCDP, parser.ParseType("scdp"));
    EXPECT_EQ(SCHD, parser.ParseType("schd"));
    EXPECT_EQ(SCLP, parser.ParseType("sclp"));
    EXPECT_EQ(SCMO, parser.ParseType("scmo"));
    EXPECT_EQ(SCRM, parser.ParseType("scrm"));
    EXPECT_EQ(SCHS, parser.ParseType("schs"));

    EXPECT_EQ(SCCD, parser.ParseType("c"));
    EXPECT_EQ(SCDP, parser.ParseType("d"));
    EXPECT_EQ(SCHD, parser.ParseType("h"));
    EXPECT_EQ(SCLP, parser.ParseType("l"));
    EXPECT_EQ(SCMO, parser.ParseType("m"));
    EXPECT_EQ(SCRM, parser.ParseType("r"));
    EXPECT_EQ(SCHS, parser.ParseType("s"));

    EXPECT_EQ(UNDEFINED, parser.ParseType(""));
    EXPECT_EQ(UNDEFINED, parser.ParseType("xyz"));
}
