/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 08.508.001
* Copyright (C) 2018 Sony Interactive Entertainment Inc.
*/

#ifndef _SCE_WAVE_PSSL_H
#define _SCE_WAVE_PSSL_H

#ifndef	__cplusplus
#error A C++ compiler is needed to process this header
#endif	// !def __cplusplus

///////////////////////////////////////////////////////////////////////////////
// Basic types
///////////////////////////////////////////////////////////////////////////////

#if !defined(DOXYGEN_IGNORE)
#if defined(__ORBIS__) || defined(__linux__) || defined(SCE_WAVE_STATIC)
#	define SCE_WAVE_INTERFACE
#	define SCE_WAVE_CALL_CONVENTION
#	include <stdint.h>
#	include <stddef.h>
#else
#	ifdef SCE_WAVE_EXPORTS
#		define SCE_WAVE_INTERFACE				__declspec( dllexport )
#	else
#		define SCE_WAVE_INTERFACE				__declspec( dllimport )
#	endif
#	ifndef SCE_WAVE_CALL_CONVENTION
#		define SCE_WAVE_CALL_CONVENTION			__cdecl
#	endif
#if _MSC_VER < 1600 && !defined(__linux__)
typedef signed char			int8_t;
typedef unsigned char		uint8_t;
typedef short				int16_t;
typedef unsigned short		uint16_t;
typedef __int32				int32_t;
typedef unsigned __int32	uint32_t;
typedef __int64				int64_t;
typedef unsigned __int64	uint64_t;
#else
#include <stdint.h>
#endif /* _MSC_VER */
#endif /* __ORBIS__ */
#endif /* DOXYGEN_IGNORE */

/** @file	wave_psslc.h

	The shared library version of the shader compiler allows tools and
	applications to compile/preprocess shaders (and/or generate file
	dependency information about them) without having to spawn an external
	process. Users may also choose to replace the native file system with
	a virtualized one, by using a set of callbacks. 	The compiler may be
	invoked as follows:

	sce::Shader::Wave::Psslc::run
		Performs a full compile, returning a sce::Shader::Wave::Psslc::Output
		object containing both the binary and SDB.

	There is a corresponding destroy function, sce::Shader::Wave::Psslc::destroyOutput,
	that must be called in order to free the resources associated
	with a job.
	Creation and destruction do not have to be performed on the same thread.

	The sce::Shader::Wave::Psslc::run function requires two inputs:

	sce::Shader::Wave::Psslc::Options
		This structure is the equivalent of the command-line options for the
		standalone shader compiler. It may also be used as a compilation ID by
		extending the struct and passing the appropriate pointer to psscdll.
		This pointer will be provided on all callbacks.

	sce::Shader::Wave::Psslc::CallbackList
		Provides the compiler with an interface to the file system - whether
		it is interface provided by the operating system or a virtualized one.
		For details regarding the individual callbacks, please refer to the
		documentation of sce::Shader::Wave::Psslc::CallbackList.
		Please note that if a callback list is provided, it should always be
		initialized via sce::Shader::Wave::Psslc::initializeCallbackList.
		Please noted that where indicated, the callbacks argument is optional.
		If no callbacks are provided, the operating system's file system will
		be used.
*/

///////////////////////////////////////////////////////////////////////////////
// DLL API Version
///////////////////////////////////////////////////////////////////////////////
#define SCE_WAVE_API_VERSION 28

///////////////////////////////////////////////////////////////////////////////
// DLL API error code
///////////////////////////////////////////////////////////////////////////////
#define SCE_WAVE_RESULT_OK							0
#define SCE_WAVE_RESULT_INVALID_INPUT_PARAMETER		1
#define SCE_WAVE_RESULT_API_VERSION_MISMATCH		2

namespace sce
{
namespace Gnm
{
	///////////////////////////////////////////////////////////////////////////////
	// Forward declarations from Gnm namespace
	///////////////////////////////////////////////////////////////////////////////
	class InputUsageSlot;
}
namespace Shader
{
namespace Wave
{
namespace Psslc
{

	///////////////////////////////////////////////////////////////////////////////
	// Forward declarations
	///////////////////////////////////////////////////////////////////////////////

	struct OptionsBase;
	struct Options;
	struct OptionsWithCommandLine;
	struct SourceFile;
	struct SourceLocation;

	///////////////////////////////////////////////////////////////////////////////
	// Typedefs
	///////////////////////////////////////////////////////////////////////////////

	/**	@brief	A callback used when the compiler needs to open a file.

		The includedFrom location will either be 0, if a primary file is being
		opened, or it will point to the location of the #include directive.
		If the file could not be opened, 0 is returned and errorString will be
		updated to point to a representative message, explaining why the file
		could not be opened.

		On success, a non-zero pointer is returned. The caller takes ownership,
		calling sce::Shader::Wave::Psslc::CallbackReleaseFile when the returned
		sce::Shader::Wave::Psslc::SourceFile is no longer required.

		Note that <c>fileName</c> must not be copied by reference into the returned
		sce::Shader::Wave::Psslc::SourceFile. The lifetime of all parameters other
		than <c>userData</c> ends when the callback returns.

		@param[in] fileName
			The absolute path of the file to be opened.

		@param[in] includedFrom
			The include location. Set to 0 for a primary file.

		@param[in] compileOptions
			The original options pointer used to invoke this compile
			or nullptr if the OptionsWithCommandLine API is being used.

		@param[out] errorString
			If 0 is returned and this pointer is non-zero, this output param should
			be updated to contain a diagnostic message.

		@return
			A sce::Shader::Wave::Psslc::SourceFile object to be destroyed later with
			sce::Shader::Wave::Psslc::CallbackReleaseFile.

		@ingroup orbis_wave
	*/
	typedef SourceFile* (*CallbackOpenFile)(
		const char *fileName,
		const SourceLocation *includedFrom,
		const OptionsBase *compileOptions,
		void *userData,
		const char **errorString);

	/**	@brief	A callback used when the compiler needs to release file data.

		This function is used to release memory that was previously allocated as
		part of a call to sce::Shader::Wave::Psslc::CallbackOpenFile. This will
		be called once per "fileName" associated with that
		sce::Shader::Wave::Psslc::SourceFile object when the resources for
		the compile session are released.

		@param[in] file
			The sce::Shader::Wave::Psslc::SourceFile object to be destroyed.

		@param[in] compileOptions
			The original options pointer used to invoke this compile
			or nullptr if the OptionsWithCommandLine API is being used.

		@ingroup orbis_wave
	*/
	typedef void (*CallbackReleaseFile)(
		const SourceFile *file,
		const OptionsBase *compileOptions,
		void *userData);

