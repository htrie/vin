#include <iomanip>

#include "Common/Utility/Counters.h"
#include "Common/Utility/StringManipulation.h"

#include "Visual/Utility/DXHelperFunctions.h"
#include "Visual/Renderer/GlobalSamplersReader.h"

#include "EffectGraphParameter.h"

#include "Include/magic_enum/magic_enum.hpp"

namespace Renderer
{
	namespace DrawCalls
	{
		namespace ParameterUtil
		{
			void Read(const JsonValue& param_obj, float& data)
			{
				if (param_obj.HasMember(L"value"))
					data = param_obj[L"value"].GetFloat();
			}

			void Save(JsonValue& param_obj, JsonAllocator& allocator, const float value, const float default_value)
			{
				param_obj.RemoveMember(L"value");
				if (value != default_value)
					param_obj.AddMember(L"value", JsonValue().SetFloat(value), allocator);
			}

			void Read(const JsonValue& param_obj, simd::vector4& data, bool& srgb, const unsigned num_elements)
			{
				if (num_elements == 1)
				{
					Read(param_obj, data.x);
					srgb = false;
				}
				else if(param_obj.HasMember(L"value"))
				{
					if (const auto& obj = param_obj[L"value"]; obj.IsArray())
					{
						const auto& values = obj.GetArray();
						if (num_elements != values.Size())
							throw std::runtime_error("Number of parameters does not match number of values in Float4 parameter");

						for (unsigned i = 0; i < (unsigned)values.Size(); ++i)
							data[i] = values[i].GetFloat();
					}

					if (param_obj.HasMember(L"srgb"))
						srgb = param_obj[L"srgb"].GetBool();
					else
						srgb = false;
				}
			}

			void Save(JsonValue& param_obj, JsonAllocator& allocator, const simd::vector4& value, const simd::vector4& default_value, const bool srgb, const unsigned num_elements)
			{
				if (num_elements == 1)
					Save(param_obj, allocator, value.x, default_value.x);
				else
				{
					param_obj.RemoveMember(L"value");

					JsonValue vector_obj(rapidjson::kArrayType);
					if (value != default_value)
					{
						for (unsigned i = 0; i < num_elements; ++i)
							vector_obj.PushBack(JsonValue().SetFloat(value[i]), allocator);
					}
					if (!vector_obj.Empty())
						param_obj.AddMember(L"value", vector_obj, allocator);

					if (srgb)
						param_obj.AddMember(L"srgb", JsonValue().SetBool(srgb), allocator);
				}
			}

			void Read(const JsonValue& param_obj, bool& data)
			{
				if (param_obj.HasMember(L"value"))
					data = param_obj[L"value"].GetBool();
			}

			void Save(JsonValue& param_obj, JsonAllocator& allocator, const bool value, const bool default_value)
			{
				param_obj.RemoveMember(L"value");
				if (value != default_value)
					param_obj.AddMember(L"value", JsonValue().SetBool(value), allocator);
			}

			void Read(const JsonValue& param_obj, int& data)
			{
				if (param_obj.HasMember(L"value"))
					data = param_obj[L"value"].GetInt();
			}

			void Save(JsonValue& param_obj, JsonAllocator& allocator, const int value, const int default_value)
			{
				param_obj.RemoveMember(L"value");
				if (value != default_value)
					param_obj.AddMember(L"value", JsonValue().SetInt(value), allocator);
			}

			void Read(const JsonValue& param_obj, std::wstring& texture_path, Device::TexResourceFormat& format, bool& srgb)
			{
				if (param_obj.HasMember(L"path"))
					texture_path = param_obj[L"path"].GetString();

				if (param_obj.HasMember(L"format"))
					format = Utility::StringToTexResourceFormat(WstringToString(param_obj[L"format"].GetString()));
				else
					format = Device::TexResourceFormat::DXT5;

				if (param_obj.HasMember(L"srgb"))
					srgb = param_obj[L"srgb"].GetBool();
			}

