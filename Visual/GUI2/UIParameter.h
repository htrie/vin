#pragma once

#include "Visual/Renderer/DrawCalls/EffectGraphParameter.h"

struct ImVec2;

namespace Device
{
	namespace GUI
	{
		using GraphType = Renderer::DrawCalls::GraphType;

		class UIParameter
		{
		public:
			typedef Renderer::DrawCalls::JsonValue JsonValue;
			typedef Renderer::DrawCalls::JsonAllocator JsonAllocator;
			typedef Memory::Vector<std::string, Memory::Tag::EffectGraphParameterInfos> ParamNames;
			typedef Renderer::DrawCalls::ParamRanges ParamRanges;

			enum class FileRefreshMode
			{
				None,
				Clear,
				Path,
				Export,
			};

		protected:
			UIParameter(void* param_ptr, const ParamNames& names, const ParamRanges& mins, const ParamRanges& maxs, unsigned index) : names(names), mins(mins), maxs(maxs), param_ptr(param_ptr), index(index) {}

		public:
			virtual ~UIParameter() {}

			virtual bool OnRender(float total_width, size_t num_params = 1, bool show_icon = true, bool horizontal_layout = false) = 0;
			virtual bool OnRenderLayoutEditor() { return false; }

			virtual std::optional<std::string_view> GetIcon() const { return std::nullopt; }
			virtual bool HasLayout() const { return false; }
			virtual bool HasEditor() const { return true; }
			virtual bool PreferHorizontalDisplay() const { return false; }
			virtual int GetPriority() const { return 0; }
			virtual GraphType GetType() const = 0;
			virtual std::optional<uint64_t> GetMergeHash() const { return std::nullopt; }

			virtual void GetValue(JsonValue& value, JsonAllocator& allocator) const = 0;
			virtual void SetValue(const JsonValue& value) = 0;

			virtual void GetLayout(JsonValue& value, JsonAllocator& allocator) const {}
			virtual void SetLayout(const JsonValue& value) {}

			virtual FileRefreshMode GetFileRefreshMode() const { return FileRefreshMode::None; }

			void Load(const JsonValue& value) { SetValue(value); SetLayout(value); }
			void Save(JsonValue& value, JsonAllocator& allocator) const { GetValue(value, allocator); GetLayout(value, allocator); }

			void SetUniqueID(size_t _id) { id = _id; }
			void SetData(void* p) { param_ptr = p; }
			void SetIsInstance(bool v) { is_instance = v; }

			const ParamNames& GetNames() const { return names; }
			const ParamRanges& GetMins() const { return mins; }
			const ParamRanges& GetMaxs() const { return maxs; }

			size_t GetUniqueID() const { return id; }
			void* GetData() const { return param_ptr; }
			unsigned GetIndex() const { return index; }

		protected:
			const ParamNames names;
			const ParamRanges mins;
			const ParamRanges maxs;
			const unsigned index;
			size_t id = 0;
			void* param_ptr = nullptr;

			bool is_instance = false;
		};

		std::unique_ptr<UIParameter> CreateParameter(GraphType type, void* param_ptr, const Memory::Vector<std::string, Memory::Tag::EffectGraphParameterInfos>& names, const Renderer::DrawCalls::ParamRanges& mins, const Renderer::DrawCalls::ParamRanges& maxs, unsigned index = 0);
		inline std::unique_ptr<UIParameter> CreateParameter(GraphType type, void* param_ptr, unsigned index) { return CreateParameter(type, param_ptr, {}, {}, {}, index); }
	}
}