	/**	@brief	A callback used to search for a named file.

		This function will search in all provided paths for the named file. If the
		file could not be located, 0 is returned and errorString will have been
		updated to a representative message, explaining why the file could not be
		located.
		On success, a non-zero string is returned. The caller takes ownership and
		will release the allocation via the
		sce::Shader::Wave::Psslc::CallbackReleaseFileName	callback.

		@param[in] fileName
			The name of the file to be located.

		@param[in] includedFrom
			The location of the include directive. Set to 0 for primary files.

		@param[in] searchPathCount
			The number of search paths provided via 'searchPaths'.

		@param[in] searchPaths
			The array of search paths. The size is provided via 'searchPathCount'.

		@param[in] compileOptions
			The original options pointer used to invoke this compile
			or nullptr if the OptionsWithCommandLine API is being used.

		@param[out] errorString
			If 0 is returned and if this pointer is non-zero, it should be updated
			to contain a diagnostic message.

		@return
			A relative or absolute path to the file or 0 if the file could not be
			located.

 		@ingroup orbis_wave
	*/
	typedef const char* (*CallbackLocateFile)(
		const char *fileName,
		const SourceLocation *includedFrom,
		uint32_t searchPathCount,
		const char *const*searchPaths,
		const OptionsBase *compileOptions,
		void *userData,
		const char **errorString);

	/**	@brief	A callback used to retrieve the absolute path name for a given file.

		Files are uniquely identified by absolute paths. If two include files lead
		to the same absolute path, the previously found file is used and no call
		to sce::Shader::Wave::Psslc::CallbackOpenFile will be made. This function
		allows for a translation from a relative path scheme to an absolute path scheme.

		If there is no valid absolute path for the given file, 0 should be returned.
		If a non-zero string is returned, the caller takes ownership and will release
		the allocation via the sce::Shader::Wave::Psslc::CallbackReleaseFileName callback.
		This string will be the name passed to sce::Shader::Wave::Psslc::CallbackOpenFile.

		@param[in] fileName
			The (possibly relative) file path for an include file, as provided
			by sce::Shader::Wave::Psslc::CallbackLocateFile.

		@param[in] includedFrom
			The location of the include directive.

		@param[in] compileOptions
			The original options pointer used to invoke this compile
			or nullptr if the OptionsWithCommandLine API is being used.

		@return
			The absolute file path or 0 if the file could not be located.

		@ingroup orbis_wave
	*/
	typedef const char* (*CallbackAbsolutePath)(
		const char *fileName,
		const SourceLocation *includedFrom,
		const OptionsBase *compileOptions,
		void *userData);

	/**	@brief	A callback for the compiler to release a file name.

		This function is used to release memory that was previously allocated as
		part of a call to sce::Shader::Wave::Psslc::CallbackLocateFile or
		sce::Shader::Wave::Psslc::CallbackAbsolutePath.

		@param[in] fileName
			The file name string to be destroyed.

		@param[in] compileOptions
			The original options pointer used to invoke this compile
			or nullptr if the OptionsWithCommandLine API is being used.

		@ingroup orbis_wave
	*/
	typedef void (*CallbackReleaseFileName)(
		const char *fileName,
		const OptionsBase *compileOptions,
		void *userData);

	/**	@brief	Provides date information for the named file.

		If the date attributes could not be read, 0 is returned and the results
		will be considered invalid.

		On success, timeLastStatusChange and timeLastModified will have been
		updated and a non-zero value is returned. The time values are encoded as time_t.

		@param[in] file
			A file object returned from sce::Shader::Wave::Psslc::CallbackOpenFile.

		@param[in] includedFrom
			The location of the include directive.

		@param[in] compileOptions
			The original options pointer used to invoke this compile
			or nullptr if the OptionsWithCommandLine API is being used.

		@param[out] timeLastStatusChange
			A pointer to the time of last status change (i.e. creationTime).

		@param[out] timeLastModified
			A pointer to the time of last modification.

		@return
			Non-zero for success, 0 on failure.

		@ingroup orbis_wave
	*/
	typedef int32_t (*CallbackFileDate)(
		const SourceFile *file,
		const SourceLocation *includedFrom,
		const OptionsBase *compileOptions,
		void *userData,
		int64_t *timeLastStatusChange,					///< using time_t
		int64_t *timeLastModified);						///< using time_t

	/**	@brief	Malloc-style memory allocation callback

		@param[in] size
			Allocation size.

		@return
			A pointer to allocated memory on success.

		@ingroup orbis_shader_compiler
	*/
	typedef void* (*CallbackMemAlloc)(size_t size, void *userData);

	/**	@brief	Free-style memory de-allocation callback

		@param[in] ptr
			A pointer to allocated block.

		@ingroup orbis_shader_compiler
	*/
	typedef void (*CallbackMemFree)(void *ptr, void *userData);

	///////////////////////////////////////////////////////////////////////////////
	// Constants
	///////////////////////////////////////////////////////////////////////////////

	/** @brief	Classifies the source language to be compiled by orbis_shader_compiler

		@ingroup orbis_wave
	*/
	typedef enum SourceLanguage {
		kSourceLanguagePssl,				///< PSSL
		kSourceLanguageEssl,				///< ESSL
	} SourceLanguage;

	/** @brief	Classifies the severity of a diagnostic

		@ingroup orbis_wave
	*/
	typedef enum DiagnosticLevel {
		kDiagnosticLevelInfo,				///< Informational message
		kDiagnosticLevelWarning,			///< Warning
		kDiagnosticLevelError				///< Error
	} DiagnosticLevel;

	/** @brief	Classifies the target profiles

		@ingroup orbis_wave
	*/
	typedef enum TargetProfile {
		kTargetProfileVsVs        = 0,				///< vertex program
		kTargetProfileVsLs        = 1,				///< vertex program as a local shader
		kTargetProfileVsEs        = 2,				///< vertex program as an export shader
		kTargetProfileVsEsOnChip  = 3,				///< vertex program as an export shader, using LDS
		kTargetProfilePs          = 4,				///< pixel (fragment) program
		kTargetProfileCs          = 5,				///< compute program
		kTargetProfileGs          = 6,				///< geometry program
		kTargetProfileHs          = 7,				///< hull program
		kTargetProfileDsVs        = 8,				///< domain program as a vertex shader
		kTargetProfileDsEs        = 9,				///< domain program as an export shader
		kTargetProfileGsOnChip    = 10,				///< geometry program, using LDS
		kTargetProfileHsOffChip   = 15,				///< off-chip hull program
		kTargetProfileDsVsOffChip = 16,				///< off-chip domain program as a vertex shader
		kTargetProfileDsEsOffChip = 17				///< off-chip domain program as an export shader
	} TargetProfile;

	/** @brief	Classifies default callbacks

		@ingroup orbis_wave
	*/
	typedef enum CallbackDefaults {
		kCallbackDefaultsSystemFiles,				///< Default callback functions for using system files and paths.
		kCallbackDefaultsTrivial					///< Default callback functions for using only the "openFile" callback.
	} CallbackDefaults;

	/** @brief	Classifies locale in which to print messages

		@ingroup orbis_wave
	*/
	typedef enum Locale {
		kLocaleEnglish,							///< English language setting.
		kLocaleJapanese							///< Japanese language setting.
	} Locale;

