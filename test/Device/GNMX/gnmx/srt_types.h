/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 08.508.001
 * Copyright (C) 2014 Sony Interactive Entertainment Inc.
 */

#ifndef _SCE_SHADER_SRT_TYPES_H
#define _SCE_SHADER_SRT_TYPES_H 1
#ifdef __cplusplus

namespace sce
{
	namespace Gnm
	{
		class Texture;
		class Buffer;
		class Sampler;
	}

namespace Shader
{
namespace Srt
{

template<typename T> using Texture1D			= sce::Gnm::Texture;
template<typename T> using Texture2D			= sce::Gnm::Texture;
template<typename T> using MS_Texture2D			= sce::Gnm::Texture;
template<typename T> using TextureCube			= sce::Gnm::Texture;
template<typename T> using Texture3D			= sce::Gnm::Texture;
template<typename T> using Texture1D_Array		= sce::Gnm::Texture;
template<typename T> using Texture2D_Array		= sce::Gnm::Texture;
template<typename T> using MS_Texture2D_Array	= sce::Gnm::Texture;
template<typename T> using TextureCube_Array	= sce::Gnm::Texture;
template<typename T> using RW_Texture1D			= sce::Gnm::Texture;
template<typename T> using RW_Texture2D			= sce::Gnm::Texture;
template<typename T> using RW_TextureCube		= sce::Gnm::Texture;
template<typename T> using RW_Texture3D			= sce::Gnm::Texture;
template<typename T> using RW_Texture1D_Array	= sce::Gnm::Texture;
template<typename T> using RW_Texture2D_Array	= sce::Gnm::Texture;
template<typename T> using RW_TextureCube_Array	= sce::Gnm::Texture;
template<typename T> using DataBuffer			= sce::Gnm::Buffer;
template<typename T> using RegularBuffer		= sce::Gnm::Buffer;
template<typename T> using RW_DataBuffer		= sce::Gnm::Buffer;
template<typename T> using RW_RegularBuffer		= sce::Gnm::Buffer;

typedef sce::Gnm::Buffer	ByteBuffer;
typedef sce::Gnm::Buffer	RW_ByteBuffer;
typedef sce::Gnm::Buffer	ConstantBuffer;
typedef sce::Gnm::Sampler	SamplerState;

} // namespace Srt {
} // namespace Shader {
} // namespace sce {

#endif // __cplusplus
#endif // _SCE_SHADER_SRT_TYPES_H
