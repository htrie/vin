#pragma once

#include "Common/Loaders/CEGamepadButtonEnum.h"
#include "Common/Loaders/CEGamepadTypeEnum.h"
#include "Common/Utility/Geometric.h"

namespace Utility
{
	class Gamepad
	{
	public:
		bool IsValid() const;
		Loaders::GamepadTypeValues::GamepadType GetGamepadType() const { return Loaders::GamepadTypeValues::Generic; }

		uint32_t GetButtons() const;
		const std::array<uint32_t, Loaders::GamepadButtonValues::NumGamepadButtonValues>& GetLogicalToDeviceMask() const;

		float GetLeftTrigger() const;
		float GetRightTrigger() const;
		Vector2d GetNormalisedLeftThumbstick( float deadzone_multiplier ) const;
		Vector2d GetNormalisedRightThumbstick( float deadzone_multiplier ) const;

		void Update();
	};
}