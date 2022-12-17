#pragma once

#include <array>

struct ExceptionGNMX : public std::runtime_error
{
	ExceptionGNMX(const std::string& text) : std::runtime_error(text)
	{
		LOG_CRIT(L"[GNMX] " << StringToWstring(text));
		abort(); // Exit immediately to get callstack.
	}
};

class OwnerGNMX
{
	Gnm::OwnerHandle handle = Gnm::kInvalidOwnerHandle;

public:
	OwnerGNMX(const Memory::DebugStringA<>& name)
	{
	#if defined(DEVELOPMENT_CONFIGURATION)
		auto status = Gnm::registerOwner(&handle, name.c_str());
		if (status == SCE_GNM_ERROR_FAILURE) // Happens when PA Debug is disabled.
			return;
		if (status != SCE_GNM_OK)
			LOG_WARN(L"[GNMX] Failed to register owner with status " << status);
	#endif
	}

	~OwnerGNMX()
	{
		if (handle)
			Gnm::unregisterOwnerAndResources(handle);
	}

	Gnm::OwnerHandle GetHandle() const { return handle; }
};


class ResourceGNMX
{
	Gnm::OwnerHandle owner = Gnm::kInvalidOwnerHandle;
	Gnm::ResourceHandle handle = Gnm::kInvalidResourceHandle;

public:
	ResourceGNMX(Gnm::OwnerHandle owner)
		: owner(owner) {}

	void Register(Gnm::ResourceType type, void* mem, size_t size, const char* name) // TODO: Do at constructor time.
	{
	#if defined(DEVELOPMENT_CONFIGURATION)
		std::array<char, 64> name_buffer; // Maximum name buffer
		constexpr size_t max_length = name_buffer.size() - 1; // Allow room for \0
		const auto copy_length = std::min(max_length, strlen(name));
		memcpy(name_buffer.data(), name, copy_length);
		name_buffer[copy_length] = '\0';

		auto status = Gnm::registerResource(&handle, owner, mem, size, name_buffer.data(), type, 0);
		if (status == SCE_GNM_ERROR_FAILURE) // Happens when PA Debug is disabled.
			return;
		if (status != SCE_GNM_OK)
			LOG_WARN(L"[GNMX] Failed to register resource with status " << status);
	#endif
	}

	virtual ~ResourceGNMX()
	{
		if (handle)
			Gnm::unregisterResource(handle);
	}
};
