//---------------------------------------------------------------------------
//
// SCSI2Pi, SCSI device emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2025 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "s2p_thread.h"
#include "command/command_context.h"
#include "shared/s2p_exceptions.h"

using namespace s2p_util;

string S2pThread::Init(int port, const callback &cb, shared_ptr<logger> logger)
{
    exec = cb;
    s2p_logger = logger;

    return server.Init(port);
}

void S2pThread::Start()
{
    assert(server.IsRunning());

#ifndef __APPLE__
    service_thread = jthread([this]() {Execute();});
#else
    service_thread = thread([this]() {Execute();});
#endif
}

// This method might be called twice when pressing Ctrl-C, because of the installed handlers
void S2pThread::Stop()
{
    server.CleanUp();
}

bool S2pThread::IsRunning() const
{
    return server.IsRunning() && service_thread.joinable();
}

void S2pThread::Execute() const
{
    while (server.IsRunning()) {
        if (const int fd = server.Accept(); fd != -1) {
            ExecuteCommand(fd);
            close(fd);
        }
    }
}

void S2pThread::ExecuteCommand(int fd) const
{
    CommandContext context(fd, *s2p_logger);
    try {
        if (context.ReadCommand()) {
            exec(context);
        }
    }
    catch (const IoException &e) {
        s2p_logger->warn(e.what());

        // Try to return an error message (this may fail if the exception was caused when returning the actual result)
        PbResult result;
        result.set_msg(e.what());
        try {
            context.WriteResult(result);
        }
        catch (const IoException&) { // NOSONAR Ignored because not relevant when writing the result
            // Ignore
        }
    }
}
