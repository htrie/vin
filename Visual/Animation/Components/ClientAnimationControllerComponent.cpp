
#include <iomanip>
#include <thread>
#include <mutex>
#include <limits>

#include "Common/Utility/Logger/Logger.h"
#include "Common/FileReader/ASTReader.h"

#include "ClientInstanceCommon/Game/GameObject.h"
#include "ClientInstanceCommon/Game/Components/Positioned.h"
#include "ClientInstanceCommon/Game/Components/Animated.h"
#include "ClientInstanceCommon/Animation/AnimatedObject.h"

#include "Visual/Profiler/JobProfile.h"
#include "Visual/Animation/AnimationSystem.h"
#include "Visual/Animation/AnimationTracks.h"
#include "Visual/Animation/Components/AnimatedRenderComponent.h"
#include "Visual/Physics/PhysicsSystem.h"
#include "Visual/Animation/Components/SkinMeshComponent.h"
#include "Visual/Mesh/SkinnedMeshDataStructures.h"
#include "Visual/Renderer/Scene/SceneSystem.h"
#include "Visual/Animation/Components/BoneGroupsComponent.h"
#include "Visual/Animation/Components/ComponentsStatistics.h"
#include "Visual/Animation/Components/TrailsEffectsComponent.h"
#include "Visual/Animation/Components/ParticleEffectsComponent.h"
#include "Visual/Mesh/BoneConstants.h"
#include "Visual/Animation/Components/SoundEventsComponent.h"
#include "Visual/GpuParticles/GpuParticleSystem.h"
#include "Visual/Device/WindowDX.h"

#include "ClientAnimationControllerComponent.h"
#include "Common/Utility/ContainerOperations.h"

#if WIP_PARTICLE_SYSTEM
#include "Visual/Renderer/EffectGraphSystem.h"
#endif
namespace Animation
{

	namespace Components
	{
		std::string ClientAnimationControllerStatic::lock_bone_name = "<lock>";
		std::string ClientAnimationControllerStatic::root_bone_name = "<root>";


		ClientAnimationControllerStatic::ClientAnimationControllerStatic(const Objects::Type & parent)
			: AnimationObjectStaticComponent(parent)
		{
		}

		COMPONENT_POOL_IMPL(ClientAnimationControllerStatic, ClientAnimationController, 1024)

		bool ClientAnimationControllerStatic::SetValue( const std::string& name, const std::wstring& value )
		{
			if( name == "skeleton" )
			{
				skeleton = Mesh::AnimatedSkeletonHandle( value );
				return true;
			}

			if( name == "socket" )
			{
				if( skeleton.IsNull() )
					throw Resource::Exception() << "Tried to add a socket without a skeleton";

				Socket new_socket;
				new_socket.name = WstringToString( value );

				if( new_socket.name.empty() )
					throw Resource::Exception() << "Tried to add a socket with no name!";

				if( GetBoneIndexByName( new_socket.name ) != Bones::Invalid )
					throw Resource::Exception() << "Tried to add a socket with the same name as an existing bone!";

				if( AnyOf( additional_sockets, [&]( const Socket & socket ) { return &socket != &new_socket && socket.name == new_socket.name; } ) )
					throw Resource::Exception() << "Tried to add a socket with the same name as an existing socket!";

				new_socket.index = static_cast< unsigned >( GetNumBonesAndSockets() );
				new_socket.parent_index = Bones::Invalid;

#ifndef PRODUCTION_CONFIGURATION
				new_socket.origin = parent_type.current_origin;
#endif
				additional_sockets.push_back( std::move( new_socket ) );
				return true;
			}

			if( name == "parent" )
			{
				if( additional_sockets.empty() )
					throw Resource::Exception() << "Tried to set parent without a socket";

				auto& new_socket = additional_sockets.back();
				new_socket.parent_index = GetBoneIndexByName( WstringToString( value ) );
				if( new_socket.parent_index == Bones::Invalid )
					throw Resource::Exception() << "socket_parent does not exist. socket: " << new_socket.name.c_str() << ", parent: " << WstringToString( value ).c_str();

				return true;
			}

			if( name == "scale" )
			{
				if( additional_sockets.empty() )
					throw Resource::Exception() << "Tried to set scale without a socket";

				auto& new_socket = additional_sockets.back();

				std::wstringstream stream( value );
				simd::vector3 scale;
				stream >> scale.x >> scale.y >> scale.z;

				new_socket.offset = new_socket.offset * simd::matrix::scale( scale );

				return true;
			}

			if( name == "rotation" )
			{
				if( additional_sockets.empty() )
					throw Resource::Exception() << "Tried to set rotation without a socket";

				auto& new_socket = additional_sockets.back();

				std::wstringstream stream( value );
				simd::vector3 rot;
				stream >> rot.x >> rot.y >> rot.z;

				new_socket.offset = new_socket.offset * simd::matrix::yawpitchroll( rot.y, rot.x, rot.z );

				return true;
			}

			if( name == "translation" )
			{
				if( additional_sockets.empty() )
					throw Resource::Exception() << "Tried to set translation without a socket";

				auto& new_socket = additional_sockets.back();

				std::wstringstream stream( value );
				simd::vector3 pos;
				stream >> pos.x >> pos.y >> pos.z;

				new_socket.offset = new_socket.offset * simd::matrix::translation( pos );

				return true;
			}

			if( name == "ignore_parent_axis" )
			{
				if( additional_sockets.empty() )
					throw Resource::Exception() << "Tried to set ignore_parent_axis without a socket";

				auto& new_socket = additional_sockets.back();

				const auto lower = LowercaseString( value );
				new_socket.ignore_parent_axis.set( 0, Contains( lower, L'x' ) );
				new_socket.ignore_parent_axis.set( 1, Contains( lower, L'y' ) );
				new_socket.ignore_parent_axis.set( 2, Contains( lower, L'z' ) );

				if( new_socket.ignore_parent_axis.none() )
					throw Resource::Exception() << "Failed to set ignore_parent_axis for socket (must be a combination of x y or z)";

				return true;
			}

			if( name == "ignore_parent_rotation" )
			{
				if( additional_sockets.empty() )
					throw Resource::Exception() << "Tried to set ignore_parent_rotation without a socket";

				auto& new_socket = additional_sockets.back();

				const auto lower = LowercaseString( value );
				new_socket.ignore_parent_rotation.set( 0, StringContains( lower, L"yaw" ) );
				new_socket.ignore_parent_rotation.set( 1, StringContains( lower, L"pitch" ) );
				new_socket.ignore_parent_rotation.set( 2, StringContains( lower, L"roll" ) );

				if( new_socket.ignore_parent_rotation.none() )
					throw Resource::Exception() << "Failed to set ignore_parent_rotation for socket (must be a combination of yaw pitch or roll)";

				return true;
			}

			if (name == "attachment_groups")
			{
				if (!attachment_bones.parent_bones.empty() || attachment_bones.child_bone_group != "")
					throw Resource::Exception() << "Tried setting attachment groups when attachment bones were already specified";

				std::string bone_group_pair = WstringToString(value);
				std::stringstream s(bone_group_pair);
				s >> attachment_groups.parent_bone_group;
				s >> attachment_groups.child_bone_group;
				return true;
			}

			if( name == "attachment_bones" )
			{
				if (attachment_groups.parent_bone_group != "" || attachment_groups.child_bone_group != "")
					throw Resource::Exception() << "Tried setting attachment bones when attachment groups were already specified";
				if (!attachment_bones.parent_bones.empty() || attachment_bones.child_bone_group != "")
					throw Resource::Exception() << "Tried setting attachment bones when attachment bones were already specified";

				std::stringstream s(WstringToString(value));
				std::string prev_bone_name, next_bone_name;

				s >> prev_bone_name;
				while( s >> next_bone_name )
				{
					attachment_bones.parent_bones.push_back( prev_bone_name );
					prev_bone_name = std::move( next_bone_name );
				}

				attachment_bones.child_bone_group = prev_bone_name;
				return true;
			}

			return false;
		}

		bool ClientAnimationControllerStatic::SetValue(const std::string &name, const float value)
		{
			if (name == "linear_stiffness")
			{
				this->linear_stiffness = value;
				return true;
			}
			if (name == "linear_allowed_contraction")
			{
				this->linear_allowed_contraction = value;
				return true;
			}
			if (name == "bend_stiffness")
			{
				this->bend_stiffness = value;
				return true;
			}
			if (name == "bend_allowed_angle")
			{
				this->bend_allowed_angle = value;
				return true;
			}
			if (name == "viscosity_multiplier" || name == "normal_viscosity_multiplier")
			{
				this->normal_viscosity_multiplier = value;
				return true;
			}
			if (name == "tangent_viscosity_multiplier")
			{
				this->tangent_viscosity_multiplier = value;
				return true;
			}
			if (name == "enclosure_radius")
			{
				this->enclosure_radius = value;
				return true;
			}
			if (name == "enclosure_angle")
			{
				this->enclosure_angle = value;
				return true;
			}
			if (name == "enclosure_radius_additive")
			{
				this->enclosure_radius_additive = value;
				return true;
			}
			if (name == "animation_velocity_factor")
			{
				this->animation_velocity_factor = value;
				return true;
			}
			if (name == "animation_force_factor")
			{
				this->animation_force_factor = value;
				return true;
			}
			if (name == "animation_velocity_multiplier")
			{
				this->animation_velocity_multiplier = value;
				return true;
			}
			if (name == "animation_max_velocity")
			{
				this->animation_max_velocity = value;
				return true;
			}
			if (name == "gravity")
			{
				this->gravity = value;
				return true;
			}
			return false;
		}

		bool ClientAnimationControllerStatic::SetValue(const std::string & name, const bool value)
		{
			if (name == "use_shock_propagation")
			{
				this->use_shock_propagation = value;
				return true;
			}
			if (name == "enable_collisions")
			{
				this->enable_collisions = value;
				return true;
			}
			if (name == "recreate_on_scale")
			{
				this->recreate_on_scale = value;
				return true;
			}
			return false;
		}

		void ClientAnimationControllerStatic::PopulateFromRow( const AnimatedObjectDataTable::RowTypeHandle& row )
		{
			skeleton = Mesh::AnimatedSkeletonHandle( *row );
		}

		static bool zero( const simd::vector3& v ) { return fabs( v.x ) < 1e-7f && fabs( v.y ) < 1e-7f && fabs( v.z ) < 1e-7f; }
		static bool one( const simd::vector3& v ) { return fabs( v.x - 1.0f ) < 1e-7f && fabs( v.y - 1.0f ) < 1e-7f && fabs( v.z - 1.0f ) < 1e-7f; }

		void ClientAnimationControllerStatic::SaveComponent( std::wostream& stream ) const
		{
#ifndef PRODUCTION_CONFIGURATION
			const auto* parent = parent_type.parent_type->GetStaticComponent< ClientAnimationControllerStatic >();
			assert( parent );

			WriteValueNamedConditional( L"skeleton", skeleton.GetFilename(), skeleton != parent->skeleton );

			for( auto& socket : additional_sockets )
			{
				if( socket.origin != parent_type.current_origin )
					continue;

				WriteValueNamed( L"socket", socket.name.c_str() );
				PushIndent();

				WriteValueNamedConditional( L"parent", GetBoneNameByIndex( socket.parent_index ).c_str(), socket.parent_index != Bones::Invalid );

				simd::vector3 translation, rotation, scale;
				simd::matrix rotation_mat;
				socket.offset.decompose( translation, rotation_mat, scale );
				rotation = rotation_mat.yawpitchroll();

				WriteCustomConditional( stream << L"scale = \"" << scale.x << " " << scale.y << " " << scale.z << "\"", !one( scale ) );
				WriteCustomConditional( stream << L"rotation = \"" << rotation.y << " " << rotation.x << " " << rotation.z << "\"", !zero( rotation ) );
				WriteCustomConditional( stream << L"translation = \"" << translation.x << " " << translation.y << " " << translation.z << "\"", !zero( translation ) );
				WriteCustomConditional( stream << L"ignore_parent_axis = \""
					<< ( socket.ignore_parent_axis[0] ? "x" : "" )
					<< ( socket.ignore_parent_axis[1] ? "y" : "" )
					<< ( socket.ignore_parent_axis[2] ? "z" : "" ) << "\"", 
					socket.ignore_parent_axis.any() );
				WriteCustomConditional( stream << L"ignore_parent_rotation = \""
					<< ( socket.ignore_parent_rotation[0] ? "yaw" : "" )
					<< ( socket.ignore_parent_rotation[1] ? "pitch" : "" )
					<< ( socket.ignore_parent_rotation[2] ? "roll" : "" ) << "\"",
					socket.ignore_parent_rotation.any() );
			}

			stream << std::fixed;
			stream << std::setprecision( 3 );

			float eps = 1e-3f;
			WriteValueConditional( linear_stiffness, fabs( linear_stiffness - parent->linear_stiffness ) > eps );
			WriteValueConditional( linear_allowed_contraction, fabs( linear_allowed_contraction - parent->linear_allowed_contraction ) > eps );
			WriteValueConditional( bend_stiffness, fabs( bend_stiffness - parent->bend_stiffness ) > eps );
			WriteValueConditional( bend_allowed_angle, fabs( bend_allowed_angle - parent->bend_allowed_angle ) > eps );
			WriteValueConditional( normal_viscosity_multiplier, fabs( normal_viscosity_multiplier - parent->normal_viscosity_multiplier ) > eps );
			WriteValueConditional( tangent_viscosity_multiplier, fabs( tangent_viscosity_multiplier - parent->tangent_viscosity_multiplier ) > eps );
			WriteValueConditional( enclosure_radius, fabs( enclosure_radius - parent->enclosure_radius ) > eps );
			WriteValueConditional( enclosure_angle, fabs( enclosure_angle - parent->enclosure_angle ) > eps );
			WriteValueConditional( enclosure_radius_additive, fabs( enclosure_radius_additive - parent->enclosure_radius_additive ) > eps );
			WriteValueConditional( animation_velocity_factor, fabs( animation_velocity_factor - parent->animation_velocity_factor ) > eps );
			WriteValueConditional( animation_force_factor, fabs( animation_force_factor - parent->animation_force_factor ) > eps );
			WriteValueConditional( animation_velocity_multiplier, fabs( animation_velocity_multiplier - parent->animation_velocity_multiplier ) > eps );
			WriteValueConditional( animation_max_velocity, fabs( animation_max_velocity - parent->animation_max_velocity ) > eps );
			WriteValueConditional( gravity, fabs( gravity - parent->gravity ) > eps );

			WriteValueConditional( use_shock_propagation, use_shock_propagation != parent->use_shock_propagation );
			WriteValueConditional( enable_collisions, enable_collisions != parent->enable_collisions );
			WriteValueConditional( recreate_on_scale, recreate_on_scale != parent->recreate_on_scale );

			if( attachment_groups.parent_bone_group != parent->attachment_groups.parent_bone_group || attachment_groups.child_bone_group != parent->attachment_groups.child_bone_group )
			{
				if( attachment_groups.parent_bone_group != "" && attachment_groups.child_bone_group != "" )
				{
					WriteCustom( stream << L"attachment_groups = \"" << StringToWstring( attachment_groups.parent_bone_group ) << L" " << StringToWstring( attachment_groups.child_bone_group ) << L"\"" );
				}
			}

			if( attachment_bones.parent_bones != parent->attachment_bones.parent_bones || attachment_bones.child_bone_group != parent->attachment_bones.child_bone_group )
			{
				if( !attachment_bones.parent_bones.empty() && attachment_bones.child_bone_group != "" )
					WriteCustom(
					{
						stream << L"attachment_bones = \"";
						for( auto& bone : attachment_bones.parent_bones )
							stream << StringToWstring( bone ) << L" ";
						stream << StringToWstring( attachment_bones.child_bone_group ) << L"\"";
					} );
			}
#endif
		}