	/** @brief	Classifies wait insertion mode

	@ingroup orbis_wave
	*/
	typedef enum WaitCntMode {
		kWaitCntModeGlobal,		///< Default value, consider the whole program when inserting s_waitcnt instructions
		kWaitCntModeBasicblock,	///< consider only individual basic blocks
		kWaitCntModeDebug		///< Debug mode, inserts s_waitcnt after every instruction
	} WaitCntMode;

	/** @brief	Hardware target

		@ingroup orbis_wave
	*/
	typedef enum HardwareTarget {
		kHardwareTargetCommon,			///< Supported on both Neo and Base
		kHardwareTargetBase,			///< Will only run on Base
		kHardwareTargetNeo				///< Will only run on Neo
	} HardwareTarget;

	/** @brief	Gradient adjust mode

		@ingroup orbis_wave
	*/
	typedef enum GradientAdjust {
		kGradientAdjustNever,			///< Never use adjust for sample instructions
		kGradientAdjustAlways,			///< Always use adjust for sample instructions
		kGradientAdjustAuto				///< Use adjust based on UV computations
	} GradientAdjust;

	/** @brief	Gradient adjust mode

	@ingroup orbis_wave
	*/
	typedef enum SampleRate {
		kSampleRateNever,				///< Never run at sample rate
		kSampleRateAlways,				///< Always run at sample rate
		kSampleRateAuto					///< Auto-detect execution rate
	} SampleRate;

	/** @brief	Default barycentric mode

		@ingroup orbis_wave
	*/
	typedef enum BarycentricMode {
		kBarycentricModeCenter,			///< Default barycentric mode is center
		kBarycentricModeCentroid,		///< Default barycentric mode is centroid
		kBarycentricModeSample			///< Default barycentric mode is sample
	} BarycentricMode;

	/** @brief	Handles SRT resources present in the SRT signature but absent
		from the source code

		@ingroup orbis_wave
	*/
	typedef enum SrtSignatureMissingResource {
		kSrtSignatureMissingResourceError,					///< Error out when a resource is missing (default)
		kSrtSignatureMissingResourceRemove,					///< Remove the resource entirely if it is missing
		kSrtSignatureMissingResourceRemoveFromCompactTable,	///< Remove the resource but only if REMOVE_UNUSED_ELEMENT is set of the table
		kSrtSignatureMissingResourceKeepAndPad				///< Keep and insert padding
	} SrtSignatureMissingResource;

	/** @brief	Kind of 'this' for nfe

	@ingroup orbis_wave
	*/
	typedef enum ThisKind {
		kThisKindReference,			///< 'this' is a generic reference
		kThisKindPointer			///< 'this' is a generic pointer
	} ThisKind;

	/** @brief	Srt signature resource kinds

	@ingroup orbis_wave
	*/
	typedef enum SrtSignatureResourceKind {
		kSrtSignatureCBV,
		kSrtSignatureUAV,
		kSrtSignatureSRV,
		kSrtSignatureSampler,
		kSrtSignatureNamedConstantBuffer,
		kSrtSignatureNamedTexture,
		kSrtSignatureNamedBuffer,
		kSrtSignatureNamedSampler
	} SrtSignatureResourceKind;

	/**	@brief	Describes top-level SRT signature elements

		@ingroup orbis_wave
	*/
	typedef enum SrtSignatureTopElementKind {
		kSrtSignatureTopResource,       ///< Resource as described above
		kSrtSignatureTopStaticSampler,  ///< Unsupported for now
		kSrtSignatureTopRootConstants,  ///< Unsuported for now
		kSrtSignatureTopDescriptorTable ///< A pointer to a set of elements
	} SrtSignatureTopElementKind;

	/**	@brief	Describes elements in descriptor tables

		@ingroup orbis_wave
	*/
	typedef enum SrtSignatureTableElementKind {
		kSrtSignatureTableResource,       ///< Resource as described above
		kSrtSignatureTableDescriptorTable ///< A pointer to a set of elements
	} SrtSignatureTablelementKind;

	/**	@brief	Describes behaviour for all elements in the SRT signature

		@ingroup orbis_wave
	*/
	enum SrtSignatureFlag : uint64_t
	{
		/// How to setup the constant buffer from GNM side
		CONSTANT_BUFFER_USE_INIT_AS_CONSTANT_BUFFER  = 0x0,
		CONSTANT_BUFFER_USE_INIT_AS_BYTE_BUFFER      = 0x1,
		CONSTANT_BUFFER_USE_INIT_AS_REGULAR_BUFFER   = 0x2,
		CONSTANT_BUFFER_USE_POINTER                  = 0x3,
		CONSTANT_BUFFER_USE_INLINED                  = 0x4,

		/// How to setup the layout of constant fields
		CONSTANT_BUFFER_LEGACY_LAYOUT                = 0x0 << 3,
		CONSTANT_BUFFER_C_LAYOUT                     = 0x1 << 3,

		/// Inlined constant buffers can only use C layout
		INLINED_CONSTANT_BUFFER = CONSTANT_BUFFER_USE_INLINED | CONSTANT_BUFFER_C_LAYOUT,

		/// We can optionally compact and remove unused elements
		REMOVE_UNUSED_ELEMENT                        = 0x1 << 4,
		DO_NOT_REMOVE_UNUSED_ELEMENT                 = 0x2 << 4,

		/// We can optionally remove empty table all together
		REMOVE_EMPTY_TABLE                           = 0x1 << 6,
		DO_NOT_REMOVE_EMPTY_TABLE                    = 0x2 << 6,

		/// We can optionally preserve unused constants in used constant buffers
		CONSTANT_BUFFER_KEEP_UNUSED_CONSTANTS        = 0x1 << 8,

		// For missing resource declare there size
		SRT_SIGNATURE_16_BYTE_RESOURCE               = 0x1 << 9,
		SRT_SIGNATURE_32_BYTE_RESOURCE               = 0x1 << 10
	};

	/**	@brief	Optimization settings for uniform atomics

		@ingroup orbis_wave
	*/
	typedef enum OptimizeUniformAtomics
	{
		kOptimizeUniformAtomicsNever,	///< do not optimize (default)
		kOptimizeUniformAtomicsUniform,	///< optimize if address and value are uniform
		kOptimizeUniformAtomicsAlways	///< optimize even if value is diverging
	} OptimizeUniformAtomics;

	///////////////////////////////////////////////////////////////////////////////
	// Structs
	///////////////////////////////////////////////////////////////////////////////

	/**	@brief	Describes a source file

		@note	If the source file is referenced only through a #line directive,
				the compiler will generate a sce::Shader::Wave::Psslc::SourceFile
				object with a NULL 'text' field and a 'size' field of 0.

		@ingroup orbis_wave
	*/
	typedef struct SourceFile {
		const char *fileName;							///< The relative or absolute name of the file.
		const char *text;								///< The contents of the source file.
		uint32_t size;									///< The size of the 'text' array in bytes, not including the terminating '\0'.
	} SourceFile;

