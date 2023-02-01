
#include "Dualshock4Device.h"
#include "Common/Utility/ContainerOperations.h"
#include "Common/Utility/WindowsHeaders.h"

namespace Utility::GamepadDevice
{
#if defined(_WIN32) && !defined(_XBOX_ONE)
	bool IsDualshock4Controller( const RID_DEVICE_INFO_HID& info )
	{
		static constexpr DWORD SonyVendorId{ 0x054C };
		static constexpr std::array<DWORD, 2> Ds4ProductIds{
			0x05C4, // gen 1
			0x09CC, // gen 2
		};

		return info.dwVendorId == SonyVendorId
			&& Container::Contains( Ds4ProductIds, info.dwProductId );
	}
#endif

	void UpdateDualshock4( GamepadDeviceState& state, const BYTE raw_data[], const DWORD byte_count )
	{
		// Documentation on the data format:
		// Note that the format is different when connected via bluetooth
		// https://www.psdevwiki.com/ps4/DS4-USB#Structure_HID_transaction_.28portion.29
		// https://www.psdevwiki.com/ps4/DS4-BT#Structure_HID_transaction_.28portion.29

		assert( state.type == GamepadDeviceType::Dualshock4 );

		static constexpr DWORD InputByteCountUsb{ 64 };
		static constexpr DWORD InputByteCountBluetooth{ 547 };

		unsigned int offset{ 0U };

		if( byte_count == InputByteCountUsb )
		{
			const bool is_valid = (raw_data[0] == 0x01);
			if( !is_valid )
				return;
		}
		else if( byte_count == InputByteCountBluetooth )
		{
			if( raw_data[0] == 0x01 )
			{
				offset = 0; // using the same format as usb
			}
			else if( raw_data[0] == 0x11 )
			{
				offset = 2; // using the bluetooth format
			}
			else
			{
				// unsupported format
				return;
			}
		}
		else
		{
			// unsupported byte count
			return;
		}

		// input
		{
			const unsigned char left_stick_x   = raw_data[offset + 1];
			const unsigned char left_stick_y   = raw_data[offset + 2];
			const unsigned char right_stick_x  = raw_data[offset + 3];
			const unsigned char right_stick_y  = raw_data[offset + 4];
			const unsigned char left_trigger   = raw_data[offset + 8];
			const unsigned char right_trigger  = raw_data[offset + 9];
			const unsigned char dpad           = 0b1111 & raw_data[offset + 5];

			using Digital = DualShock4DigitalInputValues::DualShock4DigitalInputs;
			state.digital_inputs.set(Digital::Square,           1 & (raw_data[offset + 5] >> 4) );
			state.digital_inputs.set(Digital::Cross,            1 & (raw_data[offset + 5] >> 5) );
			state.digital_inputs.set(Digital::Circle,           1 & (raw_data[offset + 5] >> 6) );
			state.digital_inputs.set(Digital::Triangle,         1 & (raw_data[offset + 5] >> 7) );
			state.digital_inputs.set(Digital::L1,               1 & (raw_data[offset + 6] >> 0) );
			state.digital_inputs.set(Digital::R1,               1 & (raw_data[offset + 6] >> 1) );
			state.digital_inputs.set(Digital::Share,            1 & (raw_data[offset + 6] >> 4) );
			state.digital_inputs.set(Digital::Options,          1 & (raw_data[offset + 6] >> 5) );
			state.digital_inputs.set(Digital::L3,               1 & (raw_data[offset + 6] >> 6) );
			state.digital_inputs.set(Digital::R3,               1 & (raw_data[offset + 6] >> 7) );
			state.digital_inputs.set(Digital::PS,               1 & (raw_data[offset + 7] >> 0) );
			state.digital_inputs.set(Digital::TouchPadButton,   1 & (raw_data[offset + 7] >> 1) );
			state.digital_inputs.set(Digital::DPadUp,           (dpad == 0 || dpad == 1 || dpad == 7));
			state.digital_inputs.set(Digital::DPadRight,        (dpad == 1 || dpad == 2 || dpad == 3));
			state.digital_inputs.set(Digital::DPadDown,         (dpad == 3 || dpad == 4 || dpad == 5));
			state.digital_inputs.set(Digital::DPadLeft,         (dpad == 5 || dpad == 6 || dpad == 7));

			using Analog = DualShock4AnalogInputValues::DualShock4AnalogInputs;
			state.analog_inputs[Analog::LeftStick_Left] = std::max(-(left_stick_x / 255.0f * 2 - 1), 0.f);
			state.analog_inputs[Analog::LeftStick_Right] = std::max(left_stick_x / 255.0f * 2 - 1, 0.f);
			state.analog_inputs[Analog::LeftStick_Up] = std::max(-(left_stick_y / 255.0f * 2 - 1), 0.f );
			state.analog_inputs[Analog::LeftStick_Down] = std::max(left_stick_y / 255.0f * 2 - 1, 0.f );
			state.analog_inputs[Analog::RightStick_Left] = std::max(-(right_stick_x / 255.0f * 2 - 1), 0.f );
			state.analog_inputs[Analog::RightStick_Right] = std::max(right_stick_x / 255.0f * 2 - 1, 0.f );
			state.analog_inputs[Analog::RightStick_Up] = std::max(-(right_stick_y / 255.0f * 2 - 1), 0.f );
			state.analog_inputs[Analog::RightStick_Down] = std::max(right_stick_y / 255.0f * 2 - 1, 0.f);
			state.analog_inputs[Analog::L2] = left_trigger / 255.0f;
			state.analog_inputs[Analog::R2] = right_trigger / 255.0f;

			// Note: It's possible to read battery level and the dual touchpad inputs here as well
			// but since we currently don't have a use for them I've left them out
		}

		// output
		{
#if defined(_WIN32) && !defined(_XBOX_ONE)
			state.hid_info.CheckOverlapped();
			if( const auto file = state.hid_info.GetOutputFile(); file != INVALID_HANDLE_VALUE )
			{
				// TODO: if we ever want to support rumble, setting the light LED colour we'd send the output data here using WriteFile()
			}
#endif
		}
	}

	Vector2d GetDualshock4NormalisedThumbstickValue( const GamepadDeviceState& state, Loaders::GamepadThumbstickValues::GamepadThumbstick thumbstick, float deadzone_multiplier )
	{
		static constexpr auto normalised_thumbstick = []( const float x, const float y, const float deadzone ) -> Vector2d
		{
			const Vector2d ret{ x, y };
			if( ret.length() < deadzone )
				return Vector2d{ 0.0f, 0.0f };

			return ret;
		};

		static constexpr float deadzone{ 0.2f };

		const auto& inputs = state.analog_inputs;
		using Thumbstick = Loaders::GamepadThumbstickValues::GamepadThumbstick;
		using Analog = DualShock4AnalogInputValues::DualShock4AnalogInputs;
		switch( thumbstick )
		{
		case Thumbstick::Left:   return normalised_thumbstick( inputs[Analog::LeftStick_Right] - inputs[Analog::LeftStick_Left], inputs[Analog::LeftStick_Up] - inputs[Analog::LeftStick_Down], deadzone * deadzone_multiplier );
		case Thumbstick::Right:  return normalised_thumbstick( inputs[Analog::RightStick_Right] - inputs[Analog::RightStick_Left], inputs[Analog::RightStick_Up] - inputs[Analog::RightStick_Down], deadzone * deadzone_multiplier );

		default:
			return Vector2d{};
		}
	}
}
