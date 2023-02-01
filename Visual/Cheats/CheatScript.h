#pragma once

#include "Common/Utility/Profiler/ScopedProfiler.h"

#if !defined( PRODUCTION_CONFIGURATION ) && GGG_CHEATS == 1

#ifndef CHEAT_SCRIPT_ENABLED
#define CHEAT_SCRIPT_ENABLED

#include <sstream>
#include <stdint.h>
#include <map>
#include <stack>
#include <set>
#include <vector>
#include <string>
#include <fstream>
#include <functional>
#include <streambuf>
#include <unordered_map>
#include <iosfwd>
#include <bitset>
#include <assert.h>
#include <memory>
#include <rapidjson/document.h>
#include <future>

#include "Common/Utility/Geometric.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/Logger/LogLevels.h"

#ifndef WIN32
#	include "Visual/Device/Constants.h"
#endif

#ifdef PS4
namespace std
{
	using FILE = ::FILE;
}
#endif

namespace Cheats
{
	constexpr unsigned DefaultHotKeysVersion = 3;

	using VariableCallback = std::function<bool( std::wstring& line, const size_t iter )>;

	struct ScriptPause
	{
		enum PauseType
		{
			// Base
			None,
			Duration,

			// Game
			Teleport,
			Action,
			Fence,
			SingleFrame,
		};

		virtual void Reset()
		{
			reason = None;
			duration = unsigned( -1 );
		}

		void Reset( PauseType r )
		{
			Reset();
			reason = r;
			duration = unsigned( -1 );
			assert( reason != Duration && L"Duration pause should use the duration constructor" );
		}

		void Reset( PauseType r, const unsigned length )
		{
			Reset();
			duration = length;
			reason = r;
			assert( reason == Duration && L"Only Duration pause can be used with this constructor" );
		}

		bool IsPaused() const { return reason != None; }
		bool IsPaused( PauseType r ) const { return reason == r; }

		PauseType reason = None;
		unsigned duration = ( unsigned ) -1;
	};

	struct ScriptVariable
	{
		int i = 0;
		float f = 0.0f;
		std::wstring s;

		enum State
		{
			String,
			Integer,
			Float
		} state;

		const std::wstring GetString() const;
		void UpgradeToFloat();
		void Add( const std::wstring& value );
		void Subtract( const std::wstring& value );
		void Multiply( const std::wstring& value );
		void Divide( const std::wstring& value );
		void Sqrt();
		void Abs();
		void Floor();
		void Ceil();
		void Round();
		void Set( const std::wstring& value );
		void Min( const std::wstring& value );
		void Max( const std::wstring& value );
		void Clamp( const std::wstring& value_a, const std::wstring& value_b );
	};

	class ScriptVariables
	{
	public:
		std::map<std::wstring, ScriptVariable> var_map;

		void Clear() { var_map.clear(); }
		bool Exists( const std::wstring& key ) const { return var_map.find( key ) != var_map.end(); }
		bool Remove( const std::wstring& key );
		std::wstring GetString( const std::wstring& key ) const;
		ScriptVariable& Get( const std::wstring& key );
		void Set( const std::wstring& key, const std::wstring& value );
	};

	struct ScriptFile
	{
		std::wstring filename;

		struct Line
		{
			std::wstring msg;
			std::vector<size_t> splits;
			int command = 0;
			int repeats = 0;
		};

		std::vector<Line> lines;
	};

	struct Script
	{
		Script( class ScriptInterpreter* interpreter )
			: interpreter( interpreter )
		{ }

		struct LineData
		{
			int repeats = 0;
			int repeat_max = 0;
			int data = 0;
		};

		bool IsPaused() const { return pause.IsPaused(); }
		bool IsPaused( ScriptPause::PauseType r ) const { return pause.IsPaused( r ); }

		class ScriptInterpreter* const interpreter;

		std::shared_ptr<ScriptFile> file;
		Script* root_script = nullptr;
		uint32_t line_number = 0;
		uint32_t line_depth = 0;
		ScriptVariables local_variables;
		std::vector<LineData> data;
		std::vector<std::wstring> parameters;
		bool in_try = false;
		bool locked = false;

		std::stack<int> end_tag_nest;
		ScriptPause pause;

		void Push( int i );
		int Pop();
		int Top();
	};

