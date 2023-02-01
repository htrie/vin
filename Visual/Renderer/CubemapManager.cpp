
#include "Common/File/FileSystem.h"
#include "Common/File/PathHelper.h"

#include "Visual/Device/Constants.h"
#include "Visual/Device/RenderTarget.h"
#include "Visual/Device/Device.h"
#include "Visual/Device/Texture.h"
#include "Visual/Renderer/Scene/Camera.h"
#include "Visual/Renderer/TiledLighting.h"
#include "Visual/Renderer/Scene/Light.h"
#include "Visual/Renderer/Scene/SceneSystem.h"

#include "CubemapManager.h"

namespace Renderer
{
	const simd::vector3 CubemapTask::targets[FACE_COUNT] =
	{
		simd::vector3(1,  0,  0),
		simd::vector3(-1,  0,  0),
		simd::vector3(0,  0, -1),
		simd::vector3(0,  0,  1),
		simd::vector3(0,  1,  0),
		simd::vector3(0, -1,  0),
	};

	const simd::vector3 CubemapTask::up_vectors[FACE_COUNT] =
	{
		simd::vector3(0,  0, -1),
		simd::vector3(0,  0, -1),
		simd::vector3(0, -1,  0),
		simd::vector3(0,  1,  0),
		simd::vector3(0,  0, -1),
		simd::vector3(0,  0, -1),
	};

	void CubemapCamera::Setup(const simd::vector3& position, const simd::matrix& view_matrix, const simd::matrix& proj_matrix)
	{
		UpdateMatrices(view_matrix, proj_matrix, simd::matrix::identity(), position, position, CubemapTask::NEAR_PLANE);
	}

	CubemapTask::CubemapTask(simd::vector3 position, float hor_angle, float vert_angle)
	{
		this->position = position;
		for (int i = 0; i < FACE_COUNT; i++)
		{
			auto hor_rot_matrix = simd::matrix::rotationaxis(simd::vector3(0, 0, -1), hor_angle);
			auto vert_rot_matrix = simd::matrix::rotationaxis(simd::vector3(-1, 0, 0), vert_angle);

			auto combined_matrix = vert_rot_matrix * hor_rot_matrix;

			auto target = combined_matrix * targets[i];
			auto up_vector = combined_matrix * up_vectors[i];

			const auto view_matrix = simd::matrix::lookat(position, position + target, up_vector);
			const auto projection_matrix = simd::matrix::perspectivefov(PIDIV2, 1.0f, NEAR_PLANE, 150000.0f);

			face_cameras[i].Setup(position, view_matrix, projection_matrix);
		}
	}

	CubemapTask::~CubemapTask()
	{
#ifdef WIN32
		if (!result) return;

		result->SaveToFile(L"temp_environment.dds", Device::ImageFileFormat::DDS);

		std::wstring cmft_path = L"./cmftRelease.exe";
		std::wstring params = L"--input temp_environment.dds --output0 temp_environment --outputNum 1 --output0params tga,bgra8,latlong --generateMipChain false --edgeFixup warp --outputGammaDenominator 2.2";

		if (!FileSystem::RunProgram(cmft_path, params, std::wstring(), 5000))
		{
			std::wcerr << L"Failed to convert cubemap to hdr" << std::endl;
		}
#endif
	}

	CubemapManager::CubemapManager()
	{
		for (size_t i = 0; i < tiled_lighting.size(); i++)
			tiled_lighting[i] = Memory::Pointer<TiledLighting, Memory::Tag::Render>::make();
	}

	CubemapManager::~CubemapManager()
	{

	}

