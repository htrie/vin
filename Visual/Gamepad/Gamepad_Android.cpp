#ifdef __INTELLISENSE__
#	include "Gamepad_Android.h"
#endif

namespace Utility
{
	bool Gamepad::IsValid() const 
	{ 
		//TODO
		return false;
	}

	uint32_t Gamepad::GetButtons() const 
	{ 
		//TODO
		return 0;
	}

	const std::array<uint32_t, Loaders::GamepadButtonValues::NumGamepadButtonValues>& Gamepad::GetLogicalToDeviceMask() const
	{
		static constexpr std::array<uint32_t, Loaders::GamepadButtonValues::NumGamepadButtonValues> button_masks{};
		return button_masks;
	}

	float Gamepad::GetLeftTrigger() const 
	{ 
		//TODO
		return 0;
	}

	float Gamepad::GetRightTrigger() const 
	{
		//TODO
		return 0;
	}

	Vector2d Gamepad::GetNormalisedLeftThumbstick( float deadzone_multiplier ) const
	{ 
		//TODO
		return {};
	}

	Vector2d Gamepad::GetNormalisedRightThumbstick( float deadzone_multiplier ) const
	{ 
		//TODO
		return {};
	}

	void Gamepad::Update()
	{
		//TODO
	}
}