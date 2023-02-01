#include "Common/Utility/StringManipulation.h"

#include "Visual/Renderer/ShaderSystem.h"
#include "Visual/Renderer/RendererSubsystem.h"
#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Renderer/DrawCalls/EffectGraphInstance.h"

#include "TrailsTemplate.h"
#include "Visual/Trails/TrailsTrail.h"
#include "Visual/Trails/TrailSystem.h"

namespace Trails
{
	const wchar_t* UVModeNames[UvMode::NumUvModes] =
	{
		L"Fixed",
		L"Distance",
		L"Time",
		L"None",
	};

	void TrailsTemplate::Write( std::wostream& stream ) const
	{
		const auto nl = L"\r\n";

		stream << L"\tmaterial \"";
		if( !material.IsNull() )
			stream << material.GetFilename();
		stream << L'\"' << nl;
		if (camera_facing) stream << L"\tcamera_facing " << camera_facing << nl;
		if (lock_scale) stream << L"\tlocked_scale " << lock_scale << nl;
		if (reverse_bone_group) stream << L"\treverse_bone_group " << reverse_bone_group << nl;
		if (emit_at_tip) stream << L"\temit_at_tip " << emit_at_tip << L" " << emit_width << nl;
		if (start_distance_threshold >= 0.0f) stream << L"\tstart_distance " << start_distance_threshold << nl;
		if (stop_distance_threshold >= 0.0f) stream << L"\tstop_distance " << stop_distance_threshold << nl;
		stream << L"\tmax_segment_length " << max_segment_length << nl;
		if(update_speed_multiplier_on_death != 1.f) stream << L"\tupdate_speed_multiplier_on_death " << update_speed_multiplier_on_death << nl;
		if (emit_min_speed >= 0.0f) stream << L"\temit_min_speed " << emit_min_speed << nl;
		if (emit_max_speed >= 0.0f) stream << L"\temit_max_speed " << emit_max_speed << nl;
		if (emit_start_speed >= 0.0f) stream << L"\temit_start_speed " << emit_start_speed << nl;
		if (lifetime >= 0.0f) stream << L"\tlifetime " << lifetime << L" " << scale_lifetime << nl;
		if(scale_segment_duration) stream << L"\tscale_segment_duration " << scale_segment_duration << nl;
		if(scale_with_animation) stream << L"\tscale_with_animation " << scale_with_animation << nl;
		if (double_trail) stream << L"\tdouble_trail " << double_trail << nl;
		if (move_standing_uvs) stream << L"\tmove_standing_uvs " << move_standing_uvs << nl;
		stream << L"\tu_tessellation " << u_tessellation << nl;
		stream << L"\tv_tessellation " << v_tessellation << nl;
		if(u_tessellation > 0) stream << L"\tu_spline " << spline_u << nl;
		if(v_tessellation > 0) stream << L"\tv_spline " << spline_v << nl;
		if(spline_u || spline_v) stream << L"\tspline_strength " << spline_strength << nl;
		stream << L"\tlegacy_light " << legacy_light << nl;
		if (ui_name.length() > 0) stream << L"\tui_name \"" << StringToWstring(ui_name) << L"\"" << nl;
		stream << L"\trandomness_frequency " << randomness_frequency << nl;
		stream << L"\ttrail_length " << trail_length << nl;
		stream << L"\ttrail_length_variance " << trail_length_variance << nl;
		stream << L"\tsegment_duration_variance " << segment_duration_variance << nl;
		stream << L"\tduration_length_ratio " << duration_length_ratio << nl;
		stream << L"\tsegment_duration " << segment_duration << nl;
		stream << L"\tsegment_period " << SegmentPeriod << nl;
		stream << L"\ttexture_stretch " << texture_stretch << nl;
		stream << L"\tcolor_r " << color_r << nl;
		stream << L"\tcolor_g " << color_g << nl;
		stream << L"\tcolor_b " << color_b << nl;
		stream << L"\tcolor_a " << color_a << nl;
		stream << L"\tscale " << scale << nl;
		stream << L"\tvelocity_model_x " << velocity_model_x << nl;
		stream << L"\tvelocity_model_y " << velocity_model_y << nl;
		stream << L"\tvelocity_model_z " << velocity_model_z << nl;
		stream << L"\tvelocity_world_x " << velocity_world_x << nl;
		stream << L"\tvelocity_world_y " << velocity_world_y << nl;
		stream << L"\tvelocity_world_z " << velocity_world_z << nl;
		stream << L"\temit_scale " << emit_scale << nl;
		stream << L"\temit_color_r " << emit_color_r << nl;
		stream << L"\temit_color_g " << emit_color_g << nl;
		stream << L"\temit_color_b " << emit_color_b << nl;
		stream << L"\temit_color_a " << emit_color_a << nl;
		stream << L"\tlocked_to_parent " << locked_to_parent << nl;
		stream << L"\tuv_mode " << uv_mode << nl;
	}

	class TrailsTemplate::Reader : public FileReader::TRLReader::Trail
	{
	private:
		TrailsTemplate* parent;

