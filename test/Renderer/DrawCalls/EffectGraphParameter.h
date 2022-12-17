#pragma once

#include <rapidjson/document.h>
#include "Common/Simd/Curve.h"
#include "Common/FileReader/FFXReader.h"
#include "Visual/Device/Texture.h"
#include "Visual/LUT/LUTSystem.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/State.h"
#include "Visual/Renderer/DrawCalls/DrawDataNames.h"

namespace Renderer
{
	namespace DrawCalls
	{
		using JsonEncoding = rapidjson::UTF16LE<>;
		using JsonDocument = rapidjson::GenericDocument<JsonEncoding>;
		using JsonAllocator = JsonDocument::AllocatorType;
		using JsonValue = rapidjson::GenericValue<JsonEncoding>;
		
		struct UniformInputInfo
		{
			Device::UniformInput input;
			DrawDataNames::DrawDataIdentifiers id = DrawDataNames::DrawDataIdentifiers::None;
			uint8_t index = 0;
		};
		struct BindingInputInfo
		{
			Device::BindingInput input;
			DrawDataNames::DrawDataIdentifiers id = DrawDataNames::DrawDataIdentifiers::None;
			uint8_t index = 0;
		};
		typedef Memory::Vector<UniformInputInfo, Memory::Tag::Device> UniformInputInfos;
		typedef Memory::Vector<BindingInputInfo, Memory::Tag::DrawCalls> BindingInputInfos;

		using GraphType = FileReader::FFXReader::GraphType;

		namespace ParameterUtil
		{
			struct TextureInfo
			{
				std::wstring texture_path;
				Device::TexResourceFormat format;
				bool srgb;
			};

			void Read(const JsonValue& param_obj, float& data);
			void Save(JsonValue& param_obj, JsonAllocator& allocator, const float value, const float default_value);
			void Read(const JsonValue& param_obj, simd::vector4& data, bool& srgb, const unsigned num_elements);
			void Save(JsonValue& param_obj, JsonAllocator& allocator, const simd::vector4& value, const simd::vector4& default_value, const bool srgb, const unsigned num_elements);
			void Read(const JsonValue& param_obj, bool& data);
			void Save(JsonValue& param_obj, JsonAllocator& allocator, const bool value, const bool default_value);
			void Read(const JsonValue& param_obj, int& data);
			void Save(JsonValue& param_obj, JsonAllocator& allocator, const int value, const int default_value);
			void Read(const JsonValue& param_obj, std::wstring& texture_path, Device::TexResourceFormat& format, bool& srgb);
			void Save(JsonValue& param_obj, JsonAllocator& allocator, const std::wstring& texture_path, const Device::TexResourceFormat format, const bool srgb, const bool save_empty_path = false);
			void Read(const JsonValue& param_obj, std::string& data);
			void Save(JsonValue& param_obj, JsonAllocator& allocator, const std::string& value, const std::string& default_value);
			void Read(const JsonValue& param_obj, LUT::Spline& data, float default_value = 1.0f);
			void Save(JsonValue& param_obj, JsonAllocator& allocator, const LUT::Spline& value);

			inline void Read(const JsonValue& param_obj, TextureInfo& info) { Read(param_obj, info.texture_path, info.format, info.srgb); }
			inline void Save(JsonValue& param_obj, JsonAllocator& allocator, const TextureInfo& info, const bool save_empty_path = false) { Save(param_obj, allocator, info.texture_path, info.format, info.srgb, save_empty_path); }