	struct Binding
	{
		enum Flags
		{
			ControlDown,
			AltDown,
			ShiftDown,
			MaxBindFlags
		};

		using BindFlags_t = std::bitset<MaxBindFlags>;
		BindFlags_t flags;

		Binding( const std::wstring& k, const std::wstring& v, const BindFlags_t& f )
			: key( k )
			, value( v )
			, flags( f )
			, execution( L"" )
		{ }

		Binding( const std::wstring& k, const std::wstring& v, const BindFlags_t& f, const std::wstring& e )
			: key( k )
			, value( v )
			, flags( f )
			, execution( e )
		{ }

		bool operator==( const Binding& other ) const { return flags == other.flags && key == other.key && value == other.value; }
		bool operator!=( const Binding& other ) const { return !( *this == other ); }

		std::wstring key;
		std::wstring value;
		std::wstring execution;
	};

	using ScriptCacheMap = std::unordered_map<std::wstring, std::shared_ptr<ScriptFile>>;

	struct ScriptLock
	{
		class ScriptInterpreter& parent;
		std::weak_ptr<Script> script;

		ScriptLock( ScriptInterpreter& parent, std::shared_ptr<Script> script );
		~ScriptLock();
	};

	#define BaseScriptCommands \
		CMD_POP(end)\
		CMD_PUSH(if)\
		CMD(elif)\
		CMD(else)\
		CMD_POP(endif)\
		CMD_PUSH(repeat)\
		CMD_POP(endrepeat)\
		CMD_PUSH(call)\
		CMD_POP(endcall)\
		CMD_PUSH(try)\
		CMD_POP(endtry)\
		CMD(catch)\
		CMD(throw)\
		CMD(return)\
		CMD(stop)\
		CMD(restart)\
		CMD(set)\
		CMD(setg)\
		CMD(setl)\
		CMD(rem)\
		CMD(clr)\
		CMD(abs)\
		CMD(add)\
		CMD(sub)\
		CMD(mul)\
		CMD(div)\
		CMD(sqrt)\
		CMD(ceil)\
		CMD(floor)\
		CMD(round)\
		CMD(break)\
		CMD(min)\
		CMD(max)\
		CMD(clamp)\

	enum BaseCommands
	{
		CMD_INVALID = 0,
#define CMD(x) CMD_##x,
#define CMD_PUSH(x) CMD(x)
#define CMD_POP(x) CMD(x)
		BaseScriptCommands
		BASE_CMD_MAX
#undef CMD
#undef CMD_PUSH
#undef CMD_POP
	};

	#define BaseRegisterHere \
		RegisterPrefix( VAR_INVALID,	NullCallback(),                                                                                    L"$none" )       \
		RegisterPrefix( ARGS,			ArgumentCallback( script, ARGS, GetPrefixSize(ARGS) ),                                             L"$args[" )      \
		RegisterPrefix( ARG_COUNT,		GetGenerator( std::to_wstring( script->parameters.size() ), ARG_COUNT, GetPrefixSize(ARG_COUNT) ), L"$args.count" ) \
		RegisterPrefix( ALL_ARGS,		AllArgsCallback( script, ALL_ARGS, GetPrefixSize(ALL_ARGS) ),                                      L"$args" )       \
		RegisterPrefix( VARIABLE,		GlobalVariableCallback( script, VARIABLE, GetPrefixSize(VARIABLE) ),                               L"$mem[" )       \
		RegisterPrefix( RETURN,			ReturnCallback( script, RETURN, GetPrefixSize(RETURN) ),                                           L"$result[" )    \
		RegisterPrefix( BROWSE,			BrowseCallback( script, BROWSE, GetPrefixSize(BROWSE) ),                                           L"$browse[" )    \

	enum BaseVariables
	{
	#define RegisterPrefix(name, callback, str) name,
		BaseRegisterHere
		BASE_VARIABLE_MAX
	#undef RegisterPrefix
	};

	class ScriptInterpreter
	{
		friend ScriptLock;

	public:
		using Script_t = std::shared_ptr<Script>;
		using ScriptStack_t = std::stack<Script_t>;
		using ScriptStackVector_t = std::vector<ScriptStack_t>;

		ScriptInterpreter();
		~ScriptInterpreter();

		ScriptVariables& GetGlobalVariables() { return global_variables; }

		std::vector<std::wstring> SplitScriptCommand( std::wstring_view in, bool trim = true );