			void Save(JsonValue& param_obj, JsonAllocator& allocator, const std::wstring& texture_path, const Device::TexResourceFormat format, const bool srgb, const bool save_empty_path /*= false*/)
			{
				param_obj.RemoveMember(L"path");
				param_obj.RemoveMember(L"format");
				param_obj.RemoveMember(L"srgb");

				if (!texture_path.empty() || save_empty_path)
					param_obj.AddMember(L"path", JsonValue().SetString(texture_path.c_str(), (int)texture_path.length(), allocator), allocator);

				const auto default_format = Device::TexResourceFormat::DXT5;
				if (format != default_format)
				{
					const auto format_name = StringToWstring(Utility::TexResourceFormats[(unsigned)format].name);
					param_obj.AddMember(L"format", JsonValue().SetString(format_name.c_str(), (int)format_name.length(), allocator), allocator);
				}

				param_obj.AddMember(L"srgb", JsonValue().SetBool(srgb), allocator);
			}

			void Read(const JsonValue& param_obj, std::string& data)
			{
				if (param_obj.HasMember(L"value"))
					data = WstringToString(param_obj[L"value"].GetString());
			}

			void Save(JsonValue& param_obj, JsonAllocator& allocator, const std::string& value, const std::string& default_value)
			{
				param_obj.RemoveMember(L"value");

				if (!value.empty() && value != default_value)
				{
					const auto sampler = StringToWstring(value);
					param_obj.AddMember(L"value", JsonValue().SetString(sampler.c_str(), (int)sampler.length(), allocator), allocator);
				}
			}

			void Read(const JsonValue& param_obj, LUT::Spline& data, float default_value)
			{
				if (param_obj.HasMember(L"R")) Read(param_obj[L"R"], data.r, default_value);
				if (param_obj.HasMember(L"G")) Read(param_obj[L"G"], data.g, default_value);
				if (param_obj.HasMember(L"B")) Read(param_obj[L"B"], data.b, default_value);
				if (param_obj.HasMember(L"A")) Read(param_obj[L"A"], data.a, default_value);
				data.Update();
			}

			void Save(JsonValue& param_obj, JsonAllocator& allocator, const LUT::Spline& value)
			{
				param_obj.RemoveMember(L"R");
				param_obj.RemoveMember(L"G");
				param_obj.RemoveMember(L"B");
				param_obj.RemoveMember(L"A");

				auto r = JsonValue(rapidjson::kObjectType); Save(r, allocator, value.r);
				auto g = JsonValue(rapidjson::kObjectType); Save(g, allocator, value.g);
				auto b = JsonValue(rapidjson::kObjectType); Save(b, allocator, value.b);
				auto a = JsonValue(rapidjson::kObjectType); Save(a, allocator, value.a);

				param_obj.AddMember(L"R", r, allocator);
				param_obj.AddMember(L"G", g, allocator);
				param_obj.AddMember(L"B", b, allocator);
				param_obj.AddMember(L"A", a, allocator);
			}
		}

		EffectGraphParameter::EffectGraphParameter(const EffectGraphParameterStatic& param_ref)
			: param_ref(&param_ref)
		{}

		unsigned EffectGraphParameter::ComputeParamDataHash() const
		{
			auto value_hash = MurmurHash2(&parameter, sizeof(parameter), 0x34322);
			unsigned signature[2] = { param_ref->data_id, value_hash };
			return MurmurHash2(signature, sizeof(signature), 0x34322);
		}

		unsigned EffectGraphParameter::ComputeParamTextureHash(const std::wstring& dds_name) const
		{
			const auto value_hash = MurmurHash2(dds_name.c_str(), (int)(dds_name.length() * sizeof(wchar_t)), 0x34322);
			const unsigned signature[4] = { param_ref->data_id, (unsigned)format, srgb ? 1u : 0u, value_hash };
			return MurmurHash2(signature, sizeof(signature), 0x34322);
		}

