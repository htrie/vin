#include "GamepadState.h"

#include "Visual/Utility/IMGUI/DebugGUIDefines.h"

#include "Gamepad.h"
#include "GamepadManager.h"

namespace Utility
{
	namespace
	{
		const Gamepad* current_gamepad = nullptr;
		bool imgui_focus = false;
		float deadzone_multiplier = 1.0f;
	}

	void GamepadState::SetGamepad(const Gamepad* gamepad)
	{
		current_gamepad = gamepad;
		ActiveGamepadChanged();
	}

	void GamepadState::SetDeadzoneMultiplier( float new_multiplier )
	{
		deadzone_multiplier = new_multiplier;
	}

	bool GamepadState::ButtonPressed(const Button type, const bool exclusively) const
	{
		assert(type != Button::NumGamepadButtonValues);

		if (exclusively)
			return button_state == (1 << type);
		else
			return (button_state & (1 << type)) != 0;
	}

	bool GamepadState::ButtonsPressed(const Button type1, const Button type2, const bool exclusively) const
	{
		assert(type1 != Button::NumGamepadButtonValues);
		assert(type2 != Button::NumGamepadButtonValues);

		if (exclusively)
			return button_state == ((1 << type1) | (1 << type2));
		else
			return (button_state & ((1 << type1) | (1 << type2))) != 0;
	}

	bool GamepadState::HasButtonInput() const
	{
		return button_state != 0;
	}

	bool GamepadState::GetState(GamepadState& state_out, bool imgui_overlay)
	{
		if (!current_gamepad)
			return false;

		const Gamepad& gamepad = *current_gamepad;

		if (!gamepad.IsValid())
			return false;

		const auto prev_left_thumb = state_out.previous_left_thumb;
		const auto prev_right_thumb = state_out.previous_right_thumb;

		state_out.previous_left_thumb = state_out.left_thumb;
		state_out.previous_right_thumb = state_out.right_thumb;
		state_out.button_state = 0;
		state_out.left_thumb = 0;
		state_out.right_thumb = 0;
		state_out.left_trigger = 0;
		state_out.right_trigger = 0;

	#ifdef DEBUG_GUI_ENABLED
		if (imgui_focus != imgui_overlay)
			return true;
	#endif

		// Iterate each button mask to determine if button is currently pressed.
		const auto pressed_buttons = static_cast<unsigned int>(gamepad.GetButtons());
		const auto& device_button_masks = gamepad.GetLogicalToDeviceMask();
		static_assert(Button::NumGamepadButtonValues < 32);
		for (size_t i = 0; i < Button::NumGamepadButtonValues; ++i)
		{
			if( (pressed_buttons & device_button_masks[i]) != 0 )
				state_out.button_state |= (1 << i);
		}

		const Vector2d temp_left_thumb = state_out.left_thumb;
		const Vector2d temp_right_thumb = state_out.right_thumb;
		state_out.left_thumb = gamepad.GetNormalisedLeftThumbstick( deadzone_multiplier );
		state_out.right_thumb = gamepad.GetNormalisedRightThumbstick( deadzone_multiplier );

		// Check against the previous thumbstick input to filter out unwanted input (such as the weird backwards "spring" that seems to happen to XB One controllers)
		// See issue number #59542
		const float stick_flick_filter = state_out.HasButtonInput() ? 1.1f : 1.5f;
		if ((state_out.left_thumb - prev_left_thumb).length() > stick_flick_filter)
			state_out.left_thumb = Vector2d(0.0f, 0.0f);
		if ((state_out.right_thumb - prev_right_thumb).length() > stick_flick_filter)
			state_out.right_thumb = Vector2d(0.0f, 0.0f);

		state_out.left_trigger = static_cast<unsigned char>(gamepad.GetLeftTrigger() * 255);
		state_out.right_trigger = static_cast<unsigned char>(gamepad.GetRightTrigger() * 255);

		if (state_out.left_trigger > 200)
			state_out.button_state |= (1 << Button::LeftTrigger);

		if (state_out.right_trigger > 200)
			state_out.button_state |= (1 << Button::RightTrigger);

		return true;
	}

	Loaders::GamepadTypeValues::GamepadType GamepadState::GetActiveGamepadType()
	{
		return current_gamepad ? current_gamepad->GetGamepadType() : Loaders::GamepadTypeValues::NumGamepadTypeValues;
	}

	std::pair<const GamepadDeviceState*, std::size_t> GamepadState::GetActiveGamepadDeviceState()
	{
#if defined(_WIN32) && !defined(_XBOX_ONE)
		return current_gamepad
			? std::make_pair( GamepadManager::Get().GetDeviceState( current_gamepad->GetDeviceIdx() ), current_gamepad->GetDeviceIdx() )
			: std::make_pair( nullptr, GamepadManager::InvalidDeviceId );
#else
		return std::make_pair( nullptr, GamepadManager::InvalidDeviceId );
#endif
	}

	void GamepadState::SetImGuiFocus(bool focus)
	{
		imgui_focus = focus;
	}
}