			template<size_t N>
			void Read(const JsonValue& param_obj, simd::GenericCurve<N>& data, float default_value = 0.0f)
			{
				data.points.clear();
				data.variance = 0.0f;

				const JsonValue* points = &param_obj;
				if (param_obj.IsObject())
				{
					if (param_obj.HasMember(L"variance"))
						data.variance = param_obj[L"variance"].GetFloat();

					if (param_obj.HasMember(L"points"))
						points = &param_obj[L"points"];
				}

				if (points->IsArray())
				{
					for (const auto& param_point : points->GetArray())
					{
						if (data.points.full())
							break;

						if (!param_point.HasMember(L"time") || !param_point.HasMember(L"value"))
							continue;

						const auto& time = param_point[L"time"];
						const auto& value = param_point[L"value"];
						if (time.IsNull() || value.IsNull())
							continue;

						data.points.emplace_back();
						data.points.back().time = time.GetFloat();
						data.points.back().value = value.GetFloat();
						if (param_point.HasMember(L"type"))
							if (const auto t = magic_enum::enum_cast<simd::CurveControlPoint::Interpolation>(WstringToString(param_point[L"type"].GetString())))
								data.points.back().type = *t;

						if (data.points.back().type == simd::CurveControlPoint::Interpolation::Custom)
						{
							if (param_point.HasMember(L"left_dt"))	data.points.back().left_dt = param_point[L"left_dt"].GetFloat();
							if (param_point.HasMember(L"right_dt"))	data.points.back().right_dt = param_point[L"right_dt"].GetFloat();
						}
						else if (data.points.back().type == simd::CurveControlPoint::Interpolation::CustomSmooth)
						{
							if (param_point.HasMember(L"dt"))	data.points.back().left_dt = data.points.back().right_dt = param_point[L"dt"].GetFloat();
						}
					}
				}

				data.RescaleTime(0.0f, 1.0f, default_value);
			}

			template<size_t N>
			void Save(JsonValue& param_obj, JsonAllocator& allocator, const simd::GenericCurve<N>& value)
			{
				param_obj.RemoveMember(L"points");
				param_obj.RemoveMember(L"variance");

				auto points = JsonValue(rapidjson::kArrayType);

				for (const auto& point : value.points)
				{
					auto obj = JsonValue(rapidjson::kObjectType);

					obj.AddMember(L"time", JsonValue().SetFloat(point.time), allocator);
					obj.AddMember(L"value", JsonValue().SetFloat(point.value), allocator);
					const auto type = StringToWstring(std::string(magic_enum::enum_name(point.type)));
					obj.AddMember(L"type", JsonValue().SetString(type.c_str(), (int)type.length(), allocator), allocator);

					if (point.type == simd::CurveControlPoint::Interpolation::Custom)
					{
						obj.AddMember(L"left_dt", JsonValue().SetFloat(point.left_dt), allocator);
						obj.AddMember(L"right_dt", JsonValue().SetFloat(point.right_dt), allocator);
					}
					else if (point.type == simd::CurveControlPoint::Interpolation::CustomSmooth)
					{
						obj.AddMember(L"dt", JsonValue().SetFloat(point.left_dt), allocator);
					}

					points.PushBack(obj, allocator);
				}

				param_obj.AddMember(L"points", points, allocator);
				param_obj.AddMember(L"variance", JsonValue().SetFloat(value.variance), allocator);
			}

			template<unsigned N>
			void Read(const JsonValue& param_obj, int(&data)[N], const int default_value)
			{
				if constexpr (N == 1)
				{
					Read(param_obj, data[0]);
					return;
				}

				for (unsigned i = 0; i < N; i++)
					data[i] = default_value;

				if (param_obj.HasMember(L"value"))
				{
					const auto values = param_obj[L"value"].GetArray();
					for (unsigned i = 0; i < N && i < values.Size(); i++)
						data[i] = values[i].GetInt();
				}
			}

			template<unsigned N>
			void Save(JsonValue& param_obj, JsonAllocator& allocator, int const (&value)[N], const int default_value)
			{
				if constexpr (N == 1)
				{
					Save(param_obj, allocator, value[0], default_value);
					return;
				}

				param_obj.RemoveMember(L"value");

				bool is_empty = true;
				for (unsigned i = 0; i < N; ++i)
					if (value[i] != default_value)
						is_empty = false;

				auto values = JsonValue(rapidjson::kArrayType);

				for (unsigned i = 0; i < N; ++i)
					values.PushBack(JsonValue().SetInt(value[i]), allocator);

				if (!is_empty)
					param_obj.AddMember(L"value", values, allocator);
			}
		}

		struct Param
		{
			static constexpr size_t storage_size = std::max({
				sizeof(bool),
				sizeof(int),
				sizeof(unsigned),
				sizeof(float),
				sizeof(simd::vector4),
				sizeof(simd::Spline5)
			});

			std::array<char, storage_size> storage;

			Param()
			{
				storage.fill(0);
			}

