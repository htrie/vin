/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 08.508.001
* Copyright (C) 2015 Sony Interactive Entertainment Inc.
*/

#ifndef _SCE_PSSL_TYPES_H_
#define _SCE_PSSL_TYPES_H_

#if !defined(DOXYGEN_IGNORE)

#if !defined(SHACC_PLATFORM_HPP)
	#include "host_stdint.h"
#endif // !defined(SHACC_PLATFORM_HPP)

#if !defined(DOXYGEN_IGNORE)
#if defined(__ORBIS__) || defined(SCE_SHADER_BINARY_INTERNAL_STATIC_EXPORT) || defined(__linux__)
#	define SCE_SHADER_BINARY_EXPORT   
#else
#	if defined( SCE_SHADER_BINARY_DLL_EXPORT )
#		define SCE_SHADER_BINARY_EXPORT __declspec( dllexport )
#	else
#		define SCE_SHADER_BINARY_EXPORT __declspec( dllimport )
#	endif
#endif
#endif

namespace sce
{
namespace Shader
{
namespace Binary
{
typedef enum PsslType
{
	kTypeFloat1,
	kTypeFloat2,
	kTypeFloat3,
	kTypeFloat4,
	kTypeHalf1,
	kTypeHalf2,
	kTypeHalf3,
	kTypeHalf4,
	kTypeInt1,
	kTypeInt2,
	kTypeInt3,
	kTypeInt4,
	kTypeUint1,
	kTypeUint2,
	kTypeUint3,
	kTypeUint4,
	kTypeDouble1,
	kTypeDouble2,
	kTypeDouble3,
	kTypeDouble4,
	kTypeFloat1x1,
	kTypeFloat2x1,
	kTypeFloat3x1,
	kTypeFloat4x1,
	kTypeFloat1x2,
	kTypeFloat2x2,
	kTypeFloat3x2,
	kTypeFloat4x2,
	kTypeFloat1x3,
	kTypeFloat2x3,
	kTypeFloat3x3,
	kTypeFloat4x3,
	kTypeFloat1x4,
	kTypeFloat2x4,
	kTypeFloat3x4,
	kTypeFloat4x4,
	kTypeHalf1x1,
	kTypeHalf2x1,
	kTypeHalf3x1,
	kTypeHalf4x1,
	kTypeHalf1x2,
	kTypeHalf2x2,
	kTypeHalf3x2,
	kTypeHalf4x2,
	kTypeHalf1x3,
	kTypeHalf2x3,
	kTypeHalf3x3,
	kTypeHalf4x3,
	kTypeHalf1x4,
	kTypeHalf2x4,
	kTypeHalf3x4,
	kTypeHalf4x4,
	kTypeInt1x1,
	kTypeInt2x1,
	kTypeInt3x1,
	kTypeInt4x1,
	kTypeInt1x2,
	kTypeInt2x2,
	kTypeInt3x2,
	kTypeInt4x2,
	kTypeInt1x3,
	kTypeInt2x3,
	kTypeInt3x3,
	kTypeInt4x3,
	kTypeInt1x4,
	kTypeInt2x4,
	kTypeInt3x4,
	kTypeInt4x4,
	kTypeUint1x1,
	kTypeUint2x1,
	kTypeUint3x1,
	kTypeUint4x1,
	kTypeUint1x2,
	kTypeUint2x2,
	kTypeUint3x2,
	kTypeUint4x2,
	kTypeUint1x3,
	kTypeUint2x3,
	kTypeUint3x3,
	kTypeUint4x3,
	kTypeUint1x4,
	kTypeUint2x4,
	kTypeUint3x4,
	kTypeUint4x4,
	kTypeDouble1x1,
	kTypeDouble2x1,
	kTypeDouble3x1,
	kTypeDouble4x1,
	kTypeDouble1x2,
	kTypeDouble2x2,
	kTypeDouble3x2,
	kTypeDouble4x2,
	kTypeDouble1x3,
	kTypeDouble2x3,
	kTypeDouble3x3,
	kTypeDouble4x3,
	kTypeDouble1x4,
	kTypeDouble2x4,
	kTypeDouble3x4,
	kTypeDouble4x4,
	kTypePoint,
	kTypeLine,
	kTypeTriangle,
	kTypeAdjacentline,
	kTypeAdjacenttriangle,
	kTypePatch,
	kTypeStructure,
	kTypeLong1,
	kTypeLong2,
	kTypeLong3,
	kTypeLong4,
	kTypeUlong1,
	kTypeUlong2,
	kTypeUlong3,
	kTypeUlong4,
	kTypeLong1x1,
	kTypeLong2x1,
	kTypeLong3x1,
	kTypeLong4x1,
	kTypeLong1x2,
	kTypeLong2x2,
	kTypeLong3x2,
	kTypeLong4x2,
	kTypeLong1x3,
	kTypeLong2x3,
	kTypeLong3x3,
	kTypeLong4x3,
	kTypeLong1x4,
	kTypeLong2x4,
	kTypeLong3x4,
	kTypeLong4x4,
	kTypeUlong1x1,
	kTypeUlong2x1,
	kTypeUlong3x1,
	kTypeUlong4x1,
	kTypeUlong1x2,
	kTypeUlong2x2,
	kTypeUlong3x2,
	kTypeUlong4x2,
	kTypeUlong1x3,
	kTypeUlong2x3,
	kTypeUlong3x3,
	kTypeUlong4x3,
	kTypeUlong1x4,
	kTypeUlong2x4,
	kTypeUlong3x4,
	kTypeUlong4x4,
	kTypeBool1,
	kTypeBool2,
	kTypeBool3,
	kTypeBool4,
	kTypeTexture,
	kTypeSamplerState,
	kTypeBuffer,
	kTypeUshort1,
	kTypeUshort2,
	kTypeUshort3,
	kTypeUshort4,
	kTypeUchar1,
	kTypeUchar2,
	kTypeUchar3,
	kTypeUchar4,
	kTypeShort1,
	kTypeShort2,
	kTypeShort3,
	kTypeShort4,
	kTypeChar1,
	kTypeChar2,
	kTypeChar3,
	kTypeChar4,
	kTypeVoid, // for overlapped unused ConstantBuffer member
	kTypeTextureR128,
	kTypeTypeEnd
} PsslType;

typedef enum PsslSemantic
{
	kSemanticPosition,
	kSemanticNormal,
	kSemanticColor,
	kSemanticBinormal,
	kSemanticTangent,
	kSemanticTexcoord0,
	kSemanticTexcoord1,
	kSemanticTexcoord2,
	kSemanticTexcoord3,
	kSemanticTexcoord4,
	kSemanticTexcoord5,
	kSemanticTexcoord6,
	kSemanticTexcoord7,
	kSemanticTexcoord8,
	kSemanticTexcoord9,
	kSemanticTexcoordEnd,
	kSemanticImplicit,
	kSemanticNonreferencable,
	kSemanticClip,
	kSemanticFog,
	kSemanticPointsize,
	kSemanticSPointSize = kSemanticPointsize,
	kSemanticFragcoord,
	kSemanticTarget0,
	kSemanticTarget1,
	kSemanticTarget2,
	kSemanticTarget3,
	kSemanticTarget4,
	kSemanticTarget5,
	kSemanticTarget6,
	kSemanticTarget7,
	kSemanticTarget8,
	kSemanticTarget9,
	kSemanticTarget10,
	kSemanticTarget11,
	kSemanticDepth,
	kSemanticLastcg,
	kSemanticUserDefined,
	kSemanticSClipDistance,
	kSemanticSCullDistance,
	kSemanticSCoverage,
	kSemanticSDepthOutput,
	kSemanticSDispatchthreadId,
	kSemanticSDispatchThreadId = kSemanticSDispatchthreadId,
	kSemanticSDomainLocation,
	kSemanticSGroupId,
	kSemanticSGroupIndex,
	kSemanticSGroupThreadId,
	kSemanticSPosition,
	kSemanticSVertexId,
	kSemanticSInstanceId,
	kSemanticSSampleIndex,
	kSemanticSPrimitiveId,
	kSemanticSGsinstanceId,
	kSemanticSOutputControlPointId,
	kSemanticSFrontFace,
	kSemanticSRenderTargetIndex,
	kSemanticSViewportIndex,
	kSemanticSTargetOutput,
	kSemanticSEdgeTessFactor,
	kSemanticSInsideTessFactor,
	kSemanticSpriteCoord,
	kSemanticSPointCoord = kSemanticSpriteCoord,
	kSemanticSDepthGEOutput,
	kSemanticSDepthLEOutput,
	kSemanticSVertexOffsetId,
	kSemanticSInstanceOffsetId,
	kSemanticSStencilValue,
	kSemanticSStencilOp,
	kSemanticSemanticEnd 
} PsslSemantic;

typedef enum PsslFragmentInterpType
{
	kFragmentInterpTypeSampleNone,
	kFragmentInterpTypeSampleLinear,
	kFragmentInterpTypeSamplePoint,
	kFragmentInterpTypeFragmentInterpTypeLast 
} PsslFragmentInterpType;

typedef enum PsslBufferType
{
	kBufferTypeDataBuffer,
	kBufferTypeTexture1d,
	kBufferTypeTexture2d,
	kBufferTypeTexture3d,
	kBufferTypeTextureCube,
	kBufferTypeTexture1dArray,
	kBufferTypeTexture2dArray,
	kBufferTypeTextureCubeArray,
	kBufferTypeMsTexture2d,
	kBufferTypeMsTexture2dArray,
	kBufferTypeRegularBuffer,
	kBufferTypeByteBuffer,
	kBufferTypeRwDataBuffer,
	kBufferTypeRwTexture1d,
	kBufferTypeRwTexture2d,
	kBufferTypeRwTexture3d,
	kBufferTypeRwTexture1dArray,
	kBufferTypeRwTexture2dArray,
	kBufferTypeRwRegularBuffer,
	kBufferTypeRwByteBuffer,
	kBufferTypeAppendBuffer,
	kBufferTypeConsumeBuffer,
	kBufferTypeConstantBuffer,
	kBufferTypeTextureBuffer,
	kBufferTypePointBuffer,
	kBufferTypeLineBuffer,
	kBufferTypeTriangleBuffer,
	kBufferTypeSRTBuffer,
	kBufferTypeDispatchDrawBuffer,
	kBufferTypeRwTextureCube,
	kBufferTypeRwTextureCubeArray,
	kBufferTypeRwMsTexture2d,
	kBufferTypeRwMsTexture2dArray,
	kBufferTypeTexture1dR128,
	kBufferTypeTexture2dR128,
	kBufferTypeMsTexture2dR128,
	kBufferTypeRwTexture1dR128,
	kBufferTypeRwTexture2dR128,
	kBufferTypeRwMsTexture2dR128,
	kBufferTypeBufferTypeLast 
} PsslBufferType;

typedef enum PsslInternalBufferType
{
	kInternalBufferTypeUav,
	kInternalBufferTypeSrv,
	kInternalBufferTypeLds,
	kInternalBufferTypeGds,
	kInternalBufferTypeCbuffer,
	kInternalBufferTypeTextureSampler,
	kInternalBufferTypeInternal,
	kInternalBufferTypeInternalBufferTypeLast 
} PsslInternalBufferType;

typedef enum PsslShaderType
{
	kShaderTypeVsShader,
	kShaderTypeFsShader,
	kShaderTypeCsShader,
	kShaderTypeGsShader,
	kShaderTypeHsShader,
	kShaderTypeDsShader,
	kShaderTypeShaderTypeLast 
} PsslShaderType;

typedef enum PsslCodeType
{
	kCodeTypeIl,
	kCodeTypeIsa,
	kCodeTypeScu,
	kCodeTypeCodeTypeLast 
} PsslCodeType;

//NOTE: this should generally remain corresponding with the values of sce::Gnmx::CompilerType + 1 in shaderbinaryinfo.h
typedef enum PsslCompilerType	//NOTE: stored 4 bits unsigned
{
	kCompilerTypeUnspecified	= 0,	//unspecified : pre SDK 2.500 version of some compiler
	kCompilerTypeOrbisPsslc		= 1,	//orbis-psslc shader compiler
	kCompilerTypeOrbisEsslc		= 2,	//orbis-esslc shader compiler
	kCompilerTypeOrbisWave		= 3,	//orbis-wave shader compiler
	kCompilerTypeOrbisCuAs		= 4,	//orbis-cu-as shader assembler
	kCompilerTypeLast
} PsslCompilerType;

typedef enum PsslStatus
{
	kStatusFail = -1,
	kStatusOk   =  0,
	kStatusStatusLast 
} PsslStatus;

typedef enum PsslComponentMask
{
	kComponentMask,
	kComponentMaskX,
	kComponentMaskY,
	kComponentMaskXy,
	kComponentMaskZ,
	kComponentMaskXZ,
	kComponentMaskYz,
	kComponentMaskXyz,
	kComponentMaskW,
	kComponentMaskXW,
	kComponentMaskYW,
	kComponentMaskXyW,
	kComponentMaskZw,
	kComponentMaskXZw,
	kComponentMaskYzw,
	kComponentMaskXyzw,
	kComponentMaskComponentMaskLast 
} PsslComponentMask;

typedef enum PsslGsIoType
{
	kGsIoTypeGsTri,
	kGsIoTypeGsLine,
	kGsIoTypeGsPoint,
	kGsIoTypeGsAdjTri,
	kGsIoTypeGsAdjLine,
	kGsIoTypeGsIoTypeLast 
} PsslGsIoType;

typedef enum PsslHsTopologyType
{
	kHsTopologyTypeHsPoint,
	kHsTopologyTypeHsLine,
	kHsTopologyTypeHsCwtri,
	kHsTopologyTypeHsCcwtri,
	kHsTopologyTypeHsTopologyTypeLast 
} PsslHsTopologyType; 

typedef enum PsslHsPartitioningType
{
	kHsPartitioningTypeHsInteger,
	kHsPartitioningTypeHsPowerOf2,
	kHsPartitioningTypeHsOddFactorial,
	kHsPartitioningTypeHsEvenFactorial,
	kHsPartitioningTypeHsPartitioningTypeLast 
} PsslHsPartitioningType;

typedef enum PsslHsDsPatchType
{
	kHsDsPatchTypeHsDsIsoline,
	kHsDsPatchTypeHsDsTri,
	kHsDsPatchTypeHsDsQuad,
	kHsDsPatchTypeHsDsPatchTypeLast 
} PsslHsDsPatchType; 

typedef enum PsslVertexVariant
{
	kVertexVariantVertex, 
	kVertexVariantExport, 
	kVertexVariantLocal, 
	kVertexVariantDispatchDraw,
	kVertexVariantExportOnChip, 
	kVertexVariantLast 
} PsslVertexVariant;

typedef enum PsslHullVariant
{
	kHullVariantOnChip,
	kHullVariantOnBuffer,
	kHullVariantDynamic, 
	kHullVariantLast 
} PsslHullVariant;

typedef enum PsslDomainVariant
{
	kDomainVariantVertex, 
	kDomainVariantExport, 
	kDomainVariantVertexHsOnBuffer, 
	kDomainVariantExportHsOnBuffer, 
	kDomainVariantVertexHsDynamic, 
	kDomainVariantExportHsDynamic, 
	kDomainVariantExportOnChip,
	kDomainVariantLast 
} PsslDomainVariant;

typedef enum PsslGeometryVariant
{
	kGeometryVariantOnBuffer, 
	kGeometryVariantOnChip, 
	kGeometryVariantLast 
} PsslGeometryVariant;

// return the element count from type
// return 0 for error
static inline uint32_t getPsslTypeElementCount( PsslType type )
{
	switch( type )
	{
	// vector types
	case kTypeFloat1:
	case kTypeInt1:
	case kTypeHalf1:
	case kTypeUint1:
	case kTypeDouble1:
	case kTypeLong1:
	case kTypeUlong1:
	case kTypeBool1:
	case kTypeUchar1:
	case kTypeUshort1:
	case kTypeChar1:
	case kTypeShort1:
		return 1;
	case kTypeFloat2:
	case kTypeInt2:
	case kTypeHalf2:
	case kTypeUint2:
	case kTypeDouble2:
	case kTypeLong2:
	case kTypeUlong2:
	case kTypeBool2:
	case kTypeUchar2:
	case kTypeUshort2:
	case kTypeChar2:
	case kTypeShort2:
		return 2;
	case kTypeFloat3:
	case kTypeInt3:
	case kTypeHalf3:
	case kTypeUint3:
	case kTypeDouble3:
	case kTypeLong3:
	case kTypeUlong3:
	case kTypeBool3:
	case kTypeUchar3:
	case kTypeUshort3:
	case kTypeChar3:
	case kTypeShort3:
		return 3;
	case kTypeFloat4:
	case kTypeInt4:
	case kTypeHalf4:
	case kTypeUint4:
	case kTypeDouble4:
	case kTypeLong4:
	case kTypeUlong4:
	case kTypeBool4:
	case kTypeUchar4:
	case kTypeUshort4:
	case kTypeChar4:
	case kTypeShort4:
		return 4;
	// matrix types
	case kTypeFloat1x1:
	case kTypeInt1x1:
	case kTypeHalf1x1:
	case kTypeUint1x1:
	case kTypeDouble1x1:
	case kTypeLong1x1:
	case kTypeUlong1x1:
		return 1;
	case kTypeFloat2x1:
	case kTypeInt2x1:
	case kTypeHalf2x1:
	case kTypeUint2x1:
	case kTypeDouble2x1:
	case kTypeLong2x1:
	case kTypeUlong2x1:
		return 2;
	case kTypeFloat3x1:
	case kTypeInt3x1:
	case kTypeHalf3x1:
	case kTypeUint3x1:
	case kTypeDouble3x1:
	case kTypeLong3x1:
	case kTypeUlong3x1:
		return 3;
	case kTypeFloat4x1:
	case kTypeInt4x1:
	case kTypeHalf4x1:
	case kTypeUint4x1:
	case kTypeDouble4x1:
	case kTypeLong4x1:
	case kTypeUlong4x1:
		return 4;
	case kTypeFloat1x2:
	case kTypeInt1x2:
	case kTypeHalf1x2:
	case kTypeUint1x2:
	case kTypeDouble1x2:
	case kTypeLong1x2:
	case kTypeUlong1x2:
		return 2;
	case kTypeFloat2x2:
	case kTypeInt2x2:
	case kTypeHalf2x2:
	case kTypeUint2x2:
	case kTypeDouble2x2:
	case kTypeLong2x2:
	case kTypeUlong2x2:
		return 4;
	case kTypeFloat3x2:
	case kTypeInt3x2:
	case kTypeHalf3x2:
	case kTypeUint3x2:
	case kTypeDouble3x2:
	case kTypeLong3x2:
	case kTypeUlong3x2:
		return 6;
	case kTypeFloat4x2:
	case kTypeInt4x2:
	case kTypeHalf4x2:
	case kTypeUint4x2:
	case kTypeDouble4x2:
	case kTypeLong4x2:
	case kTypeUlong4x2:
		return 8;
	case kTypeFloat1x3:
	case kTypeInt1x3:
	case kTypeHalf1x3:
	case kTypeUint1x3:
	case kTypeDouble1x3:
	case kTypeLong1x3:
	case kTypeUlong1x3:
		return 3;
	case kTypeFloat2x3:
	case kTypeInt2x3:
	case kTypeHalf2x3:
	case kTypeUint2x3:
	case kTypeDouble2x3:
	case kTypeLong2x3:
	case kTypeUlong2x3:
		return 6;
	case kTypeFloat3x3:
	case kTypeInt3x3:
	case kTypeHalf3x3:
	case kTypeUint3x3:
	case kTypeDouble3x3:
	case kTypeLong3x3:
	case kTypeUlong3x3:
		return 9;
	case kTypeFloat4x3:
	case kTypeInt4x3:
	case kTypeHalf4x3:
	case kTypeUint4x3:
	case kTypeDouble4x3:
	case kTypeLong4x3:
	case kTypeUlong4x3:
		return 12;
	case kTypeFloat1x4:
	case kTypeInt1x4:
	case kTypeHalf1x4:
	case kTypeUint1x4:
	case kTypeDouble1x4:
	case kTypeLong1x4:
	case kTypeUlong1x4:
		return 4;
	case kTypeFloat2x4:
	case kTypeInt2x4:
	case kTypeHalf2x4:
	case kTypeUint2x4:
	case kTypeDouble2x4:
	case kTypeLong2x4:
	case kTypeUlong2x4:
		return 8;
	case kTypeFloat3x4:
	case kTypeInt3x4:
	case kTypeHalf3x4:
	case kTypeUint3x4:
	case kTypeDouble3x4:
	case kTypeLong3x4:
	case kTypeUlong3x4:
		return 12;
	case kTypeFloat4x4:
	case kTypeInt4x4:
	case kTypeHalf4x4:
	case kTypeUint4x4:
	case kTypeDouble4x4:
	case kTypeLong4x4:
	case kTypeUlong4x4:
		return 16;
	default:
		return 0;
	}
}

static inline bool getPsslTypeInfo(PsslType type, PsslType &out_baseType, unsigned int &out_numComponents, unsigned int &out_numMatrixRows)
{
	out_baseType = type;
	out_numComponents = 1;
	out_numMatrixRows = 0;
	if (type <= kTypeDouble4x4 && type >= kTypeFloat1) {
		out_numComponents = 1 + ((type - kTypeFloat1) & 0x3);
		if (type <= kTypeDouble4) {
			if (type <= kTypeFloat4) // && type >= kTypeFloat1)
				out_baseType = kTypeFloat1;
			else if (type <= kTypeUint4 && type >= kTypeInt1)
				out_baseType = (type <= kTypeInt4) ? kTypeInt1 : kTypeUint1;
			else if (type <= kTypeHalf4 && type >= kTypeHalf1)
				out_baseType = kTypeHalf1;
			else //if (type >= kTypeDouble1 && type <= kTypeDouble4)
				out_baseType = kTypeDouble1;
		} else {
			out_numMatrixRows = (1 + (((type - kTypeFloat1x1)/4) & 0x3));
			if (type <= kTypeFloat4x4) // && type >= kTypeFloat1x1)
				out_baseType = kTypeFloat1;
			else if (type <= kTypeUint4x4 && type >= kTypeInt1x1)
				out_baseType = (type <= kTypeInt4x4) ? kTypeInt1 : kTypeUint1;
			else if (type <= kTypeHalf4x4 && type >= kTypeHalf1x1)
				out_baseType = kTypeHalf1;
			else //if (type <= kTypeDouble4x4 && type >= kTypeDouble1x1)
				out_baseType = kTypeDouble1;
		}
	} else if (type <= kTypeBool4 && type >= kTypeLong1) {
		out_numComponents = 1 + ((type - kTypeLong1) & 0x3);
		if (type <= kTypeUlong4) // && type >= kTypeLong1)
			out_baseType = (type <= kTypeLong4) ? kTypeLong1 : kTypeUlong1;
		else if (type >= kTypeBool1) // && type <= kTypeBool4)
			out_baseType = kTypeBool1;
		else //if (type <= kTypeUlong4x4 && type >= kTypeLong1x1)
		{
			out_numMatrixRows = (1 + (((type - kTypeLong1x1)/4) & 0x3));
			out_baseType = (type <= kTypeLong4x4) ? kTypeLong1 : kTypeUlong1;
		}
	} else if (type >= kTypeUshort1 && type <= kTypeChar4) {
		out_numComponents = 1 + ((type - kTypeUshort1) & 0x3);
		if (type <= kTypeUshort4)
			out_baseType = kTypeUshort1;
		else if (type <= kTypeUchar4)
			out_baseType = kTypeUchar1;
		else if (type <= kTypeShort4)
			out_baseType = kTypeShort1;
		else
			out_baseType = kTypeChar1;
	} else {
		switch (type) {
		case kTypePoint:				break;
		case kTypeLine:					out_baseType = kTypePoint, out_numComponents = 2;	break;	//NOTE: we use out_baseType = kTypePoint to indicate a 16-bit unsigned index
		case kTypeTriangle:				out_baseType = kTypePoint, out_numComponents = 3;	break;
		case kTypeAdjacentline:			out_baseType = kTypePoint, out_numComponents = 4;	break;
		case kTypeAdjacenttriangle:		out_baseType = kTypePoint, out_numComponents = 6;	break;
		case kTypePatch:				out_baseType = kTypePoint;							break;	//NOTE: a patch does not indicate the number of components
		case kTypeStructure:			break;
		case kTypeTexture:				break;
		case kTypeSamplerState:			break;
		case kTypeBuffer:				break;
		case kTypeVoid:					break;
		case kTypeTextureR128:			break;
		default:
			return false;
		}
	}
	return true;
}

static inline const char * getPsslTypeString( PsslType type )
{
	switch( type )
	{
	case kTypeFloat1:           return "kTypeFloat1"; 
	case kTypeFloat2:           return "kTypeFloat2"; 
	case kTypeFloat3:           return "kTypeFloat3"; 
	case kTypeFloat4:           return "kTypeFloat4"; 
	case kTypeHalf1:            return "kTypeHalf1"; 
	case kTypeHalf2:            return "kTypeHalf2"; 
	case kTypeHalf3:            return "kTypeHalf3"; 
	case kTypeHalf4:            return "kTypeHalf4"; 
	case kTypeInt1:             return "kTypeInt1"; 
	case kTypeInt2:             return "kTypeInt2"; 
	case kTypeInt3:             return "kTypeInt3"; 
	case kTypeInt4:             return "kTypeInt4"; 
	case kTypeUint1:            return "kTypeUint1"; 
	case kTypeUint2:            return "kTypeUint2"; 
	case kTypeUint3:            return "kTypeUint3"; 
	case kTypeUint4:            return "kTypeUint4"; 
	case kTypeDouble1:          return "kTypeDouble1"; 
	case kTypeDouble2:          return "kTypeDouble2"; 
	case kTypeDouble3:          return "kTypeDouble3"; 
	case kTypeDouble4:          return "kTypeDouble4"; 
	case kTypeFloat1x1:         return "kTypeFloat1x1"; 
	case kTypeFloat2x1:         return "kTypeFloat2x1"; 
	case kTypeFloat3x1:         return "kTypeFloat3x1"; 
	case kTypeFloat4x1:         return "kTypeFloat4x1"; 
	case kTypeFloat1x2:         return "kTypeFloat1x2"; 
	case kTypeFloat2x2:         return "kTypeFloat2x2"; 
	case kTypeFloat3x2:         return "kTypeFloat3x2"; 
	case kTypeFloat4x2:         return "kTypeFloat4x2"; 
	case kTypeFloat1x3:         return "kTypeFloat1x3"; 
	case kTypeFloat2x3:         return "kTypeFloat2x3"; 
	case kTypeFloat3x3:         return "kTypeFloat3x3"; 
	case kTypeFloat4x3:         return "kTypeFloat4x3"; 
	case kTypeFloat1x4:         return "kTypeFloat1x4"; 
	case kTypeFloat2x4:         return "kTypeFloat2x4"; 
	case kTypeFloat3x4:         return "kTypeFloat3x4"; 
	case kTypeFloat4x4:         return "kTypeFloat4x4"; 
	case kTypeHalf1x1:          return "kTypeHalf1x1"; 
	case kTypeHalf2x1:          return "kTypeHalf2x1"; 
	case kTypeHalf3x1:          return "kTypeHalf3x1"; 
	case kTypeHalf4x1:          return "kTypeHalf4x1"; 
	case kTypeHalf1x2:          return "kTypeHalf1x2"; 
	case kTypeHalf2x2:          return "kTypeHalf2x2"; 
	case kTypeHalf3x2:          return "kTypeHalf3x2"; 
	case kTypeHalf4x2:          return "kTypeHalf4x2"; 
	case kTypeHalf1x3:          return "kTypeHalf1x3"; 
	case kTypeHalf2x3:          return "kTypeHalf2x3"; 
	case kTypeHalf3x3:          return "kTypeHalf3x3"; 
	case kTypeHalf4x3:          return "kTypeHalf4x3"; 
	case kTypeHalf1x4:          return "kTypeHalf1x4"; 
	case kTypeHalf2x4:          return "kTypeHalf2x4"; 
	case kTypeHalf3x4:          return "kTypeHalf3x4"; 
	case kTypeHalf4x4:          return "kTypeHalf4x4"; 
	case kTypeInt1x1:           return "kTypeInt1x1"; 
	case kTypeInt2x1:           return "kTypeInt2x1"; 
	case kTypeInt3x1:           return "kTypeInt3x1"; 
	case kTypeInt4x1:           return "kTypeInt4x1"; 
	case kTypeInt1x2:           return "kTypeInt1x2"; 
	case kTypeInt2x2:           return "kTypeInt2x2"; 
	case kTypeInt3x2:           return "kTypeInt3x2"; 
	case kTypeInt4x2:           return "kTypeInt4x2"; 
	case kTypeInt1x3:           return "kTypeInt1x3"; 
	case kTypeInt2x3:           return "kTypeInt2x3"; 
	case kTypeInt3x3:           return "kTypeInt3x3"; 
	case kTypeInt4x3:           return "kTypeInt4x3"; 
	case kTypeInt1x4:           return "kTypeInt1x4"; 
	case kTypeInt2x4:           return "kTypeInt2x4"; 
	case kTypeInt3x4:           return "kTypeInt3x4"; 
	case kTypeInt4x4:           return "kTypeInt4x4"; 
	case kTypeUint1x1:          return "kTypeUint1x1"; 
	case kTypeUint2x1:          return "kTypeUint2x1"; 
	case kTypeUint3x1:          return "kTypeUint3x1"; 
	case kTypeUint4x1:          return "kTypeUint4x1"; 
	case kTypeUint1x2:          return "kTypeUint1x2"; 
	case kTypeUint2x2:          return "kTypeUint2x2"; 
	case kTypeUint3x2:          return "kTypeUint3x2"; 
	case kTypeUint4x2:          return "kTypeUint4x2"; 
	case kTypeUint1x3:          return "kTypeUint1x3"; 
	case kTypeUint2x3:          return "kTypeUint2x3"; 
	case kTypeUint3x3:          return "kTypeUint3x3"; 
	case kTypeUint4x3:          return "kTypeUint4x3"; 
	case kTypeUint1x4:          return "kTypeUint1x4"; 
	case kTypeUint2x4:          return "kTypeUint2x4"; 
	case kTypeUint3x4:          return "kTypeUint3x4"; 
	case kTypeUint4x4:          return "kTypeUint4x4"; 
	case kTypeDouble1x1:         return "kTypeDouble1x1"; 
	case kTypeDouble2x1:         return "kTypeDouble2x1"; 
	case kTypeDouble3x1:         return "kTypeDouble3x1"; 
	case kTypeDouble4x1:         return "kTypeDouble4x1"; 
	case kTypeDouble1x2:         return "kTypeDouble1x2"; 
	case kTypeDouble2x2:         return "kTypeDouble2x2"; 
	case kTypeDouble3x2:         return "kTypeDouble3x2"; 
	case kTypeDouble4x2:         return "kTypeDouble4x2"; 
	case kTypeDouble1x3:         return "kTypeDouble1x3"; 
	case kTypeDouble2x3:         return "kTypeDouble2x3"; 
	case kTypeDouble3x3:         return "kTypeDouble3x3"; 
	case kTypeDouble4x3:         return "kTypeDouble4x3"; 
	case kTypeDouble1x4:         return "kTypeDouble1x4"; 
	case kTypeDouble2x4:         return "kTypeDouble2x4"; 
	case kTypeDouble3x4:         return "kTypeDouble3x4"; 
	case kTypeDouble4x4:         return "kTypeDouble4x4"; 
	case kTypePoint:            return "kTypePoint"; 
	case kTypeLine:             return "kTypeLine"; 
	case kTypeTriangle:         return "kTypeTriangle"; 
	case kTypeAdjacentline:     return "kTypeAdjacentline"; 
	case kTypeAdjacenttriangle: return "kTypeAdjacenttriangle"; 
	case kTypePatch:            return "kTypePatch"; 
	case kTypeStructure:        return "kTypeStructure";	
	case kTypeLong1:             return "kTypeLong1"; 
	case kTypeLong2:             return "kTypeLong2"; 
	case kTypeLong3:             return "kTypeLong3"; 
	case kTypeLong4:             return "kTypeLong4"; 
	case kTypeUlong1:            return "kTypeUlong1"; 
	case kTypeUlong2:            return "kTypeUlong2"; 
	case kTypeUlong3:            return "kTypeUlong3"; 
	case kTypeUlong4:            return "kTypeUlong4"; 
	case kTypeLong1x1:           return "kTypeLong1x1"; 
	case kTypeLong2x1:           return "kTypeLong2x1"; 
	case kTypeLong3x1:           return "kTypeLong3x1"; 
	case kTypeLong4x1:           return "kTypeLong4x1"; 
	case kTypeLong1x2:           return "kTypeLong1x2"; 
	case kTypeLong2x2:           return "kTypeLong2x2"; 
	case kTypeLong3x2:           return "kTypeLong3x2"; 
	case kTypeLong4x2:           return "kTypeLong4x2"; 
	case kTypeLong1x3:           return "kTypeLong1x3"; 
	case kTypeLong2x3:           return "kTypeLong2x3"; 
	case kTypeLong3x3:           return "kTypeLong3x3"; 
	case kTypeLong4x3:           return "kTypeLong4x3"; 
	case kTypeLong1x4:           return "kTypeLong1x4"; 
	case kTypeLong2x4:           return "kTypeLong2x4"; 
	case kTypeLong3x4:           return "kTypeLong3x4"; 
	case kTypeLong4x4:           return "kTypeLong4x4"; 
	case kTypeUlong1x1:          return "kTypeUlong1x1"; 
	case kTypeUlong2x1:          return "kTypeUlong2x1"; 
	case kTypeUlong3x1:          return "kTypeUlong3x1"; 
	case kTypeUlong4x1:          return "kTypeUlong4x1"; 
	case kTypeUlong1x2:          return "kTypeUlong1x2"; 
	case kTypeUlong2x2:          return "kTypeUlong2x2"; 
	case kTypeUlong3x2:          return "kTypeUlong3x2"; 
	case kTypeUlong4x2:          return "kTypeUlong4x2"; 
	case kTypeUlong1x3:          return "kTypeUlong1x3"; 
	case kTypeUlong2x3:          return "kTypeUlong2x3"; 
	case kTypeUlong3x3:          return "kTypeUlong3x3"; 
	case kTypeUlong4x3:          return "kTypeUlong4x3"; 
	case kTypeUlong1x4:          return "kTypeUlong1x4"; 
	case kTypeUlong2x4:          return "kTypeUlong2x4"; 
	case kTypeUlong3x4:          return "kTypeUlong3x4"; 
	case kTypeUlong4x4:          return "kTypeUlong4x4"; 
	case kTypeBool1:             return "kTypeBool1";
	case kTypeBool2:             return "kTypeBool2";
	case kTypeBool3:             return "kTypeBool3";
	case kTypeBool4:             return "kTypeBool4";
	case kTypeTexture:           return "kTypeTexture";
	case kTypeSamplerState:      return "kTypeSamplerState";
	case kTypeBuffer:            return "kTypeBuffer";
	case kTypeUshort1:           return "kTypeUshort1";
	case kTypeUshort2:           return "kTypeUshort2";
	case kTypeUshort3:           return "kTypeUshort3";
	case kTypeUshort4:           return "kTypeUshort4";
	case kTypeUchar1:            return "kTypeUchar1";
	case kTypeUchar2:            return "kTypeUchar2";
	case kTypeUchar3:            return "kTypeUchar3";
	case kTypeUchar4:            return "kTypeUchar4";
	case kTypeShort1:            return "kTypeShort1";
	case kTypeShort2:            return "kTypeShort2";
	case kTypeShort3:            return "kTypeShort3";
	case kTypeShort4:            return "kTypeShort4";
	case kTypeChar1:             return "kTypeChar1";
	case kTypeChar2:             return "kTypeChar2";
	case kTypeChar3:             return "kTypeChar3";
	case kTypeChar4:             return "kTypeChar4";
	case kTypeVoid:              return "kTypeVoid";
	case kTypeTextureR128:       return "kTypeTextureR128";
	default: return "(unknown)";
	}
}

static inline const char * getPsslTypeKeywordString( PsslType type )
{
	switch( type )
	{
	case kTypeFloat1:           return "float"; 
	case kTypeFloat2:           return "float2"; 
	case kTypeFloat3:           return "float3"; 
	case kTypeFloat4:           return "float4"; 
	case kTypeHalf1:            return "half"; 
	case kTypeHalf2:            return "half2"; 
	case kTypeHalf3:            return "half3"; 
	case kTypeHalf4:            return "half4"; 
	case kTypeInt1:             return "int"; 
	case kTypeInt2:             return "int2"; 
	case kTypeInt3:             return "int3"; 
	case kTypeInt4:             return "int4"; 
	case kTypeUint1:            return "uint"; 
	case kTypeUint2:            return "uint2"; 
	case kTypeUint3:            return "uint3"; 
	case kTypeUint4:            return "uint4"; 
	case kTypeDouble1:          return "double"; 
	case kTypeDouble2:          return "double2"; 
	case kTypeDouble3:          return "double3"; 
	case kTypeDouble4:          return "double4"; 
	case kTypeFloat1x1:         return "float1x1"; 
	case kTypeFloat2x1:         return "float2x1"; 
	case kTypeFloat3x1:         return "float3x1"; 
	case kTypeFloat4x1:         return "float4x1"; 
	case kTypeFloat1x2:         return "float1x2"; 
	case kTypeFloat2x2:         return "float2x2"; 
	case kTypeFloat3x2:         return "float3x2"; 
	case kTypeFloat4x2:         return "float4x2"; 
	case kTypeFloat1x3:         return "float1x3"; 
	case kTypeFloat2x3:         return "float2x3"; 
	case kTypeFloat3x3:         return "float3x3"; 
	case kTypeFloat4x3:         return "float4x3"; 
	case kTypeFloat1x4:         return "float1x4"; 
	case kTypeFloat2x4:         return "float2x4"; 
	case kTypeFloat3x4:         return "float3x4"; 
	case kTypeFloat4x4:         return "float4x4"; 
	case kTypeHalf1x1:          return "half1x1"; 
	case kTypeHalf2x1:          return "half2x1"; 
	case kTypeHalf3x1:          return "half3x1"; 
	case kTypeHalf4x1:          return "half4x1"; 
	case kTypeHalf1x2:          return "half1x2"; 
	case kTypeHalf2x2:          return "half2x2"; 
	case kTypeHalf3x2:          return "half3x2"; 
	case kTypeHalf4x2:          return "half4x2"; 
	case kTypeHalf1x3:          return "half1x3"; 
	case kTypeHalf2x3:          return "half2x3"; 
	case kTypeHalf3x3:          return "half3x3"; 
	case kTypeHalf4x3:          return "half4x3"; 
	case kTypeHalf1x4:          return "half1x4"; 
	case kTypeHalf2x4:          return "half2x4"; 
	case kTypeHalf3x4:          return "half3x4"; 
	case kTypeHalf4x4:          return "half4x4"; 
	case kTypeInt1x1:           return "int1x1"; 
	case kTypeInt2x1:           return "int2x1"; 
	case kTypeInt3x1:           return "int3x1"; 
	case kTypeInt4x1:           return "int4x1"; 
	case kTypeInt1x2:           return "int1x2"; 
	case kTypeInt2x2:           return "int2x2"; 
	case kTypeInt3x2:           return "int3x2"; 
	case kTypeInt4x2:           return "int4x2"; 
	case kTypeInt1x3:           return "int1x3"; 
	case kTypeInt2x3:           return "int2x3"; 
	case kTypeInt3x3:           return "int3x3"; 
	case kTypeInt4x3:           return "int4x3"; 
	case kTypeInt1x4:           return "int1x4"; 
	case kTypeInt2x4:           return "int2x4"; 
	case kTypeInt3x4:           return "int3x4"; 
	case kTypeInt4x4:           return "int4x4"; 
	case kTypeUint1x1:          return "uint1x1"; 
	case kTypeUint2x1:          return "uint2x1"; 
	case kTypeUint3x1:          return "uint3x1"; 
	case kTypeUint4x1:          return "uint4x1"; 
	case kTypeUint1x2:          return "uint1x2"; 
	case kTypeUint2x2:          return "uint2x2"; 
	case kTypeUint3x2:          return "uint3x2"; 
	case kTypeUint4x2:          return "uint4x2"; 
	case kTypeUint1x3:          return "uint1x3"; 
	case kTypeUint2x3:          return "uint2x3"; 
	case kTypeUint3x3:          return "uint3x3"; 
	case kTypeUint4x3:          return "uint4x3"; 
	case kTypeUint1x4:          return "uint1x4"; 
	case kTypeUint2x4:          return "uint2x4"; 
	case kTypeUint3x4:          return "uint3x4"; 
	case kTypeUint4x4:          return "uint4x4"; 
	case kTypeDouble1x1:        return "double1x1"; 
	case kTypeDouble2x1:        return "double2x1"; 
	case kTypeDouble3x1:        return "double3x1"; 
	case kTypeDouble4x1:        return "double4x1"; 
	case kTypeDouble1x2:        return "double1x2"; 
	case kTypeDouble2x2:        return "double2x2"; 
	case kTypeDouble3x2:        return "double3x2"; 
	case kTypeDouble4x2:        return "double4x2"; 
	case kTypeDouble1x3:        return "double1x3"; 
	case kTypeDouble2x3:        return "double2x3"; 
	case kTypeDouble3x3:        return "double3x3"; 
	case kTypeDouble4x3:        return "double4x3"; 
	case kTypeDouble1x4:        return "double1x4"; 
	case kTypeDouble2x4:        return "double2x4"; 
	case kTypeDouble3x4:        return "double3x4"; 
	case kTypeDouble4x4:        return "double4x4"; 
	case kTypePoint:            return "Point"; 
	case kTypeLine:             return "Line"; 
	case kTypeTriangle:         return "Triangle"; 
	case kTypeAdjacentline:     return "Adjacentline"; 
	case kTypeAdjacenttriangle: return "Adjacenttriangle"; 
	case kTypePatch:            return "Patch"; 
	case kTypeStructure:        return "struct";	
	case kTypeLong1:            return "long"; 
	case kTypeLong2:            return "long2"; 
	case kTypeLong3:            return "long3"; 
	case kTypeLong4:            return "long4"; 
	case kTypeUlong1:           return "ulong"; 
	case kTypeUlong2:           return "ulong2"; 
	case kTypeUlong3:           return "ulong3"; 
	case kTypeUlong4:           return "ulong4"; 
	case kTypeLong1x1:          return "long1x1"; 
	case kTypeLong2x1:          return "long2x1"; 
	case kTypeLong3x1:          return "long3x1"; 
	case kTypeLong4x1:          return "long4x1"; 
	case kTypeLong1x2:          return "long1x2"; 
	case kTypeLong2x2:          return "long2x2"; 
	case kTypeLong3x2:          return "long3x2"; 
	case kTypeLong4x2:          return "long4x2"; 
	case kTypeLong1x3:          return "long1x3"; 
	case kTypeLong2x3:          return "long2x3"; 
	case kTypeLong3x3:          return "long3x3"; 
	case kTypeLong4x3:          return "long4x3"; 
	case kTypeLong1x4:          return "long1x4"; 
	case kTypeLong2x4:          return "long2x4"; 
	case kTypeLong3x4:          return "long3x4"; 
	case kTypeLong4x4:          return "long4x4"; 
	case kTypeUlong1x1:         return "ulong1x1"; 
	case kTypeUlong2x1:         return "ulong2x1"; 
	case kTypeUlong3x1:         return "ulong3x1"; 
	case kTypeUlong4x1:         return "ulong4x1"; 
	case kTypeUlong1x2:         return "ulong1x2"; 
	case kTypeUlong2x2:         return "ulong2x2"; 
	case kTypeUlong3x2:         return "ulong3x2"; 
	case kTypeUlong4x2:         return "ulong4x2"; 
	case kTypeUlong1x3:         return "ulong1x3"; 
	case kTypeUlong2x3:         return "ulong2x3"; 
	case kTypeUlong3x3:         return "ulong3x3"; 
	case kTypeUlong4x3:         return "ulong4x3"; 
	case kTypeUlong1x4:         return "ulong1x4"; 
	case kTypeUlong2x4:         return "ulong2x4"; 
	case kTypeUlong3x4:         return "ulong3x4"; 
	case kTypeUlong4x4:         return "ulong4x4"; 
	case kTypeBool1:            return "bool";
	case kTypeBool2:            return "bool2";
	case kTypeBool3:            return "bool3";
	case kTypeBool4:            return "bool4";
	case kTypeTexture:          return "Texture";
	case kTypeSamplerState:     return "SamplerState";
	case kTypeBuffer:           return "Buffer";
	case kTypeUshort1:          return "ushort1";
	case kTypeUshort2:          return "ushort2";
	case kTypeUshort3:          return "ushort3";
	case kTypeUshort4:          return "ushort4";
	case kTypeUchar1:           return "uchar1";
	case kTypeUchar2:           return "uchar2";
	case kTypeUchar3:           return "uchar3";
	case kTypeUchar4:           return "uchar4";
	case kTypeShort1:           return "short1";
	case kTypeShort2:           return "short2";
	case kTypeShort3:           return "short3";
	case kTypeShort4:           return "short4";
	case kTypeChar1:            return "char1";
	case kTypeChar2:            return "char2";
	case kTypeChar3:            return "char3";
	case kTypeChar4:            return "char4";
	case kTypeVoid:             return "void";
	case kTypeTextureR128:		return "Texture_R128";
	default: return "(unknown)";
	}
}
static inline const char * getPsslTypeNameString( PsslType type )
{
	return getPsslTypeKeywordString(type);
}

static inline const char * getPsslSemanticString( PsslSemantic semantic )
{
	switch( semantic )
	{
	case kSemanticPosition:                  return "kSemanticPosition"; 
	case kSemanticNormal:                    return "kSemanticNormal"; 
	case kSemanticColor:                     return "kSemanticColor"; 
	case kSemanticBinormal:                  return "kSemanticBinormal"; 
	case kSemanticTangent:                   return "kSemanticTangent"; 
	case kSemanticTexcoord0:                 return "kSemanticTexcoord0"; 
	case kSemanticTexcoord1:                 return "kSemanticTexcoord1"; 
	case kSemanticTexcoord2:                 return "kSemanticTexcoord2"; 
	case kSemanticTexcoord3:                 return "kSemanticTexcoord3"; 
	case kSemanticTexcoord4:                 return "kSemanticTexcoord4"; 
	case kSemanticTexcoord5:                 return "kSemanticTexcoord5"; 
	case kSemanticTexcoord6:                 return "kSemanticTexcoord6"; 
	case kSemanticTexcoord7:                 return "kSemanticTexcoord7"; 
	case kSemanticTexcoord8:                 return "kSemanticTexcoord8"; 
	case kSemanticTexcoord9:                 return "kSemanticTexcoord9"; 
	case kSemanticTexcoordEnd:               return "kSemanticTexcoordEnd"; 
	case kSemanticImplicit:                  return "kSemanticImplicit"; 
	case kSemanticNonreferencable:           return "kSemanticNonreferencable"; 
	case kSemanticClip:                      return "kSemanticClip"; 
	case kSemanticFog:                       return "kSemanticFog"; 
	case kSemanticFragcoord:                 return "kSemanticFragcoord"; 
	case kSemanticTarget0:                   return "kSemanticTarget0"; 
	case kSemanticTarget1:                   return "kSemanticTarget1"; 
	case kSemanticTarget2:                   return "kSemanticTarget2"; 
	case kSemanticTarget3:                   return "kSemanticTarget3"; 
	case kSemanticTarget4:                   return "kSemanticTarget4"; 
	case kSemanticTarget5:                   return "kSemanticTarget5"; 
	case kSemanticTarget6:                   return "kSemanticTarget6"; 
	case kSemanticTarget7:                   return "kSemanticTarget7"; 
	case kSemanticTarget8:                   return "kSemanticTarget8"; 
	case kSemanticTarget9:                   return "kSemanticTarget9"; 
	case kSemanticTarget10:                  return "kSemanticTarget10"; 
	case kSemanticTarget11:                  return "kSemanticTarget11"; 
	case kSemanticDepth:                     return "kSemanticDepth"; 
	case kSemanticLastcg:                    return "kSemanticLastcg"; 
	case kSemanticUserDefined:               return "kSemanticUserDefined"; 
	case kSemanticSClipDistance:             return "kSemanticSClipDistance"; 
	case kSemanticSCullDistance:             return "kSemanticSCullDistance"; 
	case kSemanticSCoverage:                 return "kSemanticSCoverage"; 
	case kSemanticSDepthOutput:              return "kSemanticSDepthOutput"; 
	case kSemanticSDepthGEOutput:            return "kSemanticSDepthGEOutput"; 
	case kSemanticSDepthLEOutput:            return "kSemanticSDepthLEOutput"; 
	case kSemanticSDispatchThreadId:         return "kSemanticSDispatchThreadId";
	case kSemanticSDomainLocation:           return "kSemanticSDomainLocation"; 
	case kSemanticSGroupId:                  return "kSemanticSGroupId"; 
	case kSemanticSGroupIndex:               return "kSemanticSGroupIndex"; 
	case kSemanticSGroupThreadId:            return "kSemanticSGroupThreadId"; 
	case kSemanticSPosition:                 return "kSemanticSPosition"; 
	case kSemanticSVertexId:                 return "kSemanticSVertexId"; 
	case kSemanticSInstanceId:               return "kSemanticSInstanceId"; 
	case kSemanticSSampleIndex:              return "kSemanticSSampleIndex"; 
	case kSemanticSPrimitiveId:              return "kSemanticSPrimitiveId"; 
	case kSemanticSGsinstanceId:             return "kSemanticSGsinstanceId"; 
	case kSemanticSOutputControlPointId:     return "kSemanticSOutputControlPointId"; 
	case kSemanticSFrontFace:                return "kSemanticSFrontFace"; 
	case kSemanticSRenderTargetIndex:        return "kSemanticSRenderTargetIndex"; 
	case kSemanticSViewportIndex:            return "kSemanticSViewportIndex"; 
	case kSemanticSTargetOutput:             return "kSemanticSTargetOutput"; 
	case kSemanticSEdgeTessFactor:           return "kSemanticSEdgeTessFactor"; 
	case kSemanticSInsideTessFactor:         return "kSemanticSInsideTessFactor";
	case kSemanticSPointCoord:               return "kSemanticSPointCoord";
	case kSemanticSPointSize:                return "kSemanticSPointSize";
	case kSemanticSVertexOffsetId:           return "kSemanticSVertexOffsetId";
	case kSemanticSInstanceOffsetId:         return "kSemanticSInstanceOffsetId";
	case kSemanticSStencilValue:             return "kSemanticSStencilValue";
	case kSemanticSStencilOp:                return "kSemanticSStencilOp";
	default: return "(unknown)";
	}
}

static inline const char *getPsslFragmentInterpTypeString(PsslFragmentInterpType type)
{
	switch (type)
	{
	case (kFragmentInterpTypeSampleNone):   return "kFragmentInterpTypeSampleNone"; 
	case (kFragmentInterpTypeSampleLinear): return "kFragmentInterpTypeSampleLinear"; 
	case (kFragmentInterpTypeSamplePoint):  return "kFragmentInterpTypeSamplePoint"; 
	default: return "(unknown)";
	}
}

static inline const char *getPsslBufferTypeString(PsslBufferType type)
{
	switch (type)
	{
	case (kBufferTypeDataBuffer):       return "kBufferTypeDataBuffer"; 
	case (kBufferTypeTexture1d):        return "kBufferTypeTexture1d"; 
	case (kBufferTypeTexture2d):        return "kBufferTypeTexture2d"; 
	case (kBufferTypeTexture3d):        return "kBufferTypeTexture3d"; 
	case (kBufferTypeTextureCube):      return "kBufferTypeTextureCube"; 
	case (kBufferTypeTexture1dArray):   return "kBufferTypeTexture1dArray"; 
	case (kBufferTypeTexture2dArray):   return "kBufferTypeTexture2dArray"; 
	case (kBufferTypeTextureCubeArray): return "kBufferTypeTextureCubeArray"; 
	case (kBufferTypeMsTexture2d):      return "kBufferTypeMsTexture2d"; 
	case (kBufferTypeMsTexture2dArray): return "kBufferTypeMsTexture2dArray"; 
	case (kBufferTypeRegularBuffer):    return "kBufferTypeRegularBuffer"; 
	case (kBufferTypeByteBuffer):       return "kBufferTypeByteBuffer"; 
	case (kBufferTypeRwDataBuffer):     return "kBufferTypeRwDataBuffer"; 
	case (kBufferTypeRwTexture1d):      return "kBufferTypeRwTexture1d"; 
	case (kBufferTypeRwTexture2d):      return "kBufferTypeRwTexture2d"; 
	case (kBufferTypeRwTexture3d):      return "kBufferTypeRwTexture3d"; 
	case (kBufferTypeRwTexture1dArray): return "kBufferTypeRwTexture1dArray"; 
	case (kBufferTypeRwTexture2dArray): return "kBufferTypeRwTexture2dArray"; 
	case (kBufferTypeRwRegularBuffer):  return "kBufferTypeRwRegularBuffer"; 
	case (kBufferTypeRwByteBuffer):     return "kBufferTypeRwByteBuffer"; 
	case (kBufferTypeAppendBuffer):     return "kBufferTypeAppendBuffer"; 
	case (kBufferTypeConsumeBuffer):    return "kBufferTypeConsumeBuffer"; 
	case (kBufferTypeConstantBuffer):   return "kBufferTypeConstantBuffer"; 
	case (kBufferTypeTextureBuffer):    return "kBufferTypeTextureBuffer"; 
	case (kBufferTypePointBuffer):      return "kBufferTypePointBuffer"; 
	case (kBufferTypeLineBuffer):       return "kBufferTypeLineBuffer"; 
	case (kBufferTypeTriangleBuffer):   return "kBufferTypeTriangleBuffer";
	case (kBufferTypeSRTBuffer):		return "kBufferTypeSRTBuffer";
	case (kBufferTypeDispatchDrawBuffer):		return "kBufferTypeDispatchDrawBuffer";
	case (kBufferTypeRwTextureCube):			return "kBufferTypeRwTextureCube";
	case (kBufferTypeRwTextureCubeArray):		return "kBufferTypeRwTextureCubeArray";
	case (kBufferTypeRwMsTexture2d):			return "kBufferTypeRwMsTexture2d";
	case (kBufferTypeRwMsTexture2dArray):		return "kBufferTypeRwMsTexture2dArray";
	case (kBufferTypeTexture1dR128):			return "kBufferTypeTexture1dR128";
	case (kBufferTypeTexture2dR128):			return "kBufferTypeTexture2dR128";
	case (kBufferTypeMsTexture2dR128):			return "kBufferTypeMsTexture2dR128";
	case (kBufferTypeRwTexture1dR128):			return "kBufferTypeRwTexture1dR128";
	case (kBufferTypeRwTexture2dR128):			return "kBufferTypeRwTexture2dR128";
	case (kBufferTypeRwMsTexture2dR128):		return "kBufferTypeRwMsTexture2dR128";
	default: return "(unknown)";
	}
}

static inline const char *getPsslBufferTypeKeywordString(PsslBufferType type)
{
	switch (type)
	{
	case (kBufferTypeDataBuffer):       return "DataBuffer"; 
	case (kBufferTypeTexture1d):        return "Texture1D"; 
	case (kBufferTypeTexture2d):        return "Texture2D"; 
	case (kBufferTypeTexture3d):        return "Texture3D"; 
	case (kBufferTypeTextureCube):      return "TextureCube"; 
	case (kBufferTypeTexture1dArray):   return "Texture1D_Array"; 
	case (kBufferTypeTexture2dArray):   return "Texture2D_Array"; 
	case (kBufferTypeTextureCubeArray): return "TextureCube_Array"; 
	case (kBufferTypeMsTexture2d):      return "MS_Texture2D"; 
	case (kBufferTypeMsTexture2dArray): return "MS_Texture2D_Array"; 
	case (kBufferTypeRegularBuffer):    return "RegularBuffer"; 
	case (kBufferTypeByteBuffer):       return "ByteBuffer"; 
	case (kBufferTypeRwDataBuffer):     return "RW_DataBuffer"; 
	case (kBufferTypeRwTexture1d):      return "RW_Texture1D"; 
	case (kBufferTypeRwTexture2d):      return "RW_Texture2D"; 
	case (kBufferTypeRwTexture3d):      return "RW_Texture3D"; 
	case (kBufferTypeRwTexture1dArray): return "RW_Texture1D_Array"; 
	case (kBufferTypeRwTexture2dArray): return "RW_Texture2D_Array"; 
	case (kBufferTypeRwRegularBuffer):  return "RW_RegularBuffer"; 
	case (kBufferTypeRwByteBuffer):     return "RW_ByteBuffer"; 
	case (kBufferTypeAppendBuffer):     return "AppendRegularBuffer"; 
	case (kBufferTypeConsumeBuffer):    return "ConsumeRegularBuffer"; 
	case (kBufferTypeConstantBuffer):   return "ResourceConstantBuffer"; 
	case (kBufferTypeTextureBuffer):    return "ResourceTextureBuffer"; 
	case (kBufferTypePointBuffer):      return "PointBuffer"; 
	case (kBufferTypeLineBuffer):       return "LineBuffer"; 
	case (kBufferTypeTriangleBuffer):   return "TriangleBuffer"; 
	case (kBufferTypeSRTBuffer):		return "SRTBuffer";
	case (kBufferTypeDispatchDrawBuffer): return "DispatchDrawBuffer";
	case (kBufferTypeRwTextureCube):			return "RW_TextureCube";
	case (kBufferTypeRwTextureCubeArray):		return "RW_TextureCube_Array";
	case (kBufferTypeRwMsTexture2d):			return "RW_MS_Texture2D";
	case (kBufferTypeRwMsTexture2dArray):		return "RW_MS_Texture2D_Array";
	case (kBufferTypeTexture1dR128):			return "Texture1D_R128";
	case (kBufferTypeTexture2dR128):			return "Texture2D_R128";
	case (kBufferTypeMsTexture2dR128):			return "MS_Texture2D_R128";
	case (kBufferTypeRwTexture1dR128):			return "RW_Texture1D_R128";
	case (kBufferTypeRwTexture2dR128):			return "RW_Texture2D_R128";
	case (kBufferTypeRwMsTexture2dR128):		return "RW_MS_Texture2D_R128";
	default: return "(unknown)";
	}
}

static inline const char *getPsslInternalBufferTypeString(PsslInternalBufferType type)
{
	switch (type)
	{
	case (kInternalBufferTypeUav):             return "kInternalBufferTypeUav"; 
	case (kInternalBufferTypeSrv):             return "kInternalBufferTypeSrv"; 
	case (kInternalBufferTypeLds):             return "kInternalBufferTypeLds"; 
	case (kInternalBufferTypeGds):             return "kInternalBufferTypeGds"; 
	case (kInternalBufferTypeCbuffer):         return "kInternalBufferTypeCbuffer"; 
	case (kInternalBufferTypeTextureSampler):  return "kInternalBufferTypeTextureSampler"; 
	case (kInternalBufferTypeInternal):        return "kInternalBufferTypeInternal"; 
	default: return "(unknown)";
	}
}

static inline const char *getPsslShaderTypeString(PsslShaderType type)
{
	switch (type)
	{
	case (kShaderTypeVsShader): return "kShaderTypeVsShader"; 
	case (kShaderTypeFsShader): return "kShaderTypeFsShader"; 
	case (kShaderTypeCsShader): return "kShaderTypeCsShader"; 
	case (kShaderTypeGsShader): return "kShaderTypeGsShader"; 
	case (kShaderTypeHsShader): return "kShaderTypeHsShader"; 
	case (kShaderTypeDsShader): return "kShaderTypeDsShader"; 
	default: return "(unknown)";
	}
}

static inline const char *getPsslCodeTypeString(PsslCodeType type)
{
	switch (type)
	{
	case (kCodeTypeIl):  return "kCodeTypeIl"; 
	case (kCodeTypeIsa): return "kCodeTypeIsa"; 
	case (kCodeTypeScu): return "kCodeTypeScu"; 
	default: return "(unknown)";
	}
}

static inline const char *getPsslCompilerTypeString(PsslCompilerType type)
{
	switch (type)
	{
	case (kCompilerTypeUnspecified): return "kCompilerTypeUnspecified"; 
	case (kCompilerTypeOrbisPsslc):  return "kCompilerTypeOrbisPsslc"; 
	case (kCompilerTypeOrbisEsslc):  return "kCompilerTypeOrbisEsslc"; 
	case (kCompilerTypeOrbisWave):	 return "kCompilerTypeOrbisWave"; 
	case (kCompilerTypeOrbisCuAs):   return "kCompilerTypeOrbisCuAs"; 
	default: return "(unknown)";
	}
}

static inline const char *getPsslErrorCodeString(PsslStatus type)
{
	switch (type)
	{
	case (kStatusOk):   return "kStatusOk"; 
	case (kStatusFail): return "kStatusFail"; 
	default: return "(unknown)";
	}
}

static inline const char *getPsslComponentMaskString(PsslComponentMask type)
{
	switch (type)
	{
	case (kComponentMask):     return "kComponentMask"; 
	case (kComponentMaskX):    return "kComponentMaskX"; 
	case (kComponentMaskY):    return "kComponentMaskY"; 
	case (kComponentMaskXy):   return "kComponentMaskXy"; 
	case (kComponentMaskZ):    return "kComponentMaskZ"; 
	case (kComponentMaskXZ):   return "kComponentMaskXZ"; 
	case (kComponentMaskYz):   return "kComponentMaskYz"; 
	case (kComponentMaskXyz):  return "kComponentMaskXyz"; 
	case (kComponentMaskW):    return "kComponentMaskW"; 
	case (kComponentMaskXW):   return "kComponentMaskXW"; 
	case (kComponentMaskYW):   return "kComponentMaskYW"; 
	case (kComponentMaskXyW):  return "kComponentMaskXyW"; 
	case (kComponentMaskZw):   return "kComponentMaskZw"; 
	case (kComponentMaskXZw):  return "kComponentMaskXZw"; 
	case (kComponentMaskYzw):  return "kComponentMaskYzw"; 
	case (kComponentMaskXyzw): return "kComponentMaskXyzw"; 
	default: return "(unknown)";
	}
}

static inline const char *getPsslGsIoString(PsslGsIoType type)
{
	switch (type)
	{
	case (kGsIoTypeGsTri):      return "kGsIoTypeGsTri"; 
	case (kGsIoTypeGsLine):     return "kGsIoTypeGsLine"; 
	case (kGsIoTypeGsPoint):    return "kGsIoTypeGsPoint"; 
	case (kGsIoTypeGsAdjTri):   return "kGsIoTypeGsAdjTri"; 
	case (kGsIoTypeGsAdjLine):  return "kGsIoTypeGsAdjLine"; 
	default: return "(unknown)";
	}
}
 
static inline const char *getPsslHsTopologyString(PsslHsTopologyType type)
{
	switch (type)
	{
	case (kHsTopologyTypeHsPoint):  return "kHsTopologyTypeHsPoint"; 
	case (kHsTopologyTypeHsLine):   return "kHsTopologyTypeHsLine"; 
	case (kHsTopologyTypeHsCwtri):  return "kHsTopologyTypeHsCwtri"; 
	case (kHsTopologyTypeHsCcwtri): return "kHsTopologyTypeHsCcwtri"; 
	default: return "(unknown)";
	}
}

static inline const char *getPsslHsPartitioningString(PsslHsPartitioningType type)
{
	switch (type)
	{
	case (kHsPartitioningTypeHsInteger):       return "kHsPartitioningTypeHsInteger"; 
	case (kHsPartitioningTypeHsPowerOf2):      return "kHsPartitioningTypeHsPowerOf2"; 
	case (kHsPartitioningTypeHsOddFactorial):  return "kHsPartitioningTypeHsOddFactorial"; 
	case (kHsPartitioningTypeHsEvenFactorial): return "kHsPartitioningTypeHsEvenFactorial"; 
	default: return "(unknown)";
	}
}

static inline const char *getPsslHsDsPatchString(PsslHsDsPatchType type)
{
	switch (type)
	{
	case (kHsDsPatchTypeHsDsIsoline): return "kHsDsPatchTypeHsDsIsoline"; 
	case (kHsDsPatchTypeHsDsTri):     return "kHsDsPatchTypeHsDsTri"; 
	case (kHsDsPatchTypeHsDsQuad):    return "kHsDsPatchTypeHsDsQuad"; 
	default: return "(unknown)";
	}
}

static inline const char *getPsslVertexVariantString(PsslVertexVariant type) 
{ 
	switch (type) 
	{ 
	case (kVertexVariantVertex): return "kVertexVariantVertex"; break; 
	case (kVertexVariantExport): return "kVertexVariantExport"; break; 
	case (kVertexVariantLocal):  return "kVertexVariantLocal";  break; 
	case (kVertexVariantDispatchDraw): return "kVertexVariantDispatchDraw";  break; 
	case (kVertexVariantExportOnChip): return "kVertexVariantExportOnChip"; break; 
	default: return "(unknown)"; 
	} 
}

static inline const char *getPsslDomainVariantString(PsslDomainVariant type) 
{ 
	switch (type) 
	{ 
	case (kDomainVariantVertex): return "kDomainVariantVertex"; break; 
	case (kDomainVariantExport): return "kDomainVariantExport"; break;
	case (kDomainVariantExportOnChip): return "kDomainVariantExportOnChip"; break;
	case (kDomainVariantVertexHsOnBuffer): return "kDomainVariantVertexHsOffChip"; break; 
	case (kDomainVariantExportHsOnBuffer): return "kDomainVariantExportHsOffChip"; break;
	case (kDomainVariantVertexHsDynamic): return "kDomainVariantVertexHsDynamic"; break; 
	case (kDomainVariantExportHsDynamic): return "kDomainVariantExportHsDynamic"; break; 
	default: return "(unknown)";
	} 
}

static inline const char *getPsslHullVariantString(PsslHullVariant type) 
{ 
	switch (type) 
	{ 
	case (kHullVariantOnChip): return "kHullVariantOnChip"; break; 
	case (kHullVariantOnBuffer): return "kHullVariantOffChip"; break;
	case (kHullVariantDynamic): return "kHullVariantDynamic"; break; 
	default: return "(unknown)";
	} 
}

static inline const char *getPsslGeometryVariantString(PsslGeometryVariant type)
{
	switch (type)
	{
	case (kGeometryVariantOnBuffer): return "kGeometryVariantOffChip"; break;
	case (kGeometryVariantOnChip): return "kGeometryVariantOnChip"; break;
	default: return "(unknown)";
	}
}
} // namespace Binary
} // namespace Shader
} // namespace sce

#endif // !defined(DOXYGEN_IGNORE)

#endif // _SCE_PSSL_TYPES_H_

