//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2023 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2pdump_core.h"

using namespace std;

int main(int argc, char *argv[])
{
    vector<char*> args(argv, argv + argc);

    return S2pDump().run(args);
}
