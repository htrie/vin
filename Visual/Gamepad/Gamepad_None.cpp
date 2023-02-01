#ifdef __INTELLISENSE__
#include "Gamepad_None.h"
#endif

namespace Utility
{
	bool Gamepad::IsValid() const { return false; }
	uint32_t Gamepad::GetButtons() const { return 0; }
	const std::array<uint32_t, Loaders::GamepadButtonValues::NumGamepadButtonValues>& Gamepad::GetLogicalToDeviceMask() const
	{
		static constexpr std::array<uint32_t, Loaders::GamepadButtonValues::NumGamepadButtonValues> button_masks{};
		return button_masks;
	}
	float Gamepad::GetLeftTrigger() const { return 0; }
	float Gamepad::GetRightTrigger() const { return 0; }
	Vector2d Gamepad::GetNormalisedLeftThumbstick( float deadzone_multiplier ) const { return {}; }
	Vector2d Gamepad::GetNormalisedRightThumbstick( float deadzone_multiplier ) const { return {}; }
}