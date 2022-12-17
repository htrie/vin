
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

	struct Entities // [TODO] Use class instead.
	{
		struct Entity
		{
			uint64_t template_hash = 0;
			Id cull_id;
			Id render_id;
			Id particle_id;
			Id trail_id;
		};
		Memory::FlatMap<uint64_t, Entity, Memory::Tag::Entities> ids;

		struct SharedTemplate
		{
			std::shared_ptr<Template> templ;
			unsigned ref_count = 0;
		};
		Memory::FlatMap<uint64_t, SharedTemplate, Memory::Tag::Entities> templates;

		Culls culls;
		Renders renders;
		Particles particles;
		Trails trails;

	public:
		void Clear()
		{
			ids.clear();
			templates.clear();
			culls.Clear();
			renders.Clear();
			particles.Clear();
			trails.Clear();
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
			stats.entity_count += size_t(ids.size());
			stats.cull_count += culls.Size();
			stats.render_count += renders.Size();
			stats.particle_count += particles.Size();
			stats.trail_count += trails.Size();
		}

		void Create(uint64_t entity_id, Desc& desc)
		{
			if (ids.find(entity_id) == ids.end())
			{
				auto& templ = templates[desc.GetHash()];
				if (templ.ref_count == 0)
				{
					templ.templ = std::make_shared<Template>(desc);
					templ.templ->Regather();
				}
				templ.ref_count++;

				const auto cull_id = culls.Add();
				const auto render_id = renders.Add();
				Id particle_id;
				Id trail_id;
				culls.Get(cull_id) = Cull(desc, render_id);
				renders.Get(render_id) = Render(desc, templ.templ);
				if (desc.HasEmitter())
				{
					particle_id = particles.Add();
					particles.Get(particle_id) = Particle(desc, cull_id);
				}
				if (desc.HasTrail())
				{
					trail_id = trails.Add();
					trails.Get(trail_id) = Trail(desc, cull_id);
				}
				ids[entity_id] = { desc.GetHash(), cull_id, render_id, particle_id, trail_id };
			}
		}

		void Destroy(uint64_t entity_id)
		{
			if (auto entity = ids.find(entity_id); entity != ids.end())
			{
				if (auto& templ = templates[entity->second.template_hash]; --templ.ref_count == 0)
					templates.erase(entity->second.template_hash);

				culls.Remove(entity->second.cull_id);
				renders.Remove(entity->second.render_id);
				if (auto& particle_id = entity->second.particle_id)
				{
					particles.Remove(particle_id);
				}
				if (auto& trail_id = entity->second.trail_id)
				{
					trails.Remove(trail_id);
				}
				ids.erase(entity);
			}
		}

		void Move(uint64_t entity_id, const BoundingBox& bounding_box, bool visible, const Renderer::DrawCalls::Uniforms& uniforms, unsigned instance_count_x, unsigned instance_count_y, unsigned instance_count_z)
		{
			if (auto entity = ids.find(entity_id); entity != ids.end())
			{
				auto& cull = culls.Get(entity->second.cull_id);
				cull.bounding_box = bounding_box;
				cull.visible = visible;

				auto& render = renders.Get(entity->second.render_id);
				render.draw_call->Patch(uniforms);
				render.draw_call->SetInstanceCount(instance_count_x, instance_count_y, instance_count_z);
			}
		}

		void SetVisible(uint64_t entity_id, bool visible)
		{
			if (auto entity = ids.find(entity_id); entity != ids.end())
			{
				auto& cull = culls.Get(entity->second.cull_id);
				cull.visible = visible;
			}
		}
	};


	class Commands
	{
		enum class Type : unsigned
		{
			Undefined = 0,

			Create,
			Destroy,
			Move,
			SetVisible,

			Count
		};

		class Command
		{
			Type type;

		protected:
			uint64_t entity_id;

		public:
			Command(Type type, uint64_t entity_id)
				: type(type), entity_id(entity_id) {}

			Type GetType() const { return type; }
		};

	#if defined(MOBILE)
		static constexpr size_t CommandChunkSize = 16 * Memory::KB;
		static constexpr size_t TempChunkSize = 128 * Memory::KB;
	#else
		static constexpr size_t CommandChunkSize = 256 * Memory::KB;
		static constexpr size_t TempChunkSize = 1024 * Memory::KB;
	#endif

		Memory::Stack<Memory::Tag::Entities, CommandChunkSize> commands;
		Memory::SpinMutex commands_mutex;

		Memory::Stack<Memory::Tag::Entities, TempChunkSize> temps;
		Memory::SpinMutex temps_mutex;

		template <typename C>
		void Execute(void* command, Entities& entities)
		{
			static_cast<C*>(command)->Execute(entities);
			static_cast<C*>(command)->~C();
		}

	public:
		void Reset()
		{
			commands.Trim();
			commands.Reset();
			temps.Trim();
			temps.Reset();
		}

		void GarbageCollect()
		{
			commands.Clear();
			temps.Clear();
		}

		void Clear()
		{
			commands.Clear();
			temps.Clear();
		}

		void UpdateStats(Stats& stats) const
		{
			stats.command_used_size += commands.GetUsedSize();
			stats.command_reserved_size += commands.GetReservedSize();
			stats.temp_used_size += temps.GetUsedSize();
			stats.temp_reserved_size += temps.GetReservedSize();
		}

		void* Bump(size_t size)
		{
			Memory::SpinLock lock(temps_mutex);
			return temps.Allocate(size);
		}

		template <typename C, typename... ARGS>
		void Push(bool disable_async_commands, Entities& entities, ARGS... args)
		{
			if (disable_async_commands)
			{
				Memory::SpinLock lock(commands_mutex);
				C command(args...);
				command.Execute(entities);
			}
			else
			{
				Command* command = nullptr;
				{
					Memory::SpinLock lock(commands_mutex);
					command = (Command*)commands.Allocate(sizeof(C));
				}
				assert(Memory::AlignSize((size_t)command, (size_t)16u) == (size_t)command);
				new(command) C(args...);
			}
		}

		void Flush(Entities& entities)
		{
			commands.Process([&](void* command)
			{
				switch (static_cast<Command*>(command)->GetType())
				{
				case Type::Create: Execute<Create>(command, entities); break;
				case Type::Destroy: Execute<Destroy>(command, entities); break;
				case Type::Move: Execute<Move>(command, entities); break;
				case Type::SetVisible: Execute<SetVisible>(command, entities); break;
				}
			});
		}

		class Create : public Command
		{
			Temp<Desc> desc;

		public:
			Create(uint64_t entity_id, Temp<Desc>* desc)
				: Command(Type::Create, entity_id), desc(std::move(*desc)) {}

			void Execute(Entities& entities)
			{
				entities.Create(entity_id, *desc);
			}
		};

		class Destroy : public Command
		{
		public:
			Destroy(uint64_t entity_id)
				: Command(Type::Destroy, entity_id) {}

			void Execute(Entities& entities)
			{
				entities.Destroy(entity_id);
			}
		};

		class Move : public Command
		{
			BoundingBox bounding_box;
			bool visible;
			unsigned instance_count_x;
			unsigned instance_count_y;
			unsigned instance_count_z;
			Temp<Renderer::DrawCalls::Uniforms> uniforms;

		public:
			Move(uint64_t entity_id, const BoundingBox& bounding_box, bool visible, Temp<Renderer::DrawCalls::Uniforms>* uniforms, unsigned instance_count_x, unsigned instance_count_y, unsigned instance_count_z)
				: Command(Type::Move, entity_id), bounding_box(bounding_box), visible(visible), uniforms(std::move(*uniforms)), instance_count_x(instance_count_x), instance_count_y(instance_count_y), instance_count_z(instance_count_z) {}

			void Execute(Entities& entities)
			{
				entities.Move(entity_id, bounding_box, visible, *uniforms, instance_count_x, instance_count_y, instance_count_z);
			}
		};

		class SetVisible : public Command
		{
			bool visible;

		public:
			SetVisible(uint64_t entity_id, bool visible)
				: Command(Type::SetVisible, entity_id), visible(visible) {}

			void Execute(Entities& entities)
			{
				entities.SetVisible(entity_id, visible);
			}
		};
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
					draw_func(el .first, el.second);
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

		std::array<Commands, 2> commands;
		std::atomic_uint push_index = 0;
		std::atomic_uint flush_index = 1;

		Memory::FlatMap<std::shared_ptr<Template>, Batch, Memory::Tag::Entities> batches;

		unsigned disable_async_commands = 0;

		void FlushAll()
		{
			Swap();
			Flush();
			Swap();
			Flush();
		}

	public:
		void DisableAsyncCommands(unsigned frame_count)
		{
			FlushAll();

			disable_async_commands = frame_count;
		}

		void Swap()
		{
			flush_index = (flush_index + 1) % 2;
			push_index = (push_index + 1) % 2;

			if (disable_async_commands > 0)
				disable_async_commands--;

			commands[push_index].Reset();
		}

		void GarbageCollect()
		{
			FlushAll();

			for (auto& _commands : commands)
				_commands.GarbageCollect();
		}

		void Clear()
		{
			FlushAll();

			entities.Clear();

			for (auto& _commands : commands)
				_commands.Clear();

			batches.clear();
		}

		void Regather()
		{
			entities.Regather();
		}

		void UpdateStats(Stats& stats) const
		{
			for (auto& _commands : commands)
				_commands.UpdateStats(stats);

			entities.UpdateStats(stats);
		}

		void OnCreateDevice(Device::IDevice* device)
		{
		}

		void OnResetDevice(Device::IDevice* device)
		{
		}

		void OnLostDevice()
		{
			batches.clear();

			FlushAll();
		}

		void OnDestroyDevice()
		{
			batches.clear();

			FlushAll();

			for (auto& render : entities.renders)
				render.draw_call->ClearBuffers();
		}

		std::pair<void*, bool> Bump(size_t size)
		{
			if (disable_async_commands)
				return { Memory::Allocate(Memory::Tag::Entities, size), true };
			return { commands[push_index].Bump(size), false };
		}

		template <typename C, typename... ARGS>
		void Push(ARGS... args)
		{
			commands[push_index].Push<C>(disable_async_commands, entities, args...);
		}

		void Flush()
		{
			commands[flush_index].Flush(entities);
		}

		void Tweak(unsigned hash, const Renderer::DrawCalls::Uniform& uniform)
		{
			entities.Tweak(hash, uniform);
		}

		void Process(DrawCallFunc& draw_call_func)
		{
			for (auto& cull : entities.culls)
			{
				auto& render = entities.renders.Get(cull.render_id);
				draw_call_func(cull, *render.templ, render);
			}
		}

		void SimulateParticles(ParticleFunc particle_func)
		{
			for (auto& particle : entities.particles)
				particle_func(particle, entities.culls.Get(particle.cull_id));
		}

		void SimulateTrails(TrailFunc trail_func)
		{
			for (auto& trail : entities.trails)
				trail_func(trail, entities.culls.Get(trail.cull_id));
		}

		void Cull(CullFunc cull_func)
		{
			for (auto& cull : entities.culls)
				cull_func(cull, entities.renders.Get(cull.render_id));
		}

		void Render(RenderFunc render_func, List& list)
		{
			batches.clear();

			for (auto& render : entities.renders)
			{
				if (render.visibility.any())
				{
					auto& batch = batches[render.templ];
					if (render_func(*render.templ, render))
						batch.draws.emplace_back(render.draw_call->GetBuffersHash(), render.visibility, render.draw_call.get());
				}
			}

			for (auto& batch : batches)
			{
				list.Dispatch(batch.first, batch.second);
			}
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

		List list;

		std::atomic_uint fence = 0;

	public:
		void DisableAsyncCommands(unsigned frame_count)
		{
			LOG_INFO(L"[ENTITY] Disable Async Commands: " << frame_count << L" frames");
			for (auto& bucket : buckets)
				bucket.DisableAsyncCommands(frame_count);
		}

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
			list.Clear();
		}

		void Regather()
		{
			for (auto& bucket : buckets)
				bucket.Regather();
		}

		void UpdateStats(Stats& stats) const
		{
			for (auto& bucket : buckets)
				bucket.UpdateStats(stats);
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

		std::pair<void*, bool> Bump(uint64_t entity_id, size_t size)
		{
			assert(entity_id != 0);
			return buckets[entity_id % BucketCount].Bump(size);
		}

		template <typename C, typename... ARGS>
		void Push(uint64_t entity_id, ARGS... args)
		{
			assert(entity_id != 0);
			buckets[entity_id % BucketCount].Push<C>(args...);
		}

		std::atomic_uint& Update(ParticleFunc& particle_func, TrailFunc& trail_func, CullFunc& cull_func, RenderFunc& render_func)
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
				Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Entities, Job2::Profile::EntitiesFlush, [=]() { _bucket->Flush(); fence++;
					Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Entities, Job2::Profile::EntitiesParticles, [=]() { _bucket->SimulateParticles(particle_func); fence++;
						Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Entities, Job2::Profile::EntitiesTrails, [=]() { _bucket->SimulateTrails(trail_func); fence++;
							Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Entities, Job2::Profile::EntitiesCull, [=]() { _bucket->Cull(cull_func); fence++;
								Job2::System::Get().Add(Job2::Type::High, { Memory::Tag::Entities, Job2::Profile::EntitiesRender, [=]() { _bucket->Render(render_func, *_list); fence--;
								}}); fence--;
							}}); fence--;
						}}); fence--;
					}}); fence--;
				}});
			}

			return fence;
		}

		void Tweak(unsigned hash, const Renderer::DrawCalls::Uniform& uniform)
		{
			Memory::SpinLock lock(buckets_mutex);
			for (auto& bucket : buckets)
				bucket.Tweak(hash, uniform);
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

	void System::DisableAsyncCommands(unsigned frame_count) { impl->DisableAsyncCommands(frame_count); }

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

	uint64_t System::GetNextEntityID()
	{
		static std::atomic_uint64_t entity_id;
		return ++entity_id;
	}

	std::pair<void*, bool> System::Bump(uint64_t entity_id, size_t size) { return impl->Bump(entity_id, size); }

	void System::Create(uint64_t entity_id, Temp<Desc>&& desc) { impl->Push<Commands::Create>(entity_id, entity_id, &desc); }
	void System::Destroy(uint64_t entity_id) { impl->Push<Commands::Destroy>(entity_id, entity_id); }
	void System::Move(uint64_t entity_id, const BoundingBox& bounding_box, bool visible, Temp<Renderer::DrawCalls::Uniforms>&& uniforms, unsigned instance_count_x, unsigned instance_count_y, unsigned instance_count_z) { impl->Push<Commands::Move>(entity_id, entity_id, bounding_box, visible, &uniforms, instance_count_x, instance_count_y, instance_count_z); }
	void System::SetVisible(uint64_t entity_id, bool visible) { impl->Push<Commands::SetVisible>(entity_id, entity_id, visible); }

	void System::Tweak(unsigned hash, const Renderer::DrawCalls::Uniform& uniform) { impl->Tweak(hash, uniform); } 

	std::atomic_uint& System::Update(ParticleFunc particle_func, TrailFunc trail_func, CullFunc cull_func, RenderFunc render_func) { return impl->Update(particle_func, trail_func, cull_func, render_func); }
	void System::Process(DrawCallFunc draw_call_func) { impl->Process(draw_call_func); }

	void System::ListSort() { impl->ListSort(); }
	void System::ListProcess(Renderer::DrawCalls::BlendMode blend_mode, BatchDrawFunc draw_func) { impl->ListProcess(blend_mode, draw_func); }

}
