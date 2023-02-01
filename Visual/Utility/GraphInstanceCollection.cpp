#include "Visual/Renderer/EffectGraphSystem.h"
#include "Visual/Material/Material.h"
#include "ClientCommon/Game/Components/Render.h"
#include "ClientCommon/Game/ClientGameWorld.h"

#include "GraphInstanceCollection.h"

namespace Utility
{
	GraphInstanceCollectionHandler* GraphInstanceCollection::handler = nullptr;
	GraphCollectionHandler* GraphCollection::handler = nullptr;
	MaterialCollectionHandler* MaterialCollection::handler = nullptr;
	RenderComponentCollectionHandler* RenderComponentCollection::handler = nullptr;

	void GraphInstanceCollection::SetHandler(GraphInstanceCollectionHandler* _handler )
	{
		handler = _handler;
	}

	void GraphInstanceCollection::OnSaveGraphInstance(const Renderer::DrawCalls::InstanceDesc* instance_desc, Renderer::DrawCalls::JsonValue& parent_obj, Renderer::DrawCalls::JsonAllocator& allocator)
	{
		if (handler)
			handler->OnSaveGraphInstance(instance_desc, parent_obj, allocator);
	}

	void GraphCollection::SetHandler(GraphCollectionHandler* _handler)
	{
		handler = _handler;
	}

	void GraphCollection::OnAddGraph(Renderer::DrawCalls::EffectGraph* graph)
	{
		if(handler)
			handler->OnAddGraph(graph);
	}

	void GraphCollection::OnRemoveGraph(Renderer::DrawCalls::EffectGraph* graph)
	{
		if(handler)
			handler->OnRemoveGraph(graph);
	}

	void MaterialCollection::OnAddMaterial(Material* material)
	{
		if(handler)
			handler->OnAddMaterial(material);
	}

	void MaterialCollection::OnRemoveMaterial(Material* material)
	{
		if(handler)
			handler->OnRemoveMaterial(material);
	}

	void MaterialCollection::SetHandler(MaterialCollectionHandler* _handler)
	{
		handler = _handler;
	}

	void RenderComponentCollection::OnAddRenderComponent(Game::Components::Render* render)
	{
		if(handler)
			handler->OnAddRenderComponent(render);
	}

	void RenderComponentCollection::OnRemoveRenderComponent(Game::Components::Render* render)
	{
		if(handler)
			handler->OnRemoveRenderComponent(render);
	}

	void RenderComponentCollection::SetHandler(RenderComponentCollectionHandler* _handler)
	{
		handler = _handler;
	}

}