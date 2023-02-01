// Parameter name |  draw call data type  |  gameplay data type  |   flags (optional, see DynamicParameterManager.h - DynamicParameterInfo::FlagTypes)

#define CORE_FUNCTIONS\
	X( AnimationData, DrawCalls::GraphType::Float4, simd::vector4 )\

#define CLIENT_FUNCTIONS\
	X( LifeManaShield, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( ParentLifeManaShield, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( MovementSpeed, DrawCalls::GraphType::Float, float )\
	X( PositionOfMonolith, DrawCalls::GraphType::Float3, simd::vector3 )\
	X( MonolithCrystalDirection, DrawCalls::GraphType::Float3, simd::vector3 )\
	X( CharacterClassIndex, DrawCalls::GraphType::Int, int, DynamicParameterInfo::CacheData )\
	X( ObjectSeed, DrawCalls::GraphType::Float, float )\
	X( AttachedObjectSeed, DrawCalls::GraphType::Float, float )\
	X( PlayerTrailingGhost, DrawCalls::GraphType::Float3, simd::vector3 )\
	X( LocalPlayerPosition, DrawCalls::GraphType::Float3, simd::vector3 )\
	X( ObjectBoundingBox, DrawCalls::GraphType::Float3, simd::vector3 )\
	X( MorphRatio, DrawCalls::GraphType::Float, float )\
	X( CurrentMovementSpeed, DrawCalls::GraphType::Float, float )\
	X( SpecificObjectPosition, DrawCalls::GraphType::Float3, simd::vector3 )\
	X( ObjectTransform, DrawCalls::GraphType::Float4x4, simd::matrix )\
	X( AttachedObjectTransform, DrawCalls::GraphType::Float4x4, simd::matrix )\
	X( AfflictionOrigin, DrawCalls::GraphType::Float3, simd::vector3 )\
	X( GuardSkillData, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( HarvestFluidAmount, DrawCalls::GraphType::Float, float )\
	X( PenanceBrandChargeCount, DrawCalls::GraphType::Float, float )\
	X( WinterBrandIntensity, DrawCalls::GraphType::Float, float )\
	X( OshabiPhaseColour, DrawCalls::GraphType::Float3, simd::vector3 )\
	X( BlackHoleSkillPosition, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( TrackedKills, DrawCalls::GraphType::Float, float )\
	X( RitualSacrificePercent, DrawCalls::GraphType::Float, float )\
	X( MavenStasisOrbData, DrawCalls::GraphType::Float3, float )\
	X( MavenCreationProgress, DrawCalls::GraphType::Float, float )\
	X( UltimatumOrigin, DrawCalls::GraphType::Float3, simd::vector3 )\
	X( UltimatumTimeStopProgress, DrawCalls::GraphType::Float, float )\
	X( BannerStages, DrawCalls::GraphType::Float, float )\
	X( NearestPlayerPosition, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( VoidBurstTimer, DrawCalls::GraphType::Float, float )\
	X( CurrentBossPosition, DrawCalls::GraphType::Float3, simd::vector3 )\
	X( ArchnemesisPetrifyAmount, DrawCalls::GraphType::Float, float )\
	X( TangleDrowningInterpolation, DrawCalls::GraphType::Float, float )\
	X( ArchnemesisHotHTCorruption, DrawCalls::GraphType::Float, float )\
	X( BodyArmourStatValues, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( HelmetStatValues, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( BootsStatValues, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( GlovesStatValues, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( CharacterFXStatValues, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( WeaponShieldStatValues, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( BackAttachmentStatValues, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( AmuletStatValues, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( KillFlare, DrawCalls::GraphType::Float, float )\
	X( TreasureExplosionVariation, DrawCalls::GraphType::Float, float, DynamicParameterInfo::CacheData )\
	X( SpiritWeaponEffectDialogueVolume, DrawCalls::GraphType::Float, float )\
	X( AnimatedStateMachineValues, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( MasqueradePortalPosition, DrawCalls::GraphType::Float3, simd::vector3 )\
	X( AtlasNodeColour, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( DynamicTexture0UV, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( VaalSoulsGainedInPast10s, DrawCalls::GraphType::Float, float )\
	X( ChronomancerArmourGemFade, DrawCalls::GraphType::Float, float )\
	X( WindVector, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( EnergyShieldHitPosition, DrawCalls::GraphType::Float4, simd::vector4 )\
	X( BloodColour, DrawCalls::GraphType::Float3, simd::vector3 )\

