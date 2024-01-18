//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pctl_core.h"

using namespace std;

int main(int argc, char *argv[])
{
    const vector<char*> args(argv, argv + argc);

    return S2pCtl().Run(args);
}
