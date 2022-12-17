#pragma once

#include <memory>
#include <collection.h>

#include "Common/Loaders/CEGamepadButtonEnum.h"
#include "Common/Loaders/CEGamepadTypeEnum.h"
#include "Common/Utility/Geometric.h"

namespace Utility
{
	class Gamepad
	{
	public:

		Windows::Xbox::Input::IGamepad^ GetXboxGamepad() const;

		bool IsValid() const;
		Loaders::GamepadTypeValues::GamepadType GetGamepadType() const { return Loaders::GamepadTypeValues::XboxController; }

		Windows::Xbox::Input::GamepadButtons GetButtons() const;
		const std::array<uint32_t, Loaders::GamepadButtonValues::NumGamepadButtonValues>& GetLogicalToDeviceMask() const;
		float GetLeftTrigger() const;
		float GetRightTrigger() const;
		Vector2d GetNormalisedLeftThumbstick( float deadzone_multiplier ) const;
		Vector2d GetNormalisedRightThumbstick( float deadzone_multiplier ) const;

		bool WasAPressed() const;

		void Update(Windows::Xbox::System::User^ user, Windows::Foundation::Collections::IVectorView< Windows::Xbox::Input::IGamepad^ >^ cached_gamepads, Platform::Collections::Vector< Windows::Xbox::System::User^ >^ cached_gamepad_users, bool signed_in);

	private:

		static float NormaliseThumbstickValue(const float value);

		Windows::Xbox::Input::IGamepad^        gamepad = nullptr;
		Windows::Xbox::Input::IGamepadReading^ gamepad_reading = nullptr;
		Windows::Xbox::Input::GamepadButtons   last_gamepad_buttons = Windows::Xbox::Input::GamepadButtons::None;
		uint32_t                               pressed_buttons = 0;
	};
}