	/**	@brief	Describes a location in the source code.

		@ingroup orbis_wave
	*/
	typedef struct SourceLocation {
		const SourceFile *file;	///< The file containing the location.
		uint32_t lineNumber;							///< The line number of the location.
		uint32_t columnNumber;							///< The column number of the location.
	} SourceLocation;

	/**	@brief	Describes input usage slots layout in user-sgpr for the purpose of explicit layout matching.

		@ingroup orbis_wave
	*/
	typedef struct InputSlotsTemplate
	{
		uint32_t numPrimaryInputUsageSlots;							///< size of the primary input usage slot array
		uint32_t numSecondaryInputUsageSlots;						///< size of the secondary input usage slot array (eg: copy shader)
		uint32_t vertexOffsetSlot;									///< for indirect-draw, holds vertex offset user sgpr index
		uint32_t instanceOffsetSlot;								///< for indirect-draw, holds instance offset user sgpr index
		const sce::Gnm::InputUsageSlot *primaryInputUsageSlots;		///< pointer to usage slots for primary program
		const sce::Gnm::InputUsageSlot *secondaryInputUsageSlots;	///< pointer to usage slots for secondary program
	} InputSlotsTemplate;

	/**	@brief	Static sampler are unsupported for now

		@ingroup orbis_wave
	*/
	typedef struct SrtSignatureStaticSampler
	{
	} SrtSignatureStaticSampler;
	
	/**	@brief	Srt constants are unsupported for now

		@ingroup orbis_wave
	*/
	typedef struct SrtSignatureRootConstants
	{
	} SrtSignatureRootConstants;

	/**	@brief	All resources to bind

		@ingroup orbis_wave
	*/
	typedef struct SrtSignatureResource
	{
		SrtSignatureResourceKind kind;
		union
		{
			uint32_t index;
			char const *name;
		};
		uint64_t flags;
	} SrtSignatureResource;

	/**	@brief	A resource to set in the SRT signature

		@ingroup orbis_wave
	*/
	typedef struct SrtSignatureDescriptorTable
	{
		struct SrtSignatureTableElement const *elements;
		uint32_t elementCount;
		uint64_t flags;
	} SrtSignatureDescriptorTable;

	/**	@brief	An element that goes in descriptor tables

		@ingroup orbis_wave
	*/
	typedef struct SrtSignatureTableElement
	{
		SrtSignatureTableElementKind kind;
		union
		{
			SrtSignatureResource resource;
			SrtSignatureDescriptorTable table;
		};
	} SrtSignatureTableElement;

	/**	@brief	An element that goes at the top of the SRT signature

		@ingroup orbis_wave
	*/
	typedef struct SrtSignatureTopElement
	{
		SrtSignatureTopElementKind kind;
		union
		{
			SrtSignatureResource resource;
			SrtSignatureStaticSampler staticSampler;
			SrtSignatureRootConstants rootConstants;
			SrtSignatureDescriptorTable table;
		};
	} SrtSignatureTopElement;

	/**	@brief	Describes top-level SRT signature

		@ingroup orbis_wave
	*/
	typedef struct SrtSignature
	{
		char const *name;
		SrtSignatureTopElement const *elements;
		uint32_t elementCount;
		uint64_t flags;
	} SrtSignature;

	/** @brief Used to distinguish both DLL options

		@ingroup orbis_wave
	*/
	enum OptionsClass : uint32_t
	{
		kOptions,
		kOptionsWithCommandLine
	};

	/** @brief Shared by Options and OptionsWithCommandLine
		@ingroup orbis_wave
	*/
	typedef struct OptionsBase {
		OptionsClass optionsClass;
	} OptionsBase;

