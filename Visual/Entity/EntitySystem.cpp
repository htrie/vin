
#include "Common/Geometry/Bounding.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Common/Job/JobSystem.h"

#include "Visual/Renderer/DrawCalls/DrawCall.h"
#include "Visual/Profiler/JobProfile.h"
#include "Visual/Engine/Plugins/PerformancePlugin.h"

#include "EntitySystem.h"


namespace Entity
{

	typedef Memory::SparseId<uint16_t> Id;

	struct SharedTemplate
	{
		std::shared_ptr<Template> templ;
		unsigned ref_count = 0;
	};

	class TemplatesBucket
	{
		Memory::FlatMap<uint64_t, SharedTemplate, Memory::Tag::Entities> templates;
		Memory::SpinMutex templates_mutex;

	public:
		void Clear()
		{
			templates.clear();
		}

		std::shared_ptr<Template> FindOrCreate(uint64_t key, const Desc& desc)
		{
			Memory::SpinLock lock(templates_mutex);
			auto& templ = templates[key];
			if (templ.ref_count == 0)
			{
				templ.templ = std::make_shared<Template>(desc);
				templ.templ->Regather();
			}
			templ.ref_count++;
			return templ.templ;
		}

		void Destroy(uint64_t key)
		{
			Memory::SpinLock lock(templates_mutex);
			if (auto& templ = templates[key]; --templ.ref_count == 0)
				templates.erase(key);
		}

		void Regather()
		{
			for (auto& templ : templates)
				templ.second.templ->Regather();
		}

		void Tweak(unsigned hash, const Renderer::DrawCalls::Uniform& uniform)
		{
			for (auto& templ : templates)
				templ.second.templ->Tweak(hash, uniform);
		}

		void UpdateStats(Stats& stats) const
		{
			stats.template_count += templates.size();
		}
	};


	class List
	{
		struct Segment
		{
			Memory::FlatMap<uint64_t, std::pair<std::shared_ptr<Template>, Batch>, Memory::Tag::Entities> batches;
			Memory::SpinMutex batches_mutex;

			Memory::SmallVector<std::pair<std::shared_ptr<Template>, Batch>, 64, Memory::Tag::Entities> list;

		public:
			void Clear()
			{
				batches.clear();
				list.clear();
			}

			void UpdateStats(Stats& stats, unsigned& batch_size_count, unsigned& batch_size_total, unsigned& sub_batch_size_count, unsigned& sub_batch_size_total) const
			{
				for (auto& el : list)
				{
					stats.batch_count++;

					const auto batch_size = (unsigned)el.second.draws.size();
					batch_size_count++;
					batch_size_total += batch_size;

					stats.batch_size_min = std::min(stats.batch_size_min, (float)batch_size);
					stats.batch_size_max = std::max(stats.batch_size_max, (float)batch_size);
					stats.batch_size_histogram[std::min((unsigned)stats.batch_size_histogram.size() - 1, batch_size)] += 1.0f;

					Memory::FlatMap<uint64_t, unsigned, Memory::Tag::Entities> sub_batch_sizes;
					for (auto& it : el.second.draws)
					{
						sub_batch_sizes[it.call->GetBuffersHash()]++;
					}
					for (auto& size : sub_batch_sizes)
					{
						stats.sub_batch_count++;

						const auto sub_batch_size = size.second;
						sub_batch_size_count++;
						sub_batch_size_total += sub_batch_size;

						stats.draw_call_count += sub_batch_size;

						stats.sub_batch_size_min = std::min(stats.sub_batch_size_min, (float)sub_batch_size);
						stats.sub_batch_size_max = std::max(stats.sub_batch_size_max, (float)sub_batch_size);
						stats.sub_batch_size_histogram[std::min((unsigned)stats.sub_batch_size_histogram.size() - 1, sub_batch_size)] += 1.0f;
					}
				}
			}

			void Dispatch(const std::shared_ptr<Template>& templ, const Batch& batch)
			{
				Memory::SpinLock lock(batches_mutex);
				auto& out = batches[templ->GetHash()];
				out.first = templ;
				for (auto& draw : batch.draws)
					out.second.draws.emplace_back(draw);
			}

			void Sort()
			{
				list.reserve(batches.size());
				for (auto& batch : batches)
					list.push_back(batch.second);
				batches.clear();

				std::stable_sort(list.begin(), list.end(), [](const auto& a, const auto& b) // Sort batches.
					{
						return a.first->GetHash() > b.first->GetHash();
					});

				for (auto& el : list)
				{
					std::stable_sort(el.second.draws.begin(), el.second.draws.end(), [&](auto& a, auto& b) // Sort sub-batches.
						{
							if (a.sub_hash != b.sub_hash)
							return a.sub_hash < b.sub_hash;
					return a.call < b.call;
						});

					for (auto& draw : el.second.draws)
					{
						el.second.visibility |= draw.visibility;
					}
				}
			}