		void ClientAnimationControllerStatic::OnTypeInitialised()
		{
			if( skeleton.IsNull() )
			{
#ifndef PRODUCTION_CONFIGURATION
				if( parent_type.load_parent_types_only )
					return;
#endif
				throw Resource::Exception() << L"Skeleton file not specified";
			}

			animation_controller_index = parent_type.GetStaticComponentIndex< AnimationControllerStatic >();
			bone_groups_index = parent_type.GetStaticComponentIndex< BoneGroupsStatic >();

			// backwards compatibility for old (pre 4.0.0) bone naming
			auto NewBoneToOldBone = [](const std::string& new_name) -> const char* 
			{
				if( new_name == "aux_L_Weapon_attachment" ) return "L_Weapon";
				if( new_name == "aux_R_Weapon_attachment" ) return "R_Weapon";
				if( new_name == "aux_R_Foot_attachment" ) return "RightFoot";
				if( new_name == "aux_L_Foot_attachment" ) return "LeftFoot";
				if( new_name == "aux_Back_attachment" ) return "Back";
				if( new_name == "aux_Head_attachment" ) return "Head_Attachment";
				return nullptr;
			};
			for( auto x = 0; x < skeleton->GetBones().size(); x++ )
			{
				const auto old_name = NewBoneToOldBone( skeleton->GetBones()[x].name );
				if( old_name && GetBoneIndexByName(old_name) == Bones::Invalid )
				{
					Socket new_socket;
					new_socket.name = old_name;
					new_socket.index = static_cast< unsigned >( GetNumBonesAndSockets() );
					new_socket.parent_index = x;
					additional_sockets.push_back( std::move( new_socket ) );
				}
			}
		}

		const char* ClientAnimationControllerStatic::GetComponentName()
		{
			return "ClientAnimationController";
		}

		bool IsBoneArrayIndex(size_t index)
		{
			return index != size_t(Bones::Root) && index != size_t(Bones::Lock) && index != size_t(Bones::Invalid);
		}


		unsigned ClientAnimationControllerStatic::GetBoneIndexByName( const std::string& bone_name ) const
		{
			return GetBoneIndexByName( bone_name.c_str() );
		}

		unsigned ClientAnimationControllerStatic::GetBoneIndexByName( const char* const bone_name ) const
		{
			if( !bone_name || !*bone_name )
				return Bones::Invalid;

			if( strncmp(bone_name, "-1", 2) == 0 || strncmp(bone_name, "<root>", 6) == 0 )
				return Bones::Root;
			if( strncmp(bone_name, "-2", 2) == 0 || strncmp(bone_name, "<lock>", 6) == 0 )
				return Bones::Lock;

			const auto result = skeleton->GetBoneIndexByName( bone_name );
			if( result != Bones::Invalid )
				return result;

			for( unsigned i = 0; i < additional_sockets.size(); ++i )
			{
				if( additional_sockets[i].name == bone_name )
					return static_cast< unsigned >( GetNumBones() + i );
			}
			return Bones::Invalid;
		}

		const std::string& ClientAnimationControllerStatic::GetBoneNameByIndex( const unsigned bone_index ) const
		{
			if( bone_index < GetNumBones() )
				return skeleton->GetBoneName( bone_index );

			// Better safe then sorry?
			if( bone_index == Bones::Root )
				return root_bone_name;
			if( bone_index == Bones::Lock )
				return lock_bone_name;

			assert( bone_index < GetNumBonesAndSockets() );
			if( bone_index >= GetNumBonesAndSockets() )
			{
				LOG_CRIT_F( L"Out of bounds bone access in ClientAnimationControllerStatic::GetBoneNameByIndex, bone: {}", bone_index );

			#ifndef PRODUCTION_CONFIGURATION
				throw std::runtime_error( "Out of bounds bone access in ClientAnimationControllerStatic::GetBoneNameByIndex, check log for details" );
			#endif

				return root_bone_name;
			}

			return additional_sockets[bone_index - GetNumBones()].name;
		}

		const simd::matrix& ClientAnimationControllerStatic::GetBindSpaceTransform( const unsigned bone_index ) const
		{
			if( bone_index < GetNumBones() )
				return skeleton->GetBindSpaceTransform( bone_index );

			assert( bone_index < GetNumBonesAndSockets() );
			if( bone_index >= GetNumBonesAndSockets() )
			{
				LOG_CRIT_F( L"Out of bounds bone access in ClientAnimationControllerStatic::GetBindSpaceTransform, bone: {}", bone_index );

			#ifndef PRODUCTION_CONFIGURATION
				throw std::runtime_error( "Out of bounds bone access in ClientAnimationControllerStatic::GetBindSpaceTransform, check log for details" );
			#endif

				static simd::matrix identity = simd::matrix::identity();
				return identity;
			}

			return additional_sockets[bone_index - GetNumBones()].offset;
		}

		void ClientAnimationControllerStatic::GetCurrentBoneLayout( const simd::matrix* tracks_matrix, std::vector< simd::vector3 >& positions_out, std::vector< unsigned > &indices_out ) const
		{
			skeleton->GetCurrentBoneLayout( tracks_matrix, positions_out, indices_out );
		}

		ClientAnimationController::ClientAnimationController( const ClientAnimationControllerStatic& static_ref, Objects::Object& object )
			: AnimationObjectComponent( object ), static_ref( static_ref ), first_frame( true )
		{
			tracks = TracksCreate(static_ref.skeleton);

			AnimationController* animation_controller = GetObject().GetComponent< AnimationController >( static_ref.animation_controller_index );
			anim_cont_list_ptr = animation_controller->GetListener( *this );

			is_initialized = false;
			needs_physics_reset = false;
			prev_coords = Physics::Coords3f::defCoords();

			ResetCurrentAnimation();

			morphing_info.ratio = 0.0f;
		}

		void ClientAnimationController::OnConstructionComplete()
		{
			AdvanceAnimations( 0.0f );
		}

		void ClientAnimationController::LoadPhysicalRepresentation()
		{
			if (is_initialized)
				return;
			is_initialized = true;

			Physics::Coords3f objectCoords = Physics::Coords3f::defCoords();
			const auto* animated_render = GetObject().GetComponent< AnimatedRender >();
			if(animated_render)
			{
				objectCoords = Physics::MakeCoords3f(animated_render->GetTransform());
			}
			this->coord_orientation = Physics::MixedProduct(objectCoords.xVector, objectCoords.yVector, objectCoords.zVector);
			RecreatePhysicalRepresentation(objectCoords);
		}

		void ClientAnimationController::SetupSceneObjects()
		{
			if (palette)
				Animation::System::Get().Remove(palette); // Shouldn't be needed, but it seems the Setup/Destroy pair is not properly called. Sanitize here.
			palette = std::make_shared<Animation::Palette>(TracksGetFinalTransformPalette(tracks), TracksGetUVAlphaPalette(tracks));
			Animation::System::Get().Add(palette);

		}

		void ClientAnimationController::DestroySceneObjects()
		{
			Animation::System::Get().Remove(palette);
			palette.reset();
		}

		ClientAnimationController::~ClientAnimationController()
		{
			if (palette)
				Animation::System::Get().Remove(palette); // Shouldn't be needed, but it seems the Setup/Destroy pair is not properly called. Sanitize here too.

			if (tracks)
				TracksDestroy(tracks);

			if (is_initialized)
				Physics::System::Get().RemoveDeferredRecreateCallback(this);
		}

		const simd::matrix* ClientAnimationController::GetFinalTransformPalette() const { return TracksGetFinalTransformPalette(tracks)->data(); }
		const simd::vector4* ClientAnimationController::GetUVAlphaPalette() const { return TracksGetUVAlphaPalette(tracks)->data(); }
		int ClientAnimationController::GetPaletteSize() const { return TracksGetPaletteSize(tracks); }

		const Mesh::LightStates& ClientAnimationController::GetCurrentLightState() const { return TracksGetCurrentLightState(tracks); }

		void ClientAnimationController::RenderFrameMove( const float elapsed_time )
		{
			RenderFrameMoveInternal(elapsed_time + frame_extra_time);
			frame_extra_time = 0.0f;
		}
		
		void ClientAnimationController::RenderFrameMoveNoVirtual( AnimationObjectComponent* component, const float elapsed_time )
		{
			static_cast<ClientAnimationController*>(component)->RenderFrameMoveInternal(elapsed_time);
		}

		void ClientAnimationController::PhysicsRecreateDeferred()
		{
			if (is_initialized)
				Physics::System::Get().AddDeferredRecreateCallback(this);
		}

		void ClientAnimationController::PhysicsPreStep()
		{
			if (is_initialized && deformable_mesh_handle.Get())
			{
				if(static_ref.recreate_on_scale)
					ProcessMirroring(); //second pass to change mirroring of the whole object including deformable mesh
				ProcessVertexDefPositions();
				ProcessAttachedVertices();
#if (!defined(CONSOLE) && !defined(__APPLE__) && !defined(ANDROID)) && (defined(TESTING_CONFIGURATION) || defined(DEVELOPMENT_CONFIGURATION))
				if( GetAsyncKeyState(VK_SPACE) & 0x8000 && Device::WindowDX::GetWindow()->IsActive() )
				{
					needs_physics_reset = true;
				}
#endif 
				if(	needs_physics_reset)
					deformable_mesh_handle.Get()->RestorePosition();
				needs_physics_reset = false;
			}
		}

		void ClientAnimationController::PhysicsPostStep()
		{
			if (is_initialized && deformable_mesh_handle.Get())
			{
				if (static_ref.use_shock_propagation)
					deformable_mesh_handle.Get()->PropagateShock();
				deformable_mesh_handle.Get()->EncloseVertices();
			}
		}


		void ClientAnimationController::PhysicsPostUpdate()
		{
			if (is_initialized && deformable_mesh_handle.Get())
			{
				ProcessPhysicsDrivenBones();
			}
		}

		void ClientAnimationController::RenderFrameMoveInternal( const float elapsed_time )
		{
			AdvanceAnimations(elapsed_time);
		}

