#pragma once

#if defined(_WIN32) && !defined(_XBOX_ONE)
#include <string_view>

#include "Common/Loaders/CEGamepadButtonEnum.h"
#include "Common/Loaders/CEGamepadThumbstickEnum.h"
#include "Common/Utility/Geometric.h"

#include "Visual/Device/Constants.h"
#include "GamepadDeviceState.h"

typedef struct _XINPUT_STATE XINPUT_STATE;

namespace Utility::GamepadDevice
{
	constexpr std::size_t MaxNumXInputControllers{ 4 };

	namespace XInputDigitalInputValues
	{
		enum XInputDigitalInputs : int
		{
			A,
			B,
			X,
			Y,
			DPadLeft,
			DPadRight,
			DPadUp,
			DPadDown,
			LeftBumper,
			RightBumper,
			LeftStickButton,
			RightStickButton,
			Back,
			Start,
			NumXInputDigitalInputs,
		};
		static_assert(NumXInputDigitalInputs < GamepadDeviceState::MaxDigitalInputsCount);
	}

	namespace XInputAnalogInputValues
	{
		enum XInputAnalogInputs : int
		{
			LeftTrigger,
			RightTrigger,
			LeftStick_Left,
			LeftStick_Right,
			LeftStick_Up,
			LeftStick_Down,
			RightStick_Left,
			RightStick_Right,
			RightStick_Up,
			RightStick_Down,
			NumXInputAnalogInputs,
		};
		static_assert(NumXInputAnalogInputs < GamepadDeviceState::MaxAnalogInputsCount);
	}

	static constexpr std::array<uint32_t, Loaders::GamepadButtonValues::NumGamepadButtonValues> XInputLogicalToDigitalButtonMasks
	{
		1 << XInputDigitalInputValues::DPadUp,    // DPadUp
		1 << XInputDigitalInputValues::DPadDown,  // DPadDown
		1 << XInputDigitalInputValues::DPadLeft,  // DPadLeft
		1 << XInputDigitalInputValues::DPadRight, // DPadRight
		1 << XInputDigitalInputValues::Start, // Start
		1 << XInputDigitalInputValues::Back, // Back
		1 << XInputDigitalInputValues::LeftStickButton, // LeftThumb
		1 << XInputDigitalInputValues::RightStickButton, // RightThumb
		1 << XInputDigitalInputValues::LeftBumper, // LeftShoulder
		1 << XInputDigitalInputValues::RightBumper, // RightShoulder
		1 << XInputDigitalInputValues::A, // A
		1 << XInputDigitalInputValues::B, // B
		1 << XInputDigitalInputValues::X, // X
		1 << XInputDigitalInputValues::Y, // Y
		0, // LeftTrigger
		0, // RightTrigger
		0, // L4
		0, // R4
		0, // L5
		0, // R5
		0, // Share
	};

	bool IsXboxController( std::wstring_view device_name );
	void UpdateXInputDevice( GamepadDeviceState& state, const XINPUT_STATE& xinput_state );

	Vector2d GetXInputNormalisedThumbstickValue( const GamepadDeviceState& state, Loaders::GamepadThumbstickValues::GamepadThumbstick thumbstick, float deadzone_multiplier );
}
#endif