		bool HandleMessage( const std::wstring& message );

		void WriteNewConfig( std::string filename );
		virtual void OnWriteNewConfig( std::ofstream& file );

		void ReloadConfig();
		virtual void ProcessConfig( const rapidjson::Document& document );

		virtual void Paste( const std::wstring& msg );

		void FailHotkey() { hotkey_result = false; }

		bool ScriptExists( const std::wstring& filename ) const;
		void OpenScript( const std::wstring& filename );
		bool OpenFile( std::wstring path );;

		bool ProcessFile( std::wistringstream& stream );
		bool ProcessFile( const std::wstring& stream ) { std::wistringstream s( stream ); return ProcessFile( s ); }
		void ProcessScript( std::wistringstream& stream );
		void ProcessScript( const std::wstring& stream ) { std::wistringstream s( stream ); ProcessScript( s ); }

		bool ProcessHotkey( const unsigned int uMsg, const WPARAM wParam, const LPARAM lParam );

		Binding MakeBinding( const std::wstring& key, const std::wstring& value, const std::wstring& execution = L"" );
		Binding MakeBinding( const unsigned int uMsg, const WPARAM wParam, const LPARAM lParam );

		bool IsScript( const std::wstring& command );
		virtual bool IsScriptInternal( const std::wstring& command ) { return false; }

		void ProcessScriptStacks();
		bool AbortScriptStack( const std::wstring& reason );
		bool AbortScriptStack( ScriptStack_t& stack, const std::wstring& reason );
		void StopScriptStack();
		void ToggleMute() { mute = !mute; }

		bool Running() const { return active_stack != -1; }
		virtual void Update( const unsigned frame_length );

		virtual void Launch();

		void Pause( unsigned duration );
		void PauseFor( ScriptPause::PauseType reason );

		void SetLast( const std::wstring& new_default );

		const std::set<std::wstring>& GetSearchPaths() const { return search_paths; }
		const std::wstring& GetWorkingDirectory() const;
		const std::string GetWriteDirectory() const;

		virtual const std::wstring GetWriteDirectoryW() const;

		std::wstring GetJsonString( const std::wstring& key ) const;

		virtual VariableCallback GetCallback( const Script* script, const int variable );
		void ProcessVariables( const Script* script, std::wstring& line, const std::vector<int>& disable );

		void PrintVariables();

		virtual void ReloadCache();
		void AddToCache( std::wstring path );
		bool WaitForCacheLoaded( const int time = 0 );

		std::shared_ptr<ScriptFile> GetCachedScript( const std::wstring& path );

		std::shared_ptr<ScriptCacheMap>& GetCache();

		void SetLogLevel( const uint32_t level );
		void Log( const std::wstring& message, const Log::LogLevels::Level level );

		void AddBinding( const Binding& binding, const bool overwrite_existing = true );
		void RemoveBinding( const Binding& binding );
		bool HasBinding( const Binding& binding ) const;
		void ClearAllBindings() { bindings.clear(); }
		void ResetBindingsToDefault( const char* app_name = "path_of_exile" );
		void RestoreMissingBindings( const char* app_name = "path_of_exile" );
		void WaitForNewBinding( std::wstring execution ) { wait_for_new_bind = std::move( execution ); }
		void CancelWaitForNewBinding() { wait_for_new_bind.clear(); }
		bool IsWaitingForNewBinding() const { return !wait_for_new_bind.empty(); }
		void WaitForRebind( const int bind_index ) { wait_for_rebind = bind_index; }
		void CancelWaitForRebind() { wait_for_rebind = -1; }
		bool IsWaitingForRebind() const { return wait_for_rebind >= 0; }
		bool IsWaitingForRebind( const int index ) const { return wait_for_rebind == index; }
		std::vector<Binding>& GetBindings() { return bindings; };

		static const std::wstring& GetSaveLastDirectory();
		static void SetSaveLastDirectory( const std::wstring& new_dir );

		virtual void SendMsg( const std::wstring& msg ) {}
		virtual void PrintMsg( const std::wstring& msg ) {}
		virtual bool Ready() const { return true; }

		virtual const std::wstring& CommandToString( const int cmd );

		int StringToCommand( const std::wstring& str, const ScriptFile* script );
		virtual int StringToCommandInternal( const std::wstring& str );