		void ClientAnimationController::AdvanceAnimations( const float elapsed_time )
		{
			PROFILE;

		#if defined(PROFILING)
			Stats::Tick(Type::ClientAnimationController);
		#endif

			const float prev_anim_pos = GetCurrentAnimationPosition();

			const auto* animation_controller = GetObject().GetComponent< AnimationController >(static_ref.animation_controller_index);

			const auto triggered_event_times = HandleAnimationEvents(elapsed_time);

			float elapsed_time_final = elapsed_time;
			const float global_speed_multiplier = animation_controller->GetGlobalSpeedMultiplier();
			const float animation_speed = current_animation.speed_multiplier * global_speed_multiplier;

			if( frame_pause_remaining > 0.0f )
			{
				elapsed_time_final = elapsed_time / 8.0f;
				frame_pause_lost_time += ( elapsed_time - elapsed_time_final );
			
				frame_pause_remaining -= elapsed_time;
			
				if( frame_pause_remaining <= 0.0f )
					frame_pause_remaining = 0.0f;
			}
			else if( frame_pause_lost_time > 0.0f )
			{
				const auto time_left = ( GetCurrentAnimationLength() - GetCurrentAnimationPosition() ) * animation_speed;

				if( time_left > 0.0f )
					elapsed_time_final += ( frame_pause_lost_time / time_left ) * elapsed_time;

				frame_pause_lost_time = std::max( 0.0f, elapsed_time );
			}

			bool hasMesh = !!deformable_mesh_handle.Get();

			float elapsed_time_offset = 0.0f;
			bool compute_detailed_animation = elapsed_time_final > 0.0f;

			auto particle_component = GetObject().GetComponent<ParticleEffects>();
			auto trails_component = GetObject().GetComponent<TrailsEffects>();

			if ((!particle_component || particle_component->Empty()) && (!trails_component || trails_component->Empty()))
				compute_detailed_animation = false;

			float last_detailed_frame = -time_since_last_detailed_key;
			time_since_last_detailed_key += elapsed_time_final;

			if (compute_detailed_animation)
			{
				const float min_delta = 0.001f;
				float future_keyframe = -1.0f;
				auto keyframe_times = TracksGetNextKeyFrames(tracks, elapsed_time_final, &future_keyframe, min_delta);
				
				if (animation_controller->IsSubdivideCurrentAnim())
				{
					if (!keyframe_times.empty())
					{
						std::sort(keyframe_times.begin(), keyframe_times.end());

						const auto s = keyframe_times.size();
						for (size_t a = 0; a < s; a++)
						{
							const auto mid = (keyframe_times[a] + last_detailed_frame) * 0.5f;
							if(mid >= 0.0f)
								keyframe_times.push_back(mid);

							last_detailed_frame = keyframe_times[a];
							time_since_last_detailed_key = elapsed_time_final - keyframe_times[a];
						}
					}

					if (future_keyframe >= 0.0f)
					{
						const auto mid = (future_keyframe + last_detailed_frame) * 0.5f;
						if (mid >= 0.0f && mid <= elapsed_time_final)
							keyframe_times.push_back(mid);
					}
				}

				for (const auto& a : triggered_event_times)
					keyframe_times.push_back(a);

				if (trails_component && animation_speed > 0.0f)
				{
					const auto event_times = trails_component->GetEffectEndTimes();
					for (const auto& event_time : event_times)
					{
						const float dt = (current_animation.elapsed_time - event_time) / animation_speed;
						if (dt >= 0.0f && dt <= elapsed_time_final)
							keyframe_times.push_back(elapsed_time_final - dt);
					}
				}

				if (!keyframe_times.empty())
				{
					const auto num_keyframes = TracksReduceNextKeyFrames(keyframe_times.data(), keyframe_times.size(), elapsed_time_final, min_delta);

					if (num_keyframes > 0)
					{
						const auto bone_count = GetNumBonesAndSockets();
						const auto id = Animation::System::Get().ReserveDetailedStorage(bone_count, num_keyframes, elapsed_time_final);
						if (trails_component)
							trails_component->SetAnimationStorageID(id);

						if (particle_component)
							particle_component->SetAnimationStorageID(id);

						Utility::StackArray< simd::vector3 > final_bones(STACK_ALLOCATOR(simd::vector3, bone_count));

						for (size_t a = 0; a < num_keyframes; a++)
						{
							const auto key = std::min(elapsed_time_final, std::max(keyframe_times[a] - elapsed_time_offset, 0.0f) + elapsed_time_offset);
							if (a == 0 || (key - elapsed_time_offset) > 1e-5f)
							{
								TracksRenderFrameMove(tracks, static_ref.skeleton, key - elapsed_time_offset);
								elapsed_time_offset = key;

								for (size_t b = 0; b < bone_count; b++)
									final_bones[b] = GetBonePosition(unsigned(b));
							}

							Animation::System::Get().PushDetailedStorage(id, a, key, final_bones);
						}
					}
				}
			}

			TracksRenderFrameMove( tracks, static_ref.skeleton, std::max(0.0f, elapsed_time_final - elapsed_time_offset));

			ProcessParentAttachedBoneGroups();
			ProcessParentAttachedBones();

			ProcessMorphingBones();

			bool hasCollisions = collision_ellipsoids.size() > 0 || collision_spheres.size() > 0;

			if((hasMesh || hasCollisions) && static_ref.recreate_on_scale)
			{
				ProcessMirroring();
			}
			if (hasMesh)
			{
				deformable_mesh_handle.Get()->SetEnabled(true);

				const auto* animated_render = GetObject().GetComponent< AnimatedRender >();
				if(animated_render)
				{
					Physics::Coords3f objectCoords = Physics::MakeCoords3f(animated_render->GetTransform());
					Physics::Vector3f size = Physics::Vector3f(200.0f, 200.0f, 200.0f);
					Physics::AABB3f aabb(objectCoords.pos - size, objectCoords.pos + size);
					deformable_mesh_handle.Get()->SetCullingAABB(aabb);

					float animation_control_ratio = 1.0f;
					int bone_control_index = GetBoneIndexByName("aux_animation_control_ratio");
					if (bone_control_index != Bones::Invalid)
					{
						const auto& data = TracksGetFinalTransformPalette(tracks)->data()[bone_control_index];
						animation_control_ratio = std::max(0.0f, data[0][0]);  // Uses X scale
					}
					deformable_mesh_handle.Get()->SetAnimationControlRatio(animation_control_ratio);
				}
				ProcessTeleportation();
			}
			if(hasCollisions)
			{
				ProcessAttachedCollisionGeometry();
			}

			if ( elapsed_time > 0.0f )
			{
				first_frame = false;
			}

			// update animation position for Positioned component if metadata has movement data
			if (current_animation.animation_index != -1 && current_animation.has_movement_data)
			{
				auto& metadata = animation_controller->GetAnimationMetadata(current_animation.animation_index);
				assert(metadata.HasMovementPositions());
				if (metadata.HasMovementPositions()) 
				{
					auto obj = GetObject().GetVariable<Game::GameObject>(GEAL::GameObjectId);
					if (obj)
						obj->GetPositioned().SetMovementAnimationTime(prev_anim_pos, GetCurrentAnimationPosition(), true);
				}
			}
		}

		Memory::SmallVector<float, 8, Memory::Tag::Animation> ClientAnimationController::HandleAnimationEvents(const float elapsed_time)
		{
			Memory::SmallVector<float, 8, Memory::Tag::Animation> final_event_times;

			if(current_animation.animation_index == -1)
				return final_event_times;

			PROFILE;

			auto animation_controller = GetObject().GetComponent< AnimationController >(static_ref.animation_controller_index);
			const auto& events = animation_controller->GetAnimationEvents(current_animation.animation_index);
			const float global_speed_multiplier = animation_controller->GetGlobalSpeedMultiplier();

			// Instead of just moving the animation forward, we are going to peek ahead for events and step up to them in turn
			float remaining_time = elapsed_time;
			float travel_total = 0.0f;

			while( remaining_time > 0.0f )
			{
				// Calculate the speed multiplier again after each event, as it may change during iteration.
				const float speed_multiplier = current_animation.speed_multiplier * global_speed_multiplier;
				if( speed_multiplier == 0.0f )
					break;

				// Calculate how far we would move, assuming there are no events to trigger
		 		const float delta = remaining_time * speed_multiplier;

				// Calculate where we would end up if we decided to move forward
				const float end_time = std::min( current_animation.elapsed_time + delta, current_animation.length() );

				// If there are no events to trigger, we can end iteration now
				if( current_animation.next_event_time > end_time )
				{
					current_animation.elapsed_time = end_time;
					break;
				}

				// If this is the last event, we have reached the end of the animation
				if( current_animation.next_event_index == events.size() )
				{
					assert( current_animation.elapsed_time + delta >= current_animation.length() );
					assert( current_animation.length() == end_time );

					// Calculate how far the end of the animation is
					const float travel = current_animation.length() - current_animation.elapsed_time;
					const float travel_realtime = travel / speed_multiplier;
					travel_total += travel_realtime;

					final_event_times.push_back(travel_total);

					// Set the elapsed time to the end time we have calculated, which may be past the end of the current animation
					current_animation.elapsed_time = current_animation.end_time;

					if( animation_controller->IsLoopingAnimation( current_animation.animation_index ) )
					{
						// If we have queued an animation during a looping animation, we want to start the queued animation at the end of the current loop
						if( animation_controller->HasQueuedAnimation() )
						{
							break;
						}
						else
						{
							if( const auto* loop_point = animation_controller->FindFirstEvent( current_animation.animation_index, LoopEventId ) )
							{
								// If we have a loop point we should deliberately set the animation position.
								// This is so that the event track can be recalculated correctly starting from this new position.
								float cached_pos = current_animation.elapsed_time;
								current_animation.elapsed_time = loop_point->time;
								RecalculateEventTimes();
								NotifyListeners( &ClientAnimationControllerListener::OnAnimationPositionSet, current_animation );

								// If the position hasn't moved we can assume that we are looping at the last frame of the animation.
								// We can just "pretend" to spend the remaining time now, to avoid an infinite loop
								constexpr float one_frame = 1.0f / 30.0f;
								if( abs( current_animation.end_time - current_animation.elapsed_time ) < one_frame )
									break;
							}
							else
							{
								// We are looping the current animation. We set the time to zero instead of just subtracting by the animation length,
								// as we need to make sure that any speed multipliers at the beginning of the animation are applied correctly by this loop.
								current_animation.elapsed_time = 0.0f;

								// We should reset the event track now
								GenerateNextEventTime();
							}

							PROFILE_NAMED( "::OnAnimationLooped" );
							NotifyListeners( &ClientAnimationControllerListener::OnAnimationLooped, *this, current_animation.animation_index );
						}
					}
					else
					{
						// We have finished this animation
						const auto animation_index = current_animation.animation_index;

						// Clear the current animation data
						current_animation.animation_index = -1;
						current_animation.speed_multiplier = 0.0f;
						current_animation.has_movement_data = false;

						{
							PROFILE_NAMED( "::OnAnimationEnd" );
							NotifyListeners( &ClientAnimationControllerListener::OnAnimationEnd, *this, animation_index, travel_total);
						}
					}

					// If we are no longer playing an animation, we can exit the loop
					if( current_animation.animation_index == -1 )
						break;
					
					// Spend the traveled time and continue iteration
					remaining_time -= travel_realtime;
				}
				else
				{
					// Move to the next event time
					const float travel = current_animation.next_event_time - current_animation.elapsed_time;
					const float travel_realtime = travel / speed_multiplier;
					travel_total += travel_realtime;

					current_animation.elapsed_time = current_animation.next_event_time;

					// Cache the animation index/position, as they may be about to change
					const unsigned animation_index = current_animation.animation_index;
					const float animation_time = current_animation.elapsed_time;

					if( animation_events_enabled )
					{
						PROFILE_NAMED( "::OnAnimationEvent" );
						//Notify everyone of the event

						auto trigger_event = [&]( const auto& e )
						{
							final_event_times.push_back(travel_total);
							NotifyListeners(&ClientAnimationControllerListener::OnAnimationEvent, *this, e, travel_total);
						};

						const auto& e = *events[current_animation.next_event_index];
						trigger_event( e );
					}

					// Ensure that the animation index/position haven't changed due to events triggering before we generate the next event time.
					// If either have changed, then the event tracks have already been modified
					if( current_animation.animation_index == animation_index && current_animation.elapsed_time == animation_time )
						GenerateNextEventTime();

					// Spend the traveled time and continue iteration
					remaining_time -= travel_realtime;
				}

				if( current_animation.animation_index == -1 )
					break;
			}

			return final_event_times;
		}

		void ClientAnimationController::GenerateNextEventTime()
		{
			auto animation_controller = GetObject().GetComponent<AnimationController>(static_ref.animation_controller_index);
			const auto& events = animation_controller->GetAnimationEvents(current_animation.animation_index);

			auto generate_event_time = [&]()
			{
				if( current_animation.next_event_index == events.size() )
					current_animation.next_event_time = current_animation.length();
				else
					current_animation.next_event_time = events[current_animation.next_event_index]->time;
			};

			//If the last event was the end of the animation then go to the first event
			if( current_animation.next_event_index == events.size() )
			{
				current_animation.next_event_index = 0;
				generate_event_time();
			}
			else
			{
				++current_animation.next_event_index;
				generate_event_time();
			}
		}

		struct SpeedMultipliers
		{
			SpeedMultipliers( const float unbound )
				: unbound( unbound ), bound( unbound > 0.0f ? Clamp( unbound, 1e-3f, 1e3f ) : 1.0f ) { }

			float unbound, bound;
		};

		// Returns a blend time in ms to a value in seconds, scaled by animation speed multipliers
		inline float calculate_blend_time(const int blend_time, const float animation_speed)
		{
			// If the blend_time is not specified (<0), default to 100ms
			return (blend_time < 0 ? 100 : blend_time) / (1000.0f * animation_speed);
		};

		inline AnimationSet::PlaybackMode GetPlaybackMode(const AnimationController* animation_controller, int animation_index, AnimationSet* animation_set)
		{
			// Allow animation views to override the playback mode specified in the animation set
			if (auto* view = animation_controller->GetAnimationViewMetadata(animation_index))
				return view->looping ? AnimationSet::PLAY_LOOPING : AnimationSet::PLAY_ONCE;
			return animation_set->GetPlaybackMode();
		};

		ClientAnimationController::StartAnimationInfo ClientAnimationController::GetStartAnimationInfo(const AnimationController* animation_controller, const CurrentAnimation& animation, uint32_t current_animation_index, const bool blend)
		{
			StartAnimationInfo result;

			auto& new_metadata = animation_controller->GetAnimationMetadata(animation.animation_index);

			const float global_speed_multiplier = animation_controller->GetGlobalSpeedMultiplier();

			// Calculate the speed information for the next animation we are going to play
			const SpeedMultipliers new_speed(animation.speed_multiplier * global_speed_multiplier);
			bool suppress_blending = false;

			// Disable blending if animation speed is below a threshold
			if (new_speed.bound < 2e-3f)
				suppress_blending = true;

			// The shortest of: the current animation length, the new animation length
			// Initially we just set this to the predicted length of the new animation, though this may change shortly.
			float shortest_animation_length = (new_metadata.length / 1000.0f) / new_speed.bound;

			// The longest of: The current animation blend time, the new animation blend time
			// Initially we just set this to the blend time of the new animation, though this may change shortly.
			float longest_blend_time = calculate_blend_time(new_metadata.blend_in_time, new_speed.bound);
			if (animation_controller->GetBlendTimeOverride() >= 0.0f)
			{
				// If the animation controller specifies a blend time override, use that as the longest blend time.
				longest_blend_time = animation_controller->GetBlendTimeOverride();
			}
			else if (animation.blend_time_override >= 0.0f && new_metadata.blend_in_time == -1)
			{
				// If the incoming animation specifies a blend time override, use that as the longest blend time.
				longest_blend_time = animation.blend_time_override;
			}
			else if (current_animation_index != -1)
			{
				// Calculate the speed information for the animation we are currently playing
				const SpeedMultipliers current_speed(current_animation.speed_multiplier * global_speed_multiplier);

				// Disable blending if animation speed is below a threshold
				if (current_speed.bound < 2e-3f)
					suppress_blending = true;

				// Cache the metadata for the animation we are currently playing
				auto& curr_metadata = animation_controller->GetAnimationMetadata(current_animation_index);
				// If the current animation has a shorter animation length, replace our shortest length value
				shortest_animation_length = std::min(shortest_animation_length, current_animation.length() / current_speed.bound);
				// If the current animation has a longer blend out time, replace our longest blend time value
				longest_blend_time = std::max(longest_blend_time, calculate_blend_time(curr_metadata.blend_out_time, current_speed.bound));
			}

			// Determine the animation set we are going to use for this animation
			auto* view = animation_controller->GetAnimationViewMetadata(animation.animation_index);
			const float actual_blend_time = std::min(longest_blend_time, shortest_animation_length);

			result.blend_enabled = blend && !first_frame && !suppress_blending; // Determine now whether or not we plan to enable blending for this animation
			result.actual_blend_time = actual_blend_time;
			result.speed = new_speed.unbound;
			result.animation_set_index = view ? view->source_animation_index : animation.animation_index;
			
			if( animation.secondary_animation_index < animation_controller->GetNumAnimations() )
			{
				auto* view = animation_controller->GetAnimationViewMetadata( animation.secondary_animation_index );
				result.secondary_animation_set_index = view ? view->source_animation_index : animation.secondary_animation_index;
			}

			return result;
		}

