#pragma once

//#define DEBUG_BUDDY_ALLOCATOR

namespace GpuParticles
{
	template<Memory::Tag G, typename U, size_t SLI = 4>
	class BuddyAllocator
	{
		static_assert(SLI <= 6, "SLI can't be higher than 6");

	public:
		using NodeId = Memory::SparseId<U>;

		struct Range
		{
			U offset;
			U size;
		};

	private:
		static constexpr size_t MIN_FLI = SLI - 1;
		static constexpr size_t NUM_SL_BUCKETS = 1 << SLI;
		static constexpr size_t SLI_MASK = size_t(1 << SLI) - 1;
		static constexpr U MIN_SIZE = U(1) << SLI;

		struct Node
		{
			U size = U(0);
			U offset = U(0);
			NodeId next_free;
			NodeId prev_free;
			NodeId next_physical;
			NodeId prev_physical;
			bool is_free = true;
		};

		struct Level
		{
			uint64_t mask = 0;
			NodeId free[NUM_SL_BUCKETS];
		};

		uint64_t mask = 0;
		Memory::SmallVector<Level, 16, G> levels;
		Memory::SparseSet<Node, 16, G, U> nodes;

		static inline uint8_t BitScanLSB(uint64_t mask)
		{
		#if defined(_MSC_VER) && defined(_WIN64)
			unsigned long pos;
			if (_BitScanForward64(&pos, mask))
				return static_cast<uint8_t>(pos);
			return UINT8_MAX;
		#elif defined __GNUC__ || defined __clang__
			return static_cast<uint8_t>(__builtin_ffsll(mask)) - 1U;
		#else
			uint8_t pos = 0;
			uint64_t bit = 1;
			do
			{
				if (mask & bit)
					return pos;
				bit <<= 1;
			} while (pos++ < 63);
			return 0xFF;
		#endif
		}

		static inline uint8_t BitScanMSB(uint64_t mask)
		{
		#if defined(_MSC_VER) && defined(_WIN64)
			unsigned long pos;
			if (_BitScanReverse64(&pos, mask))
				return static_cast<uint8_t>(pos);
		#elif defined __GNUC__ || defined __clang__
			if (mask)
				return 63 - static_cast<uint8_t>(__builtin_clzll(mask));
		#else
			uint8_t pos = 63;
			uint64_t bit = 1ULL << 63;
			do
			{
				if (mask & bit)
					return pos;
				bit >>= 1;
			} while (pos-- > 0);
		#endif
			return 0xFF;
		}

		static inline size_t ComputeFLI(U size)
		{
			if (size < MIN_SIZE)
				return 0;

			return size_t(BitScanMSB(size)) - MIN_FLI;
		}

		static inline size_t ComputeSLI(U size, size_t f)
		{
			if (f == 0)
				return size & SLI_MASK;

			return size_t(size >> (f + MIN_FLI - SLI)) & SLI_MASK;
		}

		static inline U RoundSize(U size)
		{
			const auto f = ComputeFLI(size);
			if (f == 0)
				return size;

			return size + (U(1) << (f - 1)) - 1;
		}

		static inline U GetBlockSize(size_t f, size_t s)
		{
			if (f == 0)
				return U(s);

			return (U(1) << (f + MIN_FLI)) + (U(s) << (f + MIN_FLI - SLI));
		}

