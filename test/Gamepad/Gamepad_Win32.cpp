#ifdef __INTELLISENSE__
#include "Gamepad_Win32.h"
#endif

#include "Common/Loaders/CEGamepadType.h"
#include "Visual/Device/WindowDX.h"

#include "GamepadManager.h"
#include "Devices/XInputDevice.h"
#include "Devices/Dualshock4Device.h"

namespace Utility
{
	bool Gamepad::IsValid() const
	{
		const auto* const device_state = GetDeviceState();
		if( device_state == nullptr )
			return false;

#ifndef UI_IGNORE_LOSING_WINDOW_FOCUS
		const auto window = Device::WindowDX::GetWindow();
		if( window == nullptr || !window->IsForeground() )
			return false;
#endif

		return device_state->is_connected;
	}

	uint32_t Gamepad::GetButtons() const
	{
		return IsValid() ? GetDeviceState()->digital_inputs.extract<uint32_t>() : 0;
	}

	const std::array<uint32_t, Loaders::GamepadButtonValues::NumGamepadButtonValues>& Gamepad::GetLogicalToDeviceMask() const
	{
		if( IsValid() )
		{
			const auto* const state = GetDeviceState();
			switch( state->type )
			{
			case GamepadDeviceType::XInputDevice:
				return GamepadDevice::XInputLogicalToDigitalButtonMasks;

			case GamepadDeviceType::Dualshock4:
				return GamepadDevice::DualShock4LogicalToDigitalButtonMasks;
			}
		}

		static constexpr std::array<uint32_t, Loaders::GamepadButtonValues::NumGamepadButtonValues> empty{};
		return empty;
	}

	float Gamepad::GetLeftTrigger() const
	{
		if( IsValid() )
		{
			const auto* const state = GetDeviceState();
			switch( state->type )
			{
			case GamepadDeviceType::XInputDevice:
				return state->analog_inputs[GamepadDevice::XInputAnalogInputValues::LeftTrigger];

			case GamepadDeviceType::Dualshock4:
				return state->analog_inputs[GamepadDevice::DualShock4AnalogInputValues::L2];
			}
		}

		return 0.f;
	}

	float Gamepad::GetRightTrigger() const
	{
		if( IsValid() )
		{
			const auto* const state = GetDeviceState();
			switch( state->type )
			{
			case GamepadDeviceType::XInputDevice:
				return state->analog_inputs[GamepadDevice::XInputAnalogInputValues::RightTrigger];

			case GamepadDeviceType::Dualshock4:
				return state->analog_inputs[GamepadDevice::DualShock4AnalogInputValues::R2];
			}
		}

		return 0.f;
	}

	Loaders::GamepadTypeValues::GamepadType Gamepad::GetGamepadType() const
	{
		if( IsValid() )
		{
			const auto* const state = GetDeviceState();
			switch( state->type )
			{
			case GamepadDeviceType::XInputDevice:
				return Loaders::GamepadTypeValues::XboxController;

			case GamepadDeviceType::Dualshock4:
				return Loaders::GamepadTypeValues::PS4Controller;

			case GamepadDeviceType::Generic:
				return Loaders::GamepadTypeValues::Generic;
			}
		}

		return Loaders::GamepadTypeValues::NumGamepadTypeValues;
	}

	Vector2d Gamepad::GetNormalisedThumbstick( Loaders::GamepadThumbstickValues::GamepadThumbstick thumbstick, float deadzone_multiplier ) const
	{
		if( IsValid() )
		{
			const auto* const state = GetDeviceState();
			switch( state->type )
			{
			case GamepadDeviceType::XInputDevice:
				return GamepadDevice::GetXInputNormalisedThumbstickValue( *state, thumbstick, deadzone_multiplier );

			case GamepadDeviceType::Dualshock4:
				return GamepadDevice::GetDualshock4NormalisedThumbstickValue( *state, thumbstick, deadzone_multiplier );
			}
		}
		return Vector2d{ 0, 0 };
	}

	const GamepadDeviceState* Gamepad::GetDeviceState() const
	{
		return GamepadManager::Get().GetDeviceState( device_idx );
	}
}