		void ClientAnimationController::OnAnimationStart( const AnimationController& animation_controller, const CurrentAnimation& new_animation, const bool blend )
		{
			assert(new_animation.layer == Animation::ANIMATION_LAYER_DEFAULT);

			// Reset frame pause lost time
			frame_pause_lost_time = 0.0f;

			const auto remap = FindPairFirst( visual_animation_remaps, new_animation.animation_index );
			if( remap != visual_animation_remaps.end() )
			{
				assert( new_animation.secondary_animation_index == -1 ); // This will just break

				CurrentAnimation remapped;
				remapped.animation_index = remap->second;
				remapped.elapsed_time = new_animation.elapsed_time;;
				remapped.speed_multiplier = new_animation.speed_multiplier;
				remapped.blend_time_override = new_animation.blend_time_override;
				remapped.begin_time = 0.0f;
				remapped.end_time = animation_controller.GetAnimationMetadata( remap->second ).length / 1000.0f;
				remapped.has_movement_data = false;
				remapped.layer = new_animation.layer;
				OnAnimationStart( animation_controller, remapped, blend );
				return;
			}

			StartAnimationInfo info = GetStartAnimationInfo(&animation_controller, new_animation, current_animation.animation_index, blend);
			AnimationSet* animation_set = static_ref.skeleton->GetAnimationByIndex(info.animation_set_index);

			// Set up our new animation as the current animation
			current_animation.animation_index = new_animation.animation_index;
			current_animation.secondary_animation_index = new_animation.secondary_animation_index;
			current_animation.secondary_animation_blend_ratio = new_animation.secondary_animation_blend_ratio;
			current_animation.elapsed_time = new_animation.elapsed_time;
			current_animation.next_event_index = static_cast< unsigned >( animation_controller.GetAnimationEvents( current_animation.animation_index ).size() );
			current_animation.speed_multiplier = new_animation.speed_multiplier;
			current_animation.blend_time_override = new_animation.blend_time_override;
			current_animation.begin_time = new_animation.begin_time;
			current_animation.end_time = new_animation.end_time;
			current_animation.has_movement_data = new_animation.has_movement_data;
			current_animation.layer = new_animation.layer;

			// Generate the first event time
			GenerateNextEventTime();

			if( current_animation.secondary_animation_index < animation_controller.GetNumAnimations() )
			{
				TracksOnPrimaryAnimationStart( tracks
					, animation_set
					, current_animation.layer
					, nullptr // can easily add bone group if required
					, info.speed
					, info.blend_enabled ? info.actual_blend_time : 0.0f
					, current_animation.begin_time
					, current_animation.end_time
					, current_animation.elapsed_time
					, GetPlaybackMode( &animation_controller, current_animation.animation_index, animation_set )
					, current_animation.secondary_animation_blend_ratio
				);

				animation_set = static_ref.skeleton->GetAnimationByIndex( info.secondary_animation_set_index );

				TracksOnSecondaryAnimationStart( tracks
					, animation_set
					, current_animation.layer
					, nullptr // can easily add bone group if required
					, info.speed
					, info.blend_enabled ? info.actual_blend_time : 0.0f
					, current_animation.begin_time
					, current_animation.end_time
					, current_animation.elapsed_time
					, GetPlaybackMode( &animation_controller, current_animation.secondary_animation_index, animation_set )
					, current_animation.secondary_animation_blend_ratio
				);
			}
			else
			{
				TracksOnAnimationStart( tracks
					, animation_set
					, current_animation.layer
					, nullptr // can easily add bone group if required
					, info.speed
					, info.blend_enabled ? info.actual_blend_time : 0.0f
					, current_animation.begin_time
					, current_animation.end_time
					, current_animation.elapsed_time
					, GetPlaybackMode( &animation_controller, current_animation.animation_index, animation_set )
					, 1.0f
				);
			}

			NotifyListeners( &ClientAnimationControllerListener::OnAnimationStart, current_animation, blend );
		}

		void ClientAnimationController::OnAdditionalAnimationStart(const AnimationController&, const CurrentAnimation& new_animation, const bool blend, const std::string& mask_bone_group)
		{
			assert(new_animation.layer > Animation::ANIMATION_LAYER_DEFAULT);

			const auto* animation_controller = GetObject().GetComponent< AnimationController >(static_ref.animation_controller_index);
			
			uint32_t current_animation_index = TracksGetAnimationIndex(tracks, new_animation.layer); // get current animation index from the layer?

			StartAnimationInfo info = GetStartAnimationInfo(animation_controller, new_animation, current_animation_index, blend);
			AnimationSet* animation_set = static_ref.skeleton->GetAnimationByIndex(info.animation_set_index);

			// Getting group bone indices if required
			const std::vector<unsigned>* mask_bone_indices_ptr = nullptr;
			const auto* bone_groups = GetObject().GetComponent< BoneGroups >(static_ref.bone_groups_index);
			if( bone_groups && !mask_bone_group.empty() )
			{
				const auto index = bone_groups->static_ref.BoneGroupIndexByName( mask_bone_group );
				if( index < bone_groups->static_ref.NumBoneGroups() )
				{
					const auto& group = bone_groups->static_ref.GetBoneGroup( index );
					mask_bone_indices_ptr = &group.bone_indices;
				}
			}

			TracksOnAnimationStart(tracks
				, animation_set
				, new_animation.layer
				, mask_bone_indices_ptr
				, info.speed
				, info.blend_enabled ? info.actual_blend_time : 0.0f
				, new_animation.begin_time
				, new_animation.end_time
				, new_animation.elapsed_time
				, GetPlaybackMode(animation_controller, new_animation.animation_index, animation_set)
				, 1.0f - new_animation.secondary_animation_blend_ratio
			);
		}

		void ClientAnimationController::OnAppendAnimation(const AnimationController& controller, const CurrentAnimation& animation, const bool blend, const std::string& mask_bone_group, const bool is_masked_override)
		{
			const auto* animation_controller = GetObject().GetComponent< AnimationController >(static_ref.animation_controller_index);
			StartAnimationInfo info = GetStartAnimationInfo(animation_controller, animation, -1, blend); // -1 for current animation because append animation's blend time doesn't depend on current animation
			AnimationSet* animation_set = static_ref.skeleton->GetAnimationByIndex(info.animation_set_index);

			// Getting group bone indices if required
			const std::vector<unsigned>* mask_bone_indices_ptr = nullptr;
			const auto* bone_groups = GetObject().GetComponent< BoneGroups >( static_ref.bone_groups_index );

			if( is_masked_override )
			{
				assert( !mask_bone_group.empty() );
				assert( animation.layer > Animation::ANIMATION_LAYER_DEFAULT );

				const std::vector<unsigned>* mask_bone_translation_indices_ptr = nullptr;
				const std::vector<unsigned>* mask_bone_rotation_indices_ptr = nullptr;
				if( bone_groups && !mask_bone_group.empty() )
				{
					std::vector< std::string > groups;
					SplitString( groups, mask_bone_group );
					assert( groups.size() >= 1 && groups.size() <= 3 );

					if( const auto index = bone_groups->static_ref.BoneGroupIndexByName( groups[0] ); index < bone_groups->static_ref.NumBoneGroups() )
						mask_bone_indices_ptr = &bone_groups->static_ref.GetBoneGroup( index ).bone_indices;
					if( groups.size() >= 2 )
						if( const auto index = bone_groups->static_ref.BoneGroupIndexByName( groups[1] ); index < bone_groups->static_ref.NumBoneGroups() )
							mask_bone_translation_indices_ptr = &bone_groups->static_ref.GetBoneGroup( index ).bone_indices;
					if( groups.size() >= 3 )
						if( const auto index = bone_groups->static_ref.BoneGroupIndexByName( groups[2] ); index < bone_groups->static_ref.NumBoneGroups() )
							mask_bone_rotation_indices_ptr = &bone_groups->static_ref.GetBoneGroup( index ).bone_indices;
				}

				TracksAddMaskedOverride( tracks
					, animation_set
					, animation.layer
					, mask_bone_indices_ptr
					, mask_bone_translation_indices_ptr
					, mask_bone_rotation_indices_ptr
					, info.speed
					, info.blend_enabled ? info.actual_blend_time : 0.0f
					, animation.begin_time
					, animation.end_time
					, animation.elapsed_time
					, GetPlaybackMode( animation_controller, animation.animation_index, animation_set )
				);
			}
			else
			{
				if( bone_groups && !mask_bone_group.empty() )
					if( const auto index = bone_groups->static_ref.BoneGroupIndexByName( mask_bone_group ); index < bone_groups->static_ref.NumBoneGroups() )
						mask_bone_indices_ptr = &bone_groups->static_ref.GetBoneGroup( index ).bone_indices;

				TracksAppendAnimation( tracks
					, animation_set
					, animation.layer
					, mask_bone_indices_ptr
					, info.speed
					, info.blend_enabled ? info.actual_blend_time : 0.0f
					, animation.begin_time
					, animation.end_time
					, animation.elapsed_time
					, GetPlaybackMode( animation_controller, animation.animation_index, animation_set )
				);
			}
		}

		void ClientAnimationController::OnFadeAnimations(const AnimationController& controller, const float blend_time, const unsigned layer)
		{
			TracksFadeAllAnimations(tracks, blend_time, layer);
		}

		void ClientAnimationController::OnAnimationSpeedChange( const AnimationController&, const CurrentAnimation& animation )
		{
			assert(animation.layer == Animation::ANIMATION_LAYER_DEFAULT);

			// Check we are not receiving an event for an animation that we are not playing yet, or are not playing anymore (OOS?).
			if( current_animation.animation_index != animation.animation_index )
				return;

			const float global_speed_multiplier    = GetObject().GetComponent< AnimationController >( static_ref.animation_controller_index )->GetGlobalSpeedMultiplier();
			const float animation_speed_multiplier = animation.speed_multiplier * global_speed_multiplier;

			TracksOnAnimationSpeedChange(tracks, animation_speed_multiplier);

			current_animation.speed_multiplier = animation.speed_multiplier;
			NotifyListeners( &ClientAnimationControllerListener::OnAnimationSpeedChange, current_animation );
		}

		void ClientAnimationController::OnAnimationSecondaryBlendRatioChange( const AnimationController&, const CurrentAnimation& animation )
		{
			assert( animation.layer == Animation::ANIMATION_LAYER_DEFAULT );

			// Check we are not receiving an event for an animation that we are not playing yet, or are not playing anymore (OOS?).
			if( current_animation.animation_index != animation.animation_index )
				return;

			TracksOnSecondaryBlendRatioChange( tracks, animation.secondary_animation_blend_ratio );

			current_animation.secondary_animation_blend_ratio = animation.secondary_animation_blend_ratio;
		}

		void ClientAnimationController::OnAnimationLayerSpeedSet(const AnimationController& controller, const float speed, const unsigned layer)
		{
			assert(layer > Animation::ANIMATION_LAYER_DEFAULT);

			const float global_speed_multiplier = GetObject().GetComponent< AnimationController >(static_ref.animation_controller_index)->GetGlobalSpeedMultiplier();
			const float animation_speed_multiplier = speed * global_speed_multiplier;
			TracksOnAnimationSpeedChange(tracks, animation_speed_multiplier, layer);
		}

		void ClientAnimationController::OnGlobalAnimationSpeedChange( const AnimationController &, const float new_speed, const float old_speed )
		{
			TracksOnGlobalAnimationSpeedChange( tracks, new_speed, old_speed );
		}

		void ClientAnimationController::OnAnimationPositionSet( const AnimationController&, const CurrentAnimation& animation )
		{
			assert(animation.layer == Animation::ANIMATION_LAYER_DEFAULT);
			
			// Check we are not receiving an event for an animation that we are not playing yet, or are not playing anymore (OOS?).
			if (current_animation.animation_index != animation.animation_index)
				return;

			TracksOnAnimationPositionSet(tracks, animation.elapsed_time);
			current_animation.elapsed_time = animation.elapsed_time;

			// note: this may cancel blending of current animation (sets weight to 1)
			RecalculateEventTimes();

			NotifyListeners(&ClientAnimationControllerListener::OnAnimationPositionSet, current_animation);
		}

		void ClientAnimationController::OnAnimationLayerPositionSet(const AnimationController& controller, const float time, const unsigned layer)
		{
			assert(layer > Animation::ANIMATION_LAYER_DEFAULT);
			TracksOnAnimationPositionSet(tracks, time, layer);
		}