	#ifdef DEBUG_BUDDY_ALLOCATOR
		void Validate(NodeId block) const
		{
			assert(nodes.Contains(block));
			
			auto& node = nodes.Get(block);
			assert(node.next_free == NodeId() || (node.is_free && nodes.Contains(node.next_free) && nodes.Get(node.next_free).prev_free == block));
			assert(node.prev_free == NodeId() || (node.is_free && nodes.Contains(node.prev_free) && nodes.Get(node.prev_free).next_free == block));
			assert(node.next_physical == NodeId() || nodes.Contains(node.next_physical));
			assert(node.prev_physical == NodeId() || nodes.Contains(node.prev_physical));

			if (node.next_physical != NodeId())
			{
				auto& next = nodes.Get(node.next_physical);
				assert(next.offset == node.offset + node.size);
				assert(next.prev_physical == block);
			}

			if (node.prev_physical != NodeId())
			{
				auto& prev = nodes.Get(node.prev_physical);
				assert(prev.offset + prev.size == node.offset);
				assert(prev.next_physical == block);
			}

			const auto f = ComputeFLI(node.size);
			const auto s = ComputeSLI(node.size, f);
			assert(node.size >= GetBlockSize(f, s));

			if (node.is_free)
			{
				assert(levels[f].free[s] != NodeId());
				assert(levels[f].free[s] == block || node.prev_free != NodeId());
			}
			else
			{
				assert(node.next_free == NodeId() && node.prev_free == NodeId());
			}
		}
	#endif

		NodeId PopBlock(NodeId block, size_t f, size_t s)
		{
			auto& level = levels[f];
			auto& node = nodes.Get(block);

			assert((mask & (uint64_t(1) << f)) != 0);
			assert((level.mask & (uint64_t(1) << s)) != 0);

			if (level.free[s] == block)
			{
				assert(node.prev_free == NodeId());
				level.free[s] = node.next_free;
				if (level.free[s] == NodeId())
				{
					level.mask ^= uint64_t(1) << s;
					if (level.mask == 0)
						mask ^= uint64_t(1) << f;
				}
			}
			else
			{
				assert(node.prev_free != NodeId());
				nodes.Get(node.prev_free).next_free = node.next_free;
			}

			if (node.next_free != NodeId())
				nodes.Get(node.next_free).prev_free = node.prev_free;

			node.next_free = NodeId();
			node.prev_free = NodeId();
			node.is_free = false;

		#ifdef DEBUG_BUDDY_ALLOCATOR
			Validate(block);
		#endif

			return block;
		}

		NodeId PopBlock(NodeId block)
		{
			assert(block != NodeId());
			auto& node = nodes.Get(block);
			const auto f = ComputeFLI(node.size);
			const auto s = ComputeSLI(node.size, f);
			assert(node.size >= GetBlockSize(f, s));

			return PopBlock(block, f, s);
		}

		NodeId PopBlock(size_t f, size_t s)
		{
			auto& level = levels[f];
			auto block = level.free[s];

			if (block == NodeId())
				return NodeId();

			return PopBlock(block, f, s);
		}

		NodeId PushBlock(NodeId block, size_t f, size_t s)
		{
			auto& node = nodes.Get(block);
			assert(node.size >= GetBlockSize(f, s));

			auto& level = levels[f];
			mask |= uint64_t(1) << f;
			level.mask |= uint64_t(1) << s;

			node.is_free = true;
			node.next_free = level.free[s];
			node.prev_free = NodeId();
			if (node.next_free != NodeId())
				nodes.Get(node.next_free).prev_free = block;

			level.free[s] = block;

		#ifdef DEBUG_BUDDY_ALLOCATOR
			Validate(block);
		#endif

			return block;
		}

		NodeId PushBlock(NodeId block)
		{
			assert(block != NodeId());
			auto& node = nodes.Get(block);
			const auto f = ComputeFLI(node.size);
			const auto s = ComputeSLI(node.size, f);

			return PushBlock(block, f, s);
		}

		void SplitBlock(NodeId block, U size)
		{
			assert(nodes.Get(block).size >= size);

			const auto remaining = nodes.Get(block).size - size;
			if (remaining == 0)
				return;

			const auto f = ComputeFLI(remaining);
			const auto s = ComputeSLI(remaining, f);

			assert(GetBlockSize(f, s) <= remaining);

			auto nb = nodes.Add();
			auto& new_node = nodes.Get(nb);
			auto& node = nodes.Get(block);

			new_node.size = remaining;
			new_node.is_free = true;
			new_node.offset = node.offset + size;
			new_node.next_physical = node.next_physical;
			new_node.prev_physical = block;

			if (new_node.next_physical != NodeId())
				nodes.Get(new_node.next_physical).prev_physical = nb;

			node.next_physical = nb;
			node.size = size;

			PushBlock(nb, f, s);

		#ifdef DEBUG_BUDDY_ALLOCATOR
			Validate(block);
		#endif
		}

