// Copyright (c) - Graphical Playground. All rights reserved.
// For more information, see https://graphical-playground/legal
// mailto:support AT graphical-playground DOT com

#include "input/InputTypes.hpp"
#include "input/InputEnums.hpp"   // IWYU pragma: keep
#include <gtest/gtest.h>
#include <type_traits>            // IWYU pragma: keep

namespace gp::hal::input::tests
{

static_assert(kInvalidInputDeviceId == 0u, "kInvalidInputDeviceId must be 0.");
static_assert(kMaxGamepads == 8u, "Max gamepads must be 8.");
static_assert(kMaxTouchContacts == 16u, "Max touch contacts must be 16.");

constexpr bool testConstexprStick2D()
{
    Stick2D s1;
    if (s1.x != 0.0f || s1.y != 0.0f)
    {
        return false;
    }

    Stick2D s2(3.0f, 4.0f);
    if (s2.lengthSquared() != 25.0f)
    {
        return false;
    }

    Stick2D s3(3.0f, 4.0f);
    if (s2 != s3)
    {
        return false;
    }

    return true;
}

static_assert(testConstexprStick2D(), "Stick2D constexpr tests failed.");

TEST(Stick2DTest, ConstructorsAndLengthSquared)
{
    const Stick2D defaultStick;
    EXPECT_FLOAT_EQ(defaultStick.x, 0.0f);
    EXPECT_FLOAT_EQ(defaultStick.y, 0.0f);
    EXPECT_FLOAT_EQ(defaultStick.lengthSquared(), 0.0f);

    const Stick2D explicitStick(0.6f, 0.8f);
    EXPECT_FLOAT_EQ(explicitStick.x, 0.6f);
    EXPECT_FLOAT_EQ(explicitStick.y, 0.8f);
    EXPECT_NEAR(explicitStick.lengthSquared(), 1.0f, 1e-5f);

    EXPECT_TRUE(explicitStick == Stick2D(0.6f, 0.8f));
    EXPECT_FALSE(explicitStick == defaultStick);
}

TEST(Vector3fInputTest, Constructors)
{
    const Vector3fInput defaultVec;
    EXPECT_FLOAT_EQ(defaultVec.x, 0.0f);
    EXPECT_FLOAT_EQ(defaultVec.y, 0.0f);
    EXPECT_FLOAT_EQ(defaultVec.z, 0.0f);

    const Vector3fInput explicitVec(1.0f, -2.5f, 9.81f);
    EXPECT_FLOAT_EQ(explicitVec.x, 1.0f);
    EXPECT_FLOAT_EQ(explicitVec.y, -2.5f);
    EXPECT_FLOAT_EQ(explicitVec.z, 9.81f);
}

TEST(AnalogDeadzoneConfigTest, DefaultsAndExplicitValues)
{
    const AnalogDeadzoneConfig defaultConfig;
    EXPECT_FLOAT_EQ(defaultConfig.innerDeadzone, 0.12f);
    EXPECT_FLOAT_EQ(defaultConfig.outerDeadzone, 0.95f);
    EXPECT_EQ(defaultConfig.mode, DeadzoneMode::ScaledRadial);

    const AnalogDeadzoneConfig axialConfig(0.05f, 0.99f, DeadzoneMode::Axial);
    EXPECT_FLOAT_EQ(axialConfig.innerDeadzone, 0.05f);
    EXPECT_FLOAT_EQ(axialConfig.outerDeadzone, 0.99f);
    EXPECT_EQ(axialConfig.mode, DeadzoneMode::Axial);
}

TEST(ButtonStateFlagsTest, BitwiseOperations)
{
    ButtonStateFlags flags = ButtonStateFlags::None;
    EXPECT_EQ(flags, ButtonStateFlags::None);

    flags |= ButtonStateFlags::Held;
    EXPECT_EQ(flags & ButtonStateFlags::Held, ButtonStateFlags::Held);

    flags |= ButtonStateFlags::PressedThisFrame;
    EXPECT_EQ(flags & ButtonStateFlags::PressedThisFrame, ButtonStateFlags::PressedThisFrame);

    flags &= ~ButtonStateFlags::Held;
    EXPECT_EQ(flags & ButtonStateFlags::Held, ButtonStateFlags::None);
    EXPECT_EQ(flags & ButtonStateFlags::PressedThisFrame, ButtonStateFlags::PressedThisFrame);
}

TEST(InputEnumsTest, GamepadSlots)
{
    EXPECT_EQ(static_cast<gp::UInt8>(GamepadSlot::Player1), 0u);
    EXPECT_EQ(static_cast<gp::UInt8>(GamepadSlot::Player8), 7u);
    EXPECT_EQ(static_cast<gp::UInt8>(GamepadSlot::Count), 8u);
    EXPECT_EQ(static_cast<gp::UInt8>(GamepadSlot::Any), 0xFFu);
}

TEST(InputEnumsTest, DeadzoneModes)
{
    EXPECT_EQ(static_cast<gp::UInt8>(DeadzoneMode::None), 0u);
    EXPECT_EQ(static_cast<gp::UInt8>(DeadzoneMode::Axial), 1u);
    EXPECT_EQ(static_cast<gp::UInt8>(DeadzoneMode::ScaledRadial), 2u);
}

}   // namespace gp::hal::input::tests
