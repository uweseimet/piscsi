//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2021-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#pragma once

#include <climits>
#include <string>
#include <sstream>
#include <vector>

using namespace std;

namespace s2p_util
{
// Separator for compound options like ID:LUN
static const char COMPONENT_SEPARATOR = ':';

struct StringHash
{
    using is_transparent = void;

    size_t operator()(string_view sv) const
    {
        hash<string_view> hasher;
        return hasher(sv);
    }
};

string Join(const auto &collection, const string_view separator = ", ")
{
    ostringstream s;

    for (const auto &element : collection) {
        if (s.tellp()) {
            s << separator;
        }

        s << element;
    }

    return s.str();
}

string GetVersionString();
string GetHomeDir();
pair<int, int> GetUidAndGid();
vector<string> Split(const string&, char, int = INT_MAX);
string GetLocale();
bool GetAsUnsignedInt(const string&, int&);
string ProcessId(int, int, const string&, int&, int&);
string Banner(string_view, bool = true);

string GetExtensionLowerCase(string_view);

void LogErrno(const string&);
}