			void Process(BatchDrawFunc& draw_func)
			{
				for (auto& el : list)
					draw_func(el.first, el.second);
			}

			bool IsEmpty() const { return batches.size() == 0; }
		};

		std::array<std::array<Segment, Renderer::DrawCalls::EffectOrder::Value::Count>, (unsigned)Renderer::DrawCalls::BlendMode::NumBlendModes> segments;

	public:
		void Clear()
		{
			for (auto& __segments : segments)
				for (auto& _segments : __segments)
					_segments.Clear();
		}

		void UpdateStats(Stats& stats) const
		{
			stats.batch_count = 0;
			stats.sub_batch_count = 0;
			stats.draw_call_count = 0;

			stats.batch_size_min = std::numeric_limits<float>::max();
			stats.batch_size_max = 0.0f;
			stats.batch_size_histogram.fill(0.0f);

			stats.sub_batch_size_min = std::numeric_limits<float>::max();
			stats.sub_batch_size_max = 0.0f;
			stats.sub_batch_size_histogram.fill(0.0f);

			unsigned batch_size_count = 0;
			unsigned batch_size_total = 0;

			unsigned sub_batch_size_count = 0;
			unsigned sub_batch_size_total = 0;

			for (auto& __segments : segments)
				for (auto& _segments : __segments)
					_segments.UpdateStats(stats, batch_size_count, batch_size_total, sub_batch_size_count, sub_batch_size_total);

			stats.batch_size_min = batch_size_count > 0 ? stats.batch_size_min : 0.0f;
			stats.batch_size_avg = batch_size_count > 0 ? float(batch_size_total / (double)batch_size_count) : 0.0f;

			stats.sub_batch_size_min = sub_batch_size_count > 0 ? stats.sub_batch_size_min : 0.0f;
			stats.sub_batch_size_avg = sub_batch_size_count > 0 ? float(sub_batch_size_total / (double)sub_batch_size_count) : 0.0f;
		}

		void Dispatch(const std::shared_ptr<Template>& templ, const Batch& batch)
		{
			segments[(unsigned)templ->GetBlendMode()][(unsigned)templ->GetEffectOrder()].Dispatch(templ, batch);
		}

