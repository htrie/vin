#pragma once
#include "Visual/Renderer/DynamicParameterManager.h"

//core functions that operate in game and in tools and do not require a GameObject/ClientInstanceSession/ClientGameWorld

namespace Game
{
	extern const Renderer::DynamicParamFunction core_functions[];
	extern const Renderer::DynamicParamFunction* core_functions_end;
}