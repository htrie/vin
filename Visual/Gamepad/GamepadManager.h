#pragma once

#include <functional>
#include <vector>

#include "Common/Utility/Listenable.h"
#include "Common/Utility/Memory/Memory.h"
#include "Common/Loaders/CEGamepadButton.h"
#include "Common/Loaders/CEGamepadButtonEnum.h"
#include "Common/Loaders/CEGamepadType.h"
#include "Common/Loaders/CEGamepadTypeEnum.h"
#include "Common/Loaders/CEGamepadThumbstick.h"
#include "Common/Loaders/CEGamepadThumbstickEnum.h"

#include "Visual/Device/Constants.h"
#include "Devices/GamepadDeviceState.h"

#include "GamepadManagerListener.h"

namespace Device
{
	class WindowDX;
}

namespace Utility
{
	//<!> WIP, currently only used on PC
	class GamepadManager
		: public Utility::Listenable<GamepadManagerListener>
	{
	public:
		using DeviceId_T = std::size_t;
		static constexpr auto InvalidDeviceId{ std::numeric_limits<GamepadManager::DeviceId_T>::max() };

	public:
		GamepadManager( const GamepadManager& ) = delete;
		GamepadManager& operator=( const GamepadManager& ) = delete;
		GamepadManager( GamepadManager&& ) = delete;
		GamepadManager& operator=( GamepadManager&& ) = delete;
		~GamepadManager();

		static GamepadManager& Get();

		bool IsInitialised() const { return hwnd != NULL; }
		void Initialise( const Device::WindowDX& window );
		void Deinitialise();

		void MsgProc( HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param );
		void UpdateGamepads();

		std::size_t GetNumConnectedDevices() const;

		DeviceId_T GetDeviceIdByDeviceName( std::wstring_view device_name ) const;
		const GamepadDeviceState* GetDeviceState( DeviceId_T device_id ) const noexcept;
		std::wstring GetDeviceName( DeviceId_T device_id ) const;
		void EnumerateConnectedDevices( const std::function<void( const GamepadDeviceState& device, DeviceId_T device_id )>& func ) const;

		Loaders::GamepadTypeValues::GamepadType GetGamepadType( DeviceId_T device_id ) const;
		static Loaders::GamepadTypeValues::GamepadType GetGamepadType( GamepadDeviceType device_type );

	private:
		explicit GamepadManager() = default;

		std::vector<GamepadDeviceState> states{};
		::time_t next_device_list_update_time{};
		HWND hwnd{ NULL };
	};
}
