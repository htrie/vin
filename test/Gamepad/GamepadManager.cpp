#include "GamepadManager.h"

#include "Visual/Device/WindowDX.h"

namespace Utility
{
	GamepadManager& GamepadManager::Get()
	{
		static GamepadManager instance{};
		return instance;
	}

	GamepadManager::~GamepadManager()
	{
		assert( !IsInitialised() );
	}

	GamepadManager::DeviceId_T GamepadManager::GetDeviceIdByDeviceName( std::wstring_view device_name ) const
	{
		if( !device_name.empty() )
		{
			for( std::size_t i = 0; i < states.size(); ++i )
			{
				const auto& state = states[i];
				if( state.device_name == device_name )
				{
					return i;
				}
			}
		}
		return std::numeric_limits<std::size_t>::max();
	}

	const GamepadDeviceState* GamepadManager::GetDeviceState( DeviceId_T device_idx ) const noexcept
	{
		return (device_idx < states.size()) ? &(states[device_idx]) : nullptr;
	}

	std::wstring GamepadManager::GetDeviceName( DeviceId_T device_id ) const
	{
		if( const auto* const device_state = GetDeviceState( device_id ) )
		{
			return device_state->device_name;
		}

		return std::wstring{};
	}

	std::size_t GamepadManager::GetNumConnectedDevices() const
	{
		return Container::CountIf( states, []( const GamepadDeviceState& state ) { return state.is_connected; } );
	}

	void GamepadManager::EnumerateConnectedDevices( const std::function<void( const GamepadDeviceState& device, DeviceId_T device_id )>& func ) const
	{
		for( std::size_t idx = 0; idx < states.size(); ++idx )
		{
			auto& state = states[idx];
			if( state.is_connected )
			{
				func( state, idx );
			}
		}
	}

	Loaders::GamepadTypeValues::GamepadType GamepadManager::GetGamepadType( DeviceId_T device_id ) const
	{
		if( const auto* const state = GetDeviceState( device_id ) )
		{
			return GetGamepadType( state->type );
		}

		return Loaders::GamepadTypeValues::NumGamepadTypeValues;
	}

	Loaders::GamepadTypeValues::GamepadType GamepadManager::GetGamepadType( const GamepadDeviceType device_type )
	{
		using HardwareType = GamepadDeviceType;
		switch( device_type )
		{
		case HardwareType::XInputDevice:
			return Loaders::GamepadTypeValues::XboxController;

		case HardwareType::Dualshock4:
			return Loaders::GamepadTypeValues::PS4Controller;

		case HardwareType::Generic:
			return Loaders::GamepadTypeValues::Generic;

		default:
			return Loaders::GamepadTypeValues::NumGamepadTypeValues;
		}
	}
}

#if defined(_WIN32) && !defined(_XBOX_ONE)
#include "Win32/GamepadManagerWin32.cpp"
#else
#include "None/GamepadManagerNone.cpp"
#endif