		virtual const wchar_t* GetVariablePrefix( const int cmd );
		virtual int GetPrefixSize( const int cmd );

		virtual bool IsPop( const int cmd ) const;
		virtual bool IsPush( const int cmd ) const;

		virtual std::wstring GetExternalScriptHeader( const std::wstring& call_to ) { return {}; }

		void DoLast();
		void SaveLast( const std::wstring& path );

	protected:

		ScriptInterpreter( const ScriptInterpreter& other ) = delete;
		ScriptInterpreter( ScriptInterpreter&& other ) = delete;
		ScriptInterpreter& operator=( const ScriptInterpreter& other ) = delete;
		ScriptInterpreter& operator=( ScriptInterpreter&& other ) = delete;

		enum CommandResult { Continue, NextLine, ReEnterLoop, TerminateScript, TerminateAllScripts };
		virtual int HandleScriptCommand( Script* script, const std::wstring& line, const int end_tag_override = -1 );

		Script_t GetCurrentScript();
		ScriptStack_t& GetCurrentStack();

		void ProcessScriptStack( ScriptStack_t& stack );

		virtual int GetNumVariables() const { return BASE_VARIABLE_MAX; }
		virtual int GetNumCommands();

		//

		static std::wstring save_last_directory;

		std::shared_ptr<ScriptCacheMap> script_cache;

		ScriptStackVector_t script_stacks;
		unsigned active_stack = ( unsigned ) -1;

		ScriptVariables global_variables;

		std::set<std::wstring> search_paths;
		const std::wstring empty = L"";

		std::unordered_map<std::string, std::string> json_values;

		std::vector<Binding> bindings;
		std::wstring wait_for_new_bind;
		int wait_for_rebind = -1;

		std::vector<std::future<void>> process_handles;

		std::wstring last;

		int script_lock = 0;

		std::wofstream log_file;
		uint32_t log_level = 0;

		enum { LoopTimeMs = 1000 / 60 };
		int loop_timer = LoopTimeMs;

		bool first_session = true;
		bool launched = false;
		bool first_launch = true;
		bool listening = false;
		bool mute = false;
		bool hotkey_result = false;
	};

	// Utility
	std::pair<size_t, size_t> GetPair( const std::wstring& line, const wchar_t open, const wchar_t close, const size_t offset = 0 );
	bool TryIncrement( Script* script, int pair );
	bool IsInteger( const std::wstring& s );
	int FindEnd( const Script* script );
	void MoveToNextTag( Script* script, int tag = CMD_end );

	template<typename T>
	size_t FindSymbol( const std::wstring& string, const T& symbol, size_t offset = 0 )
	{
		size_t found = string.find( symbol, offset );

		// Ignore symbols inside quotation marks '"'
		const auto CountQuotesLeft  = [&]() { return std::count( string.begin(), string.begin() + found, L'\"' ); };
		const auto CountQuotesRight = [&]() { return std::count( string.begin() + found + StringLength( symbol ), string.end(), L'\"' ); };
		const auto InsideQuotes		= [&]() { return CountQuotesLeft() % 2 == 1 && CountQuotesRight() % 2 == 1; };

		// Symbols are cancelled with a preceeding '\' character.
		while( found > 0 && found != std::wstring::npos && ( string[found - 1] == L'\\' || InsideQuotes() ) )
			found = string.find( symbol, offset + found + 1 );

		return found;
	}

	template<typename T> std::wstring Convert( const T& in ) { return std::to_wstring( in ); }
	inline std::wstring Convert( const Location& in ) { return std::to_wstring( in.x ) + L" " + std::to_wstring( in.y ); }
	inline std::wstring Convert( const Vector2d& in ) { return std::to_wstring( in.x ) + L" " + std::to_wstring( in.y ); }
	inline std::wstring Convert( const std::wstring& in ) { return in; }
	inline std::wstring Convert( const wchar_t* in ) { return std::wstring( in ); }

	template<typename T>
	VariableCallback GetGenerator( const T& in, const int cmd, const int len )
	{
		return [=]( std::wstring& line, const size_t iter )
		{
			line = line.substr( 0, iter ) + Convert( in ) + line.substr( iter + len );
			return true;
		};
	};

	const wchar_t* const * GetVariablesBegin();
	const wchar_t* const * GetVariablesEnd();
}

#endif
#endif