	/** @brief	Describes the input data for a compilation job.

		@ingroup orbis_wave
	*/
	typedef struct Options : OptionsBase {
		uint32_t version;								///< Set by sce::Shader::Wave::Psslc::initializeOptions.
		void *reserved;									///< Set by sce::Shader::Wave::Psslc::initializeOptions.
		SourceLanguage sourceLanguage;					///< Which language (PSSL, Cg, etc.) we are compiling
		const char *mainSourceFile;						///< The main source file to compile or preprocess.
		TargetProfile targetProfile;					///< The target profile.
		const char *entryFunctionName;					///< The name of the entry function. Usually "main".
		uint32_t searchPathCount;						///< The number of search paths for include files.
		const char* const *searchPaths;					///< The search paths for include files.
		uint32_t macroDefinitionCount;					///< The number of macro definitions provided.
		const char* const *macroDefinitions;			///< The macro definitions in the form: MACRONAME or MACRONAME=VALUE
		uint32_t includeFileCount;						///< The number of files to force include.
		const char* const *includeFiles;				///< The files to include before the main source file.
		uint32_t suppressedWarningsCount;				///< The number of warnings to suppressed.
		const uint32_t *suppressedWarnings;				///< The id numbers of the warnings to be suppressed.
		Locale locale;									///< The language (English or Japanese) to use in diagnostics.
		void *userData;									///< User data passed to callback functions (optional)

		// frontend options
		int32_t generatePreprocessedOutput;				///< Equivalent to -E if non-zero.
		int32_t generateDependencies;					///< Non-zero if any of the -M... options is specified
		int32_t emitLineDirectives;						///< Equivalent to -E -P if zero, -E if non-zero.
		int32_t emitComments;							///< Equivalent to -E -C if non-zero, -E if zero.
		const char *dependencyTargetName;				///< Argument to -MT or -MQ.
		int32_t emitPhonyDependencies;					///< Equivalent to -MP if non-zero.
		int32_t ignoreMissingDependencies;				///< Equivalent to -MG if non-zero.
		int32_t cleanDependencyTarget;					///< Non-zero if -MT was specified.
		int32_t useFx;									///< Equivalent to -fx if non-zero, -nofx otherwise.
		int32_t noStdlib;								///< Equivalent to -nostdlib if non-zero.

		// codegen options
		int32_t optimizationLevel;						///< Equivalent to -O?. Valid range is 0-4.
		int32_t useFastmath;							///< Equivalent to -fastmath if non-zero.
		int32_t useFastprecision;							///< Equivalent to -fastprecision if non-zero.
		int32_t warningsAsErrors;						///< Equivalent to -Werror if non-zero.
		int32_t performanceWarnings;					///< Equivalent to -Wperf if non-zero.
		int32_t warningLevel;							///< Equivalent to -W?. Valid range is 0-4.
		int32_t pedantic;								///< Equivalent to -pedantic if non-zero.
		int32_t pedanticError;							///< Equivalent to -pedantic-error if non-zero.
		int32_t noStrictOverloads;						///< Equivalent to -no-strict-overloads if non-zero.
		int32_t sdbCache;								///< Equivalent to -cache if 1, -cache-ast if 2, -xmlcache if 3, -xmlcache-ast if 4

		/// Additional inputs
		uint32_t gsStreamOutSpecCount;					///< Number of GS stream definitions provided.
		uint32_t layoutFromSbDataSize;					///< Size of the SB passed in the layoutFromSbData argument
		uint32_t layoutFromSdbDataSize;					///< Size of the XML SDB passed in the layoutFromSdbData argument
		uint32_t attributeMappingSize;					///< size of the supplied attributeMapping table (0-16)
		uint32_t attributeInstancingSize;				///< size of the supplied attributeInstancing table (0-16)
		const char* const *gsStreamOutSpecs;			///< GS stream definitions (-gsstream).
		const void* layoutFromSbData;					///< (optional) SB file data which will be used as template for user SGPR allocation
		const void* layoutFromSdbData;					///< (optional) XML SDB file data which will be used as template for user SGPR allocation
		const InputSlotsTemplate* inputSlotsTemplate;	///< (optional) explicit template for user SGPR allocation
		const int32_t* attributeMapping;				///< Table of attribute mappings for a fetch shader
		const int32_t* attributeInstancing;				///< Table of instancing mode for each attribute, possible values for table entries are defined in sce::Gnm::FetchShaderInstancingMode

		uint32_t psMrtFormat;							///< PS MRT format (-mrtformat).
		uint32_t maxUserDataRegs;						///< How many SGPRs used for user params (0-16, default 16)
		uint32_t maxEudCount;							///< Max extended user data count (8-240, default 48)
		uint32_t shaderVGPRs;							///< VGPRs available for shader codegen (24-252, default 252)
		uint32_t shaderSGPRs;							///< SGPRs available for shader codegen (4-102, default 102)
		uint32_t safeNormalize;							///< Check for zero-sized normals and return vec(0.0) if vector is null
		uint32_t dxClamp;								///< Enable DX clamp mode (default: enabled)
		uint32_t optimizeForDebug;						///< equivalent to -Od
		uint32_t invPos;								///< generate a position invariant vertex shader
		uint32_t ttrace;								///< equivalent to -ttrace
		uint32_t inlinefetchshader;						///< inline fetch shader inside vertex shader
		uint32_t enablevalidation;						///< insert instrumentation code for runtime validation
		uint32_t disableGenericVccAlloc;				///< disable generic allocation for vcc/m0 register
		uint32_t discardEarlyout;						///< Enable early-out optimization for discard (default: enabled)
		uint32_t smallStoreCoalescingWar;				///< Enable WAR for small store coalescing (default: enabled)
		uint32_t srtSymbols;							///< Enable full SRT symbol generation (default: enabled)
		uint32_t dontStripDefaultCb;					///< do not strip dead uniform from the default constant buffer
		uint32_t packingParameter;						///< enable parameter packing
		uint32_t indirectDraw;							///< enable indirect draw
		uint32_t disableKcacheForRegularBuffer;			///< dont use scalar loads for RegularBuffers
		uint32_t conditionalTextureloads;				///< dont hoist texture loads into conditional blocks
		uint32_t vmcluster;								///< set maximum cluster size for vm instructions
		uint32_t enableldsdirect;						///< enable lds direct feature
		uint32_t disablePicLiteralBuffer;				///< disable PIC code for literal buffer
		uint32_t cacheUserSourceHash;					///< explicitely select the source hash (default to 0)
		uint32_t cacheGenSourceHash;					///< auto-generate the source hash
		uint32_t minVgprCount;							///< Number of VGPR the compiler can freely use (default to 0 i.e. the compiler must use as few as possible)
		uint32_t minSgprCount;							///< Number of SGPR the compiler can freely use (default to 0 i.e. the compiler must use as few as possible)
		uint32_t vgprReadFirstLane;						///< Force uniform VGPRs to go to SGPRs when used cross-blocks
		uint32_t aggressiveRecoloring;					///< May increase pressure in register allocation to decrease latency (default to 1)
		uint32_t sharpLoadICM;							///< Aggressively move v#/t#/s# out of the loops
		uint32_t preRegAllocReschedule;					///< Pre-RA instruction scheduler to decrease shader latency (default to 1)
		uint32_t postRegAllocReschedule;				///< Post-RA instruction scheduler to decrease shader latency (default to 1)
		uint32_t structuralSink;						///< Enable more involved code sinking using structural analysis (default to 1)
		uint32_t targetOccupancy;						///< Set target number of wavefronts per SIMD unit
		uint32_t targetOccupancyAtAllCosts;				///< Set target number of wavefronts per SIMD unit, and try different middle-end configurations if occupancy is too high
		uint32_t psslcApiSlotAssignment;				///< Try to match psslc API slot assignment (default to 0)
		uint32_t automaticLdsSpilling;					///< Compiler will spill to LDS when it estimates perf-wise (default to 1)
		uint32_t removeRedundancy;						///< Enable common expression elimination in middle end (enabled by default)
		uint32_t uniqueLdsOffset;						///< Compute the offset only once per program (disabled by default)
		WaitCntMode waitCntMode;						///< Control insertion of s_waitcnt instructions
		uint32_t realTypes;								///< Enable use of real types in shaders [experimental] (disabled by default)
		uint32_t aggressiveLoopRestructuring;			///< Restructure loops with multiple exits
		uint32_t primitivesPerPs;						///< Number of primitives per pixel shader (used by LDS spilling)
		uint32_t liveRangeSplitting;					///< Enable live range splitting for spilling (default: 1)
		uint32_t allowSpill;							///< Flag to enable spill to scratch.
		uint32_t sbiVersion;							///< Select version of shader binary to produce (>=2 for sdk 2.500, 0 for lower)
		uint32_t breadthFirstElementOrdering;			///< Use breadth first ordering for buffer element symbols
		uint32_t writeConstantBlock;					///< Write the constant block in the shader binary
		uint32_t createLiteralBuffer;					///< Put constant data in shader function in literal buffer (default: on)
		uint32_t globalScheduling;						///< Enable inter-block instruction scheduling (default to 1)
		uint32_t unrollAllLoops;						///< Try to unroll all loops (default to 0)
		uint32_t preserveVmClusters;					///< Preserve cluster of VM loads in all instruction schedulers
		uint32_t programOrderDag;						///< Force scheduling of instruction in program order
		uint32_t optimizeInstancingForKCache;			///< Generate two vertex shader programs one with uniform instanceID and one with diverging instanceID
		uint32_t removeUnusedVertexOutput;				///< Remove unused vertex outputs from export semantics and symbols
		uint32_t reservedUserSgpr;						///< Number of SGPRs reserved by the user (allocated at the start of user SGPR range)
		uint32_t conditionalcodehoisting;				///< Enable code hoisting inside conditional blocks
		HardwareTarget hardwareTarget;					///< Choose which target the compiler generates code for
		GradientAdjust gradientAdjust;					///< Choose gradient adjust mode
		uint32_t forceScalarization;					///< Do not try to keep vec2 of shorts/halfs alive in the middle end
		uint32_t asmVectorizer;							///< Enable late vectorization in final codegen passes
		uint32_t allocateSubRegisters;					///< Allocate sub-dword in register to decrease register pressure
		BarycentricMode barycentricMode;				///< Choose default barycentric mode
		uint32_t aggressiveInterlockRemoval;			///< May increase SGPR pressure to get rid of hardware interlocks
		uint32_t reserveLds;							///< Amount of LDS reserved by the user
		uint32_t slcImageStore;							///< Force use of SLC for all image stores
		uint32_t slcBufferStore;						///< Force use of SLC for all buffer stores
		SampleRate sampleRate;							///< Select sample rate mode
		uint32_t smemCoalescing;						///< Enable smem coalescing
		uint32_t vmemCoalescing;						///< Enable vmem coalescing
		uint32_t clearAdjustSignBit;					/// Clear adjust sign bit of gradient adjusted texture UVs for non-discarding shaders (default to 0)
		sce::Shader::Wave::Psslc::Options **pipelineStageOptions; ///< Options for each stage
		uint32_t pipelineStageCount;					///< Number of stages to compile in the pipeline
		uint32_t rangeTracking;							///< Enable range tracking pass if non-zero (enabled by default)
		uint32_t unswitchLoops;							///< Enable loop unswitching (enabled by default)
		uint32_t integerGatherAdjust;					///< Adjust coordinates of integer-typed image gathers by half a texel
		uint32_t divergentVmcnt;						///< Insert vmcnt waits based on divergent control flow
		uint32_t undefAsZero;							///< Replaces undefined values by zero
		uint32_t codegen;								///< Runs codegen. Use '0' when you only care about reflection data and nothing else
		uint32_t regAllocPreRegAllocSchedule;			///< Uses brute force (register allocator based) pre-register allocation scheduler (default: 0)
		uint32_t pssl2;									///< Enable PSSL2 features (default: 1)
		uint32_t pssl2EnforceConstMethods;				///< Enforce proper handling of const methods (default: 1)
		uint32_t pssl2ZeroOutArray;						///< Zero out missing fields when initializing arrays (default: 1)
		ThisKind pssl2ThisKind;							///< Kind of 'this' (pointer (like C++) or variable (like HLSL))
		uint32_t pssl2FastComp;							///< Entirely skips parser and semantic checks for unused functions
		uint32_t pssl2CreatePch;						///< Create a PCH (only for PSSL2)
		char const *pssl2Pch;							///< Optional PCH (only for PSSL2)
		uint32_t pssl2MaxInstantiationDepth;			///< Max template instantiation depth (only for PSSL2)
		uint32_t pssl2FlatBaseInit;						///< Whether braces of base initializers must be elided (default: 0)
		uint32_t applyIndexInstanceOffset;				///< Always applies the index/instance offset even with no fetch shader (default: 0)
		uint32_t rematsrtloads;							///< Rematerialize SRT loads to lower SGPR pressure (default: 0)
		uint32_t hoistDiscard;							///< Enable experimental global discard hoisting (default: 1)
		uint32_t densePackingParameter;					///< Enable dense parameter packing
		uint32_t allowResourceInUserSgprs;				///< Allow usage of resources in user sgprs (default: 1)
		uint32_t showBufferSmrdUsage;					///< Show SMRD usages for buffers (default: 0)
		uint32_t padConstantBufferInSrtSignature;		///< Pad each constant buffer V# to 8 dwords to match T# sizes (default: 0)
		uint32_t padMemoryBufferInSrtSignature;			///< Pad each memory buffer V# to 8 dwords to match T# sizes (default: 0)
		uint32_t padSamplerInSrtSignature;				///< Pad each sampler S# to 8 dwords to match T# sizes (default: 0)
		char const *srtSignatureName;					///< Optional SRT signature to attach to the program (given by name)
		SrtSignature const **srtSignatures;				///< Optional SRT signature content. If you want to use this signature, give it the same name as srtSignatureName
		uint32_t srtSignatureCount;						///< Number of SRT signatures to process
		uint32_t alignPatchConstant;					///< Align patch constant leaves to 16 bytes (default: 1)
		uint32_t tessFactorInLds;						///< Hull shader always writes tessellation factors in LDS as patch constant (default). 
		uint32_t forceInLoop;							///< Forces statements inside a loop to run inside a loop. For example, consider: 'for (...) { if (cond) { code; break; }'. Statement 'code' will run at each iteration and not once after we exit the loop (default: 0).
		uint32_t integerNarrowing;						///< Enables integer narrowing (default: 0).
		uint32_t preRaLatencySchedule;					///< Enables pre-register allocation aggressive latency scheduling (default: 0).
		SrtSignatureMissingResource srtSignatureMissingResource; ///< Specifies what to do with resources present in the SRT signature but absent in the source code (default: kSrtSignatureMissingResourceError).
		OptimizeUniformAtomics optimizeUniformAtomics;	///< Reduce the EXEC mask of uniform-address atomics (default: kOptimizeUniformAtomicsNever).
		uint32_t scalarizeSharps;						///< Scalarize diverging v#/t#/s#
		uint32_t strictConversions;						///< Disable aggressive conversion optimizations (default: 0).
		uint32_t cppLayout;								///< Applies C++ rules to type sizes and alignments (default: 0).
		uint32_t demandedBits;							///< Enable demanded bits tracking based simplifications
		uint32_t unflatten;								///< Optimize certain conditional moves into conditional control flow (default: 1).
		uint32_t optimizevmindex;						///< Optimize vm instruction index (default: 1).
	} Options;

