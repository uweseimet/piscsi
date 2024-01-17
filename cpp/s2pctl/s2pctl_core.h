//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <vector>
#include <string>

using namespace std;

class ScsiCtl
{

public:

    ScsiCtl() = default;
    ~ScsiCtl() = default;

    int Run(const vector<char*>&) const;

private:

    void Banner(const vector<char*>&) const;
};