		void EffectGraphParameter::FillFromData(const JsonValue& param_obj)
		{
			PROFILE_ONLY(Memory::StackTag stack(Memory::Tag::EffectGraphNodes));

			auto type = GetType();
			switch (type)
			{
			case GraphType::Bool:
			{
				bool result = param_ref->defaults[0] > 0.f;
				ParameterUtil::Read(param_obj, result);
				parameter.Set(result);
				hash = ComputeParamDataHash();
				break;
			}
			case GraphType::Int:
			{
				int result = (int)param_ref->defaults[0];
				ParameterUtil::Read(param_obj, result);
				parameter.Set(result);
				hash = ComputeParamDataHash();
				break;
			}
			case GraphType::UInt:
			{
				int result = (int)param_ref->defaults[0];
				ParameterUtil::Read(param_obj, result);
				parameter.Set(std::max(0, result));
				hash = ComputeParamDataHash();
				break;
			}
			case GraphType::Float:
			{
				float result = param_ref->defaults[0];
				ParameterUtil::Read(param_obj, result);
				parameter.Set(result);
				hash = ComputeParamDataHash();
				break;
			}
			case GraphType::Float2:
			case GraphType::Float3:
			case GraphType::Float4:
			{
				simd::vector4 result(0.0f);
				for (unsigned i = 0; i < param_ref->num_elements; ++i)
					result[i] = param_ref->defaults[i];
				unsigned num_elements = param_ref->num_elements;
				assert(num_elements > 1);
				ParameterUtil::Read(param_obj, result, srgb, num_elements);
				parameter.Set(result);
				hash = ComputeParamDataHash();
				break;
			}
			case GraphType::Texture:
			case GraphType::Texture3d:
			case GraphType::TextureCube:
			{
				srgb = true;

				std::wstring dds_name;
				ParameterUtil::Read(param_obj, dds_name, format, srgb);
				if (!dds_name.empty())
				{
					if (param_ref->type == GraphType::Texture)
					{
						if (srgb)
							binding_parameter.SetTextureResource(::Texture::Handle(::Texture::SRGBTextureDesc(dds_name)));
						else
							binding_parameter.SetTextureResource(::Texture::Handle(::Texture::LinearTextureDesc(dds_name)));
					}
					else if (param_ref->type == GraphType::Texture3d)
						binding_parameter.SetTextureResource(::Texture::Handle(::Texture::VolumeTextureDesc(dds_name, srgb)));
					else if (param_ref->type == GraphType::TextureCube)
						binding_parameter.SetTextureResource(::Texture::Handle(::Texture::CubeTextureDesc(dds_name, srgb)));
				}
				else
					binding_parameter.SetTextureResource(::Texture::Handle());
				hash = ComputeParamTextureHash(dds_name);
				break;
			}
			case GraphType::Sampler:
			{
				std::string result;
				ParameterUtil::Read(param_obj, result);
				if (!result.empty())
				{
					const auto& samplers = GetGlobalSamplersReader().GetSamplers();
					for (unsigned index = 0; index < samplers.size(); ++index)
					{
						if (samplers[index].name == result)
						{
							parameter.Set((int)index);
							break;
						}
					}
				}
				else
					parameter.Set((int)0);
				break;
			}
			case GraphType::SplineColour:
			{
				ParameterUtil::Read(param_obj, spline_colour);
				parameter.Set(spline_colour.Fetch());
				break;
			}
			case GraphType::Spline5:
			{
				ParameterUtil::Read(param_obj, curve);
				parameter.Set(curve.MakeNormalizedTrack());
				hash = ComputeParamDataHash();
				break;
			}
			}
		}