		template<typename T, size_t N> static void CopyPoints(T(&dst)[N], const T(&src)[N])
		{
			for (size_t a = 0; a < N; a++)
				dst[a] = src[a];
		}
	public:
		Reader(TrailsTemplate* parent) : parent(parent) {}

		void SetMaterial(const std::wstring& filename) override
		{
			if(!filename.empty())
			{
				parent->material = MaterialHandle(filename);
				parent->has_normals = parent->material->GetEffectGraphs()[0]->HasTbnNormals();
			}
		}
		void SetData(const FileReader::TRLReader::Data& data) override
		{
			parent->legacy_light = data.legacy_light;
			parent->emit_at_tip = data.emit_at_tip;
			parent->emit_width = data.emit_width;
			parent->camera_facing = data.camera_facing;
			parent->reverse_bone_group = data.reverse_bone_group;
			parent->locked_to_parent = data.locked_to_parent;
			parent->lock_scale = data.locked_scale;
			parent->double_trail = data.double_trail;
			parent->spline_u = data.spline_u && data.u_tessellation > 0;
			parent->spline_v = data.spline_v && data.v_tessellation > 0;
			parent->spline_strength = data.spline_u || data.spline_v ? data.spline_strength : 0.0f;
			parent->move_standing_uvs = data.move_standing_uvs;
			parent->uv_mode = UvMode::Value(data.uv_mode);
			parent->start_distance_threshold = data.start_distance;
			parent->stop_distance_threshold = data.stop_distance;
			parent->max_segment_length = data.max_segment_length;
			parent->trail_length = data.trail_length;
			parent->duration_length_ratio = data.duration_length_ratio;
			parent->update_speed_multiplier_on_death = data.update_speed_multiplier_on_death;
			parent->scale_segment_duration = data.scale_segment_duration;
			parent->texture_stretch = data.texture_stretch;
			parent->color_r = data.color_r;
			parent->color_g = data.color_g;
			parent->color_b = data.color_b;
			parent->color_a = data.color_a;
			parent->scale = data.scale;
			parent->velocity_model_x = data.velocity_model_x;
			parent->velocity_model_y = data.velocity_model_y;
			parent->velocity_model_z = data.velocity_model_z;
			parent->velocity_world_x = data.velocity_world_x;
			parent->velocity_world_y = data.velocity_world_y;
			parent->velocity_world_z = data.velocity_world_z;
			parent->emit_min_speed = data.emit_min_speed;
			parent->emit_max_speed = data.emit_max_speed;
			parent->emit_start_speed = data.emit_start_speed;
			parent->lifetime = data.lifetime;
			parent->scale_lifetime = data.scale_lifetime;
			parent->scale_with_animation = data.scale_with_animation;
			parent->randomness_frequency = data.randomness_frequency;
			parent->trail_length_variance = data.trail_length_variance;
			parent->segment_duration_variance = data.segment_duration_variance;
			parent->u_tessellation = std::min(data.u_tessellation, unsigned(15));
			parent->v_tessellation = std::min(data.v_tessellation, unsigned(15));
			parent->ui_name = data.ui_name;

			parent->color_r.RescaleTime(0.0f, 1.0f);
			parent->color_g.RescaleTime(0.0f, 1.0f);
			parent->color_b.RescaleTime(0.0f, 1.0f);
			parent->color_a.RescaleTime(0.0f, 1.0f);
			parent->scale.RescaleTime(0.0f, 1.0f);
			parent->velocity_model_x.RescaleTime(0.0f, 1.0f);
			parent->velocity_model_y.RescaleTime(0.0f, 1.0f);
			parent->velocity_model_z.RescaleTime(0.0f, 1.0f);
			parent->velocity_world_x.RescaleTime(0.0f, 1.0f);
			parent->velocity_world_y.RescaleTime(0.0f, 1.0f);
			parent->velocity_world_z.RescaleTime(0.0f, 1.0f);

			parent->emit_color_r = data.emit_color_r;
			parent->emit_color_g = data.emit_color_g;
			parent->emit_color_b = data.emit_color_b;
			parent->emit_color_a = data.emit_color_a;
			parent->emit_scale = data.emit_scale;
			parent->segment_duration = data.segment_duration;

			parent->emit_color_r.RescaleTime(0.0f, 1.0f);
			parent->emit_color_g.RescaleTime(0.0f, 1.0f);
			parent->emit_color_b.RescaleTime(0.0f, 1.0f);
			parent->emit_color_a.RescaleTime(0.0f, 1.0f);
			parent->emit_scale.RescaleTime(0.0f, 1.0f);

			parent->enabled = true;
		}
	};

	Resource::UniquePtr<FileReader::TRLReader::Trail> TrailsTemplate::GetReader(Resource::Allocator& allocator)
	{
		return Resource::UniquePtr<FileReader::TRLReader::Trail>(allocator.Construct<Reader>(this).release());
	}

