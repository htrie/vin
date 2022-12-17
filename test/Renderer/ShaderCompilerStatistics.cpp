#include <iostream>
#include <vector>

#include "Common/Utility/StringManipulation.h"

#include "Visual/Renderer/DrawCalls/EffectGraph.h"
#include "Visual/Renderer/EffectGraphSystem.h"

#include "ShaderCompilerStatistics.h"

#define WIDE2(x) L ## x
#define WIDE(x) WIDE2(x)
#define PRINT_PROFILE_COUNT(name) std::to_wstring(stats->profileCounts[static_cast<uint32_t>(ShaderProfile::name)])
#define PRINT_PROFILE_NAME(name) WIDE( #name )
#define PRINT_PROFILE(name, content) if(usedProfiles[static_cast<uint32_t>(ShaderProfile::name)] == true) {line << StringWithPosition{content(name), offset}; offset += slotSize; }
#define PRINT_TYPE_STAT_LINE(name, cntFX, percFX, cntMat, percMat, profile) line << StringWithPosition{ name, 2 }; {size_t offset = maxTL + 4; if(logFX){line << StringWithPosition{cntFX, offset} << StringWithPosition{percFX, offset+11}; offset+= 17;} if(logMats){line << StringWithPosition{cntMat, offset} << StringWithPosition{percMat, offset+11}; offset+= 17;} const size_t slotSize = 10; PRINT_PROFILE(VS, profile);PRINT_PROFILE(PS, profile);PRINT_PROFILE(GS, profile);PRINT_PROFILE(DS, profile);PRINT_PROFILE(HS, profile);PRINT_PROFILE(CS, profile);PRINT_PROFILE(Other, profile);}line << L'\n';std::wcout << line.str();line.str(L"");

namespace Renderer {
	namespace ShaderCompiler {
		static constexpr size_t MAX_SHOWN_GRAPHS = 20;
		struct StringWithPosition {
			std::wstring Text;
			size_t Position;

			std::wostream& Append(std::wostream& stream) const;
			friend std::wostream& operator<<(std::wostream& stream, const StringWithPosition& t) { return t.Append(stream); }
		};
		class SingleGraphStats {
			public:
				void LogGraph(const std::string& name);
				void Sort();

				uint32_t GetGraphCount() const;
				size_t GetSortedCount() const;
				const std::pair<std::wstring, size_t>& GetSorted(size_t i) const;
				size_t GetLongestGraphNameLength() const;
				uint32_t Contains(const std::wstring& name) const;
			private:
				std::atomic<uint32_t> graphCount = 0;
				size_t sortedCount = 0;
				std::mutex graph_lock;
				Memory::Map<std::wstring, uint32_t, Memory::Tag::ShaderCache> graphs;
				std::pair<std::wstring, size_t> sorted[MAX_SHOWN_GRAPHS];
		};
		class GraphStatistics {
			friend class Statistics;
			public:
				GraphStatistics(const std::wstring& name);

				void LogGraph(const std::string& name, bool matGraph);
				void LogProfile(Statistics::ShaderProfile profile);
				SingleGraphStats& GetFXStats();
				SingleGraphStats& GetMatStats();

				const std::wstring& GetName() const;
				const SingleGraphStats& GetFXStats() const;
				const SingleGraphStats& GetMatStats() const;
				uint32_t GetGraphCount() const;
				size_t GetLongestGraphNameLength() const;

			private:
				const std::wstring name;
				std::atomic<uint32_t> profileCounts[static_cast<uint32_t>(Statistics::ShaderProfile::NUM_SHADER_PROFILES)] = { 0 };
				SingleGraphStats fxGraphs;
				SingleGraphStats matGraphs;
		};

