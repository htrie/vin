#pragma once

#include "Common/Utility/Event.h"
#include "Common/Utility/Geometric.h"
#include "Common/Loaders/CEGamepadButtonEnum.h"
#include "Common/Loaders/CEGamepadTypeEnum.h"

namespace Utility
{
	class Gamepad;
	struct GamepadDeviceState;

	class GamepadState
	{
	public:
		using Button = Loaders::GamepadButtonValues::GamepadButton;

		// Note: Triggers are special cased as a button when depressed to certain pressures

		bool ButtonPressed(const Button type, const bool exclusively = false) const;
		bool ButtonsPressed(const Button type1, const Button type2, const bool exclusively = false) const;
		bool HasButtonInput() const;

		const Vector2d& GetLeftThumb() const { return left_thumb; }
		const Vector2d& GetRightThumb() const { return right_thumb; }

		unsigned char GetLeftTrigger() const { return left_trigger; }
		unsigned char GetRightTrigger() const { return right_trigger; }

		void Clear() { *this = GamepadState(); }

		/// Gets the current state of the controller.
		/// @return true if a controller is connected, false otherwise.
		static bool GetState(GamepadState& state, bool imgui_overlay = false);

		static Loaders::GamepadTypeValues::GamepadType GetActiveGamepadType();
		static std::pair<const GamepadDeviceState*, std::size_t> GetActiveGamepadDeviceState();

		static void SetImGuiFocus(bool focus);
		static void SetGamepad(const Gamepad* gamepad);
		static void SetDeadzoneMultiplier( float new_multiplier );

		inline static Event<> ActiveGamepadChanged{};

	private:

		uint32_t button_state = 0;
		Vector2d left_thumb = Vector2d(0, 0);
		Vector2d right_thumb = Vector2d(0, 0);
		// Previous thumbstick input used for removing unwanted sudden input changes
		Vector2d previous_left_thumb = Vector2d(0, 0);
		Vector2d previous_right_thumb = Vector2d(0, 0);

		unsigned char left_trigger = 0;
		unsigned char right_trigger = 0;
	};
}