		void EffectGraphParameter::SaveData(JsonValue& param_obj, JsonAllocator& allocator, const bool force_save /*= false*/) const
		{
			auto type = GetType();
			switch (type)
			{
			case GraphType::Bool:
			{
				const auto bool_value = *parameter.AsBool();
				const auto default_value = force_save ? !bool_value : param_ref->defaults[0] > 0.f;
				ParameterUtil::Save(param_obj, allocator, bool_value, default_value);
				break;
			}
			case GraphType::Int:
			{
				const auto default_value = force_save ? INT_MAX : (int)param_ref->defaults[0];
				ParameterUtil::Save(param_obj, allocator, *parameter.AsInt(), default_value);
				break;
			}
			case GraphType::UInt:
			{
				const auto default_value = force_save ? INT_MAX : (int)param_ref->defaults[0];
				ParameterUtil::Save(param_obj, allocator, int(*parameter.AsUInt()), default_value);
				break;
			}
			case GraphType::Float:
			{
				const auto default_value = force_save ? FLT_MAX : param_ref->defaults[0];
				auto value = *parameter.AsFloat();
				ParameterUtil::Save(param_obj, allocator, value, default_value);
				break;
			}
			case GraphType::Float2:
			case GraphType::Float3:
			case GraphType::Float4:
			{
				unsigned num_elements = param_ref->num_elements;
				assert(num_elements > 1);
				simd::vector4 default_value(0.0f);
				if (force_save)
					default_value = simd::vector4(FLT_MAX);
				else
				{
					for (unsigned i = 0; i < num_elements; ++i)
						default_value[i] = param_ref->defaults[i];
				}
				JsonValue vector_obj(rapidjson::kArrayType);
				ParameterUtil::Save(param_obj, allocator, *parameter.AsVector(), default_value, srgb, num_elements);
				break;
			}
			case GraphType::Texture:
			case GraphType::Texture3d:
			case GraphType::TextureCube:
			{
				ParameterUtil::Save(param_obj, allocator, binding_parameter.texture_handle.GetFilename(), format, srgb);
				break;
			}
			case GraphType::Sampler:
			{
				const auto& samplers = GetGlobalSamplersReader().GetSamplers();
				const auto default_sampler = (!force_save && samplers.size() > 0) ? samplers[0].name : "";
				const auto index = *parameter.AsInt();
				ParameterUtil::Save(param_obj, allocator, samplers[index].name, default_sampler);
				break;
			}
			case GraphType::SplineColour:
			{
				ParameterUtil::Save(param_obj, allocator, spline_colour);
				break;
			}
			case GraphType::Spline5:
			{
				ParameterUtil::Save(param_obj, allocator, curve);
				break;
			}
			}
		}

		UniformInputInfo EffectGraphParameter::GetUniformInputInfo() const
		{
			auto type = GetType();
			switch (type)
			{
			case GraphType::Bool: return { ::Device::UniformInput(0).SetBool(parameter.AsBool()), (DrawDataNames::DrawDataIdentifiers)param_ref->data_id, 0 };
			case GraphType::Int: return { ::Device::UniformInput(0).SetInt(parameter.AsInt()), (DrawDataNames::DrawDataIdentifiers)param_ref->data_id, 0 };
			case GraphType::UInt: return { ::Device::UniformInput(0).SetUInt(parameter.AsUInt()), (DrawDataNames::DrawDataIdentifiers)param_ref->data_id, 0 };
			case GraphType::Float: return { ::Device::UniformInput(0).SetFloat(parameter.AsFloat()), (DrawDataNames::DrawDataIdentifiers)param_ref->data_id, 0 };
			case GraphType::SplineColour:
			case GraphType::Float2:
			case GraphType::Float3:
			case GraphType::Float4: return { ::Device::UniformInput(0).SetVector(parameter.AsVector()), (DrawDataNames::DrawDataIdentifiers)param_ref->data_id, 0 };
			case GraphType::Spline5: return { ::Device::UniformInput(0).SetSpline5(parameter.AsSpline5()), (DrawDataNames::DrawDataIdentifiers)param_ref->data_id, 0 };
			default: return UniformInputInfo();
			}
		}

		BindingInputInfo EffectGraphParameter::GetBindingInputInfo() const
		{
			auto type = GetType();
			switch (type)
			{
			case GraphType::Texture:
			case GraphType::Texture3d:
			case GraphType::TextureCube: return { binding_parameter, (DrawDataNames::DrawDataIdentifiers)param_ref->data_id, 0 };
			default: return BindingInputInfo();
			}
		}

	}
}