		void ClientAnimationController::OnStartAnimationPositionProgressed( const AnimationController& controller )
		{
			if( current_animation.animation_index != controller.GetCurrentAnimation() )
				return;

			auto* sound_comp = GetObject().GetComponent< Animation::Components::SoundEvents >();
			sound_previously_enabled = sound_comp ? sound_comp->IsEnabled() : true;

			if( sound_comp )
				sound_comp->SetEnabled( false );
		}

		void ClientAnimationController::OnAnimationPositionProgressed( const AnimationController& controller, const float previous_time, const float new_time, const float desired_delta )
		{
			// Check we are not receiving an event for an animation that we are not playing yet, or are not playing anymore (OOS?).
			if( current_animation.animation_index != controller.GetCurrentAnimation() )
				return;

			float elapsed_time = new_time - previous_time;
			float delta = std::min( elapsed_time, desired_delta );
			unsigned num_divisions = static_cast< unsigned >( elapsed_time / delta );
			float current_time = previous_time;
			for( unsigned i = 0; i < num_divisions; ++i )
			{
				const float elapsed_time = new_time - current_time;
				const float delta = std::min( elapsed_time, desired_delta );
				AdvanceAnimations(delta);
				current_time += delta;
				NotifyListeners(&ClientAnimationControllerListener::OnAnimationPositionProgressed, current_animation, 0.f, delta);
			}

			if( auto* sound_comp = GetObject().GetComponent< Animation::Components::SoundEvents >() )
				sound_comp->SetEnabled( sound_previously_enabled );
		}

		void ClientAnimationController::OnAnimationTimeSpent( const AnimationController& controller, const float time )
		{
			frame_extra_time += time;
		}

		void ClientAnimationController::RecalculateEventTimes()
		{
			if (current_animation.animation_index == -1)
				return;
			
			auto animation_controller = GetObject().GetComponent< AnimationController >(static_ref.animation_controller_index);
			const auto& events = animation_controller->GetAnimationEvents(current_animation.animation_index);

			//Work out what the next event is now
			const size_t num_events = events.size();
			size_t i;
			for(i = 0; i < num_events; ++i)
			{
				const float event_time = events[i]->time;
				if(event_time >= current_animation.elapsed_time)
					break;
			}
			current_animation.next_event_index = static_cast< unsigned >(i == 0 ? num_events : i - 1);
			GenerateNextEventTime();
		}

		float ClientAnimationController::GetCurrentAnimationLength() const
		{
			if( current_animation.animation_index == -1 )
				return 0.0f;
			return current_animation.length();
		}

		void ClientAnimationController::ApplyFramePause( const float duration )
		{
			frame_pause_remaining = duration;
		}

		void ClientAnimationController::GetCurrentBoneLayout( std::vector< simd::vector3 >& positions_out, std::vector< unsigned >& indices_out ) const
	 	{
			static_ref.GetCurrentBoneLayout( &TracksGetTransformPalette(tracks)[ 0 ], positions_out, indices_out );

			for( auto& socket : static_ref.additional_sockets )
			{
				const auto mat = GetBoneTransform( socket.index );
				positions_out.emplace_back(  mat[3][0], mat[3][1], mat[3][2]  );
				indices_out.push_back( socket.parent_index );
				indices_out.push_back( socket.index );
			}
	 	}

		unsigned int ClientAnimationController::GetObjectBoneIndexByName( const Game::GameObject& object, const char* const bone_name )
		{
			const auto* const animated = object.GetComponent< Game::Components::Animated >();
			if( animated == nullptr )
				return Bones::Invalid;

			const auto* const animated_object = animated->GetAnimatedObject();
			if( animated_object == nullptr )
				return Bones::Invalid;

			const auto* const animation_controller = animated_object->GetComponent< ClientAnimationController >();
			if( animation_controller == nullptr )
				return Bones::Invalid;

			const auto bone_index = animation_controller->GetBoneIndexByName( bone_name );
			if( bone_index == Bones::Invalid )
				return Bones::Invalid;

			return bone_index;
		}

		simd::matrix ClientAnimationController::GetBoneTransform( const unsigned bone_index ) const
		{
			assert( bone_index < GetNumBonesAndSockets() );
			if( bone_index >= GetNumBonesAndSockets() )
			{
			#if !defined(PRODUCTION_CONFIGURATION) && !defined(MOBILE)
				throw std::runtime_error( "Out of bounds bone access in ClientAnimationController::GetBoneTransform" );
			#endif

				return simd::matrix::identity();
			}

			if( static_ref.IsSocket( bone_index ) )
			{
				auto& socket = static_ref.additional_sockets[bone_index - GetNumBones()];

				if( socket.parent_index == Bones::Invalid )
				{
					return socket.offset;
				}
				else
				{
					simd::matrix animation_matrix;
					if( !first_frame && bone_index < bone_info_indices.size() && bone_info_indices[socket.parent_index] != size_t( -1 ) )
						animation_matrix = physics_driven_bone_infos[bone_info_indices[socket.parent_index]].cached_matrix;
					else
						animation_matrix = TracksGetFinalTransformPalette( tracks )->data()[socket.parent_index];

					auto transformation_matrix = static_ref.GetBindSpaceTransform( socket.parent_index ) * animation_matrix;

					if( socket.ignore_parent_axis.any() || socket.ignore_parent_rotation.any() )
					{
						simd::vector3 translation, scale;
						simd::matrix rotation;
						transformation_matrix.decompose( translation, rotation, scale );

						if( socket.ignore_parent_axis.any() )
							translation = simd::vector3( socket.ignore_parent_axis[0] ? 0.0f : translation.x,
														 socket.ignore_parent_axis[1] ? 0.0f : translation.y,
														 socket.ignore_parent_axis[2] ? 0.0f : translation.z );

						if( socket.ignore_parent_rotation.any() )
						{
							simd::vector3 yaw_pitch_roll = rotation.yawpitchroll();

							yaw_pitch_roll = simd::vector3( socket.ignore_parent_rotation[0] ? 0.0f : yaw_pitch_roll.x,
															socket.ignore_parent_rotation[1] ? 0.0f : yaw_pitch_roll.y,
															socket.ignore_parent_rotation[2] ? 0.0f : yaw_pitch_roll.z );
							rotation = simd::matrix::yawpitchroll( yaw_pitch_roll );
						}

						transformation_matrix = simd::matrix::scale( scale ) * rotation * simd::matrix::translation( translation );
					}

					return socket.offset * transformation_matrix;
				}
			}

			simd::matrix animation_matrix;
			if( !first_frame && bone_index < bone_info_indices.size() && bone_info_indices[bone_index] != size_t( -1 ) )
				animation_matrix = physics_driven_bone_infos[bone_info_indices[bone_index]].cached_matrix;
			else
				animation_matrix = TracksGetFinalTransformPalette( tracks )->data()[bone_index];

			return static_ref.GetBindSpaceTransform( bone_index ) * animation_matrix;
		}

		simd::vector3 ClientAnimationController::GetBonePosition( const unsigned bone_index ) const
		{
			assert( bone_index < GetNumBonesAndSockets() );
			if( bone_index >= GetNumBonesAndSockets() )
			{
			#ifndef PRODUCTION_CONFIGURATION
				throw std::runtime_error( "Out of bounds bone access in ClientAnimationController::GetBonePosition" );
			#endif

				return simd::vector3( 0, 0, 0 );
			}

			const auto& mat = GetBoneTransform( bone_index );
			return mat.translation();
		}

		simd::vector3 ClientAnimationController::GetBoneWorldPosition( const unsigned bone_index ) const
		{
			const auto* animated_render = GetObject().GetComponent< Animation::Components::AnimatedRender >();
			assert( animated_render );
			return animated_render->GetTransform().mulpos( GetBonePosition( bone_index ) );
		}

		simd::vector3 ClientAnimationController::GetBoneBindPosition( const unsigned bone_index ) const
		{
			assert( bone_index < GetNumBonesAndSockets() );
			if( bone_index >= GetNumBonesAndSockets() )
			{
			#ifndef PRODUCTION_CONFIGURATION
				throw std::runtime_error( "Out of bounds bone access in ClientAnimationController::GetBoneBindPosition" );
			#endif

				return simd::vector3( 0, 0, 0 );
			}

			const auto& mat = static_ref.GetBindSpaceTransform( bone_index );
			return simd::vector3(mat[3][0], mat[3][1], mat[3][2]);
		}

		const ClientAnimationControllerStatic::Socket* ClientAnimationController::GetSocket( const unsigned bone_index ) const
		{
			if( bone_index >= GetNumBones() && bone_index < GetNumBonesAndSockets() )
				return &static_ref.additional_sockets[bone_index - GetNumBones()];
			return nullptr;
		}

		const unsigned ClientAnimationController::GetClosestBoneTo( const simd::vector3& world_position, const bool influence_bones_only /*= false*/, const uint32_t bone_group /*= -1*/, const uint32_t bone_group_exclude /*= -1*/ ) const
		{
			if( GetNumBonesAndSockets() == 0 )
				return Bones::Invalid;

			const Animation::Components::AnimatedRender* animated_render = GetObject( ).GetComponent< Animation::Components::AnimatedRender >( );
			assert( animated_render );

			//Put the world position in model space.
			auto world_to_model = animated_render->GetTransform().inverse();
			auto position_model_space = world_to_model.mulpos(world_position);
			return GetClosestBoneToLocalPoint( position_model_space, influence_bones_only, bone_group, bone_group_exclude );
		}

		const unsigned ClientAnimationController::GetClosestBoneToLocalPoint( const simd::vector3& local_position, const bool influence_bones_only /*= false*/, const uint32_t bone_group /*= -1*/, const uint32_t bone_group_exclude /*= -1*/ ) const
		{
			const auto* bone_groups = GetObject().GetComponent< BoneGroups >( static_ref.bone_groups_index );
			const BoneGroupsStatic::BoneGroup* exclude_group = bone_group_exclude < bone_groups->static_ref.NumBoneGroups() ? &bone_groups->static_ref.GetBoneGroup( bone_group_exclude ) : nullptr;

			unsigned int closest_bone_index = Bones::Invalid;
			float closest = std::numeric_limits< float >::max();

			auto check_closest = [&]( const unsigned index )
			{
				const auto distance_vec = GetBonePosition( index ) - local_position;
				const float distance_sq = distance_vec.sqrlen();

				if( exclude_group )
					for( auto bone : exclude_group->bone_indices )
						if( bone == index )
							return;

				if( distance_sq < closest )
				{
					closest = distance_sq;
					closest_bone_index = index;
				}
			};

			if( bone_groups && ( bone_group < bone_groups->static_ref.NumBoneGroups() ) )
			{
				// Specific bone group was specified; Only look in that bone group for the closest bone
				const auto& group = bone_groups->static_ref.GetBoneGroup( bone_group );
				for( auto bone : group.bone_indices )
				{
					if( influence_bones_only && bone == 0 )
						continue;

					check_closest( bone );
				}
			}
			else
			{
				const unsigned int start_bones = influence_bones_only ? 1 : 0;
				const unsigned int end_bones = static_cast< unsigned >( influence_bones_only ? GetNumInfluenceBones() : GetNumBonesAndSockets() );
				for (unsigned int i = start_bones; i < end_bones; ++i)
					check_closest( i );
			}


			return closest_bone_index;
		}

		void ClientAnimationController::AddVisualAnimationRemap( const unsigned source, const unsigned destination ) 
		{
			assert( !ContainsPairFirst( visual_animation_remaps, source ) ); 
			assert( !ContainsPairFirst( visual_animation_remaps, destination ) );
			visual_animation_remaps.emplace_back( source, destination ); 
		}

		void ClientAnimationController::RemoveVisualAnimationRemap( const unsigned source ) 
		{ 
			RemoveFirstIf( visual_animation_remaps, [source]( const auto& val ) { return val.first == source; } ); 
		}

		std::mutex ellipsoidAddMutex;
		std::mutex sphereAddMutex;
		std::mutex capsuleAddMutex;
		std::mutex boxAddMutex;

		Physics::Vector3f ScaleSize(Physics::Coords3f worldCoords, Physics::Vector3f unscaled_size)
		{
			return Physics::Vector3f(worldCoords.xVector.Len() * unscaled_size.x, worldCoords.yVector.Len() * unscaled_size.y, worldCoords.zVector.Len() * unscaled_size.z);
		}

