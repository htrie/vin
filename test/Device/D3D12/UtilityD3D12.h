#pragma once

namespace Device
{
	struct ExceptionD3D12 : public std::runtime_error
	{
		ExceptionD3D12(const std::string text) : std::runtime_error(text.c_str())
		{
			LOG_CRIT(L"[D3D12] " << StringToWstring(text));
			abort(); // Exit immediately to get callstack.
		}
	};

    inline D3D12_RESOURCE_BARRIER GetResourceBarrierD3D12(ID3D12Resource* resource, D3D12_RESOURCE_STATES prev_state, D3D12_RESOURCE_STATES new_state)
    {
        D3D12_RESOURCE_BARRIER result = {};

        if (new_state == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        {
            result.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            result.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            result.UAV.pResource = resource;
        }
        else
        {
            result.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            result.Transition.pResource = resource;
            result.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            result.Transition.StateBefore = prev_state;
            result.Transition.StateAfter = new_state;
            result.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        }

        return result;
    }

    inline D3D12_RESOURCE_BARRIER GetUAVResourceBarrierD3D12(ID3D12Resource* resource)
    {
        return GetResourceBarrierD3D12(resource, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }

}
