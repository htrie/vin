#pragma once

#include "Common/Utility/Bitset.h"
#include "Common/FileReader/FFXReader.h"
#include "Visual/Renderer/DrawCalls/Uniform.h"

namespace Game { class GameObject; }
namespace Animation { class AnimatedObject; }

namespace Renderer
{
	class FragmentLinker;

	namespace DrawCalls
	{
		using GraphType = FileReader::FFXReader::GraphType;
		class EffectGraph;
	}

	struct DynamicParam
	{
		std::array<char, sizeof(simd::matrix)> storage; // bool, int, float, simd::vector, simd::matrix

		DynamicParam() noexcept
		{
			storage.fill( 0 );
		}

		bool* AsBool() { return (bool*)storage.data(); }
		int* AsInt() { return (int*)storage.data(); }
		float* AsFloat() { return (float*)storage.data(); }
		simd::vector4* AsVector() { return (simd::vector4*)storage.data(); }
		simd::matrix* AsMatrix() { return (simd::matrix*)storage.data(); }

		void Set(bool b) { *AsBool() = b; }
		void Set(int i) { *AsInt() = i; }
		void Set(float f) { *AsFloat() = f; }
		void Set(const simd::vector3& v) { *AsVector() = simd::vector4(v, 0.0f); }
		void Set(const simd::vector4& v) { *AsVector() = v; }
		void Set(const simd::matrix& m) { *AsMatrix() = m; }
	};

	struct DynamicParamFunction
	{
		typedef void(*Func)(const Game::GameObject* object, const ::Animation::AnimatedObject& animated_object, DynamicParam& data);
		DynamicParamFunction(const std::string& name, Func f) : name(name), func(f) { }
		const std::string name;
		Func func;
	};

	struct DynamicParameterInfo
	{
		std::string name;
		DrawCalls::GraphType data_type;
		unsigned data_id = 0;
		
		enum FlagTypes
		{
			CacheData,
			UpdatedExternally,
			NumFlags,
		};
		
		typedef Utility::Bitset< NumFlags > Flags;
		Flags flags;

		DynamicParamFunction::Func func = nullptr;
		DynamicParameterInfo( const std::string& name, DrawCalls::GraphType data_type, const DynamicParameterInfo::Flags& flags );
	};

	class DynamicParameterManager
	{
	public:
		DynamicParameterManager();

		void RegisterFunctions(const DynamicParamFunction* begin, const DynamicParamFunction* end);

		typedef Memory::FlatMap<unsigned, DynamicParameterInfo, Memory::Tag::DrawCalls> DynamicParameterMap;
		DynamicParameterMap::iterator begin() { return dynamic_parameter_infos.begin(); }
		DynamicParameterMap::const_iterator begin() const { return dynamic_parameter_infos.cbegin(); }
		DynamicParameterMap::iterator end() { return dynamic_parameter_infos.end(); }
		DynamicParameterMap::const_iterator end() const { return dynamic_parameter_infos.cend(); }

		const DynamicParameterInfo* GetParamInfo(unsigned hash) const;	

	private:
		DynamicParameterMap dynamic_parameter_infos;
		void AddParamInfo(const std::string& name, const DrawCalls::GraphType type, const DynamicParameterInfo::Flags& flags );
	};

	DynamicParameterManager& GetDynamicParameterManager();


	struct DynamicParameter
	{
		unsigned id = 0;
		DrawCalls::GraphType type = (DrawCalls::GraphType)0;
		DynamicParam* param = nullptr;

		DynamicParameter(const unsigned id, const DrawCalls::GraphType type, DynamicParam* param)
			: id(id), type(type), param(param) {}

		uint64_t ToIdHash() const;
		DrawCalls::Uniform ToUniform() const;
	};

	typedef Memory::SmallVector<DynamicParameter, 2, Memory::Tag::DrawCalls> DynamicParameters;


	class DynamicParameterLocalStorage
	{
		struct ParamInfo
		{
			const DynamicParameterInfo* info = nullptr;
			DynamicParam param;
			bool has_been_updated = false;

			ParamInfo() noexcept {}
			ParamInfo(const DynamicParameterInfo* info) : info(info) {}
		};

		Memory::Map<unsigned, ParamInfo, Memory::Tag::DrawCalls> param_infos; // Note: Do not change from using Map as we need to preserve the addresses due to draw data references

		struct CustomParamInfo
		{
			unsigned id = 0;
			DynamicParam param;

			CustomParamInfo() {}
			CustomParamInfo(const unsigned id)
				: id(id) {}
		};

		Memory::Map<unsigned, CustomParamInfo, Memory::Tag::DrawCalls> custom_param_infos; // Note: Do not change from using Map as we need to preserve the addresses due to draw data references

	public:
		void Clear()
		{
			param_infos.clear();
			custom_param_infos.clear();
		}

		void Append(Renderer::DynamicParameters& dynamic_parameters, const DrawCalls::EffectGraph& graph);

		template <typename F>
		void Process( F func )
		{
			for ( auto itr = param_infos.begin(); itr != param_infos.end(); ++itr )
				func( itr->second );
		}

		template <typename F>
		void ProcessCustom(F func)
		{
			for (auto itr = custom_param_infos.begin(); itr != custom_param_infos.end(); ++itr)
				func(itr->second);
		}
	};

}

