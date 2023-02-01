#pragma once

namespace Device::AllocatorUtilsD3D12
{

	class FreeRangeAllocator
	{
		struct FreeRange
		{
			uint32_t begin = 0;
			uint32_t end = 0;
		};

		uint32_t start;
		uint32_t count;

		// Ranges are always arranged from lowest to highest
		Memory::List<FreeRange, Memory::Tag::Device> free_ranges;

	public:
		FreeRangeAllocator(uint32_t start, uint32_t count)
			: start(start)
			, count(count)
		{
			free_ranges.push_back({ start, start + count });
		}

		const auto& GetFreeRanges() const { return free_ranges; }

		std::optional<uint32_t> Allocate()
		{
			if (free_ranges.empty())
				return std::nullopt;

			FreeRange& free_range = free_ranges.front();
			const uint32_t result = free_range.begin;
			free_range.begin += 1;
			if (free_range.begin == free_range.end)
				free_ranges.pop_front();

			return result;
		}

		void Free(const uint32_t value)
		{
			assert(value >= start && value < start + count);

			auto inserted_it = free_ranges.end();

			for (auto it = free_ranges.begin(); it != free_ranges.end(); it++)
			{
				auto& free_range = *it;

				// Value is before the start of the range
				if (value < free_range.begin - 1)
				{
					inserted_it = free_ranges.insert(it, { value, value + 1 });
					break;
				}

				// Value is ajacent to the current range begin
				if (free_range.begin > 0 && value == free_range.begin - 1)
				{
					free_range.begin -= 1;
					assert(free_range.begin >= start);
					inserted_it = it;
					break;
				}

				// Value is ajacent to the current range end
				if (value == free_range.end)
				{
					free_range.end += 1;
					assert(free_range.end <= start + count);
					inserted_it = it;
					break;
				}

				// Value is within the free range. Not allowed.
				if (value >= free_range.begin && value < free_range.end)
					throw ExceptionD3D12("Invalid deallocation");
			}

			// Nothing found - either empty free_range list or value is greater than the end of the last free range
			if (inserted_it == free_ranges.end())
			{
				free_ranges.push_back({ value, value + 1 });
				inserted_it = std::prev(free_ranges.end());
			}

			// Try to merge left neighbour
			if (inserted_it != free_ranges.begin())
			{
				const auto prev_it = std::prev(inserted_it);
				if (prev_it->end == inserted_it->begin)
				{
					inserted_it->begin = prev_it->begin;
					free_ranges.erase(prev_it);
				}
			}

			// Try to merge right neighbour
			const auto next_it = std::next(inserted_it);
			if (next_it != free_ranges.end())
			{
				if (inserted_it->end == next_it->begin)
				{
					inserted_it->end = next_it->end;
					free_ranges.erase(next_it);
				}
			}
		}
	};

	class DescriptorRingAllocator
	{
	public:
		struct Stats
		{
			uint64_t current_count = 0;
			uint64_t max_count = 0;
		};
	
	private:
		Memory::SmallVector<uint64_t, 3, Memory::Tag::Device> frame_descriptor_counter;
		Memory::SmallVector<uint64_t, 3, Memory::Tag::Device> frame_fences;
		uint64_t begin = 0;
		uint64_t count = 0;
		uint64_t capacity;
		uint64_t last_completed_fence = -1;
		uint64_t last_frame = -1;

		PROFILE_ONLY(Stats stats;)

		void Deallocate(uint64_t deallocate_count)
		{
			assert(count >= deallocate_count);
			begin = (begin + deallocate_count) % capacity;
			count -= deallocate_count;
		}

		void DeallocateCompleted(uint64_t completed_fence)
		{
			for (uint32_t i = 0; i < frame_fences.size(); i++)
			{
				if (completed_fence >= frame_fences[i])
				{
					Deallocate(frame_descriptor_counter[i]);
					frame_descriptor_counter[i] = 0;
					frame_fences[i] = -1;
				}
			}
		}

	public:
		DescriptorRingAllocator(const uint64_t capacity, uint64_t frame_count)
			: capacity(capacity)
			, frame_descriptor_counter(frame_count, 0)
			, frame_fences(frame_count, -1)
		{
		}

		uint64_t GetRemainingSpace() const
		{
			if (capacity >= count)
				return capacity - count;
			else
				return 0;
		}

		uint64_t GetCount() const { return count; }
		uint64_t GetCapacity() const { return capacity; }

		bool CanAllocate(uint64_t count)
		{
			return GetRemainingSpace() >= count;
		}

		uint64_t Allocate(uint64_t frame_fence, uint64_t completed_fence, uint64_t allocate_count)
		{
			const auto frame_index = frame_fence % frame_descriptor_counter.size();

			if (last_completed_fence != completed_fence)
			{
				DeallocateCompleted(completed_fence);
				last_completed_fence = completed_fence;
			}

			if (last_frame != frame_fence)
			{
				frame_fences[frame_index] = frame_fence;
				last_frame = frame_fence;
			}
			
			assert(CanAllocate(allocate_count));

			uint64_t result = (begin + count) % capacity;

			// Allocation must be continuous so start from the beginning if there is not enough room until the buffer end
			if (result + allocate_count > capacity)
			{
				allocate_count += capacity - result;
				result = 0;
			}
			
			count += allocate_count;
			frame_descriptor_counter[frame_index] += allocate_count;

			PROFILE_ONLY(stats.current_count = GetCount();)
			PROFILE_ONLY(stats.max_count = std::max(stats.max_count, stats.current_count);)

			return result;
		}

		PROFILE_ONLY(const Stats& GetStats() const { return stats; })

	};

	inline void* D3D12MAAllocationFunction(size_t Size, size_t Alignment, void* pUserData)
	{
		return Memory::Allocate(Memory::Tag::Device, Size, Alignment);
	}

	inline void D3D12MAFreeFunction(void* pMemory, void* pUserData)
	{
		Memory::Free(pMemory);
	}
}