	/** @brief Describes the input data for a compilation job using argc/argv-style settings.

		@ingroup orbis_wave
	*/
	struct OptionsWithCommandLine : OptionsBase {
		uint32_t version;								///< Set by sce::Shader::Wave::Psslc::initializeOptions.
		void *reserved;									///< Set by sce::Shader::Wave::Psslc::initializeOptions.
		uint32_t argc;									///< Number of array elements in argv.
		const char * const *argv;						///< Settings in the same format as on the command line.
		void *userData;									///< User data passed to callback functions (optional)
		const InputSlotsTemplate* inputSlotsTemplate;	///< (optional) explicit template for user SGPR allocation
		SrtSignature const **srtSignatures;				///< Optional SRT signature content. If you want to use this signature, give it the same name as srtSignatureName
		uint32_t srtSignatureCount;						///< Number of SRT signatures to process
	};

	/**	@brief	Lists the user defined callbacks for compiler operations.

		The sce::Shader::Wave::Psslc::CallbackList structure is used in each of
		sce::Shader::Wave::Psslc::run, sce::Shader::Wave::Psslc::PreprocessProgram and
		sce::Shader::Wave::Psslc::GenerateDependencies in the same fashion.
		In order to initialize instances of this structure, please always use
		sce::Shader::Wave::Psslc::initializeCallbackList.

		For details regarding the individual callbacks, please refer to their
		respective documentations.

		@ingroup orbis_wave
	*/
	typedef struct CallbackList {
		CallbackOpenFile openFile;					///< The callback used to open a file.
		CallbackReleaseFile releaseFile;			///< The callback used to release an opened file (optional).
		CallbackLocateFile locateFile;				///< The callback used to indicate a file exists (optional).
		CallbackAbsolutePath absolutePath;			///< The callback used to indicate the absolute path of a file (optional).
		CallbackReleaseFileName releaseFileName;	///< The callback used to release an absolute path string (optional).
		CallbackFileDate fileDate;					///< The callback used to indicate file modification date (optional).
		CallbackMemAlloc memAlloc;					///< The callback used for memory allocation (optional).
		CallbackMemFree memFree;					///< The callback used for memory de-allocation (optional).
	} CallbackList;

