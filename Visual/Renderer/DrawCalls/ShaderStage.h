#pragma once

namespace Renderer
{
	namespace DrawCalls
	{
		///Used to indicate stages of shaders.
		enum ShaderStage
		{
			VertexStart,
			///During this stage, vectors are in model space.
			///This stage can be turned off.
			VertexModelSpace, 
			///During this stage, vectors are in world space.
			VertexWorldSpace,
			VertexEnd,
			///Used for fragments that needs to modify the post-projected position
			VertexPostEnd,

			//temporary
			PixelStartInit,

			PixelStart,

			///Fragments in this shader stage are only used in shadow maps
			PixelStartShadowOnly,
			PixelColourShadowOnly,
			PixelPreLightingShadowOnly,
			PixelEndShadowOnly,

			///The pre lit colour can be modified in this stage
			PixelColour,

			///temporary
			PixelColour2,

			PixelPreLighting,

			///The stage where alpha test clipping will be done, everything past this won't have an effect to alpha testing
			PixelClip,

			///Stage where all lighting parameters are initialized to be changed in later stages.
			PixelLightingInit,

			///Lighting stage for standard phong lighting and order-independent effects
			PixelLightingPhong,

			///Lighting stage for standard phong lighting and order-independent effects
			PixelLightingEnvironment,

			///Lighting stage that always runs after standard lighting but before applying it
			PixelLightingCustom,

			///Lighting stage applies lighting result to the fragment
			PixelLightingEnd,

			///The post lighting colour can be modified in this stage.
			PixelPostLighting,

			PixelFog,
			PixelPostFog,

			PixelEnd,

			NumShaderStages
		};
	}
}