		void Sort()
		{
			for (auto& __segments : segments)
				for (auto& _segments : __segments)
					if (!_segments.IsEmpty())
						Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Entities, Job2::Profile::EntitiesSort, [&]() { _segments.Sort(); } });
		}

		void Process(Renderer::DrawCalls::BlendMode blend_mode, BatchDrawFunc& draw_func)
		{
			for (auto effect_order = (Renderer::DrawCalls::EffectOrder::Value)0; effect_order < Renderer::DrawCalls::EffectOrder::Value::Count; ++(unsigned&)effect_order)
				segments[(unsigned)blend_mode][(unsigned)effect_order].Process(draw_func);
		}
	};


	class Bucket
	{
		Entities entities;
		Memory::SpinMutex entities_mutex;

		Memory::SmallVector<uint64_t, 64, Memory::Tag::Entities> destroy_entity_ids;
		Memory::SpinMutex destroy_mutex;

		Memory::FlatMap<std::shared_ptr<Template>, Batch, Memory::Tag::Entities> batches;

		void Destroy(uint64_t entity_id)
		{
			uint64_t template_key = 0;
			{
				Memory::SpinLock lock(entities_mutex);
				if (const auto id = ExtractSparseID(entity_id); entities.Contains(id))
				{
					auto& entity = entities.Get(id);
					template_key = entity.templ->GetHash();
					entities.Remove(id);
				}
			}
			System::Get().ReleaseTemplate(template_key);
		}

		void FlushDestroy()
		{
			Memory::SpinLock lock(destroy_mutex);
			for (auto& entity_id : destroy_entity_ids)
				Destroy(entity_id);
			destroy_entity_ids.clear();
		}

	public:
		void Swap()
		{
		}

		void GarbageCollect()
		{
			FlushDestroy();
		}

		void Clear()
		{
			FlushDestroy();

			entities.Clear();
			batches.clear();
		}

		void UpdateStats(Stats& stats) const
		{
			stats.entity_count += size_t(entities.Size());
		}

		void OnCreateDevice(Device::IDevice* device)
		{
		}

		void OnResetDevice(Device::IDevice* device)
		{
		}

		void OnLostDevice()
		{
			FlushDestroy();

			batches.clear();
		}

		void OnDestroyDevice()
		{
			FlushDestroy();

			batches.clear();

			for (auto& entity : entities)
				entity.draw_call->ClearBuffers();
		}

		uint32_t GetNextSparseID()
		{
			Memory::SpinLock lock(entities_mutex);
			const auto id = entities.Add();
			static_assert(sizeof(Id) == sizeof(uint32_t));
			return *(uint32_t*)&id;
		}

		Id ExtractSparseID(uint64_t entity_id) const
		{
			return *(Id*)&entity_id;
		}

		void Create(uint64_t entity_id, const Desc& desc)
		{
			auto templ = System::Get().GrabTemplate(desc.GetHash(), desc);
			Memory::SpinLock lock(entities_mutex);
			if (const auto id = ExtractSparseID(entity_id); entities.Contains(id))
			{
				entities.Get(id) = Entity(desc, std::move(templ));
			}
		}

		void DeferDestroy(uint64_t entity_id)
		{
			Memory::SpinLock lock(destroy_mutex);
			destroy_entity_ids.push_back(entity_id);
		}

		void Move(uint64_t entity_id, const BoundingBox& bounding_box, bool visible, const Renderer::DrawCalls::Uniforms& uniforms, unsigned instance_count_x, unsigned instance_count_y, unsigned instance_count_z)
		{
			Memory::SpinLock lock(entities_mutex);
			if (const auto id = ExtractSparseID(entity_id); entities.Contains(id))
			{
				auto& entity = entities.Get(id);
				entity.bounding_box = bounding_box;
				entity.visible = visible;
				entity.draw_call->Patch(uniforms);
				entity.draw_call->SetInstanceCount(instance_count_x, instance_count_y, instance_count_z);
			}
		}

		void SetVisible(uint64_t entity_id, bool visible)
		{
			Memory::SpinLock lock(entities_mutex);
			if (const auto id = ExtractSparseID(entity_id); entities.Contains(id))
			{
				auto& entity = entities.Get(id);
				entity.visible = visible;
			}
		}

		void Process(DrawCallFunc& draw_call_func)
		{
			Memory::SpinLock lock(entities_mutex);
			for (auto& entity : entities)
				draw_call_func(*entity.templ, entity);
		}

		void Update(UpdateFunc update_func, List& list)
		{
			FlushDestroy();

			Memory::SpinLock lock(entities_mutex);

			batches.clear();

			for (auto& entity : entities)
				if (auto visibility = update_func(*entity.templ, entity); visibility.any())
					batches[entity.templ].draws.emplace_back(entity.draw_call->GetBuffersHash(), visibility, entity.draw_call.get());

			for (auto& batch : batches)
				list.Dispatch(batch.first, batch.second);
		}
	};


	class Impl
	{
	#if defined(MOBILE)
		static const unsigned BucketCount = 4;
	#else
		static const unsigned BucketCount = 8;
	#endif

		std::array<Bucket, BucketCount> buckets;
		Memory::SpinMutex buckets_mutex;

		std::array<TemplatesBucket, BucketCount> templates_buckets;

		List list;

		std::atomic_uint fence = 0;

	public:
		void Swap()
		{
			for (auto& bucket : buckets)
				bucket.Swap();
		}

		void GarbageCollect()
		{
			for (auto& bucket : buckets)
				bucket.GarbageCollect();
		}

		void Clear()
		{
			for (auto& bucket : buckets)
				bucket.Clear();
			for (auto& templates_bucket : templates_buckets)
				templates_bucket.Clear();
			list.Clear();
		}

		void Regather()
		{
			for (auto& templates_bucket : templates_buckets)
				templates_bucket.Regather();
		}

		void UpdateStats(Stats& stats) const
		{
			for (auto& bucket : buckets)
				bucket.UpdateStats(stats);
			for (auto& templates_bucket : templates_buckets)
				templates_bucket.UpdateStats(stats);
			list.UpdateStats(stats);
		}

		void OnCreateDevice(Device::IDevice* device)
		{
			for (auto& bucket : buckets)
				bucket.OnCreateDevice(device);
		}

		void OnResetDevice(Device::IDevice* device)
		{
			for (auto& bucket : buckets)
				bucket.OnResetDevice(device);
		}

		void OnLostDevice()
		{
			list.Clear();

			for (auto& bucket : buckets)
				bucket.OnLostDevice();
		}

		void OnDestroyDevice()
		{
			list.Clear();

			for (auto& bucket : buckets)
				bucket.OnDestroyDevice();
		}

		uint64_t GetNextEntityID()
		{
			static std::atomic_uint32_t bucket_index = 0;
			const auto bucket_id = (bucket_index++) % BucketCount;
			const auto sparse_id = buckets[bucket_id].GetNextSparseID();
			return ((uint64_t)(bucket_id + 1) << 32) | (uint64_t)sparse_id; // Add 1 to avoid using entity_id 0 (used for empty entity).
		}

		unsigned ExtractBucketID(uint64_t entity_id) const
		{
			return (uint32_t)(entity_id >> 32) - 1;
		}

		std::shared_ptr<Template> GrabTemplate(uint64_t key, const Desc& desc)
		{
			return templates_buckets[key % BucketCount].FindOrCreate(key, desc);
		}

		void ReleaseTemplate(uint64_t key)
		{
			return templates_buckets[key % BucketCount].Destroy(key);
		}

		void Create(uint64_t entity_id, const Desc& desc)
		{
			buckets[ExtractBucketID(entity_id) % BucketCount].Create(entity_id, desc);
		}

		void Destroy(uint64_t entity_id)
		{
			buckets[ExtractBucketID(entity_id) % BucketCount].DeferDestroy(entity_id);
		}

		void Move(uint64_t entity_id, const BoundingBox& bounding_box, bool visible, const Renderer::DrawCalls::Uniforms& uniforms, unsigned instance_count_x, unsigned instance_count_y, unsigned instance_count_z)
		{
			buckets[ExtractBucketID(entity_id) % BucketCount].Move(entity_id, bounding_box, visible, uniforms, instance_count_x, instance_count_y, instance_count_z);
		}

		void SetVisible(uint64_t entity_id, bool visible)
		{
			buckets[ExtractBucketID(entity_id) % BucketCount].SetVisible(entity_id, visible);
		}

		std::atomic_uint& Update(UpdateFunc& update_func)
		{
			PROFILE;
			PROFILE_ONLY(Memory::StackTag tag(Memory::Tag::Entities));

			Memory::SpinLock lock(buckets_mutex);

			list.Clear();

			for (auto& bucket : buckets)
			{
				auto* _bucket = &bucket;
				auto* _list = &list;

				fence++;
				Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Entities, Job2::Profile::EntitiesUpdate, [=]() {
					_bucket->Update(update_func, list);
					fence--;
				} });
			}

			return fence;
		}

		void Tweak(unsigned hash, const Renderer::DrawCalls::Uniform& uniform)
		{
			for (auto& templates_bucket : templates_buckets)
				templates_bucket.Tweak(hash, uniform);
		}

		void Process(DrawCallFunc draw_call_func)
		{
			for (auto& bucket : buckets)
				bucket.Process(draw_call_func);
		}

		void ListSort()
		{
			list.Sort();
		}

		void ListProcess(Renderer::DrawCalls::BlendMode blend_mode, BatchDrawFunc& draw_func)
		{
			list.Process(blend_mode, draw_func);
		}
	};


	System& System::Get()
	{
		static System instance;
		return instance;
	}

	System::System() : ImplSystem() {}

	void System::Swap() { impl->Swap(); }
	void System::GarbageCollect() { impl->GarbageCollect(); }
	void System::Clear() { impl->Clear(); }

	void System::Regather() { impl->Regather(); }

	Stats System::GetStats() const
	{
		Stats stats;
		impl->UpdateStats(stats);
		return stats;
	}

	void System::OnCreateDevice(Device::IDevice* device) { impl->OnCreateDevice(device); }
	void System::OnResetDevice(Device::IDevice* device) { impl->OnResetDevice(device); }
	void System::OnLostDevice() { impl->OnLostDevice(); }
	void System::OnDestroyDevice() { impl->OnDestroyDevice(); }

	uint64_t System::GetNextEntityID() { return impl->GetNextEntityID(); }

	std::shared_ptr<Template> System::GrabTemplate(uint64_t key, const Desc& desc) { return impl->GrabTemplate(key, desc); }
	void System::ReleaseTemplate(uint64_t key) { return impl->ReleaseTemplate(key); }

	void System::Create(uint64_t entity_id, const Desc& desc) { impl->Create(entity_id, desc); }
	void System::Destroy(uint64_t entity_id) { impl->Destroy(entity_id); }
	void System::Move(uint64_t entity_id, const BoundingBox& bounding_box, bool visible, const Renderer::DrawCalls::Uniforms& uniforms, unsigned instance_count_x, unsigned instance_count_y, unsigned instance_count_z) { impl->Move(entity_id, bounding_box, visible, uniforms, instance_count_x, instance_count_y, instance_count_z); }
	void System::SetVisible(uint64_t entity_id, bool visible) { impl->SetVisible(entity_id, visible); }

	void System::Tweak(unsigned hash, const Renderer::DrawCalls::Uniform& uniform) { impl->Tweak(hash, uniform); } 

	std::atomic_uint& System::Update(UpdateFunc update_func) { return impl->Update(update_func); }
	void System::Process(DrawCallFunc draw_call_func) { impl->Process(draw_call_func); }

	void System::ListSort() { impl->ListSort(); }
	void System::ListProcess(Renderer::DrawCalls::BlendMode blend_mode, BatchDrawFunc draw_func) { impl->ListProcess(blend_mode, draw_func); }

}
