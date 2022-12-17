#pragma once

#include <pad.h>
#include <memory>
#include <unordered_map>

#include "Common/Loaders/CEGamepadButtonEnum.h"
#include "Common/Loaders/CEGamepadTypeEnum.h"
#include "Common/Utility/Geometric.h"

namespace Utility
{
	struct GamepadData
	{
		ScePadData data = { 0 };
		ScePadControllerInformation info = { 0 };
	};

	struct OpenGamepad;

	class Gamepad
	{
	public:

		static bool Initialise();
		static void Uninitialise();

		SceUserServiceUserId GetUser() const;

		bool IsValid() const;
		Loaders::GamepadTypeValues::GamepadType GetGamepadType() const { return Loaders::GamepadTypeValues::PS4Controller; }

		uint32_t GetButtons() const;
		const std::array<uint32_t, Loaders::GamepadButtonValues::NumGamepadButtonValues>& GetLogicalToDeviceMask() const;
		float GetLeftTrigger() const;
		float GetRightTrigger() const;
		Vector2d GetNormalisedLeftThumbstick( float deadzone_multiplier ) const;
		Vector2d GetNormalisedRightThumbstick( float deadzone_multiplier ) const;

		void Update(const SceUserServiceUserId user);

	private:

		void UpdateOpenGamepads();
		GamepadData ReadOpenGamepad(const OpenGamepad& gamepad);

		static float NormaliseThumbstickValue(const uint8_t value);
		static float NormaliseThumbstickValue(const float value);

		SceUserServiceUserId user = SCE_USER_SERVICE_USER_ID_INVALID;
		GamepadData gamepad_reading = { 0 };

		std::unordered_map< SceUserServiceUserId, std::shared_ptr< OpenGamepad > > open_gamepads;
	};

	struct OpenGamepad
	{
		OpenGamepad(const SceUserServiceUserId user)
			: user(user)
			, port(0)
		{
			port = scePadOpen(user, SCE_PAD_PORT_TYPE_STANDARD, 0, nullptr);
		}
		~OpenGamepad()
		{
			if (IsOpen())
				scePadClose(port);
		}
		SceUserServiceUserId GetUser() const { return user; }
		bool IsOpen() const { return port > 0; }
		int GetPort() const { return port; }
	private:
		SceUserServiceUserId user;
		int port;
	};
}