		void ClientAnimationController::AddColliders(const SkinMesh* skinMesh, Physics::Coords3f worldCoords)
		{
			for (size_t animatedMeshIndex = 0; animatedMeshIndex < skinMesh->GetNumAnimatedMeshes(); animatedMeshIndex++)
			{
				auto &currMeshData = skinMesh->GetAnimatedMesh(animatedMeshIndex);
				auto &currMesh = currMeshData.mesh;
				auto &visibility = currMeshData.combined_collider_visibility;

				auto &meshPhysicalVertices = currMesh->GetPhysicalVertices();
				auto &meshPhysicalIndices = currMesh->GetPhysicalIndices();

				auto &meshCollisionEllipsoids = currMesh->GetCollisionEllipsoids();
				for (size_t ellipsoidIndex = 0; ellipsoidIndex < meshCollisionEllipsoids.size(); ellipsoidIndex++)
				{
					if (visibility.find(meshCollisionEllipsoids[ellipsoidIndex].name) != visibility.end()) continue;

					CollisionEllipsoid newbie;
					newbie.bone_index = meshCollisionEllipsoids[ellipsoidIndex].bone_index;
					auto bone_def_matrix = static_ref.skeleton->GetBoneDefMatrix(newbie.bone_index);
					newbie.rel_coords = Physics::MakeCoords3f(bone_def_matrix).GetLocalCoords(meshCollisionEllipsoids[ellipsoidIndex].coords);
					Physics::Vector3f radii = meshCollisionEllipsoids[ellipsoidIndex].radii;

					Physics::Coords3f resCoords = worldCoords.GetWorldCoords(meshCollisionEllipsoids[ellipsoidIndex].coords);
					{
						std::lock_guard<std::mutex> lock(ellipsoidAddMutex);
						newbie.unscaled_radii = radii;
						newbie.ellipsoid_handle = Physics::System::Get().GetParticleSystem()->AddEllipsoidObstacle(resCoords, ScaleSize(resCoords, radii));
					}
					collision_ellipsoids.push_back(std::move(newbie));
				}

				auto &meshCollisionSpheres = currMesh->GetCollisionSpheres();
				int existing_sphere_offset = static_cast< int >( collision_spheres.size() );
				std::vector<int> sphere_index_offsets(meshCollisionSpheres.size(), 0);
				auto increase_offset = [](int& value) { value += 1; };
				for (size_t sphereIndex = 0; sphereIndex < meshCollisionSpheres.size(); sphereIndex++)
				{
					if (visibility.find(meshCollisionSpheres[sphereIndex].name) != visibility.end())
					{
						std::for_each(sphere_index_offsets.begin() + sphereIndex + 1, sphere_index_offsets.end(), increase_offset);
						continue;
					}
					CollisionSphere newbie;
					newbie.bone_index = meshCollisionSpheres[sphereIndex].bone_index;

					Physics::Coords3f modelspaceCoords = Physics::Coords3f::defCoords();
					modelspaceCoords.pos = meshCollisionSpheres[sphereIndex].pos;

					auto bone_def_matrix = static_ref.skeleton->GetBoneDefMatrix(newbie.bone_index);
					newbie.rel_coords = Physics::MakeCoords3f(bone_def_matrix).GetLocalCoords(modelspaceCoords);
					float radius = meshCollisionSpheres[sphereIndex].radius;

					Physics::Coords3f resCoords = worldCoords.GetWorldCoords(modelspaceCoords);
					Physics::Vector3f scaledRadius = ScaleSize(resCoords, Physics::Vector3f(radius, radius, radius));

					{
						std::lock_guard<std::mutex> lock(sphereAddMutex);
						newbie.sphere_handle = Physics::System::Get().GetParticleSystem()->AddSphereObstacle(resCoords, (scaledRadius.x + scaledRadius.y + scaledRadius.z) / 3.0f);
						newbie.unscaled_radius = radius;
					}
					collision_spheres.push_back(std::move(newbie));
				}

				auto &meshCollisionCapsules = currMesh->GetCollisionCapsules();
				for (size_t capsuleIndex = 0; capsuleIndex < meshCollisionCapsules.size(); capsuleIndex++)
				{
					CollisionCapsule newbie;
					bool skip = false;
					for (size_t sphereNumber = 0; sphereNumber < 2; sphereNumber++)
					{
						int sphere_index = meshCollisionCapsules[capsuleIndex].sphere_indices[sphereNumber];
						if (visibility.find(meshCollisionSpheres[sphere_index].name) != visibility.end())
							skip = true;
						// sphere_index_offsets represents sphere index change after some spheres were disabled
						// existing_sphere_offset is the count of spheres already added to the collision_capsules by the previous AnimatedMeshData
						int offset = -sphere_index_offsets[sphere_index] + existing_sphere_offset;
						newbie.sphere_indices[sphereNumber] = sphere_index + offset;
					}

					if (skip) continue;

					{
						std::lock_guard<std::mutex> lock(capsuleAddMutex);
						newbie.capsule_handle = Physics::System::Get().GetParticleSystem()->AddCapsuleObstacle(
							collision_spheres[newbie.sphere_indices[0]].sphere_handle,
							collision_spheres[newbie.sphere_indices[1]].sphere_handle);
					}
					collision_capsules.push_back(std::move(newbie));
				}

				auto &meshCollisionBoxes = currMesh->GetCollisionBoxes();
				for (size_t boxIndex = 0; boxIndex < meshCollisionBoxes.size(); boxIndex++)
				{
					if (visibility.find(meshCollisionBoxes[boxIndex].name) != visibility.end()) continue;
					CollisionBox newbie;
					newbie.bone_index = meshCollisionBoxes[boxIndex].bone_index;
					auto bone_def_matrix = static_ref.skeleton->GetBoneDefMatrix(newbie.bone_index);
					newbie.rel_coords = Physics::MakeCoords3f(bone_def_matrix).GetLocalCoords(meshCollisionBoxes[boxIndex].coords);
					//these radius calculations are exact only in case of axis-aligned boxor in case of uniform scaling
					Physics::Vector3f half_size = meshCollisionBoxes[boxIndex].half_size;

					Physics::Coords3f resCoords = worldCoords.GetWorldCoords(meshCollisionBoxes[boxIndex].coords);
					Physics::Vector3f scaledHalfSize = ScaleSize(resCoords, half_size);

					{
						std::lock_guard<std::mutex> lock(boxAddMutex);
						newbie.box_handle = Physics::System::Get().GetParticleSystem()->AddBoxObstacle(resCoords, scaledHalfSize);
						newbie.unscaled_size = half_size;
					}
					collision_boxes.push_back(std::move(newbie));
				}
			}
		}

		void ClientAnimationController::RecreatePhysicalRepresentation(Physics::Coords3f worldCoords)
		{
			needs_physics_reset = true;
			collision_ellipsoids.clear();
			collision_spheres.clear();
			collision_capsules.clear();
			physics_driven_bone_infos.clear();
			bone_info_indices.clear();
			attachment_bone_infos.clear();
			skinned_physical_vertices.clear();
			vertex_def_positions.clear();
			deformable_mesh_handle = Physics::DeformableMeshHandle();

			const auto* animated_render = GetObject().GetComponent< AnimatedRender >();
			if (animated_render && animated_render->IsFlagSet( AnimatedRender::Flags::RenderingDisabled ) )
				return;

			this->prev_coords = worldCoords;

			auto skinMesh = GetObject().GetComponent<Animation::Components::SkinMesh>();
			if (skinMesh)
			{
				auto lock = skinMesh->LockAnimatedMeshes();

				std::vector<Physics::Vector3f> physicalVertexPositions;

				struct Influence
				{
					size_t vertexIndex;
					size_t bone_index;
					float weight;
				};

				std::vector<int> physicalIndices;
				std::vector<int> attachmentIndices;

				AddColliders(skinMesh, worldCoords);

				for (size_t animatedMeshIndex = 0; animatedMeshIndex < skinMesh->GetNumAnimatedMeshes(); animatedMeshIndex++)
				{
					auto &currMeshData = skinMesh->GetAnimatedMesh(animatedMeshIndex);
					auto &currMesh = currMeshData.mesh;
					auto &visibility = currMeshData.combined_collider_visibility;

					auto &meshPhysicalVertices    = currMesh->GetPhysicalVertices();
					auto &meshPhysicalIndices     = currMesh->GetPhysicalIndices();

					size_t vertexOffset = skinned_physical_vertices.size();
					for(size_t vertexIndex = 0; vertexIndex < meshPhysicalVertices.size(); vertexIndex++)
					{
						SkinnedPhysicalVertex skinnedVertex;
						for(size_t boneNumber = 0; boneNumber < SkinnedPhysicalVertex::bones_count; boneNumber++)
						{
							size_t bone_index = meshPhysicalVertices[vertexIndex].bone_indices[boneNumber];
							skinnedVertex.bone_indices[boneNumber] = bone_index;
							skinnedVertex.bone_weights[boneNumber] = meshPhysicalVertices[vertexIndex].bone_weights[boneNumber];
							auto bone_def_matrix = static_ref.skeleton->GetBoneDefMatrix(bone_index);
							skinnedVertex.local_positions[boneNumber] = Physics::MakeCoords3f(bone_def_matrix).GetLocalPoint(meshPhysicalVertices[vertexIndex].pos);
						}
						skinned_physical_vertices.push_back(skinnedVertex);
						physicalVertexPositions.push_back(worldCoords.GetWorldPoint(meshPhysicalVertices[vertexIndex].pos));
						vertex_def_positions.push_back(physicalVertexPositions.back());

					}
					for(size_t index = 0; index < meshPhysicalIndices.size(); index++)
					{
						physicalIndices.push_back( static_cast< int >( vertexOffset + meshPhysicalIndices[index] ) );
					}


					auto &meshAttachmentVertices = currMesh->GetPhysicalAttachmentVertices();
					for(size_t vertexNumber = 0; vertexNumber < meshAttachmentVertices.size(); vertexNumber++)
					{
						attachmentIndices.push_back( static_cast< int >( meshAttachmentVertices[vertexNumber] + vertexOffset ) );
					}
				}

				assert(skinned_physical_vertices.size() == physicalVertexPositions.size());

				if(physicalVertexPositions.size() > 0) //physical meshes present
				{
					deformable_mesh_handle = Physics::System::Get().AddDeformableMesh( //automatically removes current mesh if it already exists
						physicalVertexPositions.data(),
						physicalVertexPositions.size(),
						physicalIndices.data(),
						physicalIndices.size(),
						Physics::Coords3f::defCoords(),
						static_ref.linear_stiffness,
						static_ref.linear_allowed_contraction,
						static_ref.bend_stiffness,
						static_ref.bend_allowed_angle,
						static_ref.normal_viscosity_multiplier,
						static_ref.tangent_viscosity_multiplier,
						static_ref.enclosure_radius,
						static_ref.enclosure_angle,
						static_ref.enclosure_radius_additive,
						static_ref.animation_force_factor,
						static_ref.animation_velocity_factor,
						static_ref.animation_velocity_multiplier,
						static_ref.animation_max_velocity,
						static_ref.gravity,
						static_ref.enable_collisions);

					deformable_mesh_handle.Get()->SetPhysicsCallback(this);
					bone_info_indices.resize(static_ref.skeleton->GetNumInfluenceBones());
					for (size_t bone_index = 0; bone_index < static_ref.skeleton->GetNumInfluenceBones(); bone_index++)
					{
						bone_info_indices[bone_index] = size_t(-1);

						std::string boneName = static_ref.skeleton->GetBoneName(bone_index);

						if(boneName.find("phys_") != std::string::npos)
						{
							PhysicsDrivenBoneInfo newbie;
							newbie.bone_index = bone_index;
							newbie.control_point_id = size_t(-1);
							newbie.cached_matrix = simd::matrix::identity();

							bone_info_indices[bone_index] = physics_driven_bone_infos.size();
							physics_driven_bone_infos.push_back(newbie);
						}
					}

					struct BoneInfluences
					{
						std::vector<Physics::DeformableMesh::InfluencedVertex> influencedVertices;
					};
					std::vector<BoneInfluences> boneInfluences;
					boneInfluences.resize(physics_driven_bone_infos.size());

					for (size_t vertexIndex = 0; vertexIndex < skinned_physical_vertices.size(); vertexIndex++)
					{
						auto &skinnedVertex = skinned_physical_vertices[vertexIndex];
						for (size_t boneNumber = 0; boneNumber < skinnedVertex.bones_count; boneNumber++)
						{
							Physics::DeformableMesh::InfluencedVertex influencedVertex;
							influencedVertex.vertex_index = vertexIndex;
							influencedVertex.weight = skinnedVertex.bone_weights[boneNumber];
							if (influencedVertex.weight < 1e-3f)
								continue;
							size_t physicsBoneIndex = bone_info_indices[skinnedVertex.bone_indices[boneNumber]];
							if (physicsBoneIndex == size_t(-1))
								continue;

							boneInfluences[physicsBoneIndex].influencedVertices.push_back(influencedVertex);
						}
					}

					for (size_t physicsBoneIndex = 0; physicsBoneIndex < physics_driven_bone_infos.size(); physicsBoneIndex++)
					{
						size_t bone_index = physics_driven_bone_infos[physicsBoneIndex].bone_index;
						auto bone_def_matrix = static_ref.skeleton->GetBoneDefMatrix(bone_index);
						Physics::Coords3f boneCoords = worldCoords.GetWorldCoords(Physics::MakeCoords3f(bone_def_matrix));

						std::string boneName = static_ref.skeleton->GetBoneName(bone_index);
						physics_driven_bone_infos[physicsBoneIndex].is_skinned = (boneName.find("skinned") != std::string::npos);
						if (!physics_driven_bone_infos[physicsBoneIndex].is_skinned)
						{
							physics_driven_bone_infos[physicsBoneIndex].control_point_id = deformable_mesh_handle.Get()->SetRigidControlPoint(boneCoords);
						}
						else
						{
							physics_driven_bone_infos[physicsBoneIndex].control_point_id = deformable_mesh_handle.Get()->SetSkinnedControlPoint(
								boneCoords,
								boneInfluences[physicsBoneIndex].influencedVertices.data(),
								boneInfluences[physicsBoneIndex].influencedVertices.size());
						}
					}

					deformable_mesh_handle.Get()->SetAttachmentIndices(attachmentIndices.data(), attachmentIndices.size());
				}
			}

			// Adding colliders from the attached objects
			auto* attached_animated_object = GetObject().GetComponent< AttachedAnimatedObject >();
			if (attached_animated_object)
			{
				for (auto& object : attached_animated_object->AttachedObjects())
				{
					// Only add colliders from attached objects that don't have their own ClientAnimationController
					if (object.object->GetComponent<ClientAnimationController>())
						continue;

					if (const auto* childSkinMesh = object.object->GetComponent<Animation::Components::SkinMesh>())
					{
						auto lock = childSkinMesh->LockAnimatedMeshes();
						AddColliders(childSkinMesh, worldCoords);
					}
				}
			}
		}

		void ClientAnimationController::PhysicsRecreate()
		{
			Physics::Coords3f objectCoords = Physics::Coords3f::defCoords();
			const auto* animated_render = GetObject().GetComponent< AnimatedRender >();
			if(animated_render)
			{
				objectCoords = Physics::MakeCoords3f(animated_render->GetTransform());
			}
			RecreatePhysicalRepresentation(objectCoords);
		}


