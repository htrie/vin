#pragma once

#include "Common/Loaders/CEGamepadButtonEnum.h"
#include "Common/Loaders/CEGamepadThumbstickEnum.h"
#include "Common/Utility/Geometric.h"
#include "Visual/Device/Constants.h"

#include "GamepadDeviceState.h"

typedef struct tagRID_DEVICE_INFO_HID RID_DEVICE_INFO_HID;

namespace Utility::GamepadDevice
{
	namespace DualShock4DigitalInputValues
	{
		enum DualShock4DigitalInputs : int
		{
			Square,
			Cross,
			Circle,
			Triangle,
			L1,
			R1,
			L3,
			R3,
			Options,
			Share,
			PS,
			TouchPadButton,
			DPadLeft,
			DPadRight,
			DPadUp,
			DPadDown,
			NumDualShock4DigitalInputs,
		};
		static_assert(NumDualShock4DigitalInputs < GamepadDeviceState::MaxDigitalInputsCount);
	}

	namespace DualShock4AnalogInputValues
	{
		enum DualShock4AnalogInputs : int
		{
			L2,
			R2,
			LeftStick_Left,
			LeftStick_Right,
			LeftStick_Up,
			LeftStick_Down,
			RightStick_Left,
			RightStick_Right,
			RightStick_Up,
			RightStick_Down,
			NumDualShock4AnalogInputs,
		};
		static_assert(NumDualShock4AnalogInputs < GamepadDeviceState::MaxAnalogInputsCount);
	}

	static constexpr std::array<uint32_t, Loaders::GamepadButtonValues::NumGamepadButtonValues> DualShock4LogicalToDigitalButtonMasks
	{
		1 << DualShock4DigitalInputValues::DPadUp,    // DPadUp
		1 << DualShock4DigitalInputValues::DPadDown,  // DPadDown
		1 << DualShock4DigitalInputValues::DPadLeft,  // DPadLeft
		1 << DualShock4DigitalInputValues::DPadRight, // DPadRight
		1 << DualShock4DigitalInputValues::Options, // Start
		1 << DualShock4DigitalInputValues::TouchPadButton, // Back
		1 << DualShock4DigitalInputValues::L3, // LeftThumb
		1 << DualShock4DigitalInputValues::R3, // RightThumb
		1 << DualShock4DigitalInputValues::L1, // LeftShoulder
		1 << DualShock4DigitalInputValues::R1, // RightShoulder
		1 << DualShock4DigitalInputValues::Cross, // A
		1 << DualShock4DigitalInputValues::Circle, // B
		1 << DualShock4DigitalInputValues::Square, // X
		1 << DualShock4DigitalInputValues::Triangle, // Y
		0, // LeftTrigger
		0, // RightTrigger
		0, // L4
		0, // R4
		0, // L5
		0, // R5
		1 << DualShock4DigitalInputValues::Share, // Share
	};

#if defined(_WIN32) && !defined(_XBOX_ONE)
	bool IsDualshock4Controller( const RID_DEVICE_INFO_HID& info );
#endif
	void UpdateDualshock4( GamepadDeviceState& state, const BYTE raw_data[], const DWORD byte_count );

	Vector2d GetDualshock4NormalisedThumbstickValue( const GamepadDeviceState& state, Loaders::GamepadThumbstickValues::GamepadThumbstick thumbstick, float deadzone_multiplier );
}