	std::unique_ptr<CubemapManager::CubemapRenderTarget> CubemapManager::CreateRenderTarget(unsigned size) const
	{
		auto target = std::make_unique<CubemapManager::CubemapRenderTarget>();

		target->cubemap_texture = Device::Texture::CreateCubeTexture("Cubemap render target color texture", device, size, 1, Device::UsageHint::RENDERTARGET, Device::PixelFormat::A16B16G16R16F, Device::Pool::DEFAULT, true);
		auto depth_texture = Device::Texture::CreateTexture("Cubemap render target depth texture", device, size, size, 1, Device::UsageHint::DEPTHSTENCIL, Device::PixelFormat::D24S8, Device::Pool::DEFAULT, false, false, false);
		target->depth = Device::RenderTarget::Create("Cubemap render target depth", device, std::move(depth_texture), 0);

		for (unsigned int face_id = 0; face_id < CubemapTask::FACE_COUNT; face_id++)
			target->render_targets[face_id] = Device::RenderTarget::Create("Cubemap render target face " + std::to_string(face_id), device, target->cubemap_texture, 0, face_id);

		return target;
	}

	void CubemapManager::OnCreateDevice(Device::IDevice* device)
	{
		this->device = device;
		this->render_target = CreateRenderTarget(512);

		constexpr size_t tex_size = 4;
		constexpr size_t bytes_per_pixel = 4;

		std::array<uint8_t, tex_size* tex_size* bytes_per_pixel> shadowmap_data;
		for (auto& val : shadowmap_data)
			val = std::numeric_limits<uint8_t>::max();

		screenspace_shadowmap_dummy = Device::Texture::CreateTextureFromMemory("GUI Font", device, tex_size, tex_size, shadowmap_data.data(), bytes_per_pixel);

		for (auto& tiled : tiled_lighting)
			tiled->OnCreateDevice(device);
	}

	void CubemapManager::OnDestroyDevice()
	{
		this->render_target = nullptr;
		this->device = nullptr;
		screenspace_shadowmap_dummy.Reset();

		for (auto& tiled : tiled_lighting)
			tiled->OnDestroyDevice();
	}

	void CubemapManager::OnLostDevice()
	{
		for (auto& tiled : tiled_lighting)
			tiled->OnLostDevice();
	}

	void CubemapManager::OnResetDevice(Device::IDevice* device)
	{
		for (auto& tiled : tiled_lighting)
			tiled->OnResetDevice(device);
	}

	void CubemapManager::BindInputs(size_t face_id, Device::BindingInputs& pass_binding_inputs) const
	{
		pass_binding_inputs.push_back(Device::BindingInput(Device::IdHash(DrawDataNames::ScreenspaceShadowmap, 0)).SetTexture(screenspace_shadowmap_dummy));

		tiled_lighting[face_id]->BindInputs(pass_binding_inputs);
	}

	CubemapTask* CubemapManager::Update()
	{
		if (auto* cubemap_task = cubemap_tasks.size() > 0 ? cubemap_tasks.front().get() : nullptr)
		{
			need_recompute_tiles = true;
			for (size_t i = 0; i < CubemapTask::FACE_COUNT; i++)
			{
				const auto point_lights = Renderer::Scene::System::Get().GetVisiblePointLights(cubemap_task->face_cameras[i]);
				tiled_lighting[i]->Update(device, point_lights);
			}
			return cubemap_task;
		}
		return nullptr;
	}

	void CubemapManager::ComputeTiles(Device::CommandBuffer& command_buffer, CubemapTask* cubemap_task)
	{
		if (!need_recompute_tiles)
			return;

		if (cubemap_task)
		{
			for (size_t i = 0; i < CubemapTask::FACE_COUNT; i++)
			{
				tiled_lighting[i]->ComputeTiles(device, cubemap_task->face_cameras[i], command_buffer);
			}
		}

		need_recompute_tiles = false;
	}

	void CubemapManager::PushCubemapTask(simd::vector3 position, float hor_angle, float vert_angle)
	{
		cubemap_tasks.emplace_back(std::make_shared<CubemapTask>(position, hor_angle, vert_angle));
	}

	std::shared_ptr<CubemapTask> CubemapManager::PopCubemapTask()
	{
		std::shared_ptr<CubemapTask> res;
		if (cubemap_tasks.size() > 0)
		{
			res = std::move(cubemap_tasks.front());
			cubemap_tasks.pop_front();
		}
		return res;
	}

}