		void ClientAnimationController::ProcessMirroring()
		{
			Physics::Coords3f objectCoords = Physics::Coords3f::defCoords();
			const auto* animated_render = GetObject().GetComponent< AnimatedRender >();
			if(animated_render)
			{
				objectCoords = Physics::MakeCoords3f(animated_render->GetTransform());
			}
			float newCoordOrientation = Physics::MixedProduct(objectCoords.xVector, objectCoords.yVector, objectCoords.zVector);
			if(fabs(newCoordOrientation - this->coord_orientation) > 0.2f)
			{
				this->coord_orientation = newCoordOrientation;
				if (is_initialized) 
					Physics::System::Get().AddDeferredRecreateCallback(this);
			}
		}

		void ClientAnimationController::ProcessTeleportation()
		{
			if (skinned_physical_vertices.size() == 0)
				return;

			const auto* animated_render = GetObject().GetComponent< AnimatedRender >();
			if (!animated_render || !deformable_mesh_handle.IsValid()) return;

			const auto& animToWorldMatrix = animated_render->GetTransform();
			Physics::Coords3f animToWorld = Physics::MakeCoords3f(animToWorldMatrix);

			Physics::Vector3f currCenterPos = animToWorld.pos;
			float teleportationThreshold = 300.0f; //more than 3m movement per frame is teleportation
			if ((prev_coords.pos - animToWorld.pos).Len() > teleportationThreshold)
			{
				ProcessVertexDefPositions();
				deformable_mesh_handle.Get()->RestorePosition();
			}
		}
		void ClientAnimationController::ProcessAttachedCollisionGeometry()
		{
			const auto* animated_render = GetObject().GetComponent< AnimatedRender >();
			assert(animated_render);
			const auto& animToWorldMatrix = animated_render->GetTransform();

			auto GetAttachedWorldCoords = [this, animToWorldMatrix](int bone_index, Physics::Coords3f rel_coords) -> Physics::Coords3f
			{
				auto boneDefMatrix = static_ref.skeleton->GetBoneDefMatrix(bone_index);// *final_transform_palette[collision_ellipsoids[ellipsoidIndex].bone_index];
				auto& boneDefToAnimationMatrix = TracksGetFinalTransformPalette(tracks)->data()[bone_index];// *final_transform_palette[collision_ellipsoids[ellipsoidIndex].bone_index];

				Physics::Coords3f currBoneWorldPosition = Physics::MakeCoords3f(boneDefMatrix * boneDefToAnimationMatrix * animToWorldMatrix);
				Physics::Coords3f dstWorldCoords = currBoneWorldPosition.GetWorldCoords(rel_coords);
				return dstWorldCoords;
			};

			for (size_t ellipsoidIndex = 0; ellipsoidIndex < collision_ellipsoids.size(); ellipsoidIndex++)
			{
				Physics::Coords3f dstWorldCoords = GetAttachedWorldCoords( static_cast< int >( collision_ellipsoids[ellipsoidIndex].bone_index ), collision_ellipsoids[ellipsoidIndex].rel_coords);
				Physics::Vector3f scaledRadii = ScaleSize(dstWorldCoords, collision_ellipsoids[ellipsoidIndex].unscaled_radii);

				collision_ellipsoids[ellipsoidIndex].ellipsoid_handle.Get()->SetCoords(dstWorldCoords);
				collision_ellipsoids[ellipsoidIndex].ellipsoid_handle.Get()->SetRadii(scaledRadii);
			}
			for (size_t sphereIndex = 0; sphereIndex < collision_spheres.size(); sphereIndex++)
			{
				Physics::Coords3f dstWorldCoords = GetAttachedWorldCoords( static_cast< int >( collision_spheres[sphereIndex].bone_index ), collision_spheres[sphereIndex].rel_coords);
				Physics::Vector3f scaledRadius = ScaleSize(dstWorldCoords, Physics::Vector3f(1.0f, 1.0f, 1.0f) * collision_spheres[sphereIndex].unscaled_radius);
				collision_spheres[sphereIndex].sphere_handle.Get()->SetCoords(dstWorldCoords);
				collision_spheres[sphereIndex].sphere_handle.Get()->SetRadius((scaledRadius.x + scaledRadius.y + scaledRadius.z) / 3.0f);
			}
			for (size_t boxIndex = 0; boxIndex < collision_boxes.size(); boxIndex++)
			{
				Physics::Coords3f dstWorldCoords = GetAttachedWorldCoords( static_cast< int >( collision_boxes[boxIndex].bone_index ), collision_boxes[boxIndex].rel_coords);
				Physics::Vector3f scaledHalfSize = ScaleSize(dstWorldCoords, collision_boxes[boxIndex].unscaled_size);
				collision_boxes[boxIndex].box_handle.Get()->SetCoords(dstWorldCoords);
				collision_boxes[boxIndex].box_handle.Get()->SetHalfSize(scaledHalfSize);
			}
		}


		simd::matrix ClientAnimationController::GetBoneToWorldMatrix(size_t bone_index) const
		{
			const auto* animated_render = GetObject().GetComponent< AnimatedRender >();
			assert(animated_render);
			simd::matrix anim_to_world = animated_render->GetTransform();
			auto bone_def = static_ref.skeleton->GetBoneDefMatrix(bone_index);
			auto& bone_def_to_anim = TracksGetFinalTransformPalette(tracks)->data()[bone_index];
			auto bone_to_world = bone_def * bone_def_to_anim * anim_to_world;
			return bone_to_world;
		}
		void ClientAnimationController::SetBoneToWorldMatrix(size_t bone_index, simd::matrix bone_to_world) const
		{
			const auto* animated_render = GetObject().GetComponent< AnimatedRender >();
			assert(animated_render);
			simd::matrix anim_to_world = animated_render->GetTransform();
			simd::matrix model_to_anim;
			{
				auto bone_to_model = static_ref.skeleton->GetBoneDefMatrix(bone_index);
				auto model_to_bind_bone = bone_to_model.inverse();
				auto world_to_anim = anim_to_world.inverse();

				model_to_anim = model_to_bind_bone * bone_to_world * world_to_anim;
			}

			TracksSetFinalTransformPalette(tracks, model_to_anim, static_cast< unsigned >( bone_index ) );
		}

		static simd::matrix MatrixLerp(simd::matrix src, simd::matrix dst, float ratio)
		{
			simd::vector3 src_translation, src_scale;
			simd::matrix src_rotation;
			src.decompose(src_translation, src_rotation, src_scale);

			simd::vector3 dst_translation, dst_scale;
			simd::matrix dst_rotation;
			dst.decompose(dst_translation, dst_rotation, dst_scale);

			auto res_translation = src_translation.lerp(dst_translation, ratio);
			auto res_rotation = src_rotation;// src_rotation.slerp(dst_rotation, ratio);
			auto res_scale = src_scale.lerp(dst_scale, ratio);

			return simd::matrix::scale(res_scale) * res_rotation * simd::matrix::translation(res_translation); //as always, multiplication order is transposed.
		}

		void ClientAnimationController::SetMorphingRatio(float ratio)
		{
			morphing_info.ratio = Clamp( ratio, 0.0f, 1.0f );
		}
		float ClientAnimationController::GetMorphingRatio() const
		{
			return morphing_info.ratio;
		}

		void ClientAnimationController::StartMorphing( const std::weak_ptr< AnimatedObject >& dst_object_weak, const MorphType morph_type, const bool is_src, const std::map< std::string, std::string >* manual_mapping )
		{
			morphing_info.dst_object = dst_object_weak;
			morphing_info.morph_type = morph_type;
			morphing_info.is_src = is_src;

			if (auto dst_object = dst_object_weak.lock())
			{
				const auto* dst_animated_render = dst_object->GetComponent< AnimatedRender >();
				auto *dst_client_animation = dst_object->GetComponent< ClientAnimationController >();

				if (!dst_animated_render || !dst_client_animation)
					return;

				std::map<std::string, size_t> dst_bone_name_to_index;
				for (size_t bone_index = 0; bone_index < dst_client_animation->GetNumBones(); bone_index++)
				{
					std::string name = LowercaseString(dst_client_animation->GetBoneNameByIndex( static_cast< unsigned >( bone_index ) ));
					dst_bone_name_to_index[name] = bone_index;
				}

				if( manual_mapping )
				{
					if( manual_mapping->size() != static_ref.skeleton->GetNumBones() )
						LOG_CRIT_F( L"Manual mapping size mismatch (expected {}, have {}) when morphing from {} to {}", static_ref.skeleton->GetNumBones(), manual_mapping->size(), GetObject().GetFileName(), dst_object->GetFileName() );
					morphing_info.bone_morphing_infos.resize( static_ref.skeleton->GetNumBones() );
					for( const auto& mapping : *manual_mapping )
					{
						const auto bone_index = GetBoneIndexByName( mapping.first );
						if( bone_index >= morphing_info.bone_morphing_infos.size() )
						{
							LOG_CRIT_F( L"Invalid source bone {} in manual mapping when morphing from {} to {}", StringToWstring( mapping.first ), GetObject().GetFileName(), dst_object->GetFileName() );
							continue;
						}
						auto& bone_info = morphing_info.bone_morphing_infos[bone_index];

						auto it = dst_bone_name_to_index.find( LowercaseString( mapping.second ) );
						if( it == dst_bone_name_to_index.end() )
						{
							LOG_CRIT_F( L"Invalid destination bone {} in manual mapping when morphing from {} to {}", StringToWstring( mapping.second ), GetObject().GetFileName(), dst_object->GetFileName() );
							continue;
						}
						bone_info.dst_bone_index = it->second;
					}
				}
				else
				{
					morphing_info.bone_morphing_infos.resize( static_ref.skeleton->GetNumBones() );
					for( size_t bone_index = 0; bone_index < static_ref.skeleton->GetNumBones(); bone_index++ )
					{
						auto &bone_info = morphing_info.bone_morphing_infos[ bone_index ];

						simd::matrix src_bone_to_world = GetBoneToWorldMatrix( bone_index );
						auto src_bone_world_pos = src_bone_to_world.mulpos( simd::vector3( 0.0f, 0.0f, 0.0f ) );

						auto bone_name = LowercaseString( GetBoneNameByIndex( static_cast< unsigned >( bone_index ) ) );
						/*auto it = dst_bone_name_to_index.find(bone_name);
						if (it != dst_bone_name_to_index.end())
						{
							bone_info.dst_bone_index = it->second;
						}
						else*/
						{
							bone_info.dst_bone_index = dst_client_animation->GetClosestBoneTo( src_bone_world_pos );
						}
					}
				}

				ProcessMorphingBones();
			}
		}

		float Smoothstep(float x)
		{
			//return 3.0f * x * x - 2.0f * x * x * x;
			return (x < 0.5f) ? pow(x / 0.5f, 10.0f) * 0.5f : (0.5f + 0.5f * (1.0f - pow((1.0f - x) / 0.5f, 10.0f)));
		}

		float saturate(float val)
		{
			return std::min(std::max(val, 0.0f), 1.0f);
		}
		float Smoothstep(float val, float center, float curve)
		{
			float power = 2.0f / std::max(1e-3f, 1.0f - curve) - 1.0f;
			return 1.0f - center + 
			(
				-pow(saturate(1.0f - std::max(center, val)), power) * pow(saturate(1.0f - center), -(power - 1.0f)) +
				 pow(saturate(       std::min(center, val)), power) * pow(saturate(       center), -(power - 1.0f))
			);
		}

		float ApplyMorphCurve(float ratio, MorphType morph_type, float bone_hash)
		{
			switch(morph_type)
			{
				case MorphType::Linear:
				{
					return ratio;
				}break;
				case MorphType::Pow:
				{
					float p = 1.0f / (1.0f + 30.0f * bone_hash);
					return pow(std::max(0.0f, ratio), p);
				}break;
				case MorphType::Smoothstep:
				{
					return Smoothstep(ratio);
				}break;
				case MorphType::Chunky:
				{
					//float p = 1.0f / (1.0f + 30.0f * bone_hash);
					//float p = 1.0f + 30.0f * bone_hash;
					//return pow(std::max(0.0f, ratio), p);
					return Smoothstep(ratio, bone_hash * 0.7f + 0.0f, 0.7f);
				}break;
			}
			return -1.0f;
		}

		void ClientAnimationController::ProcessMorphingBones()
		{
			auto dst_object = morphing_info.dst_object.lock();
			if (!dst_object)
				return;

			const auto* dst_animated_render = dst_object->GetComponent< AnimatedRender >();
			auto *dst_client_animation = dst_object->GetComponent< ClientAnimationController >();

			if (!dst_animated_render || !dst_client_animation || !is_initialized || !dst_client_animation->is_initialized)
				return;

			for( size_t src_bone_index = 0; src_bone_index < static_ref.skeleton->GetNumBones(); src_bone_index++ )
			{
				size_t dst_bone_index = morphing_info.bone_morphing_infos[src_bone_index].dst_bone_index;

				simd::matrix dst_bone_to_world = dst_client_animation->GetBoneToWorldMatrix( dst_bone_index );
				simd::matrix src_bone_to_world = GetBoneToWorldMatrix( src_bone_index );

				auto src_bone_to_bind = GetSkeleton()->GetBoneDefMatrix(src_bone_index);
				auto bone_center = src_bone_to_bind.mulpos(simd::vector3(0.0f, 0.0f, 0.0f));


				float ratio = morphing_info.ratio;

				float bone_hash = 0.5f * sin(bone_center.z / 20.0f) + 0.5f;
				float res_ratio = morphing_info.is_src ? ApplyMorphCurve(ratio, morphing_info.morph_type, bone_hash) : (1.0f - ApplyMorphCurve(1.0f - ratio, morphing_info.morph_type, bone_hash));

				simd::matrix interp_src_bone_to_world = MatrixLerp(src_bone_to_world, dst_bone_to_world, Clamp(res_ratio, 0.0f, 1.0f));
				SetBoneToWorldMatrix(src_bone_index, interp_src_bone_to_world);
			}
		}

