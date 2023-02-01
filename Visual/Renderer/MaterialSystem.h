#pragma once

#include "Common/Utility/System.h"

class Material;
struct MaterialDesc;

namespace Mat
{
	typedef std::shared_ptr<Material> Handle;

	std::wstring GetTempFilename(const std::wstring& filename);

	class Impl;

	struct Stats
	{
		size_t mat_count = 0;
		size_t active_mat_count = 0;
	};

	class System : public ImplSystem<Impl, Memory::Tag::Material>
	{
		System();

	protected:
		System(const System&) = delete;
		System& operator=(const System&) = delete;

	public:
		static System& Get();

		void Init(const bool ignore_temp);

		void Swap() final;
		void Clear() final;

		void SetPotato(bool enable);

		void Update(const float elapsed_time);

		void Invalidate(const std::wstring_view filename);

		Handle FindMat(const MaterialDesc& desc);

		Stats GetStats();
	};
}