		std::wstring FragmentShortName(const Resource::Wstring& filename)
		{
			std::wstring r = filename.c_str();
			auto f = r.find_last_of(L'/');
			if (f != r.npos) { r = r.substr(f + 1); }
			r = r.substr(0, r.find_last_of(L'.'));
			r = r.substr(0, r.find_first_of(L'_'));
			if (BeginsWith(r, L"animated")) { r = L"animated"; }
			return r;
		}
		void PrintTopGraphs(bool printMats, size_t maxGL, const GraphStatistics& globalStats, const Memory::Map<std::wstring, Memory::Pointer<GraphStatistics, Memory::Tag::ShaderCache>, Memory::Tag::ShaderCache>& fragments, bool logMats, bool logFX)
		{
			if ((printMats == false && logMats) || (printMats == true && logFX))
			{
				const SingleGraphStats& stats = printMats ? globalStats.GetMatStats() : globalStats.GetFXStats();

				Memory::Vector<std::pair<GraphStatistics*, size_t>, Memory::Tag::ShaderCache> offsets;
				size_t lastOff = 0;
				for (auto a = fragments.cbegin(); a != fragments.cend(); ++a)
				{
					size_t off = a->second->GetName().length();
					if (off < 9) { off = 9; }
					off += lastOff + 2;
					lastOff = off;
					offsets.push_back(std::make_pair(a->second.get(), off));
				}

				//Print Header:
				{
					std::wstringstream line;
					if (printMats) { line << StringWithPosition{ L"MATGRAPH",2 }; }
					else { line << StringWithPosition{ L"FXGRAPH",2 }; }
					line << StringWithPosition{ L"TOTAL",maxGL + 7 };
					line << StringWithPosition{ L"%",maxGL + 16 };
					for (auto a : offsets) { line << StringWithPosition{ a.first->GetName(),a.second + maxGL + 22 }; }
					line << L"\n";
					std::wcout << line.str();
				}
				//Print content:
				for (size_t a = 0; a < stats.GetSortedCount(); a++)
				{
					std::wstringstream line;
					line << StringWithPosition{ stats.GetSorted(a).first,4 } << StringWithPosition{ std::to_wstring(stats.GetSorted(a).second),maxGL + 7 } << StringWithPosition{ std::to_wstring((stats.GetSorted(a).second * 100) / stats.GetGraphCount()) + L"%",maxGL + 16 };
					for (auto b : offsets)
					{
						const SingleGraphStats& s = printMats ? b.first->GetMatStats() : b.first->GetFXStats();
						line << StringWithPosition{ std::to_wstring(s.Contains(stats.GetSorted(a).first)),b.second + maxGL + 22 };
					}
					line << L"\n";
					std::wcout << line.str();
				}
				std::wcout << std::endl;
			}
		}

		std::wostream& StringWithPosition::Append(std::wostream& stream) const
		{
			const size_t p = stream.tellp();
			if (p < Position) { stream << std::wstring(Position - p, L' '); }
			return stream << Text;
		}

		Statistics::Logger::Logger(Statistics* parent) : parent(parent) {}
		void Statistics::Logger::SetType(const DrawCalls::EffectGraphHandle& drawcall)
		{
			name = FragmentShortName(drawcall->GetFilename().c_str());
		}
		void Statistics::Logger::SetFilename(const std::wstring& key)
		{
			file = key;
		}
		void Statistics::Logger::LogGraph(const std::string& subGraph, bool matGraph)
		{
			graphs.insert(std::make_pair(subGraph, matGraph));
		}
		void Statistics::Logger::LogProfile(ShaderProfile p)
		{
			profile = p;
		}
		void Statistics::Logger::Store()
		{
			parent->StoreLogger(file, this);
		}

		void SingleGraphStats::LogGraph(const std::string& name)
		{
			graphCount.fetch_add(1, std::memory_order_relaxed);
			std::wstring n;
			WstringFromString(name, n);
			std::unique_lock<std::mutex> lock(graph_lock);
			auto f = graphs.find(n);
			if (f == graphs.end()) { graphs.insert(std::make_pair(n, 1)); }
			else { f->second++; }
		}
		void SingleGraphStats::Sort()
		{
			sortedCount = 0;
			for (auto a = graphs.begin(); a != graphs.end(); ++a)
			{
				sorted[sortedCount++] = *a;
				for (size_t b = sortedCount - 1; b > 0; --b)
				{
					if (sorted[b].second < sorted[b - 1].second) { break; }
					std::swap(sorted[b], sorted[b - 1]);
				}
				if (sortedCount == MAX_SHOWN_GRAPHS)
				{
					for (; a != graphs.end(); ++a)
					{
						if (a->second < sorted[MAX_SHOWN_GRAPHS - 1].second) { continue; }
						sorted[MAX_SHOWN_GRAPHS - 1] = *a;
						for (size_t b = MAX_SHOWN_GRAPHS - 1; b > 0; --b)
						{
							if (sorted[b].second < sorted[b - 1].second) { break; }
							std::swap(sorted[b], sorted[b - 1]);
						}
					}
					break;
				}
			}
		}