			bool* AsBool() { return reinterpret_cast<bool*>(storage.data()); }
			int* AsInt() { return reinterpret_cast<int*>(storage.data()); }
			unsigned* AsUInt() { return reinterpret_cast<unsigned*>(storage.data()); }
			float* AsFloat() { return reinterpret_cast<float*>(storage.data()); }
			simd::vector4* AsVector() { return reinterpret_cast<simd::vector4*>(storage.data()); }
			simd::Spline5* AsSpline5() { return reinterpret_cast<simd::Spline5*>(storage.data()); }

			const bool* AsBool() const { return reinterpret_cast<const bool*>(storage.data()); }
			const int* AsInt() const { return reinterpret_cast<const int*>(storage.data()); }
			const unsigned* AsUInt() const { return reinterpret_cast<const unsigned*>(storage.data()); }
			const float* AsFloat() const { return reinterpret_cast<const float*>(storage.data()); }
			const simd::vector4* AsVector() const { return reinterpret_cast<const simd::vector4*>(storage.data()); }
			const simd::Spline5* AsSpline5() const { return reinterpret_cast<const simd::Spline5*>(storage.data()); }

			void Set(bool b) { *AsBool() = b; }
			void Set(int i) { *AsInt() = i; }
			void Set(unsigned i) { *AsUInt() = i; }
			void Set(float f) { *AsFloat() = f; }
			void Set(const simd::vector3& v) { *AsVector() = simd::vector4(v, 0.0f); }
			void Set(const simd::vector4& v) { *AsVector() = v; }
			void Set(const simd::Spline5& v) { *AsSpline5() = v; }
		};

		struct EffectGraphParameterDynamic
		{
			EffectGraphParameterDynamic( const GraphType& type, const unsigned data_id, const unsigned dynamic_param_id )
				: type( type ), data_id( data_id ), dynamic_param_id( dynamic_param_id  )
			{ }

			GraphType type;
			unsigned data_id = 0;
			unsigned dynamic_param_id = 0;
		};

		typedef Memory::Vector<float, Memory::Tag::EffectGraphParameterInfos> ParamRanges;

		struct EffectGraphParameterStatic
		{
			GraphType type;
			unsigned data_id = 0;
			unsigned num_elements;
			Memory::Vector<std::string, Memory::Tag::EffectGraphParameterInfos> names;
			ParamRanges mins;
			ParamRanges maxs;
			ParamRanges defaults;
			EffectGraphParameterStatic( const GraphType& type, const unsigned data_id, unsigned num_elements, 
				const Memory::Vector<std::string, Memory::Tag::EffectGraphParameterInfos>& names,
				const ParamRanges& mins,
				const ParamRanges& maxs,
				const ParamRanges& defaults)
				: type( type ), data_id( data_id ), num_elements(num_elements), names(names), mins(mins), maxs(maxs), defaults(defaults)
			{ }
		};

		class EffectGraphParameter
		{
			unsigned ComputeParamDataHash() const;
			unsigned ComputeParamTextureHash(const std::wstring& dds_name) const;

		public:
			EffectGraphParameter(const EffectGraphParameterStatic& param_ref);

			void FillFromData(const JsonValue& param_obj);
			void SaveData(JsonValue& param_obj, JsonAllocator& allocator, const bool force_save = false) const;
			UniformInputInfo GetUniformInputInfo() const;
			BindingInputInfo GetBindingInputInfo() const;
			unsigned GetHash() const { return hash; }

			GraphType GetType() const { return param_ref->type; }
			const Memory::Vector<std::string, Memory::Tag::EffectGraphParameterInfos>& GetParameterNames() const { return param_ref->names; }
			const ParamRanges& GetParameterMins() const { return param_ref->mins; }
			const ParamRanges& GetParameterMaxs() const { return param_ref->maxs; }
			const LUT::Spline& GetColorSpline() const { return spline_colour; }
			unsigned GetIndex() const { return index; }
			void SetIndex(int value) { index = value; }
			unsigned GetDataId() const { return param_ref->data_id; }
			void CopyParameter(const EffectGraphParameter& param) { parameter = param.parameter; }
			const Param& GetParameter() const { return parameter; }

		protected:
			const EffectGraphParameterStatic* param_ref = nullptr;
			Param parameter;
			Device::BindingInput binding_parameter;
			Device::TexResourceFormat format = Device::TexResourceFormat::DXT5;
			simd::Curve5 curve;
			LUT::Spline spline_colour;
			unsigned index = -1;
			unsigned hash = 0;
			bool srgb = false;
		};
	}
}