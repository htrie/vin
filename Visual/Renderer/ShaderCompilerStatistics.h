#pragma once

#include <string>
#include <mutex>
#include <map>
#include <set>
#include <memory>
#include <atomic>
#include <sstream>

#include "Common/Resource/Handle.h"
#include "Visual/Device/Shader.h"

namespace Renderer {

	namespace DrawCalls 
	{
		class EffectGraph;
		typedef std::shared_ptr<EffectGraph> EffectGraphHandle;
	}

	namespace ShaderCompiler {

		class StatisticsLogger;
		class Statistics {
			friend class StatisticsLogger;
			public:
				enum class ShaderProfile 
				{
					VS = 0,
					PS,
					GS,
					HS,
					DS,
					CS,
					Other,

					NUM_SHADER_PROFILES,
				};

			private:
				class Logger 
				{
					friend class Statistics;
					public:
						Logger(Statistics* parent);
						void SetType(const DrawCalls::EffectGraphHandle& drawcall);
						void SetFilename(const std::wstring& key);
						void LogGraph(const std::string& subGraph, bool matGraph);
						void LogProfile(ShaderProfile profile);
						void Store();
					private:
						Statistics* const parent;
						std::wstring name;
						std::wstring file;
						Memory::Map<std::string, bool, Memory::Tag::ShaderCache> graphs;
						ShaderProfile profile = ShaderProfile::Other;
				};
				void StoreLogger(const std::wstring& fileName, Logger* logger);
			public:
				Statistics(bool logMatGraphs, bool logFXGraphs);

				StatisticsLogger CreateLogger();
				void RemoveLogger(const std::wstring& filename);

				void Print() const;
			private:
				const bool enabled;
				const bool logMats;
				const bool logFX;
				std::mutex logger_lock;
				Memory::Map<std::wstring, Memory::Pointer<Logger, Memory::Tag::ShaderCache>, Memory::Tag::ShaderCache> loggers;
		};
		class StatisticsLogger {
			private:
				Statistics::Logger* impl;

			public:
				StatisticsLogger(Statistics::Logger* = nullptr);
				StatisticsLogger(const StatisticsLogger& o) = delete;
				StatisticsLogger(StatisticsLogger&& o);
				~StatisticsLogger();
				StatisticsLogger& operator=(const StatisticsLogger& o) = delete;
				StatisticsLogger& operator=(StatisticsLogger&& o);

				void SetType(const DrawCalls::EffectGraphHandle& drawcall);
				void SetFilename(const std::wstring& key);
				void LogGraph(const std::string& subGraph, bool matGraph);
				void LogProfile(Statistics::ShaderProfile profile);
				void LogProfile(Device::ShaderType type)
				{
					switch (type)
					{
						case Device::VERTEX_SHADER:
							return LogProfile(Statistics::ShaderProfile::VS);
						case Device::PIXEL_SHADER:
							return LogProfile(Statistics::ShaderProfile::PS);
						case Device::COMPUTE_SHADER:
							return LogProfile(Statistics::ShaderProfile::CS);
						default:
							return LogProfile(Statistics::ShaderProfile::Other);
					}
				}

		};
	}
}