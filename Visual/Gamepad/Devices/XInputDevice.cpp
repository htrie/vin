#include "XInputDevice.h"

#if defined(_WIN32) && !defined(_XBOX_ONE)

#include <Xinput.h>
#pragma comment( lib, "Xinput.lib" )

#include "Common/Utility/ContainerOperations.h"
#include "Common/Utility/WindowsHeaders.h"

namespace Utility::GamepadDevice
{
    bool IsXboxController( const std::wstring_view device_name )
    {
		return device_name.find( L"IG_" ) != std::string_view::npos;
    }

    void UpdateXInputDevice( GamepadDeviceState& state, const XINPUT_STATE& xinput_state )
    {
		assert( state.type == GamepadDeviceType::XInputDevice );

		using Digital = XInputDigitalInputValues::XInputDigitalInputs;
		state.digital_inputs.set(Digital::A,                (xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_A) );
		state.digital_inputs.set(Digital::B,                (xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_B) );
		state.digital_inputs.set(Digital::X,                (xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_X) );
		state.digital_inputs.set(Digital::Y,                (xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) );
		state.digital_inputs.set(Digital::DPadLeft,         (xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) );
		state.digital_inputs.set(Digital::DPadRight,        (xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) );
		state.digital_inputs.set(Digital::DPadUp,           (xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) );
		state.digital_inputs.set(Digital::DPadDown,         (xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) );
		state.digital_inputs.set(Digital::LeftBumper,       (xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) );
		state.digital_inputs.set(Digital::RightBumper,      (xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) );
		state.digital_inputs.set(Digital::LeftStickButton,  (xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) );
		state.digital_inputs.set(Digital::RightStickButton, (xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) );
		state.digital_inputs.set(Digital::Back,             (xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) );
		state.digital_inputs.set(Digital::Start,            (xinput_state.Gamepad.wButtons & XINPUT_GAMEPAD_START) );

		using Analog = XInputAnalogInputValues::XInputAnalogInputs;
		state.analog_inputs[Analog::LeftTrigger]      = xinput_state.Gamepad.bLeftTrigger / 255.0f;
		state.analog_inputs[Analog::RightTrigger]     = xinput_state.Gamepad.bRightTrigger / 255.0f;
		state.analog_inputs[Analog::LeftStick_Left]    = -xinput_state.Gamepad.sThumbLX / 32767.0f;
		state.analog_inputs[Analog::LeftStick_Right]   = xinput_state.Gamepad.sThumbLX / 32767.0f;
		state.analog_inputs[Analog::LeftStick_Up]      = xinput_state.Gamepad.sThumbLY / 32767.0f;
		state.analog_inputs[Analog::LeftStick_Down]    = -xinput_state.Gamepad.sThumbLY / 32767.0f;
		state.analog_inputs[Analog::RightStick_Left]   = -xinput_state.Gamepad.sThumbRX / 32767.0f;
		state.analog_inputs[Analog::RightStick_Right]  = xinput_state.Gamepad.sThumbRX / 32767.0f;
		state.analog_inputs[Analog::RightStick_Up]     = xinput_state.Gamepad.sThumbRY / 32767.0f;
		state.analog_inputs[Analog::RightStick_Down]   = -xinput_state.Gamepad.sThumbRY / 32767.0f;
    }

	Vector2d GetXInputNormalisedThumbstickValue( const GamepadDeviceState& state, Loaders::GamepadThumbstickValues::GamepadThumbstick thumbstick, float deadzone_multiplier )
	{
		static constexpr auto normalised_thumbstick = []( const float x, const float y, const float deadzone ) -> Vector2d
		{
			const Vector2d ret{ x, y };
			if( ret.length() < deadzone )
				return Vector2d{ 0.0f, 0.0f };

			return ret;
		};

		const auto& inputs = state.analog_inputs;
		using Thumbstick = Loaders::GamepadThumbstickValues::GamepadThumbstick;
		using Analog = XInputAnalogInputValues::XInputAnalogInputs;
		switch( thumbstick )
		{
		case Thumbstick::Left:   return normalised_thumbstick( inputs[Analog::LeftStick_Right] - inputs[Analog::LeftStick_Left], inputs[Analog::LeftStick_Up] - inputs[Analog::LeftStick_Down], (XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE / 32767.0f) * deadzone_multiplier );
		case Thumbstick::Right:  return normalised_thumbstick( inputs[Analog::RightStick_Right] - inputs[Analog::RightStick_Left], inputs[Analog::RightStick_Up] - inputs[Analog::RightStick_Down], (XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE / 32767.0f) * deadzone_multiplier );

		default:
			return Vector2d{};
		}
	}
}

#endif
