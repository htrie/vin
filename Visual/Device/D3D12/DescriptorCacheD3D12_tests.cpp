
#include "Catch2/catch.hpp"

#include "assert.h"

#include "Common/Utility/Memory/Memory.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/Logger/Logger.h"

#include <d3d12.h>

#include "UtilityD3D12.h"
#include "AllocatorUtilsD3D12.h"

using namespace Device::AllocatorUtilsD3D12;

TEST_CASE("Free range allocator", "[engine][device]")
{
	FreeRangeAllocator allocator(5, 10);
	for (int i = 0; i < 10; i++)
	{
		REQUIRE(*allocator.Allocate() == i + 5);

		if (i != 9)
			REQUIRE(allocator.GetFreeRanges().size() == 1);
	}

	REQUIRE(allocator.GetFreeRanges().size() == 0);
	REQUIRE(!allocator.Allocate());

	for (int i = 0; i < 10; i++)
	{
		if (i % 2 == 0)
			allocator.Free(i + 5);
	}

	REQUIRE(allocator.GetFreeRanges().size() == 5);

	for (int i = 0; i < 10; i++)
	{
		if (i % 2 != 0)
			allocator.Free(i + 5);
	}

	REQUIRE(allocator.GetFreeRanges().size() == 1);
}

TEST_CASE("DescriptorRingBuffer", "[engine][device]")
{
	{
		DescriptorRingAllocator buffer(10, 3);
		REQUIRE(buffer.Allocate(0, 0, 3) == 0);
		REQUIRE(buffer.Allocate(0, 0, 3) == 3);
		REQUIRE(buffer.Allocate(0, 0, 3) == 6);
		REQUIRE(buffer.CanAllocate(1));
		REQUIRE(!buffer.CanAllocate(2));
	}

	{
		DescriptorRingAllocator buffer(10, 3);
		buffer.Allocate(1, 0, 3);
		REQUIRE(buffer.Allocate(2, 1, 3) == 3);
		REQUIRE(buffer.Allocate(3, 2, 3) == 6);
		REQUIRE(buffer.Allocate(4, 3, 3) == 0);
		REQUIRE(buffer.Allocate(5, 4, 3) == 3);
		REQUIRE(buffer.Allocate(6, 5, 3) == 6);
	}

	{
		DescriptorRingAllocator buffer(20, 3);
		buffer.Allocate(1, 0, 2);
		buffer.Allocate(1, 0, 2);
		buffer.Allocate(1, 0, 2);
		buffer.Allocate(2, 0, 2);
		buffer.Allocate(2, 0, 2);
		buffer.Allocate(2, 0, 2);
		buffer.Allocate(3, 0, 2);
		buffer.Allocate(3, 0, 2);
		buffer.Allocate(3, 0, 2);
		REQUIRE(buffer.Allocate(3, 0, 2) == 18);
		REQUIRE(buffer.GetCount() == 20);
		REQUIRE(!buffer.CanAllocate(1));
		REQUIRE(buffer.Allocate(5, 1, 2) == 0);
		REQUIRE(buffer.Allocate(5, 2, 3) == 2);
		REQUIRE(buffer.Allocate(5, 3, 3) == 5);
	}
}