	/**	@brief	Describes a single compiler diagnostic.

		@ingroup orbis_wave
	*/
	typedef struct DiagnosticMessage {
		DiagnosticLevel level;						///< The severity of the diagnostic.
		uint32_t code;								///< A unique code for each kind of diagnostic.
		const SourceLocation *location;				///< The location for which the diagnostic is reported (optional).
		const char *message;						///< The diagnostic message.
	} DiagnosticMessage;

	/**	@brief	Describes the output of a shader compiler invocation.

		On failure, programData will be 0 and the diagnosticCount will be non-zero.
		On success, programData will be non-zero and one or more non-error
		diagnostics may be present.

		@ingroup orbis_wave
	*/
	typedef struct Output {
		const uint8_t *programData;						///< The compiled program binary data (or preprocessor output).
		uint32_t programSize;							///< The compiled program (or preprocessed source) size.
		const char *sdbData;							///< The contents of the SDB file (optional).
		const char *sdbExt;								///< The extension of the SDB file (optional).
		int32_t sdbDataSize;							///< The size of the SDB file (optional).
		int32_t dependencyTargetCount;					///< The number of dependencies.
		const char* const *dependencyTargets;			///< The individual target rules.
		int32_t diagnosticCount;						///< The number of diagnostics.
		const DiagnosticMessage *diagnostics;			///< The diagnostic message array.
		uint32_t isPipeline;							///< Is it part of a pipeline output?
	} Output;

	/** @brief	Maximum number of stages in a pipeline

		@ingroup orbis_wave
	*/
	typedef enum MaxStageInPipeline {
		kMaxPipelineStageCount = 5  ///< No more that 5 stages in a pipeline
	} MaxStageInPipeline;

	/**	@brief	Describes the output of a pipelined shader compiler invocation.

		On failure, all shaderOutput will be 0 and the diagnosticCount will be non-zero.
		On success, all shaderOutput are set and diagnostics may be present.

		@ingroup orbis_wave
	*/
	typedef struct PipelineOutput {
		Output *shaderOutput[kMaxPipelineStageCount];	///< All compiled stages if success
		int32_t stageCount;								///< The number of stage in the pipeline
		int32_t diagnosticCount;						///< The number of diagnostics.
		const DiagnosticMessage *diagnostics;			///< The diagnostic message array.
	} PipelineOutput;

	///////////////////////////////////////////////////////////////////////////////
	// Functions
	///////////////////////////////////////////////////////////////////////////////

	/**	@brief	Returns the version of the compiler in string form.

		@return
		Returns a string containing the version of the compiler.

		@ingroup orbis_wave
	*/
	SCE_WAVE_INTERFACE
	char const* SCE_WAVE_CALL_CONVENTION getVersionString();

	/**	@brief	Initializes the compile options with the default values.

		The following fields must be initialized by the user:
		- mainSourceFile
		- targetProfile

		All other fields may be left in the state set by this function.

		@param[out] options
			The option structure that should be initialized.
		@param[in] version
			Must be equal to SCE_WAVE_API_VERSION
		@return
			SCE_WAVE_RESULT_API_VERSION_MISMATCH if there is a mismatch between header and dll revisions
			SCE_WAVE_RESULT_OK options are initialized
		@ingroup orbis_wave
	*/
	SCE_WAVE_INTERFACE
	uint32_t SCE_WAVE_CALL_CONVENTION initializeOptions(
		Options *options,
		uint32_t version /* = SCE_WAVE_API_VERSION*/ );

	/**	@brief	Initializes the compile options with the default values.

		@param[out] options
		The option structure that should be initialized.
		@param[in] version
		Must be equal to SCE_WAVE_API_VERSION
		@return
		SCE_WAVE_RESULT_API_VERSION_MISMATCH if there is a mismatch between header and dll revisions
		SCE_WAVE_RESULT_OK options are initialized
		@ingroup orbis_wave
	*/
	SCE_WAVE_INTERFACE
	uint32_t SCE_WAVE_CALL_CONVENTION initializeOptions(
		OptionsWithCommandLine *options,
		uint32_t version /* = SCE_WAVE_API_VERSION*/);

	/**	@brief	Initializes the callback list with the default values.

		There are two kinds of defaults available:
		- kCallbackDefaultsSystemFiles uses the native file system of the operating
			system in the same manner as the command-line version of orbis_shader_compiler.
			This is the default behavior if no callback structure is provided for
			a compilation/pre-processing/dependency job.
		- kCallbackDefaultsTrivial provides placeholder implementations of all callbacks
			but 'openFile'. The latter must always be provided by the user.

		In the trivial case, the following defaults will be used:
		- releaseFile:		Does nothing.
		- locateFile:		Returns the same path it was called with.
		- absolutePath:		Returns the same path it was called with.
		- releaseFileName:	Does nothing.
		- fileDate:			Returns the current time.

		@param[in] callbacks
			The callbacks structure to be initialized.

		@param[in] defaults
			Indicates which set of default callbacks to initialize from.

		@return
			SCE_WAVE_RESULT_INVALID_INPUT_PARAMETER if callbacks argument is NULL
			SCE_WAVE_RESULT_OK callbacks are initialized

		@ingroup orbis_wave
	*/
	SCE_WAVE_INTERFACE
	uint32_t SCE_WAVE_CALL_CONVENTION initializeCallbackList(
		CallbackList *callbacks,
		CallbackDefaults defaults);

	/**	@brief	Invoke the shader compiler.

		Invokes the Orbis shader compiler in order to compile a shader, generate
		preprocessed output or generate dependency information.

		@param[in] options
			Indicates the compile options for compiling a program.
			Also doubles as a session id where multiple compiles are being run.

		@param[in] callbacks
			Defines the callbacks to be used for file system access. If not provided,
			the default file system of the OS will be used (optional).

		@return
			A sce::Shader::Wave::Psslc::Output object, containing the binary and sdb
			outputs of the compile, and/or preprocessing output and dependency
			information. Must be destroyed using sce::Shader::Wave::Psslc::destroyOutput.
			NULL is returned only when internal compiler error occurs

		@ingroup orbis_wave
	*/
	SCE_WAVE_INTERFACE
	Output const* SCE_WAVE_CALL_CONVENTION run(
		const Options *options,
		const CallbackList *callbacks);