		NodeId MergeBlock(NodeId block)
		{
			if (auto prev_id = nodes.Get(block).prev_physical; prev_id != NodeId())
			{
				if (auto& prev = nodes.Get(prev_id); prev.is_free)
				{
					auto& node = nodes.Get(block);

					PopBlock(prev_id);
					prev.size += node.size;
					prev.next_physical = node.next_physical;
					if (prev.next_physical != NodeId())
						nodes.Get(prev.next_physical).prev_physical = prev_id;

					nodes.Remove(block);
					block = prev_id;
				}
			}

			if (auto next_id = nodes.Get(block).next_physical; next_id != NodeId())
			{
				if (auto& next = nodes.Get(next_id); next.is_free)
				{
					auto& node = nodes.Get(block);

					PopBlock(next_id);
					node.size += next.size;
					node.next_physical = next.next_physical;
					if (node.next_physical != NodeId())
						nodes.Get(node.next_physical).prev_physical = block;

					nodes.Remove(next_id);
				}
			}

		#ifdef DEBUG_BUDDY_ALLOCATOR
			Validate(block);
		#endif

			return block;
		}

	public:
		BuddyAllocator() {}
		BuddyAllocator(U block_size, U block_offset = 0) { AddBlock(block_size, block_offset); }

		void AddBlock(U block_size, U block_offset)
		{
			if (block_size < MIN_SIZE)
				return;

			const auto f = ComputeFLI(block_size);
			const auto s = ComputeSLI(block_size, f);
			assert(block_size >= GetBlockSize(f, s));

			if (levels.size() <= f)
				levels.resize(f + 1);

			auto block = nodes.Add();
			auto& node = nodes.Get(block);

			node.size = block_size;
			node.offset = block_offset;
			node.is_free = true;
			node.next_physical = NodeId();
			node.prev_physical = NodeId();

			PushBlock(block, f, s);
		}

		Range GetRange(NodeId node_id) const
		{
			if (!IsAllocated(node_id))
				return { U(0), U(0) };

			auto& node = nodes.Get(node_id);
			return { node.offset, node.size };
		}

		bool IsAllocated(NodeId node_id) const
		{
			if (!nodes.Contains(node_id))
				return false;

			auto& node = nodes.Get(node_id);
			return !node.is_free;
		}

		NodeId Allocate(U size)
		{
			const auto round_size = RoundSize(size);
			auto f = ComputeFLI(round_size);
			auto s = ComputeSLI(round_size, f);
			assert(GetBlockSize(f, s) >= size);

			NodeId block;

			if (uint64_t m = (levels[f].mask & ((~uint64_t(0)) << s)); m != 0)
			{
				s = BitScanLSB(m);
				block = PopBlock(f, s);
			}
			else if (m = (mask & ((~uint64_t(0)) << (f + 1))); m != 0)
			{
				f = BitScanLSB(m);
				assert(levels[f].mask != 0);
				s = BitScanLSB(levels[f].mask);
				block = PopBlock(f, s);
			}
			else
				return NodeId();

			assert(block != NodeId());

			SplitBlock(block, size);
			
			assert(nodes.Get(block).size >= size);
			return block;
		}

		void Deallocate(NodeId node_id)
		{
			if (!IsAllocated(node_id))
				return;

			PushBlock(MergeBlock(node_id));
		}

		void Clear()
		{
			nodes.Clear();
			levels.clear();
			mask = 0;
		}
	};
}