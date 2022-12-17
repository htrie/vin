﻿/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 08.508.001
* Copyright (C) 2016 Sony Interactive Entertainment Inc.
*/

#ifndef _SCE_GNMX_SHADER_PARSER_H
#define _SCE_GNMX_SHADER_PARSER_H

#if !defined(DOXYGEN_IGNORE)
#if defined(__ORBIS__) || defined(SCE_SHADER_BINARY_INTERNAL_STATIC_EXPORT)
#	define SCE_SHADER_BINARY_EXPORT   
#else
#	if defined( SCE_SHADER_BINARY_DLL_EXPORT )
#		define SCE_SHADER_BINARY_EXPORT __declspec( dllexport )
#	else
#		define SCE_SHADER_BINARY_EXPORT __declspec( dllimport )
#	endif
#endif
#endif

#include "shaderbinary.h"

namespace sce
{
	namespace Gnmx
	{
		/** @brief Encapsulates the pointers to a shader's code and its Gnmx shader object.

			@see parseShader(), parseGsShader()
		*/
		class ShaderInfo
		{
		public:

			union
			{
				const void *m_shaderStruct;		///< A pointer to the shader struct - typeless.

				Gnmx::VsShader *m_vsShader;		///< A pointer to the shader struct - used for VS shaders.
				Gnmx::PsShader *m_psShader;		///< A pointer to the shader struct - used for PS shaders.
				Gnmx::CsShader *m_csShader;		///< A pointer to the shader struct - used for CS shaders.
				Gnmx::LsShader *m_lsShader;		///< A pointer to the shader struct - used for LS shaders.
				Gnmx::HsShader *m_hsShader;		///< A pointer to the shader struct - used for HS shaders.
				Gnmx::EsShader *m_esShader;		///< A pointer to the shader struct - used for ES shaders.
				Gnmx::GsShader *m_gsShader;		///< A pointer to the shader struct - used for GS shaders.
			};

			const uint32_t *m_gpuShaderCode;		///< A pointer to the GPU Shader Code which will need to be copied into GPU visible memory.
			uint32_t m_gpuShaderCodeSize;			///< The size of the GPU Shader Code in bytes.
			uint32_t m_reserved;
		};


		/** @brief
			Extracts, from an <c>.sb</c> file, a pointer to the shader object, 
			and, for the GPU Shader Code that must be copied to GPU visible memory, a pointer to
			the code and its size in bytes.
			
			@param[out] shaderInfo		Receives the shader information. This pointer must not be <c>NULL</c>.
			@param[in] data				A pointer to the contents of an <c>.sb</c> file in memory. This pointer must not be <c>NULL</c>.
		*/
		SCE_SHADER_BINARY_EXPORT void parseShader(ShaderInfo* shaderInfo, const void* data);

		/** @brief
			Extracts, from an <c>.sb</c> file, shader information regarding the GS shader as well
			as shader information for its associated VS copy shader.  
			
			@param[out] vsShaderInfo		Receives the information for the GS shader. This pointer must not be <c>NULL</c>.
			@param[out] gsShaderInfo		Receives the information for the VS shader. This pointer must not be <c>NULL</c>.
			@param[in] data				A pointer to the contents of an <c>.sb</c> file in memory. This pointer must not be <c>NULL</c>.
		*/
		SCE_SHADER_BINARY_EXPORT void parseGsShader(ShaderInfo* gsShaderInfo, ShaderInfo* vsShaderInfo, const void* data);
	}
}

#endif // _SCE_GNMX_SHADER_PARSER_H