	void TrailsTemplate::SetToDefaults()
	{
		segment_duration = 1.0f;
		start_distance_threshold = -1.f;
		stop_distance_threshold = -1.f;
		max_segment_length = 40.f;
		trail_length = 0.f;
		duration_length_ratio = 0.0f;
		update_speed_multiplier_on_death = 1.f;
		scale_segment_duration = false;
		texture_stretch = 1.0f;
		emit_min_speed = -1.0f;
		emit_max_speed = -1.0f;
		emit_start_speed = -1.0f;
		lifetime = -1.0f;
		scale_lifetime = false;
		scale_with_animation = false;
		emit_at_tip = false;
		legacy_light = false;
		double_trail = false;
		move_standing_uvs = false;
		emit_width = 0.0f;
		randomness_frequency = 0.1f;
		segment_duration_variance = 0.0f;
		trail_length_variance = 0.0f;
		u_tessellation = 4;
		v_tessellation = 0;
		spline_u = u_tessellation > 0;
		spline_v = v_tessellation > 0;

		color_r.FromConstant(1.0f);
		color_g.FromConstant(1.0f);
		color_b.FromConstant(1.0f);
		color_a.FromConstant(1.0f);
		scale.FromConstant(1.0f);
		velocity_model_x.FromConstant(0.0f);
		velocity_model_y.FromConstant(0.0f);
		velocity_model_z.FromConstant(0.0f);
		velocity_world_x.FromConstant(0.0f);
		velocity_world_y.FromConstant(0.0f);
		velocity_world_z.FromConstant(0.0f);

		emit_color_r.FromConstant(1.0f);
		emit_color_g.FromConstant(1.0f);
		emit_color_b.FromConstant(1.0f);
		emit_color_a.FromConstant(1.0f);
		emit_scale.FromConstant(1.0f);
		camera_facing = false;

		locked_to_parent = false;
		uv_mode = UvMode::None;

		enabled = true;
	}

	unsigned TrailsTemplate::GetMaxSegments( ) const
	{
		const auto time = segment_duration + segment_duration_variance;
		return unsigned( time / SegmentPeriod ) + 2 + 2; //Plus 2 because you at least need one at the start and the end of the trail, Plus 2 because we keep the last two 'dead' segment around for interpolation
	}

	VertexType TrailsTemplate::GetVertexType(bool force_camera_facing) const
	{
		if (camera_facing || force_camera_facing)
			return has_normals || !legacy_light ? VertexType::VERTEX_TYPE_CAM_FACING : VertexType::VERTEX_TYPE_CAM_FACING_NO_LIGHTING;

		return has_normals || !legacy_light ? VertexType::VERTEX_TYPE_DEFAULT : VertexType::VERTEX_TYPE_DEFAULT_NO_LIGHTING;
	}

	unsigned TrailsTemplate::GetTessellationU() const
	{
	#ifdef MOBILE
		return u_tessellation / 2;
	#else
		return u_tessellation;
	#endif
	}

	unsigned TrailsTemplate::GetTessellationV() const
	{
	#if defined(CONSOLE) || defined(MOBILE)
		return v_tessellation / 2;
	#else
		return v_tessellation;
	#endif
	}

	void TrailsTemplate::Warmup() const // IMPORTANT: Keep in sync with TrailsEffectInstance::CreateDrawCalls. // [TODO] Share.
	{
		if (material.IsNull())
			return;

		auto shader_base = Shader::Base()
			.AddEffectGraphs({ L"Metadata/EngineGraphs/base.fxgraph" }, 0);

		auto renderer_base = Renderer::Base()
			.SetCullMode(Device::CullMode::NONE)
			.SetVertexElements(vertex_elements);

		switch (GetVertexType())
		{
			case Trails::VertexType::VERTEX_TYPE_CAM_FACING_NO_LIGHTING:
				shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/disable_lighting.fxgraph" }, 0);
				//Fall through
			case Trails::VertexType::VERTEX_TYPE_CAM_FACING:
				shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/trail_camerafacing.fxgraph" }, 0);
				break;
			case Trails::VertexType::VERTEX_TYPE_DEFAULT_NO_LIGHTING:
				shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/disable_lighting.fxgraph" }, 0);
				//Fall through
			case Trails::VertexType::VERTEX_TYPE_DEFAULT:
				shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/trail_default.fxgraph" }, 0);
				break;
			default:
				assert(false);
				break;
		}

		switch (uv_mode)
		{
			case Trails::UvMode::Distance:
				shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/trail_uv_distance.fxgraph" }, 0);
				break;
			case Trails::UvMode::Time:
				shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/trail_uv_time.fxgraph" }, 0);
				break;
			case Trails::UvMode::Fixed:
				shader_base.AddEffectGraphs({ L"Metadata/EngineGraphs/trail_uv_fixed.fxgraph" }, 0);
				break;
			default:
				break;
		}

		shader_base.AddEffectGraphs(material->GetEffectGraphFilenames(), 0);
		
		Shader::System::Get().Warmup(shader_base);

		renderer_base.SetShaderBase(shader_base);
		Renderer::System::Get().Warmup(renderer_base);
	}

}
