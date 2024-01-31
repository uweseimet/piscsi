//---------------------------------------------------------------------------
//
// SCSI target emulator and SCSI tools for the Raspberry Pi
//
// Copyright (C) 2022-2024 Uwe Seimet
//
//---------------------------------------------------------------------------

#include "mocks.h"
#include "shared/shared_exceptions.h"

using namespace scsi_defs;

TEST(AbstractControllerTest, ShutdownMode)
{
    MockAbstractController controller;

    EXPECT_EQ(AbstractController::shutdown_mode::NONE, controller.GetShutdownMode());
    controller.ScheduleShutdown(AbstractController::shutdown_mode::STOP_S2P);
    EXPECT_EQ(AbstractController::shutdown_mode::STOP_S2P, controller.GetShutdownMode());
    controller.ScheduleShutdown(AbstractController::shutdown_mode::STOP_PI);
    EXPECT_EQ(AbstractController::shutdown_mode::STOP_PI, controller.GetShutdownMode());
    controller.ScheduleShutdown(AbstractController::shutdown_mode::RESTART_PI);
    EXPECT_EQ(AbstractController::shutdown_mode::RESTART_PI, controller.GetShutdownMode());
}

TEST(AbstractControllerTest, SetLength)
{
    MockAbstractController controller;

    EXPECT_EQ(4096U, controller.GetBuffer().size());
    controller.SetLength(1);
    EXPECT_LE(1U, controller.GetBuffer().size());
    controller.SetLength(10000);
    EXPECT_LE(10000U, controller.GetBuffer().size());
}

TEST(AbstractControllerTest, Reset)
{
    auto bus = make_shared<MockBus>();
    auto controller = make_shared<MockAbstractController>(bus, 0);
    auto device = make_shared<MockPrimaryDevice>(0);

    controller->AddDevice(device);

    controller->SetPhase(phase_t::status);
    EXPECT_EQ(phase_t::status, controller->GetPhase());
    EXPECT_CALL(*bus, Reset());
    controller->Reset();
    EXPECT_TRUE(controller->IsBusFree());
    EXPECT_EQ(status::good, controller->GetStatus());
    EXPECT_EQ(0U, controller->GetLength());
}

TEST(AbstractControllerTest, Next)
{
    MockAbstractController controller;

    controller.SetNext(0x1234);
    EXPECT_EQ(0x1234U, controller.GetNext());
    controller.IncrementNext();
    EXPECT_EQ(0x1235U, controller.GetNext());
}

TEST(AbstractControllerTest, Message)
{
    MockAbstractController controller;

    controller.SetMessage(0x12);
    EXPECT_EQ(0x12, controller.GetMessage());
}

TEST(AbstractControllerTest, ByteTransfer)
{
    MockAbstractController controller;

    controller.SetByteTransfer(false);
    EXPECT_FALSE(controller.IsByteTransfer());
    controller.SetByteTransfer(true);
    EXPECT_TRUE(controller.IsByteTransfer());
}

TEST(AbstractControllerTest, InitBytesToTransfer)
{
    MockAbstractController controller;

    controller.SetLength(0x1234);
    controller.InitBytesToTransfer();
    EXPECT_EQ(0x1234U, controller.GetBytesToTransfer());
    controller.SetByteTransfer(false);
    EXPECT_EQ(0U, controller.GetBytesToTransfer());
}

TEST(AbstractControllerTest, GetMaxLuns)
{
    MockAbstractController controller;

    EXPECT_EQ(32, controller.GetMaxLuns());
}

TEST(AbstractControllerTest, Status)
{
    MockAbstractController controller;

    controller.SetStatus(status::reservation_conflict);
    EXPECT_EQ(status::reservation_conflict, controller.GetStatus());
}

TEST(AbstractControllerTest, DeviceLunLifeCycle)
{
    const int ID = 1;
    const int LUN = 4;

    auto controller = make_shared<NiceMock<MockAbstractController>>(ID);

    auto device1 = make_shared<MockPrimaryDevice>(LUN);
    auto device2 = make_shared<MockPrimaryDevice>(32);
    auto device3 = make_shared<MockPrimaryDevice>(-1);

    EXPECT_EQ(0, controller->GetLunCount());
    EXPECT_EQ(ID, controller->GetTargetId());
    EXPECT_TRUE(controller->AddDevice(device1));
    EXPECT_FALSE(controller->AddDevice(device2));
    EXPECT_FALSE(controller->AddDevice(device3));
    EXPECT_TRUE(controller->GetLunCount() > 0);
    EXPECT_TRUE(controller->HasDeviceForLun(LUN));
    EXPECT_FALSE(controller->HasDeviceForLun(0));
    EXPECT_NE(nullptr, controller->GetDeviceForLun(LUN));
    EXPECT_EQ(nullptr, controller->GetDeviceForLun(0));
    EXPECT_TRUE(controller->RemoveDevice(*device1));
    EXPECT_EQ(0, controller->GetLunCount());
    EXPECT_FALSE(controller->RemoveDevice(*device1));
}

TEST(AbstractControllerTest, GetOpcode)
{
    MockAbstractController controller;

    controller.SetCdbByte(0, static_cast<int>(scsi_command::cmd_inquiry));
    EXPECT_EQ(scsi_command::cmd_inquiry, controller.GetOpcode());
}

TEST(AbstractControllerTest, GetLun)
{
    const int LUN = 3;

    MockAbstractController controller;

    controller.SetCdbByte(1, LUN << 5);
    EXPECT_EQ(LUN, controller.GetLun());
}

TEST(AbstractControllerTest, Blocks)
{
    MockAbstractController controller;

    controller.SetBlocks(1);
    EXPECT_TRUE(controller.InTransfer());
    controller.DecrementBlocks();
    EXPECT_FALSE(controller.InTransfer());
}

TEST(AbstractControllerTest, Length)
{
    MockAbstractController controller;

    EXPECT_FALSE(controller.HasValidLength());

    controller.SetLength(1);
    EXPECT_EQ(1U, controller.GetLength());
    EXPECT_TRUE(controller.HasValidLength());
}

TEST(AbstractControllerTest, UpdateOffsetAndLength)
{
    MockAbstractController controller;

    EXPECT_FALSE(controller.HasValidLength());

    controller.UpdateOffsetAndLength();
    EXPECT_EQ(0U, controller.GetLength());
}

TEST(AbstractControllerTest, Offset)
{
    MockAbstractController controller;

    controller.ResetOffset();
    EXPECT_EQ(0, controller.GetOffset());

    controller.UpdateOffsetAndLength();
    EXPECT_EQ(0, controller.GetOffset());
}

TEST(AbstractControllerTest, ProcessOnController)
{
    auto bus = make_shared<MockBus>();
    auto controller = make_shared<MockAbstractController>(bus, 1);

    EXPECT_CALL(*controller, Process(-1));
    controller->ProcessOnController(0x02);
}