	/**	@brief	Invoke the shader compiler.

		Invokes the Orbis shader compiler in order to compile a shader, generate
		preprocessed output or generate dependency information.

		@param[in] options
		Indicates additional compile options for compiling a program.
		Also doubles as a session id where multiple compiles are being run.

		@param[in] callbacks
		Defines the callbacks to be used for file system access. If not provided,
		the default file system of the OS will be used (optional).

		@return
		A sce::Shader::Wave::Psslc::Output object, containing the binary and sdb
		outputs of the compile, and/or preprocessing output and dependency
		information. Must be destroyed using sce::Shader::Wave::Psslc::destroyOutput.
		NULL is returned only when internal compiler error occurs

		@ingroup orbis_wave
	*/
	SCE_WAVE_INTERFACE
	Output const* SCE_WAVE_CALL_CONVENTION run(
		const OptionsWithCommandLine *options,
		const CallbackList *callbacks);

	/**	@brief	Releases all allocations associated with a compiled/preprocessed program.

		@param[in] output
			The result from a call to sce::Shader::Wave::Psslc::run, to be destroyed.

		@return
			SCE_WAVE_RESULT_INVALID_INPUT_PARAMETER if the argument is invalid
			SCE_WAVE_RESULT_OK if successful

		@ingroup orbis_wave
	*/
	SCE_WAVE_INTERFACE
	uint32_t SCE_WAVE_CALL_CONVENTION destroyOutput(
		Output const *output);

	/**	@brief	Invoke the shader compiler for a pipeline compilation.

		Invokes the Orbis shader compiler in order to compile a shader pipeline

		@param[in] options
			Indicates the compile options for compiling each pipeline stage

		@param[in] stageCount
			The number of stages to compile. This has to match the number of options
			provided

		@param[in] callbacks
			Defines the callbacks to be used for file system access. If not provided,
			the default file system of the OS will be used (optional).

		@return
			A sce::Shader::Wave::Psslc::PipelineOutput object, contains each individual shaders

		@ingroup orbis_wave
	*/
	SCE_WAVE_INTERFACE
	PipelineOutput const* SCE_WAVE_CALL_CONVENTION runPipeline(
		Options const *options,
		CallbackList const *callbacks);

	/**	@brief	Invoke the shader compiler for a pipeline compilation.

		Invokes the Orbis shader compiler in order to compile a shader pipeline

		@param[in] options
		Indicates additional compile options for compiling each pipeline stage

		@param[in] stageCount
		The number of stages to compile. This has to match the number of options
		provided

		@param[in] callbacks
		Defines the callbacks to be used for file system access. If not provided,
		the default file system of the OS will be used (optional).

		@return
		A sce::Shader::Wave::Psslc::PipelineOutput object, contains each individual shaders

		@ingroup orbis_wave
	*/
	SCE_WAVE_INTERFACE
	PipelineOutput const* SCE_WAVE_CALL_CONVENTION runPipeline(
		OptionsWithCommandLine const *options,
		CallbackList const *callbacks);

	/**	@brief	Releases all allocations associated with a shader pipeline

		@param[in] output
			The result from a call to sce::Shader::Wave::Psslc::runPipeline, to be destroyed.

		@ingroup orbis_wave
	*/
	SCE_WAVE_INTERFACE
	uint32_t SCE_WAVE_CALL_CONVENTION destroyPipelineOutput(
		PipelineOutput const *pipelineOutput);

	/**	@brief	Given a shader binary file compute the size for a stripped version

	@param[in] binary
	A pointer to an unstripped shader binary file

	@return
	The size required for an unstripped version of the binary file or zero on error.

	@ingroup orbis_wave
	*/
	SCE_WAVE_INTERFACE
	uint32_t SCE_WAVE_CALL_CONVENTION computeStrippedShaderBinarySize(
		void const *binary);

	/**	@brief	Create a new binary file with all reflection information removed

	@param[in] binary
	A pointer to an unstripped shader binary file

	@param[out] stripped
	A pointer to an area of memory containing the stripped binary file on return.
	The size of this memory should be calculated by calling the
	computeStrippedShaderBinarySize function.

	@ingroup orbis_wave
	*/
	SCE_WAVE_INTERFACE
	void SCE_WAVE_CALL_CONVENTION stripShaderBinary(
			void const *binary,
			void *stripped);

	namespace Sdb
	{
		/**	@brief	Describes the result of SDB compression or decompression operation

		@ingroup orbis_wave
		*/
		typedef struct SdbOutput {
			const void *outputSdbData;		///< Output SDB data
			uint32_t outputSdbDataSize;		///< Output SDB data size
		} SdbOutput;

		/**	@brief	Possible return values from the getSdbType function

		@ingroup orbis_wave
		*/
		typedef enum SdbType {
			kSdbTypeInvalid,				///< Unknown data, not identified as SDB file
			kSdbTypeSdb,					///< Compressed SDB file
			kSdbTypeXmlSdb					///< Uncompressed XML SDB file
		} SdbType;

		/**	@brief	Test if supplied SDB data are in compressed or uncompressed (XML SDB) format

		@param[in] sdbData
		Pointer to SDB data

		@param[in] sdbDataSize
		Size of the SDB data

		@return
			SdbType enum value

		@ingroup orbis_wave
		*/
		SCE_WAVE_INTERFACE
		SdbType getSdbType(
			const void* sdbData,
			uint32_t sdbDataSize);

		/**	@brief	Compress XML SDB data into the SDB format

		@param[in] sdbData
		Pointer to SDB data

		@param[in] sdbDataSize
		Size of the SDB data

		@return SdbOutput structure containing compressed SDB data, NULL

		@ingroup orbis_wave
		*/
		SCE_WAVE_INTERFACE
			SdbOutput const* compressXmlSdb(
				const void* sdbData,
				uint32_t sdbDataSize);

		/**	@brief	Extract SDB data into SDB XML format

		@param[in] sdbData
		Pointer to SDB data

		@param[in] sdbDataSize
		Size of the SDB data

		@ingroup orbis_wave
		*/
		SCE_WAVE_INTERFACE
			SdbOutput const* extractXmlSdb(
				const void* sdbData,
				uint32_t sdbDataSize);

		/**	@brief	Destroy SdbOutput returned by the compressXmlSdb and extractXmlSdb functions

		@param[in] sdbOutput
		Pointer to SdbOutput data

		@return
			SCE_WAVE_RESULT_INVALID_INPUT_PARAMETER if sdbOutput is not valid
			SCE_WAVE_RESULT_OK if everything was ok

		@ingroup orbis_wave
		*/
		SCE_WAVE_INTERFACE
			uint32_t destroySdbOutput(
				SdbOutput const *sdbOutput);

	}
}	// namespace Psslc
}	// namespace Wave
}	// namespace Shader
}	// namespace sce

#endif /* _SCE_WAVE_PSSL_H */
