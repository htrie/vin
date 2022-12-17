namespace Environment
{
	struct EnvParamIds
	{
		enum struct Periodic
		{
			DirectionalLightTheta,	//horizontal angle
			Count
		};
		enum struct Float
		{
			DirectionalLightPhi,	//vertical angle
			DirectionalLightDelta,	//horizontal speed
			DirectionalLightMultiplier,
			DirectionalLightDesaturation,
			DirectionalLightCamFarPercent,
			DirectionalLightCamNearPercent,
			DirectionalLightCamExtraZ,
			DirectionalLightMinOffset,
			DirectionalLightMaxOffset,
			DirectionalLightPenumbraRadius,
			DirectionalLightPenumbraDist,

			PlayerLightIntensity,
			PlayerLightPenumbra,

			PostProcessingBloomCutoff,
			PostProcessingBloomIntensity,
			PostProcessingBloomRadius,
			PostProcessingOriginalIntensity,
			PostProcessingDoFFocusDistance,
			PostProcessingDoFFocusRange,
			PostProcessingDoFBlurScaleBackground,
			PostProcessingDoFBlurScaleForeground,
			PostProcessingDoFTransitionBackground,
			PostProcessingDoFTransitionForeground,

			EnvMappingDirectLightEnvRatio,
			EnvMappingEnvBrightness,
			EnvMappingGiAdditionalEnvLight,
			EnvMappingHorAngle,
			EnvMappingVertAngle,
			EnvMappingSolidSpecularAttenuation,
			EnvMappingWaterSpecularAttenuation,

			SSFColorAlpha,
			SSFLayerCount,
			SSFThickness,
			SSFTurbulence,
			SSFDisperseRadius,
			SSFFeathering,

			GroundScale,
			GroundScrollSpeedX,
			GroundScrollSpeedY,

			WindDirectionAngle,
			WindIntensity,

			RainDist,
			RainAmount,
			RainIntensity,
			RainHorAngle,
			RainVertAngle,
			RainTurbulence,

			CloudsScale,
			CloudsIntensity,
			CloudsMidpoint,
			CloudsSharpness,
			CloudsSpeed,
			CloudsAngle,
			CloudsFadeRadius,
			CloudsPreFade,
			CloudsPostFade,

			WaterCausticsMult,
			WaterDirectionness,
			WaterFlowFoam,
			WaterFlowIntensity,
			WaterFlowTurbulence,
			WaterOpenWaterMurkiness,
			WaterReflectiveness,
			WaterRefractionIndex,
			WaterSubsurfaceScattering,
			WaterTerrainWaterMurkiness,
			WaterWindDirectionAngle,
			WaterWindIntensity,
			WaterWindSpeed,
			WaterSwellAngle,
			WaterSwellHeight,
			WaterSwellIntensity,
			WaterSwellPeriod,

			WaterShorelineOffset,
			WaterHeightOffset,

			CameraFarPlane,
			CameraNearPlane,
			CameraExposure,
			CameraZRotation,

			EffectSpawnerRate,

			AudioVersion,
			AudioListenerHeight,
			AudioListenerOffset,

			GiAmbientOcclusionPower,
			GiIndirectLightArea,
			GiIndirectLightRampup,
			GiThicknessAngle,

			Count
		};
		enum struct Int
		{
			WaterFlowmapResolution,
			WaterShorelineResolution,

			Count
		};
		enum struct Bool
		{
			DirectionalLightShadowsEnabled,
			DirectionalLightCinematicMode,

			PlayerLightShadowsEnabled,

			PostProcessingIsEnabled,
			PostProcessingDepthOfFieldIsEnabled,

			LightningEnabled,

			GiUseForcedScreenspaceShadows,
			WaterWaitForInitialization,

			Count
		};

		enum struct Vector3
		{
			DirectionalLightColour,

			PlayerLightColour,

			DustColor,

			LinearFogColor,
			ExpFogColor,

			WaterOpenWaterColor,
			WaterTerrainWaterColor,

			SSFColor,

			Count
		};

		enum struct String
		{
			EnvMappingDiffuseCubeTex,
			EnvMappingSpecularCubeTex,

			GlobalParticleEffect,
			PlayerEnvAo,

			WaterSplashAo,

			PostTransformTex,

			AudioEnvBankName,
			AudioEnvEventName,
			AudioMusic,
			AudioAmbientSound,
			AudioAmbientSound2,
			AudioAmbientBank,
			AudioEnvAo,

			Count
		};

		enum struct VolumeTex
		{
			ColorGradingTex,
			Count
		};

		enum struct EffectArray
		{
			GlobalEffects,
			Count
		};

		enum struct ObjectArray
		{
			EffectSpawnerObjects,
			Count
		};

		enum struct FootstepArray
		{
			FootstepAudio,
			Count
		};
	};
}