		void ClientAnimationController::ProcessParentAttachedBones()
		{
			if (!deformable_mesh_handle.Get())
				return;

			if (static_ref.attachment_bones.parent_bones.empty() || static_ref.attachment_bones.child_bone_group == "")
				return;

			auto* child_animated_render = GetObject().GetComponent< AnimatedRender >();
			const auto* animation_controller = GetObject().GetComponent< AnimationController >();

			if (!animation_controller || !child_animated_render)
				return;

			auto* parent_client_animation = child_animated_render->GetParentController();
			if (!parent_client_animation)
				return;

			auto* parent_object = &(parent_client_animation->GetObject());
			const auto* parent_animated_render = parent_object->GetComponent< AnimatedRender >();
			const auto* child_bone_groups = GetObject().GetComponent< BoneGroups >();
			if (!child_bone_groups)
				return;

			simd::matrix parent_anim_to_world_matrix = parent_animated_render->GetTransform();
			simd::matrix child_anim_to_world_matrix = child_animated_render->GetTransform();

			int child_bone_group_index = child_bone_groups->static_ref.BoneGroupIndexByName(static_ref.attachment_bones.child_bone_group);
			if (child_bone_group_index == -1)
				return;

			const auto& child_bone_group = child_bone_groups->static_ref.GetBoneGroup(child_bone_group_index);

			size_t attach_bones_count = child_bone_group.bone_indices.size();
			if (static_ref.attachment_bones.parent_bones.size() != attach_bones_count)
				return;

			auto& parent_skeleton = parent_client_animation->GetSkeleton();
			for (size_t bone_number = 0; bone_number < attach_bones_count; bone_number++)
			{
				size_t parent_bone_index = parent_skeleton->GetBoneIndexByName(static_ref.attachment_bones.parent_bones[bone_number]);
				size_t child_bone_index = child_bone_group.bone_indices[bone_number];

				Physics::Coords3f parent_bone_world_pos;
				if(IsBoneArrayIndex(parent_bone_index) && parent_bone_index < parent_skeleton->GetNumBones())
				{
					auto bone_def_matrix = parent_skeleton->GetBoneDefMatrix(parent_bone_index);
					auto& bone_def_to_animation_matrix = TracksGetFinalTransformPalette(parent_client_animation->tracks)->data()[parent_bone_index];
					parent_bone_world_pos = Physics::MakeCoords3f(bone_def_matrix * bone_def_to_animation_matrix * parent_anim_to_world_matrix);
				}
				else
				{
					assert(!!"Invalid bone attachment group in parent object");
				}

				simd::matrix child_bone_matrix;
				if (IsBoneArrayIndex(child_bone_index) && child_bone_index < GetSkeleton()->GetNumBones())
				{
					auto bone_to_model = GetSkeleton()->GetBoneDefMatrix(child_bone_index);
					auto model_to_bind_bone = bone_to_model.inverse();
					auto parent_bone_to_world = Physics::MakeDXMatrix(parent_bone_world_pos);
					auto model_to_world = child_anim_to_world_matrix;
					auto world_to_model = model_to_world.inverse();

					child_bone_matrix = model_to_bind_bone * parent_bone_to_world * world_to_model;
				}
				else
				{
					assert(!!"Invalid bone attachment group in child object");
				}

				TracksSetFinalTransformPalette(tracks, child_bone_matrix, static_cast< unsigned >( child_bone_index ) );
			}
		}

		void ClientAnimationController::ProcessParentAttachedBoneGroups()
		{
			if (!deformable_mesh_handle.Get())
				return;

			if (static_ref.attachment_groups.parent_bone_group == "" || static_ref.attachment_groups.child_bone_group == "")
				return;

			auto* child_animated_render = GetObject().GetComponent< AnimatedRender >();
			const auto* animation_controller = GetObject().GetComponent< AnimationController >();

			if (!animation_controller || !child_animated_render)
				return;

			auto* parent_client_animation = child_animated_render->GetParentController();
			if (!parent_client_animation)
				return;

			auto* parent_object = &(parent_client_animation->GetObject());
			const auto* parent_animated_render = parent_object->GetComponent< AnimatedRender >();
			const auto* parent_bone_groups = parent_object->GetComponent< BoneGroups >();
			const auto* child_bone_groups = GetObject().GetComponent< BoneGroups >();
			if (!parent_bone_groups || !child_bone_groups)
				return;

			simd::matrix parent_anim_to_world_matrix = parent_animated_render->GetTransform();
			simd::matrix child_anim_to_world_matrix = child_animated_render->GetTransform();

			int parent_bone_group_index = parent_bone_groups->static_ref.BoneGroupIndexByName(static_ref.attachment_groups.parent_bone_group);
			if (parent_bone_group_index == -1)
				return;
			int child_bone_group_index = child_bone_groups->static_ref.BoneGroupIndexByName(static_ref.attachment_groups.child_bone_group);
			if (child_bone_group_index == -1)
				return;

			const BoneGroupsStatic::BoneGroup &parent_bone_group = parent_bone_groups->static_ref.GetBoneGroup(parent_bone_group_index);
			const BoneGroupsStatic::BoneGroup &child_bone_group = child_bone_groups->static_ref.GetBoneGroup(child_bone_group_index);

			if (parent_bone_group.bone_indices.size() != child_bone_group.bone_indices.size())
				return;

			size_t attach_bones_count = parent_bone_group.bone_indices.size();

			for (size_t bone_number = 0; bone_number < attach_bones_count; bone_number++)
			{
				size_t parent_bone_index = parent_bone_group.bone_indices[bone_number];
				size_t child_bone_index = child_bone_group.bone_indices[bone_number];

				Physics::Coords3f parent_bone_world_pos;
				auto parent_skeleton = parent_client_animation->GetSkeleton();
				if(IsBoneArrayIndex(parent_bone_index) && parent_bone_index < parent_skeleton->GetNumBones())
				{
					auto bone_def_matrix = parent_skeleton->GetBoneDefMatrix(parent_bone_index);
					auto& bone_def_to_animation_matrix = TracksGetFinalTransformPalette(parent_client_animation->tracks)->data()[parent_bone_index];
					parent_bone_world_pos = Physics::MakeCoords3f(bone_def_matrix * bone_def_to_animation_matrix * parent_anim_to_world_matrix);
				}
				else
				{
					assert(!!"Invalid bone attachment group in parent object");
				}

				simd::matrix child_bone_matrix;
				if (IsBoneArrayIndex(child_bone_index) && child_bone_index < GetSkeleton()->GetNumBones())
				{
					auto bone_to_model = GetSkeleton()->GetBoneDefMatrix(child_bone_index);
					auto model_to_bind_bone = bone_to_model.inverse();
					auto parent_bone_to_world = Physics::MakeDXMatrix(parent_bone_world_pos);
					auto model_to_world = child_anim_to_world_matrix;
					auto world_to_model = model_to_world.inverse();

					child_bone_matrix = model_to_bind_bone * parent_bone_to_world * world_to_model;
				}
				else
				{
					assert(!!"Invalid bone attachment group in child object");
				}

				TracksSetFinalTransformPalette(tracks, child_bone_matrix, static_cast< unsigned >( child_bone_index ) );
			}
		}

		void ClientAnimationController::ProcessAttachedVertices()
		{
			deformable_mesh_handle.Get()->ProcessAttachedVertices();

			/*auto animated_render = GetObject().GetComponent<AnimatedRender>();
			Physics::Coords3f animWorldCoords = Physics::MakeCoords3f(animated_render->GetTransform());

			std::vector<int> attachmentIndices;
			std::vector<Physics::Vector3f> attachmentPoints;
			for (size_t attachmentPoint = 0; attachmentPoint < attachment_bone_infos.size(); attachmentPoint++)
			{
				Physics::Vector3f animatedBonePos = Physics::MakeVector3f(GetBonePosition(attachment_bone_infos[attachmentPoint].bone_index));

				Physics::Vector3f attachmentWorldPos = animWorldCoords.GetWorldPoint(animatedBonePos);
				//transform = client_animation_controller->GetBoneTransform(object.bone_index) * transform;

				size_t vertexIndex = attachment_bone_infos[attachmentPoint].vertexIndex;
				//deformable_mesh_handle.Get()->AttachVertex(vertexIndex, attachmentWorldPos);
				deformable_mesh_handle.Get()->AttachVertex(vertexIndex, vertex_def_positions[vertexIndex]);
			}*/
		}

		void ClientAnimationController::ProcessVertexDefPositions()
		{
			if(skinned_physical_vertices.size() == 0)
				return;

			const auto* animated_render = GetObject().GetComponent< AnimatedRender >();
			const auto& animToWorldMatrix = animated_render->GetTransform();
			Physics::Coords3f animToWorld = Physics::MakeCoords3f(animToWorldMatrix);

			prev_coords = animToWorld;
			//float scale = animToWorld.GetWorldVector(Physics::Vector3f(1.0f, 0.0f, 0.0f)).Len();
			vertex_def_positions.resize(skinned_physical_vertices.size());

			assert(animated_render);
			for (size_t vertexIndex = 0; vertexIndex < skinned_physical_vertices.size(); vertexIndex++)
			{
				Physics::Vector3f localDefPosition = Physics::Vector3f::zero();
				for(size_t boneNumber = 0; boneNumber < SkinnedPhysicalVertex::bones_count; boneNumber++)
				{
					size_t bone_index = skinned_physical_vertices[vertexIndex].bone_indices[boneNumber];
					float boneWeight = skinned_physical_vertices[vertexIndex].bone_weights[boneNumber];
					Physics::Vector3f localPos = skinned_physical_vertices[vertexIndex].local_positions[boneNumber];
					//D3DXMATRIX boneTransformMatrix = GetBoneTransform(collision_ellipsoids[ellipsoidIndex].bone_index);
					auto boneDefMatrix = GetSkeleton()->GetBoneDefMatrix(bone_index);// *final_transform_palette[collision_ellipsoids[ellipsoidIndex].bone_index];
					auto& boneDefToAnimationMatrix = TracksGetFinalTransformPalette(tracks)->data()[bone_index];// *final_transform_palette[collision_ellipsoids[ellipsoidIndex].bone_index];

					//Physics::Coords3f currBoneWorldPosition = Physics::MakeCoords3f(boneDefMatrix * boneDefToAnimationMatrix * animToWorldMatrix);
					Physics::Coords3f currBoneAnimPosition = Physics::MakeCoords3f(boneDefMatrix * boneDefToAnimationMatrix);

					localDefPosition += currBoneAnimPosition.GetWorldPoint(localPos) * boneWeight;
				}
				vertex_def_positions[vertexIndex] = animToWorld.GetWorldPoint(localDefPosition);
			}
			this->deformable_mesh_handle.Get()->UpdateVertexDefPositions(vertex_def_positions.data(), vertex_def_positions.size());
		}

		void ClientAnimationController::ProcessPhysicsDrivenBones()
		{
			if(!deformable_mesh_handle.Get())
				return;

			const auto* animated_render = GetObject().GetComponent< AnimatedRender >();

			Physics::Coords3f animToWorld = Physics::MakeCoords3f(animated_render->GetTransform());

			for (size_t boneNumber = 0; boneNumber < physics_driven_bone_infos.size(); boneNumber++)
			{
				size_t bone_index = physics_driven_bone_infos[boneNumber].bone_index;

				Physics::Coords3f boneWorldCoords = deformable_mesh_handle.Get()->GetControlPoint(physics_driven_bone_infos[boneNumber].control_point_id);

				//to support scaling in animation
				if (physics_driven_bone_infos[boneNumber].is_skinned)
				{
					auto boneDefMatrix = GetSkeleton()->GetBoneDefMatrix(bone_index);
					auto& boneDefToAnimationMatrix = TracksGetFinalTransformPalette(tracks)->data()[bone_index];
					Physics::Coords3f refBoneWorldCoords = Physics::MakeCoords3f(boneDefMatrix * boneDefToAnimationMatrix);
					boneWorldCoords.zVector = boneWorldCoords.zVector.GetNorm() * refBoneWorldCoords.zVector.Len();
				}

				auto boneToModel = GetSkeleton()->GetBoneDefMatrix(bone_index);
				auto modelToBindBone = boneToModel.inverse();
				auto physBoneToWorld = Physics::MakeDXMatrix(boneWorldCoords);
				auto modelToWorld = animated_render->GetTransform();
				auto worldToModel = modelToWorld.inverse();

				simd::matrix animationMatrix = modelToBindBone * physBoneToWorld * worldToModel;
				physics_driven_bone_infos[boneNumber].cached_matrix = animationMatrix;
				TracksSetFinalTransformPalette(tracks, animationMatrix, static_cast< unsigned >( bone_index ) );
			}
		}
		
		void ClientAnimationController::ReloadParameters() //for asset viewer only
		{
			if(deformable_mesh_handle.Get())
			{
				this->PhysicsRecreate();
			}
		}

		void ClientAnimationController::ResetCurrentAnimation()
		{
			current_animation.animation_index = -1;
			current_animation.elapsed_time = 0.0f;
			current_animation.next_event_index = 0;
			current_animation.next_event_time = 0.0f;
			current_animation.speed_multiplier = 1.0f;
			current_animation.begin_time = 0.0f;
			current_animation.end_time = 0.0f;
			current_animation.has_movement_data = false;
			current_animation.layer = ANIMATION_LAYER_DEFAULT;
		}

		void ClientAnimationController::GetComponentInfo( std::wstringstream& stream, const bool recursive, const unsigned recursive_depth ) const
		{
			const auto* animation_controller = GetObject().GetComponent< AnimationController >( static_ref.animation_controller_index );

			const auto animation_indices = TracksGetLayeredAnimationIndices( tracks );

			for( const auto& [layer, animation_index] : animation_indices )
			{
				stream << StreamHelp::NTabs( recursive_depth ) << L"Animation Layer: " << layer << L"\n";
				stream << StreamHelp::NTabs( recursive_depth ) << L"Animation: " << StringToWstring( animation_controller->GetAnimationMetadata( animation_index ).name ) << L"\n";

			}
		}

	}
}
