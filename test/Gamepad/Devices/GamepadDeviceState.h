#pragma once

#include <array>
#include <string>

#include "Common/Utility/Bitset.h"
#include "Common/Utility/WindowsHeaders.h"

#include "GamepadDeviceTypes.h"

namespace Utility
{
	// temporary name until we refactor out/replace GamepadState
	struct GamepadDeviceState
	{
		static constexpr std::size_t MaxDigitalInputsCount{ 32 };
		static constexpr std::size_t MaxAnalogInputsCount{ 16 };

		bool is_connected{ false };
		GamepadDeviceType type{ GamepadDeviceType::NumGamepadDeviceTypes };

		Utility::Bitset<MaxDigitalInputsCount> digital_inputs{};
		std::array<float, MaxAnalogInputsCount> analog_inputs{};

		// hardware info
		std::wstring device_name{};
		std::wstring product_name{};
		std::wstring manufacturer_name{};

#if defined(_WIN32) && !defined(_XBOX_ONE)
		class HIDInfo
		{
		public:
			HIDInfo() = default;
			HIDInfo( HANDLE device, const std::wstring& device_name );
			HIDInfo( const HIDInfo& ) = delete;
			HIDInfo& operator=( const HIDInfo& ) = delete;
			HIDInfo( HIDInfo&& );
			HIDInfo& operator=( HIDInfo&& );
			~HIDInfo();

			auto& GetOutputBuffer() { return output_buffer; }
			const auto& GetOutputBuffer() const { return output_buffer; }

			void CheckOverlapped();

			const auto& GetDeviceHandle() const { return device_handle; }
			const auto& GetOutputFile() const { return output_file; }
			const auto& GetOverlapped() const { return overlapped; }

		private:
			HANDLE device_handle{ INVALID_HANDLE_VALUE }; // we do not own this one, this should be the value from WM_INPUT_DEVICE_CHANGE message.
			std::wstring device_name;

			std::array<BYTE, 96> output_buffer{};
			HANDLE output_file{ INVALID_HANDLE_VALUE };
			OVERLAPPED overlapped{};
		} hid_info{};
#endif
	};
}
