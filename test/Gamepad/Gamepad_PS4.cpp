#ifdef __INTELLISENSE__
#include "Gamepad_PS4.h"
#endif

namespace Utility
{
	bool Gamepad::Initialise()
	{
		static bool initialised = false;

		if (!initialised)
		{
			if (scePadInit() == SCE_OK)
				initialised = true;
		}

		return initialised;
	}

	void Gamepad::Uninitialise()
	{
		// Pad library does not require termination
	}

	SceUserServiceUserId Gamepad::GetUser() const
	{
		return user;
	}

	bool Gamepad::IsValid() const
	{
		return gamepad_reading.data.connected;
	}

	uint32_t Gamepad::GetButtons() const
	{
		return IsValid() ? gamepad_reading.data.buttons : 0;
	}

	const std::array<uint32_t, Loaders::GamepadButtonValues::NumGamepadButtonValues>& Gamepad::GetLogicalToDeviceMask() const
	{
		// Mapping from Utility::GamepadState::Button enumeration to SCE pad buttons.
		static constexpr std::array<uint32_t, Loaders::GamepadButtonValues::NumGamepadButtonValues> button_masks
		{
			SCE_PAD_BUTTON_UP,         // DpadUp
			SCE_PAD_BUTTON_DOWN,       // DpadDown
			SCE_PAD_BUTTON_LEFT,       // DPadLeft
			SCE_PAD_BUTTON_RIGHT,      // DPadRight
			SCE_PAD_BUTTON_OPTIONS,    // Menu
			SCE_PAD_BUTTON_TOUCH_PAD,  // View
			SCE_PAD_BUTTON_L3,         // LeftThumb
			SCE_PAD_BUTTON_R3,         // RightThumb
			SCE_PAD_BUTTON_L1,         // LeftShoulder
			SCE_PAD_BUTTON_R1,         // RightShoulder
			SCE_PAD_BUTTON_CROSS,      // A
			SCE_PAD_BUTTON_CIRCLE,     // B
			SCE_PAD_BUTTON_SQUARE,     // X
			SCE_PAD_BUTTON_TRIANGLE,   // Y
			SCE_PAD_BUTTON_L2,         // LeftTrigger
			SCE_PAD_BUTTON_R2,         // RightTrigger
			0, // L4
			0, // R4
			0, // L5
			0, // R5
			0, // Share, but we don't have access to this button on PS4 hardware
		};
		return button_masks;
	}

	float Gamepad::GetLeftTrigger() const
	{
		return IsValid() ? gamepad_reading.data.analogButtons.l2 / 255.0f : 0.0f;
	}

	float Gamepad::GetRightTrigger() const
	{
		return IsValid() ? gamepad_reading.data.analogButtons.r2 / 255.0f : 0.0f;
	}

	Vector2d Gamepad::GetNormalisedLeftThumbstick( float deadzone_multiplier ) const
	{
		return IsValid() ? Vector2d(NormaliseThumbstickValue(gamepad_reading.data.leftStick.x), -NormaliseThumbstickValue(gamepad_reading.data.leftStick.y)) : Vector2d(0.0f, 0.0f);
	}

	Vector2d Gamepad::GetNormalisedRightThumbstick( float deadzone_multiplier ) const
	{
		return IsValid() ? Vector2d(NormaliseThumbstickValue(gamepad_reading.data.rightStick.x), -NormaliseThumbstickValue(gamepad_reading.data.rightStick.y)) : Vector2d(0.0f, 0.0f);
	}

	void Gamepad::Update(const SceUserServiceUserId user)
	{
		UpdateOpenGamepads();

		SceUserServiceUserId most_recent_gamepad_user = SCE_USER_SERVICE_USER_ID_INVALID;
		GamepadData most_recent_gamepad_reading;
		uint64_t most_recent_time = 0;

		for (const auto& entry : open_gamepads)
		{
			const auto& open_gamepad = *entry.second;
			const auto& data = ReadOpenGamepad(open_gamepad);

			if (data.data.timestamp == 0)
				continue;  // Failed to read the gamepad

			if (most_recent_gamepad_reading.data.timestamp == 0 || data.data.timestamp > most_recent_time)
			{
				if (user < 0 || (!(open_gamepad.GetUser() < 0) && open_gamepad.GetUser() == user /*&& Playstation4::GetApp().IsSignedIn( user )*/))
				{
					most_recent_gamepad_user = open_gamepad.GetUser();
					most_recent_gamepad_reading = data;
					most_recent_time = data.data.timestamp;
				}
			}
		}

		this->user = most_recent_gamepad_user;
		this->gamepad_reading = most_recent_gamepad_reading;
	}

	void Gamepad::UpdateOpenGamepads()
	{
		std::unordered_map< SceUserServiceUserId, std::shared_ptr< OpenGamepad > > new_open_gamepads;

		SceUserServiceLoginUserIdList user_id_list;
		memset(&user_id_list, 0, sizeof(user_id_list));
		if (sceUserServiceGetLoginUserIdList(&user_id_list) == SCE_OK)
		{
			const auto max_num_users = sizeof(user_id_list.userId) / sizeof(*user_id_list.userId);
			for (size_t i = 0; i < max_num_users; ++i)
			{
				const auto user = user_id_list.userId[i];
				if (user < 0)
					continue;

				const auto it = open_gamepads.find(user);
				if (it == std::end(open_gamepads))
					new_open_gamepads.emplace(user, std::make_shared< OpenGamepad >(user));
				else
					new_open_gamepads.insert(*it);
			}
		}

		open_gamepads = std::move(new_open_gamepads);
	}

	GamepadData Gamepad::ReadOpenGamepad(const OpenGamepad& gamepad)
	{
		GamepadData data;

		const auto intercepted_by_system = (data.data.buttons & SCE_PAD_BUTTON_INTERCEPTED) > 0;
		if (intercepted_by_system)
			return data;

		if (!gamepad.IsOpen())
			return data;

		if (scePadReadState(gamepad.GetPort(), &data.data) != SCE_OK)
			return data;

		if (!data.data.connected)
			return data;

		if (scePadGetControllerInformation(gamepad.GetPort(), &data.info) != SCE_OK)
			return data;

		return data;
	}

	float Gamepad::NormaliseThumbstickValue(const uint8_t value)
	{
		return NormaliseThumbstickValue(value * 2.0f / 255.0f - 1.0f);
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