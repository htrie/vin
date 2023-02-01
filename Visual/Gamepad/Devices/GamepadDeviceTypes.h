#pragma once

namespace Utility
{
	// Hardware types, for display purposes we have a separate enum (see Loaders::GamepadTypeValues)
	enum struct GamepadDeviceType
	{
		Generic,
		XInputDevice,
		Dualshock4,
		NumGamepadDeviceTypes,
	};
}