		uint32_t SingleGraphStats::GetGraphCount() const { return graphCount; }
		size_t SingleGraphStats::GetSortedCount() const { return sortedCount; }
		const std::pair<std::wstring, size_t>& SingleGraphStats::GetSorted(size_t i) const { return sorted[i]; }
		size_t SingleGraphStats::GetLongestGraphNameLength() const
		{
			size_t r = 0;
			for (size_t a = 0; a < sortedCount; a++)
			{
				if (sorted[a].first.length() > r) { r = sorted[a].first.length(); }
			}
			return r;
		}
		uint32_t SingleGraphStats::Contains(const std::wstring& name) const 
		{ 
			auto f = graphs.find(name);
			if (f != graphs.end()) { return f->second; }
			return 0;
		}

		GraphStatistics::GraphStatistics(const std::wstring& name) : name(name)
		{

		}
		void GraphStatistics::LogGraph(const std::string& name, bool matGraph)
		{
			if (matGraph) { matGraphs.LogGraph(name); }
			else { fxGraphs.LogGraph(name); }
		}
		void GraphStatistics::LogProfile(Statistics::ShaderProfile profile)
		{
			if (profile >= Statistics::ShaderProfile::NUM_SHADER_PROFILES) { profile = Statistics::ShaderProfile::Other; }
			profileCounts[static_cast<uint32_t>(profile)].fetch_add(1, std::memory_order_relaxed);
		}
		SingleGraphStats& GraphStatistics::GetFXStats() { return fxGraphs; }
		SingleGraphStats& GraphStatistics::GetMatStats() { return matGraphs; }
		const SingleGraphStats& GraphStatistics::GetFXStats() const { return fxGraphs; }
		const SingleGraphStats& GraphStatistics::GetMatStats() const { return matGraphs; }
		const std::wstring& GraphStatistics::GetName() const { return name; }
		uint32_t GraphStatistics::GetGraphCount() const { return fxGraphs.GetGraphCount() + matGraphs.GetGraphCount(); }
		size_t GraphStatistics::GetLongestGraphNameLength() const
		{
			const size_t a = fxGraphs.GetLongestGraphNameLength();
			const size_t b = matGraphs.GetLongestGraphNameLength();
			return a > b ? a : b;
		}

		Statistics::Statistics(bool logMatGraphs, bool logFXGraphs) : enabled(logMatGraphs || logFXGraphs), logMats(logMatGraphs), logFX(logFXGraphs)
		{

		}

