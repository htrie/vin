#pragma once

#include <limits>
#include "Common/Utility/Geometric.h"
#include "Common/Loaders/CEGamepadButtonEnum.h"
#include "Common/Loaders/CEGamepadThumbstickEnum.h"
#include "Common/Loaders/CEGamepadTypeEnum.h"

namespace Utility
{
	struct GamepadDeviceState;

	/// <summary>
	/// Handle for a gamepad device from GamepadManager
	/// </summary>
	class Gamepad
	{
	public:
		Gamepad() noexcept = default;
		Gamepad( const std::size_t device_idx )
			: device_idx{ device_idx }
		{
		}

		bool IsValid() const;
		auto GetDeviceIdx() const noexcept { return device_idx; }

		Loaders::GamepadTypeValues::GamepadType GetGamepadType() const;

		/// device digital buttons, you'll most likely need to mask with values from GetLogicalToDeviceMask() in order to be useful
		uint32_t GetButtons() const;
		const std::array<uint32_t, Loaders::GamepadButtonValues::NumGamepadButtonValues>& GetLogicalToDeviceMask() const;

		float GetLeftTrigger() const;
		float GetRightTrigger() const;

		Vector2d GetNormalisedThumbstick( Loaders::GamepadThumbstickValues::GamepadThumbstick thumbstick, float deadzone_multiplier ) const;
		inline auto GetNormalisedLeftThumbstick( float deadzone_multiplier ) const { return GetNormalisedThumbstick( Loaders::GamepadThumbstickValues::Left, deadzone_multiplier ); }
		inline auto GetNormalisedRightThumbstick( float deadzone_multiplier ) const { return GetNormalisedThumbstick( Loaders::GamepadThumbstickValues::Right, deadzone_multiplier ); }

	private:
		const GamepadDeviceState* GetDeviceState() const;

	private:
		std::size_t device_idx{ std::numeric_limits<std::size_t>::max() };
	};
}
