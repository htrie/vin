#pragma once

#include "Common/Utility/Geometric.h"

#include "Visual/Device/Resource.h"
#include "Visual/Device/State.h"
#include "Visual/Renderer/Scene/View.h"
#include "Visual/Renderer/Scene/Camera.h"

namespace Device
{
	class Texture;
	class IDevice;
	class RenderTarget;
}

namespace Renderer
{

	class CubemapCamera;
	class TiledLighting;
	
	class CubemapCamera : public Scene::Camera
	{
	public:
		CubemapCamera() {}

		void Setup(const simd::vector3& position, const simd::matrix& view_matrix, const simd::matrix& proj_matrix);
	};

	struct CubemapTask
	{
		static constexpr size_t FACE_COUNT = 6;
		static constexpr float NEAR_PLANE = 25.0f;
		static const simd::vector3 targets[FACE_COUNT];
		static const simd::vector3 up_vectors[FACE_COUNT];

		CubemapTask(simd::vector3 position, float hor_angle, float vert_angle);
		~CubemapTask();

		simd::vector3 position;

		Device::Handle<Device::Texture> result;
		std::array<CubemapCamera, FACE_COUNT> face_cameras;
	};


	class CubemapManager
	{
	public:

		CubemapManager();
		~CubemapManager();

		struct CubemapRenderTarget
		{
			Device::Handle<Device::Texture> cubemap_texture;
			std::shared_ptr<Device::RenderTarget> render_targets[CubemapTask::FACE_COUNT];
			std::shared_ptr<Device::RenderTarget> depth;
		};

		static bool Enabled()
		{
#if defined(ENVIRONMENT_EDITING_ENABLED)
			return true;
#else
			return false;
#endif
		}

		void BindInputs(size_t face_id, Device::BindingInputs& pass_binding_inputs) const;

		void ComputeTiles(Device::CommandBuffer& command_buffer, CubemapTask* cubemap_task);

		void OnDestroyDevice();
		void OnLostDevice();
		void OnResetDevice(Device::IDevice* device);
		void OnCreateDevice(Device::IDevice* device);

		void PushCubemapTask(simd::vector3 position, float hor_angle, float vert_angle);
		std::shared_ptr<CubemapTask> PopCubemapTask();

		CubemapTask* Update();

		const CubemapRenderTarget* GetRenderTarget() { return render_target.get(); };
		
	private:
		bool need_recompute_tiles = false;
		Device::IDevice* device;
		Device::Handle<Device::Texture> screenspace_shadowmap_dummy;
		std::unique_ptr<CubemapRenderTarget> render_target;
		std::array<Memory::Pointer<TiledLighting, Memory::Tag::Render>, CubemapTask::FACE_COUNT> tiled_lighting; // May need a dynamic array if need to render more than 1 cubemap per frame

		std::deque<std::shared_ptr<CubemapTask>> cubemap_tasks;

		std::unique_ptr<CubemapRenderTarget> CreateRenderTarget(unsigned size) const;
	};

}