		void Statistics::Print() const
		{
			if (enabled)
			{
				GraphStatistics globalStats(L"");
				Memory::Map<std::wstring, Memory::Pointer<GraphStatistics, Memory::Tag::ShaderCache>, Memory::Tag::ShaderCache> fragments;
				for (auto a = loggers.cbegin(); a != loggers.cend(); ++a)
				{
					auto f = fragments.find(a->second->name);
					if (f == fragments.end()) 
					{ 
						f = fragments.emplace(a->second->name, Memory::Pointer<GraphStatistics, Memory::Tag::ShaderCache>::make(a->second->name).release()).first;
					}

					globalStats.LogProfile(a->second->profile);
					f->second->LogProfile(a->second->profile);
					for (auto b : a->second->graphs)
					{
						globalStats.LogGraph(b.first, b.second);
						f->second->LogGraph(b.first, b.second);
					}
				}

				if (logFX) { globalStats.GetFXStats().Sort(); }
				if (logMats) { globalStats.GetMatStats().Sort(); }
				size_t maxGL = globalStats.GetLongestGraphNameLength();

				Memory::Vector<GraphStatistics*, Memory::Tag::ShaderCache> sortedFragments;
				size_t maxTL = 0;
				bool usedProfiles[static_cast<uint32_t>(ShaderProfile::NUM_SHADER_PROFILES)] = { false, false, false, false, false, false, false };
				for (auto a = fragments.cbegin(); a != fragments.cend(); ++a)
				{
					const size_t tl = a->second->GetName().length();
					if (tl > maxTL) { maxTL = tl; }
					for (size_t b = 0; b < static_cast<uint32_t>(ShaderProfile::NUM_SHADER_PROFILES); b++)
					{
						if (a->second->profileCounts[b] > 0) { usedProfiles[b] = true; }
					}
					sortedFragments.push_back(a->second.get());
					for (size_t b = sortedFragments.size() - 1; b > 0; b--)
					{
						if (sortedFragments[b]->GetGraphCount() < sortedFragments[b - 1]->GetGraphCount()) { break; }
						std::swap(sortedFragments[b], sortedFragments[b - 1]);
					}
				}

				std::wcout << L"Statistics:\n";
				std::wstringstream line;
				PRINT_TYPE_STAT_LINE(L"TYPE", L"COUNT FX", L"", L"COUNT MAT", L"", PRINT_PROFILE_NAME);
				std::wcout << L"\n";
				const GraphStatistics* stats = &globalStats;
				PRINT_TYPE_STAT_LINE(L"", std::to_wstring(globalStats.GetFXStats().GetGraphCount()), L"", std::to_wstring(globalStats.GetMatStats().GetGraphCount()), L"", PRINT_PROFILE_COUNT);
				for (auto a : sortedFragments)
				{
					stats = a;
					PRINT_TYPE_STAT_LINE(stats->GetName(), std::to_wstring(stats->GetFXStats().GetGraphCount()), std::to_wstring((stats->GetFXStats().GetGraphCount() * 100) / globalStats.GetFXStats().GetGraphCount()) + L"%", std::to_wstring(stats->GetMatStats().GetGraphCount()), std::to_wstring((stats->GetMatStats().GetGraphCount() * 100) / globalStats.GetMatStats().GetGraphCount()) + L"%", PRINT_PROFILE_COUNT);
				}
				std::wcout << std::endl;
				PrintTopGraphs(false, maxGL, globalStats, fragments, logMats, logFX);
				PrintTopGraphs(true, maxGL, globalStats, fragments, logMats, logFX);
			}
		}
		StatisticsLogger Statistics::CreateLogger()
		{
			return Memory::Pointer<Logger, Memory::Tag::ShaderCache>::make(this).release();
		}
		void Statistics::RemoveLogger(const std::wstring& filename)
		{
			loggers.erase(filename);
		}

		
		void Statistics::StoreLogger(const std::wstring& fileName, Logger* logger)
		{
			if (fileName.empty())
			{
				Memory::Pointer<Logger, Memory::Tag::ShaderCache>(logger).reset();
				return;
			}

			std::unique_lock<std::mutex> lock(logger_lock);
			auto f = loggers.find(fileName);
			if (f == loggers.end() || f->second.get() != logger)
			{
				loggers[fileName] = Memory::Pointer<Logger, Memory::Tag::ShaderCache>(logger);
			}
		}

		StatisticsLogger::StatisticsLogger(Statistics::Logger* logger) : impl(logger) {}
		StatisticsLogger::StatisticsLogger(StatisticsLogger&& o) : impl(o.impl) { o.impl = nullptr; }
		StatisticsLogger::~StatisticsLogger()
		{
			if (impl != nullptr) { impl->Store(); }
		}
		StatisticsLogger& StatisticsLogger::operator=(StatisticsLogger&& o)
		{
			if (impl != nullptr) { impl->Store(); }
			impl = o.impl;
			o.impl = nullptr;
			return *this;
		}

		void StatisticsLogger::SetType(const DrawCalls::EffectGraphHandle& drawcall)
		{
			if (impl != nullptr) { impl->SetType(drawcall); }
		}
		void StatisticsLogger::SetFilename(const std::wstring& key)
		{
			if (impl != nullptr) { impl->SetFilename(key); }
		}
		void StatisticsLogger::LogGraph(const std::string& subGraph, bool matGraph)
		{
			if (impl != nullptr) { impl->LogGraph(subGraph, matGraph); }
		}
		void StatisticsLogger::LogProfile(Statistics::ShaderProfile profile)
		{
			if (impl != nullptr) { impl->LogProfile(profile); }
		}
	}
}