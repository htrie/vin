#pragma once

namespace Utility
{
	class GamepadManager;

	class GamepadManagerListener
	{
	public:
		virtual ~GamepadManagerListener() = default;

		virtual void OnGamepadDeviceConnected( GamepadManager& manager, std::size_t device_id ) {}
		virtual void OnGamepadDeviceDisconnected( GamepadManager& manager, std::size_t device_id ) {}
		virtual void OnGamepadDeviceTypeChanged( GamepadManager& manager, std::size_t device_id ) {}
	};
}
