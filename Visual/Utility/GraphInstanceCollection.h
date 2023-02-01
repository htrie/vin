#pragma once

#include <rapidjson/document.h>
#include <mutex>

class Material;

namespace Renderer
{
	namespace DrawCalls
	{
		using JsonEncoding = rapidjson::UTF16LE<>;
		using JsonDocument = rapidjson::GenericDocument<JsonEncoding>;
		using JsonAllocator = JsonDocument::AllocatorType;
		using JsonValue = rapidjson::GenericValue<JsonEncoding>;
		class EffectGraphInstance;
		class EffectGraph;
		struct InstanceDesc;
	}
}

namespace EffectGraph
{
	struct Desc;
}

namespace Game
{
	namespace Components
	{
		class Render;
	}

	class ClientGameWorld;
}

namespace Utility
{
	class GraphInstanceCollectionHandler
	{
	public:
		virtual void OnSaveGraphInstance(const Renderer::DrawCalls::InstanceDesc* instance_desc, Renderer::DrawCalls::JsonValue& parent_obj, Renderer::DrawCalls::JsonAllocator& allocator) {};
	};

	class GraphInstanceCollection
	{
	public:
		static void SetHandler(GraphInstanceCollectionHandler* _handler );
		static void OnSaveGraphInstance(const Renderer::DrawCalls::InstanceDesc* instance_desc, Renderer::DrawCalls::JsonValue& parent_obj, Renderer::DrawCalls::JsonAllocator& allocator);

	private:
		static GraphInstanceCollectionHandler* handler;
	};

	//////

	class GraphCollectionHandler
	{
	public:
		virtual void OnAddGraph(Renderer::DrawCalls::EffectGraph* graph) = 0;
		virtual void OnRemoveGraph(Renderer::DrawCalls::EffectGraph* graph) = 0;
	};

	class GraphCollection
	{
	public:
		static void SetHandler(GraphCollectionHandler* _handler );
		static void OnAddGraph(Renderer::DrawCalls::EffectGraph* graph);
		static void OnRemoveGraph(Renderer::DrawCalls::EffectGraph* graph);

	private:
		static GraphCollectionHandler* handler;
	};

	//////

	class MaterialCollectionHandler
	{
	public:
		virtual void OnAddMaterial(Material* material) = 0;
		virtual void OnRemoveMaterial(Material* material) = 0;
	};

	class MaterialCollection
	{
	public:
		static void SetHandler(MaterialCollectionHandler* _handler );
		static void OnAddMaterial(Material* material);
		static void OnRemoveMaterial(Material* material);

	private:
		static MaterialCollectionHandler* handler;
	};

	//////

	class RenderComponentCollectionHandler
	{
	public:
		virtual void OnAddRenderComponent(Game::Components::Render* render) = 0;
		virtual void OnRemoveRenderComponent(Game::Components::Render* render) = 0;
	};

	class RenderComponentCollection
	{
	public:
		static void SetHandler(RenderComponentCollectionHandler* _handler );
		static void OnAddRenderComponent(Game::Components::Render* render);
		static void OnRemoveRenderComponent(Game::Components::Render* render);

	private:
		static RenderComponentCollectionHandler* handler;
	};

}