#ifdef __INTELLISENSE__
#include "Gamepad_XBox.h"
#endif

namespace Utility
{
	Windows::Xbox::Input::IGamepad^ Gamepad::GetXboxGamepad() const
	{
		return IsValid() ? gamepad : nullptr;
	}

	bool Gamepad::IsValid() const
	{
		return gamepad_reading != nullptr;
	}

	Windows::Xbox::Input::GamepadButtons Gamepad::GetButtons() const
	{
		return IsValid() ? gamepad_reading->Buttons : Windows::Xbox::Input::GamepadButtons::None;
	}

	const std::array<uint32_t, Loaders::GamepadButtonValues::NumGamepadButtonValues>& Gamepad::GetLogicalToDeviceMask() const
	{
		// Mapping from Utility::GamepadState::Button enumeration to Windows::Xbox::Input::GamepadButtons.
		static constexpr std::array<uint32_t, Loaders::GamepadButtonValues::NumGamepadButtonValues> button_masks
		{
			static_cast<uint32_t>(Windows::Xbox::Input::GamepadButtons::DPadUp),  // DpadUp
			static_cast<uint32_t>(Windows::Xbox::Input::GamepadButtons::DPadDown),  // DpadDown
			static_cast<uint32_t>(Windows::Xbox::Input::GamepadButtons::DPadLeft),  // DPadLeft
			static_cast<uint32_t>(Windows::Xbox::Input::GamepadButtons::DPadRight),  // DPadRight
			static_cast<uint32_t>(Windows::Xbox::Input::GamepadButtons::Menu),  // Menu
			static_cast<uint32_t>(Windows::Xbox::Input::GamepadButtons::View),  // View
			static_cast<uint32_t>(Windows::Xbox::Input::GamepadButtons::LeftThumbstick),  // LeftThumb
			static_cast<uint32_t>(Windows::Xbox::Input::GamepadButtons::RightThumbstick),  // RightThumb
			static_cast<uint32_t>(Windows::Xbox::Input::GamepadButtons::LeftShoulder),  // LeftShoulder
			static_cast<uint32_t>(Windows::Xbox::Input::GamepadButtons::RightShoulder),  // RightShoulder
			static_cast<uint32_t>(Windows::Xbox::Input::GamepadButtons::A),  // A
			static_cast<uint32_t>(Windows::Xbox::Input::GamepadButtons::B),  // B
			static_cast<uint32_t>(Windows::Xbox::Input::GamepadButtons::X),  // X
			static_cast<uint32_t>(Windows::Xbox::Input::GamepadButtons::Y),  // Y
			0, 0,  // LeftTrigger & RightTrigger are special case and are handled in a different way
			0, // L4
			0, // R4
			0, // L5
			0, // R5
			0, // Share
		};
		return button_masks;
	}

	float Gamepad::GetLeftTrigger() const
	{
		return IsValid() ? gamepad_reading->LeftTrigger : 0.0f;
	}

	float Gamepad::GetRightTrigger() const
	{
		return IsValid() ? gamepad_reading->RightTrigger : 0.0f;
	}

	Vector2d Gamepad::GetNormalisedLeftThumbstick( float deadzone_multiplier ) const
	{
		return IsValid() ? Vector2d(NormaliseThumbstickValue(gamepad_reading->LeftThumbstickX), NormaliseThumbstickValue(gamepad_reading->LeftThumbstickY)) : Vector2d(0.0f, 0.0f);
	}

	Vector2d Gamepad::GetNormalisedRightThumbstick( float deadzone_multiplier ) const
	{
		return IsValid() ? Vector2d(NormaliseThumbstickValue(gamepad_reading->RightThumbstickX), NormaliseThumbstickValue(gamepad_reading->RightThumbstickY)) : Vector2d(0.0f, 0.0f);
	}

	bool Gamepad::WasAPressed() const
	{
		return (static_cast<unsigned int>(pressed_buttons) & static_cast<unsigned int>(Windows::Xbox::Input::GamepadButtons::A)) != 0;
	}

	void Gamepad::Update(Windows::Xbox::System::User^ user, Windows::Foundation::Collections::IVectorView< Windows::Xbox::Input::IGamepad^ >^ cached_gamepads, Platform::Collections::Vector< Windows::Xbox::System::User^ >^ cached_gamepad_users, bool signed_in)
	{
		Windows::Xbox::Input::IGamepad^ new_gamepad = nullptr;
		Windows::Xbox::Input::IGamepadReading^ new_gamepad_reading = nullptr;

		{
			Windows::Xbox::Input::IGamepad^ most_recent_gamepad = nullptr;
			Windows::Xbox::Input::IGamepadReading^ most_recent_gamepad_reading = nullptr;
			long long most_recent_time = 0;

			const unsigned gamepad_count = cached_gamepads->Size;
			for (unsigned i = 0; i < gamepad_count; ++i)
			{
				Windows::Xbox::Input::IGamepad^ next_gamepad = cached_gamepads->GetAt(i);
				Windows::Xbox::System::User^ gamepad_user = cached_gamepad_users->GetAt(i);

				Windows::Xbox::Input::IGamepadReading^ gamepad_reading = next_gamepad->GetCurrentReading(); // TODO: Use GetRawGamepadReading/Input.

				const auto gamepad_time = gamepad_reading->Timestamp.UniversalTime;

				if (most_recent_gamepad == nullptr || gamepad_time > most_recent_time)
				{
					if (user == nullptr || (gamepad_user != nullptr && gamepad_user->Id == user->Id && signed_in))
					{
						most_recent_gamepad = next_gamepad;
						most_recent_gamepad_reading = gamepad_reading;
						most_recent_time = gamepad_time;
					}
				}
			}

			new_gamepad = most_recent_gamepad;
			new_gamepad_reading = most_recent_gamepad_reading;
		}

		gamepad = new_gamepad;

		if (gamepad_reading == nullptr)
		{
			last_gamepad_buttons = Windows::Xbox::Input::GamepadButtons::None;
			pressed_buttons = 0;
		}
		else
		{
			const auto buttons = GetButtons();

			pressed_buttons = (static_cast<uint32_t>(last_gamepad_buttons) ^ static_cast<uint32_t>(buttons)) & static_cast<uint32_t>(buttons);
			last_gamepad_buttons = gamepad_reading->Buttons;
		}

		gamepad_reading = new_gamepad_reading;
	}

	float Gamepad::NormaliseThumbstickValue(const float value)
	{
		const float dead_zone = 0.24f;

		if (value > +dead_zone)
			return (value - dead_zone) / (1.0f - dead_zone);

		if (value < -dead_zone)
			return (value + dead_zone) / (1.0f - dead_zone);

		return 0.0f;
	}
}