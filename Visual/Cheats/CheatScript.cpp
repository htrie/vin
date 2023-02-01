#include "CheatScript.h"

// Double guarded!
#if !defined( PRODUCTION_CONFIGURATION ) && GGG_CHEATS == 1

#if defined(WIN32) && !defined(CONSOLE)
#	include <shellapi.h>
#	include <sys/stat.h>
#endif

#include <algorithm>
#include <regex>
#include <map>

#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/error/en.h>

#include "Common/Characters/CharacterUtility.h"
#include "Common/File/FileSystem.h"
#include "Common/File/PathHelper.h"
#include "Common/Utility/ArrayOperations.h"
#include "Common/Utility/ContainerOperations.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/Numeric.h"
#include "Common/Utility/Profiler/ScopedProfiler.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Console/ConsoleCommon.h"

#include "ClientInstanceCommon/Sessions/Cheats/Cheats.h"

#include "Visual/Device/WindowDX.h"
#include "Visual/GUI2/GUIConsole.h"
#include "Visual/Python/Interpreter.h"
#include "Visual/Cheats/CheatsExternal.h"

#if defined(__APPLE__)
#include "Common/Bridging/Bridge.h"
#endif

#include "Visual/GUI2/GUIConsole.h"

//#define NO_OPTIMISE
#ifdef NO_OPTIMISE
#pragma optimize( "", off )
#endif

namespace
{
	enum class FileTestResult : unsigned char
	{
		MISSING,
		OK,
		RELOAD,
	};

	Memory::Mutex file_test_mutex;
	Memory::Mutex cache_mutex;
	FileTestResult FileTest( const std::wstring& filename )
	{
#if defined(WIN32) && !defined(CONSOLE)
		static struct _stat result;
		static std::unordered_map<std::wstring, decltype( result.st_mtime )> cache;

		const bool file_exists = ( _wstat( filename.c_str(), &result ) == 0 );
		if( !file_exists )
			return FileTestResult::MISSING;

		{
			const Memory::Lock lock( file_test_mutex );
			auto existing = cache.find( filename );
			if( existing == cache.end() )
			{
				cache[filename] = result.st_mtime;
				return FileTestResult::RELOAD;
			}

			if( existing->second < result.st_mtime )
			{
				existing->second = result.st_mtime;
				return FileTestResult::RELOAD;
			}
		}

#endif

		return FileTestResult::OK;
	}
}

#define DeprecatedCommands\
	DO_REPLACE(L"elseif",L"elif")\
	DO_REPLACE(L"endif",L"end")\
	DO_REPLACE(L"endforeach",L"end")\
	DO_REPLACE(L"endcall",L"end")\
	DO_REPLACE(L"endrepeat",L"end")\

namespace Cheats
{
	std::wstring ScriptInterpreter::save_last_directory = L"Cheats/";

	const std::wstring& ScriptInterpreter::GetSaveLastDirectory()
	{
#ifdef CONSOLE
		return Console::GetTemporaryDirectory();
#endif
		return save_last_directory;
	}

	void ScriptInterpreter::SetSaveLastDirectory( const std::wstring& new_dir )
	{
#ifndef CONSOLE
		save_last_directory = new_dir;
#endif
	}

	const std::wstring script_commands[] =
	{
#define CMD(x) WIDEN(STRINGIZE(x)),
#define CMD_POP(x) CMD(x)
#define CMD_PUSH(x) CMD(x)
		BaseScriptCommands
#undef CMD
#undef CMD_POP
#undef CMD_PUSH
	};

	const std::pair<std::wstring,std::wstring> deprecated[] =
	{
		#define DO_REPLACE(OLD,NEW) {OLD,NEW},
			DeprecatedCommands
		#undef DO_REPLACE
	};

	bool deprecated_warning[] =
	{
		#define DO_REPLACE(OLD,NEW) false,
			DeprecatedCommands
		#undef DO_REPLACE
	};

	const int base_push_script_commands[] =
	{
#define CMD(x)
#define CMD_POP(x)
#define CMD_PUSH(x) CMD_##x,
		BaseScriptCommands
#undef CMD
#undef CMD_POP
#undef CMD_PUSH
	};

	const int base_pop_script_commands[] =
	{
#define CMD(x)
#define CMD_PUSH(x)
#define CMD_POP(x) CMD_##x,
		BaseScriptCommands
#undef CMD
#undef CMD_POP
#undef CMD_PUSH
	};

	constexpr const wchar_t* base_prefix[] =
	{
#define RegisterPrefix(name, callback, str) str,
		BaseRegisterHere
#undef RegisterPrefix
	};

	constexpr size_t base_prefix_size[] =
	{
#define RegisterPrefix(name, callback, str) CompileTime::StringLength(base_prefix[name]),
		BaseRegisterHere
#undef RegisterPrefix
	};

	bool IsInteger( const std::wstring& s )
	{
		float f;
		std::wistringstream iss( s );
		iss >> std::noskipws >> f;
		return iss.eof() && !iss.fail() && s.find( L"." ) == s.npos;
	}

	std::pair<size_t, size_t> GetPair( const std::wstring& line, const wchar_t open, const wchar_t close, const size_t offset )
	{
		CHEAT_PROFILE;
		size_t begin = line.npos, end = line.npos;

		// Find next bracket pair.
		int nest = 0;
		for( size_t i = offset; i < line.size(); ++i )
		{
			if( line[i] == L'\\' )
				i++;
			else if( line[i] == open )
			{
				if( begin == line.npos )
					begin = i;
				nest++;
			}
			else if( line[i] == close )
			{
				nest--;
				if( nest == 0 )
				{
					end = i;
					break;
				}
			}
		}

		return std::make_pair( begin, end );
	}

	VariableCallback GetGenerator( const std::wstring& in, const int cmd, const int len )
	{
		return [=]( std::wstring& line, const size_t iter )
		{
			line = line.substr( 0, iter ) + in + line.substr( iter + len );
			return true;
		};
	};

	bool TryIncrement( Script* script, int pair )
	{
		auto& line = script->data[pair];
		if( line.repeats < ( line.repeat_max - 1 ) )
		{
			line.repeats++;
			script->line_number = pair;
			script->line_depth = 0;
			return true;
		}

		line.repeats = 0;
		return false;
	}

	int FindEnd( const Script* script )
	{
		CHEAT_PROFILE;
		int nest = 0;
		for( int i = script->line_number + 1; i < (int) script->file->lines.size(); ++i )
		{
			const auto& command = script->file->lines[i].command;
			if( command == CMD_end )
			{
				if( nest == 0 )
					return i;
				--nest;
			}
			if( command != CMD_INVALID && script->interpreter->IsPush( command ) )
				nest++;
		}

		assert( false );
		return -1;
	}

	void MoveToNextTag( Script* script, int tag )
	{
		CHEAT_PROFILE;
		int nest_count = 0;

		const int command = script->file->lines[script->line_number].command;
		if( command != CMD_INVALID && script->interpreter->IsPush( command ) )
			script->line_number++;

		while( true )
		{
			const int command = script->file->lines[script->line_number].command;
			if( command == tag || command == CMD_end )
			{
				if( nest_count == 0 )
					break;
				--nest_count;
			}

			if( command != CMD_INVALID && script->interpreter->IsPush( command ) )
				nest_count++;

			script->line_number++;
		}
	}

	namespace
	{
		unsigned g_save_id = 0;

		bool BeginsWith_IgnoreTab( const std::wstring& s, const std::wstring& b )
		{
			const auto find = s.find( b );
			return find != s.npos && find == s.find_first_not_of( L"\t " );
		}

		bool BeginsWith_Ignore( const std::wstring& s, const std::wstring& b, const std::wstring& ignore )
		{
			const auto find = s.find( b );
			return find != s.npos && find == s.find_first_not_of( ignore );
		}

		std::vector<std::wstring> ExtractParameters( std::wistringstream& stream )
		{
			CHEAT_PROFILE;
			std::vector<std::wstring> parameters;
			std::wstring parameter;

			std::wstring multi_word_param;
			bool is_multi_word_param = false;
			while( stream >> parameter )
			{
				if( is_multi_word_param )
				{
					multi_word_param += L" " + parameter;

					if( EndsWith( parameter, L"\"" ) )
					{
						//parameters.push_back( multi_word_param.substr( 1, multi_word_param.size() - 2 ) );
						parameters.push_back( multi_word_param );
						is_multi_word_param = false;
						multi_word_param = L"";
					}
				}
				else
				{
					if( BeginsWith( parameter, L"\"" ) )
					{
						multi_word_param = parameter;
						is_multi_word_param = true;
					}
					else
						parameters.push_back( parameter );
				}
			}

			if( !multi_word_param.empty() )
				parameters.push_back( multi_word_param );

			return parameters;
		}

		VariableCallback BrowseCallback( const Script* script, const int cmd, const int len )
		{
			return [=]( std::wstring& line, const size_t iter )
			{
#ifdef WIN32
				CHEAT_PROFILE;
				size_t begin, end;
				std::tie( begin, end ) = GetPair( line, L'[', L']', iter );
				if( end == line.npos )
					return false;

				const auto before = line.substr( 0, iter );
				const auto after = line.substr( end + 1 );
				auto between = line.substr( iter + len, end - ( iter + len ) );

				std::vector<std::wstring> temp;
				::SplitString( temp, between, L'|' );

				std::wstring use;

				for( const auto& entry : temp )
					use += entry + L'\0' + entry + L'\0';

				std::vector<int> disables = {};
				script->interpreter->ProcessVariables( script, use, disables );
				TrimStringInPlace( use );

				auto value = FileSystem::GetFilename( use.c_str() );
				script->interpreter->ProcessVariables( script, value, disables );
				line = before + value + after;
				return true;
#else
				return false;
#endif
			};
		};

		VariableCallback ReturnCallback( const Script* script, const int cmd, const int len )
		{
			return [=]( std::wstring& line, const size_t iter )
			{
				CHEAT_PROFILE;
				size_t begin, end;
				std::tie( begin, end ) = GetPair( line, L'[', L']', iter );
				if( end == line.npos )
					return false;

				const auto before = line.substr( 0, iter );
				const auto after = line.substr( end + 1 );
				auto between = line.substr( iter + len, end - ( iter + len ) );

				std::vector<int> disables = {};
				script->interpreter->ProcessVariables( script, between, disables );
				TrimStringInPlace( between );

				Script* root_script = script->root_script;
				root_script->local_variables.Remove( L"result" );

				script->interpreter->ProcessScript( between );

				auto value = root_script->local_variables.GetString( L"result" );
				script->interpreter->ProcessVariables( script, value, disables );

				line = before + value + after;
				return true;
			};
		};

		VariableCallback GlobalVariableCallback( const Cheats::Script* script, const int cmd, const int len )
		{
			return [=]( std::wstring& line, const size_t iter )
			{
				CHEAT_PROFILE;
				size_t begin, end;
				std::tie( begin, end ) = GetPair( line, L'[', L']', iter );
				if( end == line.npos )
					return false;

				const auto before = line.substr( 0, iter );
				const auto after = line.substr( end + 1 ); 
				auto between = line.substr( iter + len, end - ( iter + len ) );

				std::vector<int> disables = {};
				script->interpreter->ProcessVariables( script, between, disables );
				TrimStringInPlace( between );

				auto value = script->root_script->local_variables.GetString( between );
				if( value == L"null" )
					value = script->interpreter->GetGlobalVariables().GetString( between );

				line = before + value + after;
				return true;
			};
		};

		VariableCallback NullCallback( )
		{
			return [=]( std::wstring& line, const size_t iter )
			{
				return false;
			};
		}

		VariableCallback AllArgsCallback( const Cheats::Script* script, const int cmd, const int len )
		{
			return [=]( std::wstring& line, const size_t iter )
			{
				CHEAT_PROFILE;
				const auto before = line.substr( 0, iter );
				const auto after = line.substr( iter + len );

				if( script->parameters.empty() )
				{
					line = before + after;
					return true;
				}

				line = before;
				bool first = true;
				for( const auto & parameter : script->parameters )
				{
					line += ( first ? parameter : L" " + parameter );
					first = false;
				}
				line += after;

				return true;
			};
		}

		VariableCallback ArgumentCallback( const Cheats::Script* script, const int cmd, const int len )
		{
			return [=]( std::wstring& line, const size_t iter )
			{
				CHEAT_PROFILE;
				size_t begin, end;
				std::tie( begin, end ) = GetPair( line, L'[', L']', iter );
				if( end == line.npos )
					return false;

				const auto before = line.substr( 0, iter );
				const auto after = line.substr( end + 1 );
				auto between = line.substr( iter + len, end - ( iter + len ) );

				std::vector<int> disables = {};
				script->interpreter->ProcessVariables( script, between, disables );
				TrimStringInPlace( between );

				if( script->parameters.empty() )
				{
					line = before + after;
					return true;
				}

				const auto slice_iter = FindSymbol( between, L":" );

				if( slice_iter != between.npos )
				{
					const auto slice_before = between.substr( 0, slice_iter );
					const auto slice_after = between.substr( slice_iter + 1, between.size() - slice_before.size() - 2 );
					const unsigned start_index = slice_before.empty() ? 0 : std::min( (unsigned)script->parameters.size(), (unsigned)std::stoul( slice_before ) );
					const unsigned end_index = slice_after.empty() ? (unsigned)script->parameters.size() : std::min( (unsigned)script->parameters.size(), (unsigned)( std::stoul( slice_after ) + 1 ) );

					line = before;
					bool first = true;
					for( unsigned i = start_index; i < end_index; ++i )
					{
						line += ( first ? script->parameters[i] : L" " + script->parameters[i] );
						first = false;
					}
					line += after;
				}
				else
				{
					if( !between.empty() )
					{
						const unsigned index = std::stoul( between );
						const auto& insert = ( index < script->parameters.size() ? script->parameters[index] : L"" );
						line = before + insert + after;
					}
					else
					{
						line = before;
						bool first = true;
						for( auto& param : script->parameters )
						{
							line += ( first ? param : L" " + param );
							first = false;
						}
						line += after;
					}
				}

				return true;
			};
		}

		int ExtractRepeatCountConst( const std::wstring& line )
		{
			CHEAT_PROFILE;
			if( FindSymbol( line, L"#" ) != line.npos )
				return 1; // These lines have repeats on them!

			int repeat_count = 1;
			auto find_repeat = line.find_last_of( L'x' );
			if( find_repeat > 0 && find_repeat != line.npos && line[find_repeat - 1] == L' ' )
			{
				const auto repeat = line.substr( find_repeat + 1 );
				if( IsNumber( repeat ) )
					repeat_count = std::stoi( repeat );
			}

			return repeat_count;
		}

		int ExtractRepeatCount( std::wstring& line )
		{
			CHEAT_PROFILE;
			if( line.find( L'#' ) != line.npos )
				return 1; // These lines have repeats on them!

			int repeat_count = 1;
			const auto find_repeat = line.find_last_of( L'x' );
			if( find_repeat > 0 && find_repeat != line.npos && line[find_repeat - 1] == L' ' )
			{
				const auto repeat = line.substr( find_repeat + 1 );
				if( IsNumber( repeat ) )
				{
					repeat_count = std::stoi( repeat );
					line = line.substr( 0, find_repeat );
				}
			}

			return repeat_count;
		}

		const wchar_t* ValidFile( const ScriptFile& script, const ScriptInterpreter& interpreter )
		{
			CHEAT_PROFILE;
			int nest = 0;
			int in_try_nest = 0;

			for( const auto& line : script.lines )
			{
				if( line.command == CMD_INVALID )
					continue;

				if( interpreter.IsPop( line.command ) )
				{
					if( nest == in_try_nest )
						in_try_nest = 0;

					--nest;
				}
				else if( interpreter.IsPush( line.command ) )
					++nest;

				if( line.command == CMD_try )
				{
					if( in_try_nest )
						return L"#try statement found within another #try statement.";
					in_try_nest = nest;
				}
				else if( line.command == CMD_catch )
				{
					if( !in_try_nest )
						return L"#catch statement found with no matching #try statement.";
					else if( in_try_nest != nest )
						return L"#catch statement nest-mismatch within #try statement.";
				}
			}

			if( nest > 0 )
				return L"Missing #end statement(s) detected.";
			else if( nest < 0 )
				return L"Too many #end statement(s) detected.";

			return nullptr;
		}

		bool ParseFile( std::wistream& file, ScriptFile& script, ScriptInterpreter& interpreter )
		{
			CHEAT_PROFILE;

			ScriptFile::Line new_line;

			while( std::getline( file, new_line.msg ) )
			{
				new_line.msg = RightTrimString( new_line.msg );
				new_line.command = interpreter.StringToCommand( new_line.msg, &script );
				if( new_line.command != CMD_INVALID && interpreter.IsPop( new_line.command ) )
					new_line.command = CMD_end;

				new_line.repeats = ExtractRepeatCount( new_line.msg );

				script.lines.push_back( new_line );
			}

			if( auto fail = ValidFile( script, interpreter ) )
			{
				LOG_CRIT( L"Parsing: " << script.filename << L" failed because: " << fail );
				return false;
			}

			bool call_block = false;
			size_t start = 0;
			for( size_t i = 0; i < script.lines.size(); ++i )
			{
				auto& msg = script.lines[i].msg;
				const auto cmd = script.lines[i].command;
				if( msg.empty() )
					continue;

				if( call_block )
				{
					call_block = cmd != CMD_end;
					if( call_block )
						continue;

					size_t min = 9999;
					size_t count = 0;

					for( size_t j = start; j < i; ++j )
					{
						count = script.lines[j].msg.find_first_not_of( L" \t" );
						if( count < min )
							min = count;
					}

					if( min < 9999 )
					{
						for( size_t j = start; j < i; ++j )
						{
							size_t find_tab = script.lines[j].msg.find_first_not_of( L" \t" );
							if( find_tab != std::wstring::npos && find_tab >= min )
								script.lines[j].msg = script.lines[j].msg.substr( min );
						}
					}

					continue;
				}

				if( call_block = cmd == CMD_call )
					start = i + 1;

				// Strip tabs from front.
				size_t tab = msg.find_first_not_of( L" \t" );
				if( tab != msg.npos )
					msg = msg.substr( tab );
				else if( msg[0] == L'\t' || msg[0] == L' ' )
				{
					msg.clear();
					continue;
				}

				// Strip comments from end.
				size_t comment = FindSymbol( msg, L"//" );
				if( comment != msg.npos )
					msg = msg.substr( 0, comment );
			}

			return true;
		}

		std::shared_ptr<Script> ReadFile( const std::wstring& _filename, ScriptInterpreter* interpreter )
		{
			CHEAT_PROFILE;

			auto create_instance = [interpreter]( std::shared_ptr<ScriptFile> script_file ) -> std::shared_ptr<Script>
			{
				auto script = std::make_shared<Script>( interpreter );
				script->file = script_file;

				script->data.resize( script->file->lines.size() );

				int i = 0;
				for( auto& line_data : script->data )
				{
					line_data.repeats = 0;
					line_data.repeat_max = script->file->lines[i].repeats;
					line_data.data = 0;
					++i;
				}

				script->line_depth = 0;
				script->line_number = 0;
				script->root_script = nullptr;

				return script;
			};

			if( auto cached = interpreter->GetCachedScript( _filename ) )
				return create_instance( cached );

			return nullptr;
		}

		void Restart( Script* script )
		{
			CHEAT_PROFILE;

			auto* interpreter = script->interpreter;
			auto& lines = script->file->lines;

			// Resolve nesting.
			for( ; script->line_number < lines.size(); ++script->line_number )
			{
				if( interpreter->IsPush( lines[script->line_number].command ) )
					script->Push( CMD_end );
				else if( interpreter->IsPop( lines[script->line_number].command ) )
					script->Pop();
			}

			script->in_try = false;
			script->line_number = 0;
			script->line_depth = 0;

			for( auto& line : script->data )
				line.repeats = 0;
		}

		void Return( Script* script )
		{
			CHEAT_PROFILE;
			std::vector<int> disables = {};

			auto* interpreter = script->interpreter;
			auto& lines = script->file->lines;

			auto value = lines[script->line_number].msg.substr( 7 );
			TrimStringInPlace( value );

			interpreter->ProcessVariables( script, value, disables );
			script->root_script->local_variables.Set( L"result", value );

			// Resolve nesting.
			for( ; script->line_number < lines.size(); ++script->line_number )
			{
				if( interpreter->IsPush( lines[script->line_number].command ) )
					script->Push( CMD_end );
				else if( interpreter->IsPop( lines[script->line_number].command ) )
					script->Pop();
			}
		}

		std::vector<std::wstring> SplitString( const std::wstring& in, const std::wstring& delimiter )
		{
			CHEAT_PROFILE;
			std::vector<std::wstring> out;

			const auto delim_size = delimiter.size();
			if( in.size() < delim_size )
			{
				out.push_back( in );
			}
			else
			{
				size_t start = 0;
				size_t end = 0;

				while( ( end = in.find( delimiter, start ) ) != -1 )
				{
					const std::wstring part( in, start, end - start );
					if( !part.empty() )
						out.push_back( part );
					start = end + delim_size;
				}

				std::wstring left_over( in, start );
				if( !left_over.empty() )
					out.push_back( left_over );
			}

			return out;
		}

		bool IsWarpCheat( const std::wstring& line )
		{
			CHEAT_PROFILE;
			static const std::wstring warp_cheats[] =
			{
				L"warp",
				L"warpboss",
				L"newarea",
				L"changearea",
				L"areachange",
				L"teleport",
				L"hideout",
				L"guild",
				L"delvetest",
				L"sanctumtest",
			};

			std::wstringstream stream( BeginsWith( line, L"/." ) ? line.substr( 2 ) : line.substr( 1 ) );
			std::wstring word;
			stream >> word;

			return std::any_of( std::begin( warp_cheats ), std::end( warp_cheats ), [&]( const auto& warp_cheat )
			{
				return CaseInsensitiveEq( word, warp_cheat );
			} );
		}

		enum Operators { NOT_EQUAL, EQUAL, GREATER, LESSER, GREATER_EQUAL, LESSER_EQUAL, STR_IN, STR_STARTSWITH, STR_ENDSWITH, OPERATOR_MAX };
		const std::wstring operator_strings[] = { L"!=", L"==", L">", L"<", L">=", L"<=", L"in", L"startswith", L"endswith" };
		static_assert( OPERATOR_MAX == sizeof( operator_strings ) / sizeof( operator_strings[0] ), "Number of elements in operator_strings != OPERATOR_MAX" );

		template<typename T>
		bool Evaluate( const Operators equality, const T& left, const T& right )
		{
			switch( equality )
			{
			case EQUAL:         return left == right;
			case NOT_EQUAL:     return left != right;
			case GREATER_EQUAL: return left >= right;
			case LESSER_EQUAL:  return left <= right;
			case LESSER:        return left < right;
			case GREATER:       return left > right;
			default:			return false;
			}
		}

		template<> bool Evaluate<std::wstring> ( const Operators equality, const std::wstring& _left, const std::wstring& _right )
		{
			CHEAT_PROFILE;
			std::wstring left = _left;
			std::wstring right = _right;

			while( left.size() > 1 )
			{
				if( BeginsWith( left, L"'" ) && EndsWith( left, L"'" ) )
					left = left.substr(1, left.size() - 2 );
				else if( BeginsWith( left, L"\"" ) && EndsWith( left, L"\"" ) )
					left = left.substr(1, left.size() - 2 );
				else
					break;
			}

			while( right.size() > 1 )
			{
				if( BeginsWith( right, L"'" ) && EndsWith( right, L"'" ) )
					right = right.substr(1, right.size() - 2 );
				else if( BeginsWith( right, L"\"" ) && EndsWith( right, L"\"" ) )
					right = right.substr(1, right.size() - 2 );
				else
					break;
			}

			switch( equality )
			{
			case EQUAL:				return left == right;
			case NOT_EQUAL:			return left != right;
			case GREATER_EQUAL:		return left >= right;
			case LESSER_EQUAL:		return left <= right;
			case LESSER:			return left < right;
			case GREATER:			return left > right;
			case STR_IN:			return right.find(left) != right.npos;
			case STR_STARTSWITH:	return BeginsWith( left, right );
			case STR_ENDSWITH:		return EndsWith( left, right );
			default:				return false;
			}
		}

		Operators GetOperator( const std::wstring& op_str )
		{
			int op = 0;
			for( auto& operation : operator_strings )
			{
				if( operation == op_str )
					break;
				++op;
			}

			return static_cast< Operators >( op );
		}

		bool TestCondition( const std::wstring& condition )
		{
			CHEAT_PROFILE;

			std::wstringstream str( condition );
			std::vector<std::wstring> words;

			std::wstring left, right;
			while( str >> left )
				words.emplace_back( std::move( left ) );

			if( words.empty() )
				return false;

			Operators op;

			bool in_quote = false;
			std::wstring quote_end = L"'";

			for( unsigned i = 0; i < words.size(); ++i )
			{
				if( in_quote && EndsWith( words[i], quote_end ) )
				{
					in_quote = false;
					continue;
				}
				if( in_quote = BeginsWith( words[i], L"'" ) )
				{
					quote_end = L"'";
					continue;
				}
				if( in_quote = BeginsWith( words[i], L"\"" ) )
				{
					quote_end = L"\"";
					continue;
				}

				op = GetOperator( words[i] );

				if( op != OPERATOR_MAX )
				{
					left = Utility::WJoin( words.begin(), words.begin() + i, L' ' );
					right = Utility::WJoin( words.begin() + i + 1, words.end(), L' ' );
					break;
				}
			}

			if( in_quote )
				throw RuntimeError( "Mismatched quote in condition! Condition: " + WstringToString( condition ) );

			if( op == OPERATOR_MAX )
				return condition.size() && condition != L"null" && condition != L"0" && condition != L"false";

			if( IsNumber( left ) && IsNumber( right ) ) // 8 > 4
				return Evaluate( Operators( op ), std::stof( left ), std::stof( right ) );
			else // String == String
				return Evaluate( Operators( op ), left, right );
		}

		// #if A == B
		// #if A == B || B != C
		// #if ( A == B )
		// #if ( A == B || B == C ) || A
		// #if ( A\( == B\) )

		bool TestStatement( const std::wstring& line )
		{
			CHEAT_PROFILE;
			// Isolate the conditions
			const auto iter_if = line.find( L"if" );
			auto iter_comment = FindSymbol( line, L"//" );

			std::wstring statement = iter_if == line.npos ?
				line.substr( 0, iter_comment ) :
				line.substr( iter_if + 3, iter_comment - ( iter_if + 3 ) );

			size_t begin, end;
			std::tie( begin, end ) = GetPair( statement, L'(', L')' );

			// No brackets left, evaluate now.
			if( begin == statement.npos )
			{
				if( end != statement.npos )
					throw std::runtime_error( "Bad condition detected." );

				// Split by OR operators, then return true if any statements are true.
				const auto split = Cheats::SplitString( statement, L" || " );
				return std::any_of( split.begin(), split.end(), [&]( const auto& c1 )
				{
					const auto split2 = Cheats::SplitString( c1, L" && " );
					return std::all_of( split2.begin(), split2.end(), [&]( const auto& c2 )
					{
						if( c2[0] == L'!' )
							return !TestCondition( c2.substr( 1 ) );
						return TestCondition( c2 );
					} );
				} );
			}

			if( end == statement.npos )
				throw std::runtime_error( "Bad condition detected." );

			const bool negate = begin == 0 ? false : statement[begin - 1] == L'!';
			size_t before_shift = negate ? 1 : 0;

			// Split by brackets.
			const std::wstring between = statement.substr( begin + 1, end - ( begin + 1 ) );
			const std::wstring before = statement.substr( before_shift, begin - before_shift );
			const std::wstring after = end >= statement.length() ? L"" : statement.substr( end + 1 );

			// Evaluate between brackets.
			const bool result = !negate == TestStatement( between );

			// Insert result and continue evaluating whole.
			return TestStatement( before + ( result ? L"1" : L"null" ) + after );
		}

		std::wstring GetOperatorTarget( const std::wstring& line )
		{
			std::wstring key;
			std::wstringstream stream( line );
			stream >> key >> key;

			if( stream.fail() )
				throw std::runtime_error( "Bad global variable call" );

			return key;
		}

		std::pair<std::wstring, std::wstring> GetOperatorPair( const std::wstring& line )
		{
			std::wstringstream stream( line );
			std::wstring key, value;
			stream >> key >> key >> value;

			if( stream.fail() )
				throw std::runtime_error( "Bad global variable call" );

			std::wstring temp;
			while( stream >> temp )
				value += L" " + temp;

			return std::make_pair( std::move( key ), std::move( value ) );
		}

		std::tuple< std::wstring, std::wstring, std::wstring > GetOperatorTuple( const std::wstring& line )
		{
			std::wstringstream stream( line );
			std::wstring key, value, value2;
			stream >> key >> key >> value >> value2;

			if( stream.fail() )
				throw std::runtime_error( "Bad global variable call" );

			return std::make_tuple( std::move( key ), std::move( value ), std::move( value2 ) );
		}

		ScriptVariable* GetVariable( const std::wstring& variable_name, Script* script )
		{
			CHEAT_PROFILE;
			if( script->local_variables.Exists( variable_name ) )
				return &script->local_variables.Get( variable_name );

			if( script->root_script->local_variables.Exists( variable_name ) )
				return &script->root_script->local_variables.Get( variable_name );

			if( script->interpreter->GetGlobalVariables().Exists( variable_name ) )
				return &script->interpreter->GetGlobalVariables().Get( variable_name );

			return nullptr;
		}
	}

	ScriptLock::ScriptLock( ScriptInterpreter& parent, std::shared_ptr<Script> script )
		: parent( parent ), script( script ) 
	{ 
		parent.script_lock++; 

		if( script )
		{
			assert( !script->locked );
			script->locked = true;
		}
	}

	ScriptLock::~ScriptLock()
	{ 
		parent.script_lock--; 

		if( auto s = script.lock() )
		{
			assert( s->locked );
			s->locked = false;
		}
	}

	const std::wstring ScriptVariable::GetString() const
	{
		if( state == String ) return s;
		else if( state == Float ) return std::to_wstring( f );
		else return std::to_wstring( i );
	}

	void ScriptVariable::UpgradeToFloat()
	{
		assert( state == Integer );
		f = static_cast< float >( i );
		i = 0;
		state = Float;
	}

	void ScriptVariable::Add( const std::wstring& value )
	{
		if( state == String )
			s += value;
		else if( IsInteger( value ) )
		{
			if( state == Integer ) { i += std::stoi( value ); }
			else { f += std::stof( value ); }
		}
		else if( IsNumber( value ) )
		{
			if( state == Integer ) { UpgradeToFloat(); }
			f += std::stof( value );
		}
		else { Set( GetString() + value ); }
	}

	void ScriptVariable::Subtract( const std::wstring& value )
	{
		if( state == String )
			throw std::runtime_error( "Strings cannot be subtracted" );
		else if( IsInteger( value ) )
		{
			if( state == Integer ) { i -= std::stoi( value ); }
			else { f -= std::stof( value ); }
		}
		else if( IsNumber( value ) )
		{
			if( state == Integer ) { UpgradeToFloat(); }
			f -= std::stof( value );
		}
	}

	void ScriptVariable::Multiply( const std::wstring& value )
	{
		if( state == String )
			throw std::runtime_error( "Strings cannot be multiplied" );
		else if( IsInteger( value ) )
		{
			if( state == Integer ) { i *= std::stoi( value ); }
			else { f *= std::stof( value ); }
		}
		else if( IsNumber( value ) )
		{
			if( state == Integer ) { UpgradeToFloat(); }
			f *= std::stof( value );
		}
	}

	void ScriptVariable::Divide( const std::wstring& value )
	{
		if( state == String )
			throw std::runtime_error( "Strings cannot be divided" );

		if( IsInteger( value ) )
		{
			const int divisor = std::stoi( value );
			assert( divisor != 0 );

			if( divisor == 0 )
				throw std::runtime_error( "Can't divide by zero!" );

			if( state == Integer )
				i /= divisor;
			else
				f /= static_cast<float>( divisor );
		}
		else if( IsNumber( value ) )
		{
			if( state == Integer ) { UpgradeToFloat(); }

			const float divisor = std::stof( value );
			assert( divisor != 0.0f );

			if( divisor == 0.0f )
				throw std::runtime_error( "Can't divide by zero!" );

			f /= divisor;
		}
	}

	void ScriptVariable::Sqrt()
	{
		if( state == String )
			throw std::runtime_error( "Strings cannot be square-rooted" );
		else if( state == Integer )
			i = static_cast< int >( sqrt( static_cast< float >( i ) ) );
		else
			f = sqrt( f );
	}

	void ScriptVariable::Abs()
	{
		if( state == String )
			throw std::runtime_error( "Strings cannot be numerically manipulated" );
		else if( state == Float )
			f = abs( f );
		else if( state == Integer )
			i = abs( i );
	}

	void ScriptVariable::Floor()
	{
		if( state == String )
			throw std::runtime_error( "Strings cannot be rounded" );
		else if( state == Float )
		{
			state = Integer;
			i = static_cast< int >( f );
			f = 0.0f;
		}
	}

	void ScriptVariable::Ceil()
	{
		if( state == String )
			throw std::runtime_error( "Strings cannot be rounded" );
		else if( state == Float )
		{
			state = Integer;
			i = static_cast< int >( f ) + 1;
			f = 0.0f;
		}
	}

	void ScriptVariable::Round()
	{
		if( state == String )
			throw std::runtime_error( "Strings cannot be rounded" );
		else if( state == Float )
		{
			if( f - static_cast< int >( f ) >= 0.5f )
				Ceil();
			else
				Floor();
		}
	}

	void ScriptVariable::Set( const std::wstring& value )
	{
		s.clear(); i = 0; f = 0;
		if( IsInteger( value ) )
		{
			state = Integer;
			i = std::stoi( value );
		}
		else if( IsNumber( value ) )
		{
			state = Float;
			f = std::stof( value );
		}
		else
		{
			state = String;
			s = value;
		}
	}

	bool ScriptVariables::Remove( const std::wstring& key )
	{
		if( !Exists( key ) )
			return false;
		var_map.erase( key );
		return true;
	}

	void ScriptVariable::Min( const std::wstring& value )
	{
		if( state == String )
			throw std::runtime_error( "Strings cannot be min'd" );
		else if( IsInteger( value ) )
		{
			if( state == Integer ) { i = std::min( i, std::stoi( value ) ); }
			else { f = std::min( f, std::stof( value ) ); }
		}
		else if( IsNumber( value ) )
		{
			if( state == Integer ) { UpgradeToFloat(); }
			f = std::min( f, std::stof( value ) );
		}
	}

	void ScriptVariable::Max( const std::wstring& value )
	{
		if( state == String )
			throw std::runtime_error( "Strings cannot be max'd" );
		else if( IsInteger( value ) )
		{
			if( state == Integer ) { i = std::max( i, std::stoi( value ) ); }
			else { f = std::max( f, std::stof( value ) ); }
		}
		else if( IsNumber( value ) )
		{
			if( state == Integer ) { UpgradeToFloat(); }
			f = std::max( f, std::stof( value ) );
		}
	}

	void ScriptVariable::Clamp( const std::wstring& value_a, const std::wstring& value_b )
	{
		Min( value_a );
		Max( value_b );
	}

	std::wstring ScriptVariables::GetString( const std::wstring& key ) const
	{
		if( Exists( key ) )
			return var_map.at( key ).GetString();
		return L"null";
	}

	Cheats::ScriptVariable& ScriptVariables::Get( const std::wstring& key )
	{
		if( Exists( key ) )
			return var_map[key];
		throw std::runtime_error( "Tried to get a global that doesn't exist!" );
	}

	void ScriptVariables::Set( const std::wstring& key, const std::wstring& value )
	{
		if( value.empty() || value == L"null" ) Remove( key );
		else var_map[key].Set( value );
	}

	int ScriptInterpreter::StringToCommand( const std::wstring& str, const ScriptFile* script )
	{
		CHEAT_PROFILE;
		if( str.empty() )
			return CMD_INVALID;

		const size_t find_cmd = FindSymbol( str, L'#' );
		if( find_cmd == str.npos || find_cmd + 1 >= str.size() )
			return CMD_INVALID;

		switch( str[find_cmd+1] )
		{
		case L' ':
		case L'\0':
		case L'\t':
		case L'\r':
		case L'\n':
			return CMD_INVALID;
		default: break;
		}

		std::wstringstream stream( str.substr( find_cmd + 1 ) );
		std::wstring first_word;
		stream >> first_word;

		if( first_word.empty() )
			return CMD_INVALID;

		// DEPRECATED FIXUP
		for( unsigned i = 0; i < ArraySize( deprecated ); ++i )
		{
			if( first_word == deprecated[i].first )
			{
				first_word = deprecated[i].second;

				if( !deprecated_warning[i] )
				{
					deprecated_warning[i] = true;
					PrintMsg( L"<yellow>{Warning: } <rgb(255,0,0)>{" + deprecated[i].first + L"} has been deprecated. Please change your script to use <rgb(0,255,0)>{" + deprecated[i].second + L"}" );
					if( script ) PrintMsg( L"Script: " + script->filename );
				}
			}
		}

		return StringToCommandInternal( first_word );
	}

	int ScriptInterpreter::StringToCommandInternal( const std::wstring& str )
	{
		int i = 1;
		size_t s = str.size();
		for( const auto& cmd : script_commands )
		{
			if( s == cmd.size() && str == cmd )
				return i;
			++i;
		}

		return CMD_INVALID;
	}

	const std::wstring& ScriptInterpreter::CommandToString( const int cmd )
	{
		if( cmd == CMD_INVALID || ( cmd >= GetNumCommands() ) )
			return script_commands[0];

		return script_commands[cmd];
	}

	const wchar_t* ScriptInterpreter::GetVariablePrefix( const int cmd )
	{
		if( cmd <= VAR_INVALID || cmd >= GetNumVariables() )
			return base_prefix[0];

		return base_prefix[cmd];
	}

	int ScriptInterpreter::GetPrefixSize( const int cmd )
	{
		if( cmd <= VAR_INVALID || cmd >= GetNumVariables() )
			return (int)base_prefix_size[ 0 ];

		return (int)base_prefix_size[ cmd ];
	}

	// If result is true, we have processed a command that needs to restart the loop -- These commands cannot be chained.
	// Otherwise, if result is false we can continue with the script.
	int ScriptInterpreter::HandleScriptCommand( Script* script, const std::wstring& line, const int end_tag_override )
	{
		CHEAT_PROFILE;

//#define DEBUG_SCRIPT(X) LOG_DEBUG( L"Parsing " << script->file->filename << L": " << X << L", nest size = " << script->end_tag_nest.size() )
#define DEBUG_SCRIPT(X)
		
		const auto command = end_tag_override >= 0 ? end_tag_override : script->file->lines[ script->line_number ].command;
		assert( command < ArraySize( script_commands ) );
		DEBUG_SCRIPT( L"#" << ( command == 0 ? L"INVALID" : script_commands[ command - 1 ].c_str() ) );

		switch( command )
		{
		case CMD_end:
		{
			const int top = script->Top();
			if( top != CMD_end )
				return HandleScriptCommand( script, line, top );

			script->Pop();
			return NextLine;
		}
		case CMD_break:
#ifdef WIN32
			if( IsDebuggerPresent() )
				__debugbreak();
#endif
			return NextLine;
		case CMD_if:
		{
			CHEAT_PROFILE_NAMED( "#if statement" );
			script->Push( CMD_endif );
			if( TestStatement( line ) == false )
			{
				CHEAT_PROFILE_NAMED( "#if statement (failed)" );
				DEBUG_SCRIPT( L"(false)" );

				script->line_number++;

				int nest = 0;
				while( true )
				{
					const auto cmd = script->file->lines[script->line_number].command;
					if( cmd == CMD_end )
					{
						if( nest == 0 )
						{
							script->Pop();
							return NextLine;
						}
						--nest;
					}
					else if( IsPush( cmd ) )
						nest++;
					else if( IsPop( cmd ) )
						nest--;
					else if( nest == 0 )
					{
						if( cmd == CMD_elif )
						{
							std::wstring value = script->file->lines[script->line_number].msg;
							ProcessVariables( script, value, std::vector<int>{} );
							if( TestStatement( value ) )
								break;
						}
						if( cmd == CMD_else )
							break;
					}

					script->line_number++;
				}
			}

			return NextLine;
		}
		case CMD_else: case CMD_elif:
		{
			CHEAT_PROFILE_NAMED( "#elif statement" );
			MoveToNextTag( script );
			script->Pop();
			return NextLine;
		}
		case CMD_endif:
		{
			script->Pop();
			return NextLine;
		}
		case CMD_repeat:
		{
			script->Push( CMD_endrepeat );
			CHEAT_PROFILE_NAMED( "#repeat statement" );
			int pair = FindEnd( script );
			script->data[pair].data = script->line_number;

			auto& l = script->data[script->line_number];

			l.repeats = 0;
			l.repeat_max = std::stoi( line.substr( 8 ) );

			if( l.repeat_max == 0 )
			{
				l.repeat_max = script->file->lines[script->line_number].repeats;
				MoveToNextTag( script );
				script->Pop();
			}

			return NextLine;
		}
		case CMD_endrepeat:
		{
			CHEAT_PROFILE_NAMED( "#endrepeat statement" );
			auto& line = script->data[script->line_number];
			if( !TryIncrement( script, line.data ) )
			{
				line.repeats = 0;
				line.repeat_max = script->file->lines[script->line_number].repeats;
				script->Pop();
				return NextLine;
			}

			return NextLine;
		}
		case CMD_restart:
		{
			Restart( script );
			return ReEnterLoop;
		}
		case CMD_stop:
		{
			return TerminateAllScripts;
		}
		case CMD_return:
		{
			Return( script );
			return TerminateScript;
		}
		case CMD_try:
		{
			if( script->in_try )
			{
				AbortScriptStack( L"Nested try/catch blocks are unsupported within a single script" );
				return NextLine;
			}
			else
			{
				script->Push( CMD_catch );
				script->in_try = true;
			}

			return NextLine;
		}
		case CMD_throw:
		{
			std::wstring msg = L"throw";

			const auto find = line.find( L"throw " );
			if( find != line.npos )
				msg = line.substr( find + 6 );

			AbortScriptStack( msg );
			return NextLine;
		}
		case CMD_catch:
		{
			MoveToNextTag( script );
			script->Pop();
			script->in_try = false;
			return NextLine;
		}
		case CMD_call:
		{
			CHEAT_PROFILE_NAMED( "#call statement" );
			std::wstring call_to = line.substr( 6 );
			std::wstring msg;

		#if PYTHON_ENABLED
			auto arg_str = Utility::WJoin( script->parameters.begin(), script->parameters.end(), ',', []( const std::wstring& p ) -> std::wstring
			{
				if( p.empty() )
					return L"None";
				if( p == L"None" )
					return p;
				if( IsNumber( p ) )
					return p;

				return L"'" + Replace( p, L"'", L"\\'" ) + L"'";
			} );

			Python::Execute(
				"if '_arg_stack' not in globals():\n"
				"    _arg_stack = []\n"
				"if '_args' in globals():\n"
				"    _arg_stack.append(list(_args))\n"
				"else:\n"
				"    _arg_stack.append([])\n"
				"_args = [" + WstringToString( arg_str ) + "]\n" );
		#else
			msg = L"[";

			for(auto & parameter : script->parameters)
				msg += ( !parameter.empty() ? parameter : L"null" ) + L",";

			if( msg.size() > 1 ) msg[msg.size() - 1] = L']';
			else msg.push_back( ']' );
		#endif

			script->line_number++;
			if( script->line_number == script->data.size() )
				throw std::runtime_error( "Invalid #call/#endcall block" );

			while( script->file->lines[script->line_number].command != CMD_end )
			{
				if( script->line_number == script->data.size() )
					throw std::runtime_error( "Invalid #call/#endcall block" );

				msg += script->file->lines[script->line_number].msg + L"\n";
				script->line_number++;
			}

			auto header = GetExternalScriptHeader( call_to );
			ProcessVariables( script, msg, std::vector<int>{ ARGS, ALL_ARGS } );

		#if PYTHON_ENABLED
			Python::Execute( header + msg + L"\n_args = _arg_stack.pop()" );
		#else
			CheatsExternal intermediary;
			if( !header.empty() )
				intermediary.Send( header, call_to, 2 );
			intermediary.Send( msg, call_to, 0 );
			if( intermediary.GetResultCode() != -1 )
				ProcessScript( intermediary.GetResult() );
		#endif

			return ReEnterLoop;
		}
		case CMD_setg:
		{
			const auto pair = GetOperatorPair( line );
			GetGlobalVariables().Set( pair.first, pair.second );
			return Continue;
		}
		case CMD_set:
		{
			const auto pair = GetOperatorPair( line );
			script->root_script->local_variables.Set( pair.first, pair.second );
			return Continue;
		}
		case CMD_setl:
		{
			const auto pair = GetOperatorPair( line );
			script->local_variables.Set( pair.first, pair.second );
			return Continue;
		}
		case CMD_clr:
		{
			GetGlobalVariables().Clear();
			script->root_script->local_variables.Clear();
			script->local_variables.Clear();
			return Continue;
		}
		case CMD_rem:
		{
			std::wstringstream stream( line );
			std::wstring key;
			stream >> key >> key;
			script->root_script->local_variables.Remove( key );
			script->local_variables.Remove( key );
			GetGlobalVariables().Remove( key );
			return Continue;
		}
		case CMD_add:
		{
			const auto pair = GetOperatorPair( line );
			if( auto* v = GetVariable( pair.first, script ) )
				v->Add( pair.second );
			return Continue;
		}
		case CMD_sub:
		{
			const auto pair = GetOperatorPair( line );
			if( auto* v = GetVariable( pair.first, script ) )
				v->Subtract( pair.second );
			return Continue;
		}
		case CMD_mul:
		{
			const auto pair = GetOperatorPair( line );
			if( auto* v = GetVariable( pair.first, script ) )
				v->Multiply( pair.second );
			return Continue;
		}
		case CMD_div:
		{
			const auto pair = GetOperatorPair( line );
			if( auto* v = GetVariable( pair.first, script ) )
				v->Divide( pair.second );
			return Continue;
		}
		case CMD_sqrt:
		{
			auto target = GetOperatorTarget( line );
			if( auto* v = GetVariable( target, script ) )
				v->Sqrt();
			return Continue;
		}
		case CMD_abs:
		{
			auto target = GetOperatorTarget( line );
			if( auto* v = GetVariable( target, script ) )
				v->Abs();
			return Continue;
		}
		case CMD_floor:
		{
			auto target = GetOperatorTarget( line );
			if( auto* v = GetVariable( target, script ) )
				v->Floor();
			return Continue;
		}
		case CMD_ceil:
		{
			auto target = GetOperatorTarget( line );
			if( auto* v = GetVariable( target, script ) )
				v->Ceil();
			return Continue;
		}
		case CMD_round:
		{
			auto target = GetOperatorTarget( line );
			if( auto* v = GetVariable( target, script ) )
				v->Round();
			return Continue;
		}
		case CMD_min:
		{
			const auto pair = GetOperatorPair( line );
			if( auto* v = GetVariable( pair.first, script ) )
				v->Min( pair.second );
			return Continue;
		}
		case CMD_max:
		{
			const auto pair = GetOperatorPair( line );
			if( auto* v = GetVariable( pair.first, script ) )
				v->Max( pair.second );
			return Continue;
		}
		case CMD_clamp:
		{
			std::wstring target, min, max;
			std::tie( target, min, max ) = GetOperatorTuple( line );
			if( auto* v = GetVariable( target, script ) )
				v->Clamp( min, max );
			return Continue;
		}
		default:
		{
			// This command doesn't exist, ignore it.
			return Continue;
		}
		}
		return Continue;
	}

	ScriptInterpreter::ScriptInterpreter()
		: script_cache( std::make_shared<ScriptCacheMap>() )
	{
		log_file.open( WstringToString(Log::GetLogDirectoryW() + L"/script.cheatlog"), std::wofstream::out | std::wofstream::app );
		LOG_CHEAT_DEBUG( "Interpreter created." );

	#if PYTHON_ENABLED
		if( auto* interpreter = Python::GetInterpreter() )
			interpreter->SetCallback( [this]( const std::wstring& message ) { this->ProcessScript( message ); } );
	#endif
	}

	Cheats::ScriptInterpreter::Script_t ScriptInterpreter::GetCurrentScript()
	{
		if( script_stacks.empty() )
			return nullptr;
		auto& stack = GetCurrentStack();
		return stack.empty() ? nullptr : stack.top();
	}

	Cheats::ScriptInterpreter::ScriptStack_t& ScriptInterpreter::GetCurrentStack()
	{
		if( script_stacks.empty() )
			throw std::runtime_error( "Cannot call GetCurrentStack with no stacks present" );
		return active_stack != -1 ? script_stacks[active_stack] : script_stacks.back();
	}

	ScriptInterpreter::~ScriptInterpreter()
	{
		LOG_CHEAT_DEBUG( "Interpreter destroyed." );
		log_file.close();

	#if PYTHON_ENABLED
		if( auto* interpreter = Python::GetInterpreter() )
			interpreter->DestroyInterpreter();
	#endif
	}

	void ScriptInterpreter::ReloadConfig()
	{
		const std::string filename = GetWriteDirectory() + "cheat_config.json";
		if( !std::ifstream( filename ) )
			WriteNewConfig( filename );

#pragma warning( suppress: 4996 )
		FILE* pFile = fopen( filename.c_str(), "r" );
		PROFILE_ONLY(File::RegisterFileAccess(StringToWstring(filename));)
		rapidjson::Document document;
		char buffer[ 65536 ] = { 0 };

		if( pFile )
		{
			rapidjson::FileReadStream is( pFile, buffer, sizeof( buffer ) );
			if( document.ParseStream( is ).HasParseError() )
				throw std::runtime_error( "Parse error in cheat_config.json: " + std::string( rapidjson::GetParseError_En( document.GetParseError() ) ) );
				
			ProcessConfig( document );
			fclose( pFile );
		}
		else
		{
			// Fake it
			document.SetObject();
			ProcessConfig( document );
		}
	}

	void ScriptInterpreter::ProcessConfig( const rapidjson::Document& document )
	{
		auto& cheats_enabled = document["cheats_enabled"];
		if( !cheats_enabled.IsNull() )
		{
			if( cheats_enabled.IsBool() == false )
				throw std::runtime_error( "Parse error in cheat_config.json: cheats_enabled must be a boolean!" );

			Cheats::GetGlobals().cheats_disabled = !cheats_enabled.GetBool();
		}
		else
		{
			Cheats::GetGlobals().cheats_disabled = false;
		}

		Cheats::GetGlobals().dirty = true;

		auto& _log_level = document["log_level"];

		if( !_log_level.IsNull() )
		{
			if( _log_level.IsNumber() == false )
				throw std::runtime_error( "Parse error in cheat_config.json: log_level must be a number!" );

			log_level = _log_level.GetInt();
		}
		else
		{
			log_level = 0;
		}

		auto& save_last_dir = document["save_last_directory"];
		if( !save_last_dir.IsNull() )
		{
			if( save_last_dir.IsString() == false )
				throw std::runtime_error( "Parse error in cheat_config.json: save_last_directory must be a string!" );

			auto val = StringToWstring( save_last_dir.GetString() );
			if( EndsWith( val, L"/" ) == false )
				val.push_back( L'/' );

			ScriptInterpreter::SetSaveLastDirectory( val );
		}
		else
		{
			ScriptInterpreter::SetSaveLastDirectory( GetWriteDirectoryW() );
		}

		search_paths.clear();
	#if defined(CONSOLE)
		search_paths.insert(PathHelper::MakeAbsolutePath(L"cheats"));
	#else
		search_paths.insert( L"" );
		search_paths.insert( GetWriteDirectoryW() );
		search_paths.insert( GetSaveLastDirectory() );

		auto& paths = document["additional_search_paths"];
		if( !paths.IsNull() )
		{
			if( paths.IsArray() == false )
				throw std::runtime_error( "Parse error in cheat_config.json: additional_search_paths must be an array!" );

			auto add = [&]( const std::wstring& path )
			{
				if( path.empty() )
					return;
				if( path.back() == L'/' )
					search_paths.insert( path );
				else
					search_paths.insert( path + L"/" );
			};

			add( ScriptInterpreter::GetSaveLastDirectory() );

			for( auto path = paths.Begin(); path != paths.End(); ++path )
			{
				if( path->IsString() == false )
					throw std::runtime_error( "Parse error in cheat_config.json: All paths in additional_search_paths must be strings!" );

				add( StringToWstring( path->GetString() ) );
			}
		}
	#endif

		ReloadCache();

		for( auto iter = document.MemberBegin(); iter != document.MemberEnd(); ++iter )
		{
			if( iter->value.IsString() )
				json_values[iter->name.GetString()] = iter->value.GetString();
		}
	}

	void ScriptInterpreter::Paste( const std::wstring& msg )
	{
		SetLast( msg );
		ProcessScript( msg );
		ImGuiEx::Console::AddToHistory( WstringToString( msg ) );
	}

	void ScriptInterpreter::WriteNewConfig( std::string filename )
	{
		FileSystem::CreateDirectoryChain( GetWriteDirectoryW() );
		auto new_file = std::ofstream( filename );

		if( new_file.is_open() )
		{
			OnWriteNewConfig( new_file );
			new_file.close();
		}
	}

	void ScriptInterpreter::OnWriteNewConfig( std::ofstream& file )
	{
		const char* text =
			R"({
	"additional_search_paths":
	[
		"C:/SVNs/qa/Cheats/"
	],
	
	"chat_history_directory": "Cheats/",
	"save_last_directory": "Cheats/",
	"bin_directory": "C:/SVNs/bin/Client/",
	"log_level": 0,
	"use_chat_commands": false,
	"shell_enabled": false,
	"cheats_enabled": true
})";

		file << text;
	}

	bool ScriptInterpreter::ScriptExists( const std::wstring& filename ) const
	{
		CHEAT_PROFILE;
		return script_cache ? script_cache->find( filename ) != script_cache->end() : false;
	}

	void ScriptInterpreter::OpenScript( const std::wstring& filename )
	{
#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
		const auto found = script_cache->find( filename );
		if( found != script_cache->end() )
		{
			const auto& instance = found->second;
			std::wstring path = instance->filename;
			for( wchar_t& i : path )
				if( i == L'/' )
					i = L'\\';

			ShellExecute( nullptr, nullptr, path.c_str(), nullptr, nullptr, SW_SHOW );
		}
#endif
	}

	bool ScriptInterpreter::OpenFile( std::wstring path )
	{
#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
		std::wstring full_path;
		std::replace( path.begin(), path.end(), L'/', L'\\' );

		auto IsValidPath = [&]( const std::wstring& in_path )
		{
			DWORD attributes = GetFileAttributes( in_path.c_str() );
			return attributes != INVALID_FILE_ATTRIBUTES;
		};

		if( path.empty() )
			full_path = GetWorkingDirectory();
		else if( IsValidPath( path ) )
			full_path = path;
		else
		{
			full_path = GetWorkingDirectory() + path;
			if( !IsValidPath( full_path ) )
			{
				if( !IsValidPath( full_path ) )
					return false;
			}
		}

		ShellExecute( 0, 0, ( L"\"" + full_path + L"\"" ).c_str(), 0, 0, SW_SHOW );
#endif
		return true;
	}

	bool ScriptInterpreter::ProcessFile( std::wistringstream& stream )
	{
		{// Profile block.
			CHEAT_PROFILE;
			try
			{
				std::wstring filename;
				stream >> filename;
				const auto script = ReadFile( filename, this );
				if( !script )
				{
					LOG_CHEAT_CRIT( L"Read script failed: " << filename );
					return false;
				}

				bool new_script_stack = false;
				if( script_stacks.empty() || active_stack == -1 )
				{
					LOG_CHEAT_INFO( L"Script started execution" );
					script_stacks.emplace_back( ScriptStack_t() );
					active_stack = (unsigned)script_stacks.size() - 1;
					new_script_stack = true;
				}

				auto& stack = GetCurrentStack();
				auto top = GetCurrentScript();

				script->line_number = 0;
				script->line_depth = 0;
				script->root_script = top ? top->root_script : script.get();
				script->parameters = ExtractParameters( stream );
				stack.push( std::move( script ) );
				ProcessScriptStack( stack );

				if( new_script_stack )
					active_stack = -1;
			}
			catch( const std::exception& e )
			{
				LOG_CHEAT_CRIT( L"Script exception: " + StringToWstring( e.what() ) );
				return false;
			}

			return true;
		}
	}

	void ScriptInterpreter::ProcessScript( std::wistringstream& stream )
	{
		CHEAT_PROFILE;
		auto script = std::make_shared<Script>( this );
		script->line_number = 0;
		script->line_depth = 0;

		bool new_script_stack = false;
		if( script_stacks.empty() || active_stack == -1 )
		{
			LOG_CHEAT_INFO( L"Script started execution" );
			script_stacks.emplace_back( ScriptStack_t() );
			active_stack = (unsigned)script_stacks.size() - 1;
			new_script_stack = true;
		}

		auto& stack = GetCurrentStack();
		script->file = std::make_shared<ScriptFile>();
		script->file->filename = L"";
		script->root_script = !stack.empty() ? stack.top()->root_script : script.get();

		try
		{
			stream.seekg( 0 );
			if( !ParseFile( stream, *script->file, *this ) )
			{
				LOG_CHEAT_CRIT( L"Script parse failed: " + script->file->filename );
				return;
			}
		}
		catch( const std::exception& e )
		{
			const auto msg = StringToWstring( e.what() );
			LOG_CHEAT_CRIT( L"Script exception: " + msg );
			AbortScriptStack( msg );
			return;
		}

		script->data.resize( script->file->lines.size() );

		int i = 0;
		for( auto& line_data : script->data )
		{
			line_data.repeats = 0;
			line_data.repeat_max = script->file->lines[i].repeats;
			line_data.data = 0;
			++i;
		}

		stack.push( std::move( script ) );
		ProcessScriptStack( stack );

		if( new_script_stack )
			active_stack = -1;
	}

	std::wstring Convert( WCHAR key )
	{
		CHEAT_PROFILE;
		std::wstring out;

		if( key >= L'`' && key <= L'i' )
		{
			out = L"Numpad" + std::to_wstring( key - L'`' );
		}
		else if( key >= L'p' && key <= L'z' )
		{
			out = L"F" + std::to_wstring( key - L'p' + 1 );
		}
		else if( key >= 48 && key <= 57 )
		{
			out = L"Number" + std::to_wstring( int( key ) - 48 );
		}
		else
		{
			switch( key )
			{
			case 27:   out = L"Escape"; break;
			case 219:  out = L"LeftBracket"; break;
			case 221:  out = L"RightBracket"; break;
			case 186:  out = L"Semicolon"; break;
			case 222:  out = L"Quote"; break;
			case 188:  out = L"Comma"; break;
			case 190:  out = L"Period"; break;
			case 191:  out = L"ForwardSlash"; break;
			case ',':  out = L"PrintScreen"; break;
			case 3:    out = L"ScrollLock"; break;
			case 18:   out = L"Alt"; break;
			case 20:   out = L"CapsLock"; break;
			case 17:   out = L"Control"; break;
			case 16:   out = L"Shift"; break;
			case 189:  out = L"Minus"; break;
			case 187:  out = L"Equal"; break;
			case 192:  out = L"Tilde"; break;
			case 220:  out = L"Backslash"; break;
			case '[':  out = L"Windows"; break;
			case ']':  out = L"Application"; break;
			case '-':  out = L"Insert"; break;
			case '.':  out = L"Delete"; break;
			case '$':  out = L"Home"; break;
			case '#':  out = L"End"; break;
			case '!':  out = L"PageUp"; break;
			case '\"': out = L"PageDown"; break;
			case '{':  out = L"F12"; break;
			case '&':  out = L"UpArrow"; break;
			case '(':  out = L"DownArrow"; break;
			case '%':  out = L"LeftArrow"; break;
			case '\'': out = L"RightArrow"; break;
			case 'k':  out = L"Numpad+"; break;
			case 'n':  out = L"Numpad."; break;
			case 'm':  out = L"Numpad-"; break;
			case 'j':  out = L"Numpad*"; break;
			case 'o':  out = L"Numpad/"; break;
			case '\r': out = L"Enter"; break;
			case '\t': out = L"Tab"; break;
			case '\b': out = L"Backspace"; break;
			case ' ':  out = L"Spacebar"; break;
			default:
				out.push_back( key );
				break;
			}
		}

		return out;
	}

	template<size_t Bits>
	class mybitset : public std::bitset<Bits>
	{
	public:
		mybitset( const std::initializer_list<bool>& init )
		{
			size_t iter = 0;
			for( const auto& b : init )
			{
				if( b )
					this->set( iter );

				++iter;
			}
		}

		bool only( const size_t pos ) const
		{
			if( !this->test( pos ) )
				return false;

			for( size_t i = 0; i < Bits; ++i )
			{
				if( i == pos )
					continue;
				if( this->test( i ) )
					return false;
			}

			return true;
		}
	};

	// Appropriated this spaghetti code from google.
	bool SystemModifierTest( const unsigned int uMsg, const WPARAM wParam, const LPARAM lParam, std::wstring& key, std::wstring& value )
	{
		CHEAT_PROFILE;
		auto check_bit = []( const auto& val, const auto& bit ) -> bool { return ( val & ( decltype( val )( 1 ) << bit ) ) != 0; };

		enum Flags
		{
			extended,
			alt_bit,
			max_flags
		};

		const mybitset<max_flags> flags = 
		{
			check_bit( lParam, 24 ),
			uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP && check_bit( lParam, 29 )
		};

		if( ( uMsg == WM_SYSKEYUP || uMsg == WM_KEYUP ) && wParam == VK_MENU && flags.none() )
		{
			key = L"Up";
			value = L"Alt";
			return true;
		}

		if( flags.only( alt_bit ) && wParam == VK_MENU )
		{
			key = L"Down";
			value = L"Alt";
			return true;
		}

		// Default
		return false;
	}

	bool IsKeyDown( const unsigned uMsg ) //does not trigger for Alt/Ctrl/Shift
	{
		return uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN;
	}

	bool IsValidBind( const unsigned uMsg, const std::wstring& value )
	{
		return IsKeyDown( uMsg ) && value.size() && value != L"Alt" && value != L"Control" && value != L"Shift";
	}

	bool GetKeypress( const unsigned uMsg, const WPARAM wParam, const LPARAM lParam, std::pair<std::wstring, std::wstring>& out )
	{
		std::wstring& key = out.first;
		std::wstring& value = out.second;

		if( !SystemModifierTest( uMsg, wParam, lParam, key, value ) )
		{
			switch( uMsg )
			{
			default:
				return false;
			case WM_SYSKEYDOWN:
			case WM_KEYDOWN:
				key = L"Down";
				value = Convert( WCHAR( wParam ) );
				break;
			case WM_SYSKEYUP:
			case WM_KEYUP:
				key = L"Up";
				value = Convert( WCHAR( wParam ) );
				break;
			case WM_LBUTTONDOWN:
				key = L"Down";
				value = L"LMB";
				break;
			case WM_XBUTTONDOWN:
				key = L"Down";
				value = L"Mouse4";
				break;
			case WM_XBUTTONUP:
				key = L"Up";
				value = L"Mouse4";
				break;
			case WM_NCXBUTTONDOWN:
				key = L"Down";
				value = L"Mouse5";
				break;
			case WM_NCXBUTTONUP:
				key = L"Up";
				value = L"Mouse5";
				break;
			case WM_LBUTTONUP:
				key = L"Up";
				value = L"LMB";
				break;
			case WM_RBUTTONDOWN:
				key = L"Down";
				value = L"RMB";
				break;
			case WM_RBUTTONUP:
				key = L"Up";
				value = L"RMB";
				break;
			case WM_MBUTTONDOWN:
				key = L"Down";
				value = L"MMB";
				break;
			case WM_MBUTTONUP:
				key = L"Up";
				value = L"MMB";
				break;
			case WM_MOUSEHWHEEL:
				key = L"HScroll";
				value = std::to_wstring( int( ( short ) HIWORD( wParam ) ) );
				break;
			case WM_MOUSEWHEEL:
				key = L"Scroll";
				value = std::to_wstring( int( ( short ) HIWORD( wParam ) ) );
				break;
			case WM_SETFOCUS:
				key = L"GainedFocus";
				break;
			case WM_KILLFOCUS:
				key = L"LostFocus";
				break;
			case WM_SIZE:
				key = L"Resized";
				break;
			}

			return true;
		}

		return false;
	}

	Cheats::Binding ScriptInterpreter::MakeBinding( const std::wstring& key, const std::wstring& value, const std::wstring& execution )
	{
		Binding::BindFlags_t flags;

		const auto window = Device::WindowDX::GetWindow();
		if( window->GetKeyState( VK_CONTROL ) & 0x8000 )
			flags.set( Binding::ControlDown );
		if( ( window->GetKeyState( VK_SHIFT ) & 0x8000 ) && !BeginsWith( value, L"Numpad" ) ) //cannot use Shift with Numpad keys
			flags.set( Binding::ShiftDown );
		if( window->GetKeyState( VK_MENU ) & 0x8000 )
			flags.set( Binding::AltDown );

		return Binding( key, value, flags, execution );
	}

	Cheats::Binding ScriptInterpreter::MakeBinding( const unsigned int uMsg, const WPARAM wParam, const LPARAM lParam )
	{
		std::pair<std::wstring, std::wstring> pair;
		GetKeypress( uMsg, wParam, lParam, pair );
		return MakeBinding( pair.first, pair.second );
	}

	bool ScriptInterpreter::ProcessHotkey( const unsigned int uMsg, const WPARAM wParam, const LPARAM lParam )
	{
		CHEAT_PROFILE;

		std::pair<std::wstring, std::wstring> pair;
		GetKeypress( uMsg, wParam, lParam, pair );
		hotkey_result = true;

		const auto& key = pair.first;
		const auto& value = pair.second;
		if( value.empty() )
			return false;

		if( !bindings.empty() )
		{
			//check if waiting for a keypress to create a new (KeyDown) binding, in which case don't trigger hotkeys
			if( IsWaitingForNewBinding() )
			{
				if( IsValidBind( uMsg, value ) )
				{
					if( wait_for_new_bind == L" " )
						wait_for_new_bind = L"";
					if( value != L"Escape" ) //cannot bind to Escape. Escape cancels wait_for_new_bind
						AddBinding( MakeBinding( key, value, wait_for_new_bind ) );
					CancelWaitForNewBinding();
				}
				return hotkey_result;
			}

			//check if waiting to rebind an existing binding to a new hotkey (must be KeyDown)
			if( IsWaitingForRebind() )
			{
				if( (size_t)wait_for_rebind >= bindings.size() ) //invalid index
				{
					CancelWaitForRebind();
				}
				else if( IsValidBind( uMsg, value ) )
				{
					if( value != L"Escape" ) //cannot rebind to Escape. Escape cancels wait_for_rebind
					{
						const auto rebind = MakeBinding( key, value, bindings[ wait_for_rebind ].execution );
						const auto existing = Find( bindings, rebind );
						bindings[ wait_for_rebind ] = rebind;
						if( existing != bindings.end() )
							bindings.erase( existing );
					}
					CancelWaitForRebind();
				}
				return hotkey_result;
			}

			//trigger hotkeys
			const auto bind = MakeBinding( key, value );
			for( const auto& b : bindings )
			{
				if( b == bind )
				{
					ProcessScript( b.execution );
					return hotkey_result;
				}
			}
		}

		if( ScriptExists( L"hotkey" ) )
		{
			if( !value.empty() )
				ProcessScript( L"/hotkey " + key + L" " + value );
			else
				ProcessScript( L"/hotkey " + key );

			return hotkey_result;
		}

		return false;
	}

	bool ScriptInterpreter::IsScript( const std::wstring& command )
	{
		CHEAT_PROFILE;
		if( command.empty() )                             // Empty strings cannot be scripts.
			return false;
		if( Running() )                                   // Don't splinter a script for no reason if we're already running one.
			return false;
		if( BeginsWith_IgnoreTab( command, L"/paste " ) ) // This will be handled separately.
			return false;
		if( IsWarpCheat( command ) )
			return true;

		if( IsScriptInternal( command ) )
			return true;

		// Chat commands with a repeat can be run as a script.
		if( ExtractRepeatCountConst( command ) > 1 )
			return true;

		// Chat commands with variables in them can be executed like a script.
		for( int i = 0; i < GetNumVariables(); ++i )
		{
			const auto* variable = GetVariablePrefix( i );
			if( command.find( variable ) != command.npos )
				return true;
		}

		// Chat commands that aren't chained can be ignored at this point, probably just a single cheat.
		auto split = SplitScriptCommand( command );
		if( split.empty() ) 
			return false;

		for( const auto& s : split )
		{
			if( s.empty() )
				continue;
			if( s[0] == L'/' )
				return true;
			if( IsScriptInternal( s ) )
				return true;
		}

		return false;
	}

	// #logging
	void ScriptInterpreter::SetLogLevel( const uint32_t level )
	{
		log_level = Clamp( level, 0u, 3u );
	}

	uint32_t GetLogStage( const Log::LogLevels::Level level )
	{
		switch( level )
		{
		case Log::LogLevels::Critical: return 0u; // Always log crits
		case Log::LogLevels::Warning:  return 1u; // Only log warnings at level 1
		case Log::LogLevels::Info:     return 2u; // Only log info at level 2
		case Log::LogLevels::Debug:    return 3u; // Only log debug at level 3
		}

		throw std::runtime_error("Unexpected log level");
	}

	void ScriptInterpreter::Log( const std::wstring& message, const Log::LogLevels::Level level )
	{
		CHEAT_PROFILE;
		
		if( GetLogStage( level ) > log_level )
			return; // Check if we don't care about this log first.

		// Write timestamp
		Log::WriteTimeString( log_file, Time() );

		// Write label
		switch( level )
		{
		case Log::LogLevels::Critical: log_file << L" [CRIT] "; break;
		case Log::LogLevels::Warning:  log_file << L" [WARN] "; break;
		case Log::LogLevels::Info:     log_file << L" [INFO] "; break;
		case Log::LogLevels::Debug:    log_file << L" [DEBG] "; break;
		}

		// Write message
		log_file << message << std::endl;
	}

	bool ScriptInterpreter::HasBinding( const Binding& binding ) const
	{
		for( auto& b : bindings )
		{
			if( b == binding )
				return true;
		}
		return false;
	}

	void ScriptInterpreter::AddBinding( const Binding& binding, const bool overwrite_existing )
	{
		for( auto& b : bindings )
		{
			if( b != binding )
				continue;

			if( overwrite_existing )
				b.execution = binding.execution;
			return;
		}

		bindings.push_back( binding );
	}

	void ScriptInterpreter::RemoveBinding( const Binding& binding )
	{
		RemoveIf( bindings, [&]( const auto& b ) { return b == binding; } );
	}

	void ScriptInterpreter::ResetBindingsToDefault( const char* app_name )
	{
		ClearAllBindings();
		RestoreMissingBindings( app_name );
	}

	void ScriptInterpreter::RestoreMissingBindings( const char* app_name )
	{
		//restore default bindings
		constexpr auto NoFlags	= 0ULL;
		constexpr auto Ctrl		= 1ULL << Binding::ControlDown;
		constexpr auto Shift	= 1ULL << Binding::ShiftDown;
		constexpr auto Alt		= 1ULL << Binding::AltDown;

		const auto AddDefaultKeyBinding = [this]( const wchar_t* key, const unsigned long long flags, const wchar_t* execution )
		{
			AddBinding( Binding( L"Down", key, flags, execution ), false );
		};

		if( CaseInsensitiveEq( app_name, "path_of_exile" ) )
		{
			AddDefaultKeyBinding( L"Numpad0", NoFlags, L"/newarea Programming" );
			AddDefaultKeyBinding( L"Numpad1", NoFlags, L"/g id=$self.id, /say God mode enabled" );
			AddDefaultKeyBinding( L"Numpad2", NoFlags, L"/r id=$self.id, /say Run mode enabled" );
			AddDefaultKeyBinding( L"Numpad3", NoFlags, L"/LevelUp" );
			AddDefaultKeyBinding( L"Numpad4", NoFlags, L"/monster InsectTest" );
			AddDefaultKeyBinding( L"Numpad5", NoFlags, L"/ToggleHUD" );
			AddDefaultKeyBinding( L"Numpad6", NoFlags, L"/addbuff 5000 strdexint 1000 1000 1000 id=$self.id, /say Additional attributes granted" );
			AddDefaultKeyBinding( L"Numpad7", NoFlags, L"/OpenCheatsPanel" );
			AddDefaultKeyBinding( L"Numpad8", NoFlags, L"/banditquest e" );
			AddDefaultKeyBinding( L"Numpad9", NoFlags, L"/cheatstash, /OpenUIScreen StashScreen" );

			AddDefaultKeyBinding( L"C", Ctrl, L"/infoc save_clipboard=true silent=false" );
			AddDefaultKeyBinding( L"C", Shift | Ctrl, L"/infos save_clipboard=true silent=false" );

			AddDefaultKeyBinding( L"N", Shift | Ctrl, L"/CycleCPUID" );
			AddDefaultKeyBinding( L"Q", Shift | Ctrl, L"/ToggleGlobalIllumination" );
			AddDefaultKeyBinding( L"Z", Shift | Ctrl, L"/ToggleForceDisableMerging" );
			AddDefaultKeyBinding( L"L", Shift | Ctrl, L"/TogglePotatoMode" );
			AddDefaultKeyBinding( L"F", Shift | Ctrl, L"/ToggleHighJobs" );
			AddDefaultKeyBinding( L"I", Shift | Ctrl, L"/ToggleInstancing" );
			AddDefaultKeyBinding( L"G", Shift | Ctrl, L"/ToggleGPUParticleConversion" );
			AddDefaultKeyBinding( L"W", Shift | Ctrl, L"/ToggleOverwriteFrameResolution" );

			AddDefaultKeyBinding( L"LeftArrow", Shift | Ctrl, L"/DecreaseFrameScaleWidth" );
			AddDefaultKeyBinding( L"RightArrow", Shift | Ctrl, L"/IncreaseFrameScaleWidth" );
			AddDefaultKeyBinding( L"DownArrow", Shift | Ctrl, L"/DecreaseFrameScaleHeight" );
			AddDefaultKeyBinding( L"UpArrow", Shift | Ctrl, L"/IncreaseFrameScaleHeight" );
		}
	}

	const std::wstring& ScriptInterpreter::GetWorkingDirectory()  const
	{
		return empty;
	}

	const std::string ScriptInterpreter::GetWriteDirectory() const
	{
		return WstringToString( GetWriteDirectoryW() );
	}

	const std::wstring ScriptInterpreter::GetWriteDirectoryW() const
	{
#if defined(CONSOLE)
		return Console::GetTemporaryDirectory() + L"/";
#elif defined( __APPLE__iphoneos )
		return StringToWstring( Bridge::Folders::GetPreferences()) + L"Cheats/";
#endif
#ifdef __APPLE__macosx
        return StringToWstring( Bridge::Folders::GetPreferences() + "Cheats/" );
#endif
		return L"Cheats/";
	}

	Cheats::VariableCallback ScriptInterpreter::GetCallback( const Script* script, const int variable )
	{
		CHEAT_PROFILE;
		switch( variable )
		{
	#define RegisterPrefix(name, callback, str) case name: return callback; break;
			BaseRegisterHere
	#undef RegisterPrefix
		default: throw std::runtime_error( "Bad script callback state!" );
		};
	}

	void ScriptInterpreter::ProcessVariables( const Script* script, std::wstring& line, const std::vector<int>& disable )
	{
		CHEAT_PROFILE;

		// Callbacks are recreated every call because most of them are context specific (i.e. cannot be global).
		std::map<int, VariableCallback> cached_callbacks;
		auto get_callback = [&]( const int index )
		{
			CHEAT_PROFILE_NAMED( "GetCallback" );
			if( cached_callbacks.find( index ) == cached_callbacks.end() )
				cached_callbacks[index] = GetCallback( script, index );

			return cached_callbacks[index];
		};

		for( int callback_iter = 0; callback_iter < GetNumVariables(); )
		{
			if( Contains( disable, callback_iter ) )
			{
				++callback_iter;
				continue;
			}

			const auto* prefix = GetVariablePrefix( callback_iter );

			const auto iter = line.find( prefix );
			if( iter != line.npos )
			{
				LOG_CHEAT_DEBUG( L"Executing callback: " << prefix );
				const bool success = get_callback( callback_iter )( line, iter );
				if( !success )
					++callback_iter;
			}
			else
				++callback_iter;
		}
	};


	void ScriptInterpreter::PrintVariables()
	{
		if( !GetGlobalVariables().var_map.empty() )
		{
			PrintMsg( L"Global Variables:" );
			for( auto& v : GetGlobalVariables().var_map )
				PrintMsg( Stream( v.first << L" = " << v.second.GetString() ) );
		}

		for( auto& stack : script_stacks )
		{
			if( stack.empty() )
				continue;

			auto& var_map = stack.top()->root_script->local_variables.var_map;
			if( !var_map.empty() )
			{
				PrintMsg( L"Scoped Variables:" );
				for( auto& v : var_map )
					PrintMsg( Stream( v.first << L" = " << v.second.GetString() ) );
			}
		}
	}

	void ScriptInterpreter::ReloadCache()
	{
		process_handles.clear();
		script_cache->clear();

		for( auto& path : search_paths )
		{
			process_handles.push_back( std::async( std::launch::async, [&]()
			{
		#if !defined( __APPLE__iphoneos ) && !defined( ANDROID )
			#if defined(CONSOLE) || defined(__APPLE__macosx)
				std::vector< std::wstring > results;
				FileSystem::FindAllFiles(path, results, true);
				for ( auto& result : results )
				{
					if ( result.find( L".cheat" ) != std::wstring::npos )
						AddToCache( result );
				}
			#else
				std::vector< std::wstring > results;
				FileSystem::FindFiles( path, L"*.cheat", false, true, results );
				for( auto& result : results )
					AddToCache( result );
			#endif
		#endif
			} ) );
		}
	}

	void ScriptInterpreter::AddToCache( std::wstring path )
	{
		if( EndsWith( path, L"/last.cheat" ) )
		{
			 // legacy, and now reserved, do not cache -- instead, delete.
			FileSystem::DeleteFile( path );
			return;
		}

		auto& cache = *script_cache;
		std::wifstream file( WstringToString(path) );
		auto new_file = std::make_shared<ScriptFile>();
		new_file->filename = path;

		if( ParseFile( file, *new_file, *this ) )
		{
			// Cache last modified
			FileTest( path );
			const auto begin = path.find_last_of( '/' ) + 1;
			const auto end = path.find_last_of( '.' );
			const auto stripped = path.substr( begin, end - begin );

			{
				const Memory::Lock lock( cache_mutex );
				cache[stripped] = std::move( new_file );
			}
		}
	}

	bool ScriptInterpreter::WaitForCacheLoaded( const int time )
	{
		for( const std::future<void>& process : process_handles )
		{
			if( process.valid() && process.wait_for( std::chrono::seconds( time ) ) != std::future_status::ready )
				return false;
		}

		return true;
	}

	std::shared_ptr<Cheats::ScriptFile> ScriptInterpreter::GetCachedScript( const std::wstring& path )
	{
		CHEAT_PROFILE;

		const auto found = script_cache->find( path );
		if( found == script_cache->end() )
		{
			LOG_CHEAT_DEBUG(L"Cache miss: " << path );
			return nullptr;
		}

		LOG_CHEAT_DEBUG( L"Cache hit: " << path );

		switch( FileTest( found->second->filename ) )
		{
		case FileTestResult::OK:
			LOG_CHEAT_DEBUG( L"No file change detected" );
			return found->second;
		case FileTestResult::MISSING:
			LOG_CHEAT_DEBUG( L"Script file missing" );
			script_cache->erase( path );
			return nullptr;
		case FileTestResult::RELOAD:
			LOG_CHEAT_DEBUG( L"Script file changed was detected" );
			// Reload
			AddToCache( found->second->filename );
			// Return new copy
			return ( *script_cache )[path];
		}

		throw std::runtime_error( "Bad" );
		return nullptr;
	}

	std::shared_ptr<Cheats::ScriptCacheMap>& ScriptInterpreter::GetCache()
	{
		return script_cache;
	}

	void Script::Push( int i )
	{
		end_tag_nest.push( i );
	}

	int Script::Pop()
	{
		if( end_tag_nest.empty() ) 
			return CMD_INVALID; // #end was used, no worries.
		
		int i = Top();
		end_tag_nest.pop();
		return i;
	}

	int Script::Top()
	{
		if( end_tag_nest.empty() ) 
			return CMD_INVALID;

		return end_tag_nest.top();
	}

	void ScriptInterpreter::ProcessScriptStacks()
	{
		CHEAT_PROFILE;
		if( !Ready() )
			return;

		for( unsigned i = 0; i < script_stacks.size(); ++i )
		{
			active_stack = i;
			auto& stack = script_stacks[active_stack];
			if( stack.empty() )
				continue;

			ProcessScriptStack( stack );
		}

		RemoveIf( script_stacks, []( const ScriptStack_t& stack ) { return stack.empty(); } );
		active_stack = -1;
	}

	void ScriptInterpreter::ProcessScriptStack( ScriptStack_t& stack )
	{
		stack.top()->root_script->local_variables.Remove( L"result" );

		enum IterationResult
		{
			Yield,
			Loop,
			Finish,
		};

		auto process_script = [&]()
		{
			Script_t top = stack.top();
			if( top->locked )
				return Yield;
			if( top->IsPaused() )
				return Yield;

			ScriptLock lock( *this, top );

			bool move_to_new_line = false;

			for( ; top->line_number < top->file->lines.size(); )
			{
				const std::wstring& line_const = top->file->lines[top->line_number].msg;
				const BaseCommands command = (BaseCommands)top->file->lines[top->line_number].command;
				Script::LineData& data = top->data[ top->line_number ];

				if( line_const.empty() )
				{
					top->line_depth = 0;
					++top->line_number;
					continue;
				}

				// Split by , character - Unless , occurs between "", '' or () sequences.
				const auto line_split = SplitScriptCommand( line_const );

				move_to_new_line = false;
				for( ; !move_to_new_line && data.repeats < data.repeat_max; )
				{
					for( ; !move_to_new_line && top->line_depth < line_split.size(); )
					{
						std::wstring line = TrimString( line_split[top->line_depth] );
						ProcessVariables( top.get(), line, {} );

						// Increment the depth now because we may not have a chance after processing.
						++top->line_depth;

						// Ignore empty lines.
						if( line.empty() )
							continue;

						LOG_CHEAT_DEBUG( L">> " << line );

						if( command == CMD_INVALID )
							SendMsg( line );
						else
						{
							switch( HandleScriptCommand( top.get(), line ) )
							{
							default: // This should never occur
								throw std::runtime_error( "Unknown return code received from HandleScriptCommand." );
							case TerminateScript: // We should terminate this script now.
								return Finish;
							case TerminateAllScripts: // We should terminate all scripts now.
								StopScriptStack();
								return Yield;
							case ReEnterLoop: // Perform a loop re-entry
								return Loop;
							case Continue:
								break;
							case NextLine: // Move to next line
								move_to_new_line = true;
								continue;
							}
						}

						if( stack.empty() || top != stack.top() ) // We are no longer head of this stack, exit now.
							return Yield;
						if( IsWarpCheat( line ) ) // Pause after a warp cheat has been started.
							top->pause.Reset( ScriptPause::Teleport );
						if( top->IsPaused() ) // We are now paused, do not continue.
							return Yield;
					}

					// Restart line, increment line repeat count.
					++data.repeats;
					top->line_depth = 0;
				}

				// Move to the next line, reset line position and repeat count.
				top->line_depth = 0;
				data.repeats = 0;
				++top->line_number;
			}

			return Finish;
		};

		auto iteration = [&]()
		{
			if( stack.empty() )
				return Finish;

			switch( process_script() )
			{
			case Yield: return Yield;
			case Loop: return Loop;
			case Finish:
				if( !stack.empty() )
				{
					Script_t top = std::move( stack.top() );
					assert( top->end_tag_nest.empty() );

					if( top->file->filename.empty() )
						LOG_CHEAT_DEBUG( L"Script finished" );
					else
						LOG_CHEAT_DEBUG( L"Script finished: " << top->file->filename );

					stack.pop();
				}

				// Don't iterate over new scripts if we're currently marked as "script locked".
				if( script_lock > 1 ) // Accounting for one lock in this loop
				{
					LOG_CHEAT_DEBUG( L"Lock encountered with value: " << script_lock );
					return Yield;
				}
			}

			return Finish;
		};

		auto loop = [&]()
		{
			try
			{
				// Loop until Yield/Finish.
				while( iteration() == Loop );
			}
			catch( const File::EPKError& )
			{
				throw;
			}
			catch( const std::exception& e )
			{
				const auto msg = StringToWstring( e.what() );
				LOG_CHEAT_CRIT( L"Script exception: " + msg );
				if( AbortScriptStack( msg ) )
					return Loop;
			}
			catch( ... )
			{
				LOG_CHEAT_CRIT( L"Unhandled exception occurred" );
				if( AbortScriptStack( L"Unhandled exception!" ) )
					return Loop;
			}

			return Finish;
		};

		// Loop until Yield/Finish.
		while( loop() == Loop );
	}

	std::vector<std::wstring> ScriptInterpreter::SplitScriptCommand( const std::wstring_view in, const bool trim )
	{
		CHEAT_PROFILE;

		std::vector<std::wstring> out;

		wchar_t end_quote = L'"';
		bool in_quote = false;
		bool delimited = false;
		size_t last_iter = 0;

		for( size_t i = 0; i < in.size(); ++i )
		{
			if( delimited )
				delimited = false;
			else if( in[i] == L'\\' )
				delimited = true;
			else if( in_quote )
				in_quote = in[i] != end_quote;
			else
			{
				if( in_quote = in[i] == L'"' )
					end_quote = L'"';
				else if( in_quote = in[i] == L'\'' )
					end_quote = L'\'';
				else if( in_quote = in[i] == L'(' )
					end_quote = L')';
				else if( in[i] == L',' )
				{
					if( trim )
						out.emplace_back( TrimString( in.substr( last_iter, i - last_iter ) ) );
					else
						out.emplace_back( in.substr( last_iter, i - last_iter ) );

					last_iter = i + 1;
				}
			}
		}

		if( trim )
			out.emplace_back( TrimString( in.substr( last_iter ) ) );
		else
			out.emplace_back( in.substr( last_iter ) );

		return out;
	}

	bool ScriptInterpreter::HandleMessage( const std::wstring& message )
	{
		if( Cheats::GetGlobals().cheats_disabled )
			return message != L"/enablecheats";

		if( message.empty() || message[0] != '/' )
			return true;

		if( !Running() )
			SetLast( message );

		if( IsScript( message ) )
		{
			ProcessScript( message );
			return true;
		}

		if( message == L"/ss" || message == L"/stopscripts" )
		{
			StopScriptStack();
			return true;
		}

		const bool no_script = message.size() > 2
			&& message[1] == '.'
			&& message[2] != '.';

		if ( no_script )
		{
			auto& new_message = const_cast<std::wstring&>(message);
			new_message = L'/' + message.substr( 2 );
		}

		if( BeginsWith( message, L"/paste " ) )
		{
			Paste( message.substr( 7 ) );
			return true;
		}

		const auto stripped = message.substr( 1 );
		const auto first_word = stripped.substr( 0, stripped.find( L' ' ) );

		if( !no_script && ScriptExists( first_word ) )
		{
			if( EndsWith( message, L"?" ) )
				OpenScript( first_word );
			else
				ProcessFile( stripped );

			return true;
		}

		return false;
	}

	int ScriptInterpreter::GetNumCommands()
	{
		return BASE_CMD_MAX;
	}

	bool ScriptInterpreter::IsPop( const int cmd ) const
	{
		return Contains( base_pop_script_commands, cmd );
	}

	bool ScriptInterpreter::IsPush( const int cmd ) const
	{
		return Contains( base_push_script_commands, cmd );
	}

	void ScriptInterpreter::DoLast()
	{
		ProcessScript( last );
	}

	void ScriptInterpreter::SaveLast( const std::wstring& path )
	{
		const std::wstring save_dir = GetSaveLastDirectory();
		FileSystem::CreateDirectoryChain( save_dir );

		std::wstring destination;
		if( EndsWith( path, L".cheat" ) )
			destination = save_dir + path;
		else
			destination = save_dir + path + L".cheat";

		// do the write
		{
			std::wofstream file( WstringToString(destination) );
			file << last;
		}

		AddToCache( destination );
	}

	bool ScriptInterpreter::AbortScriptStack( const std::wstring& reason )
	{
		if( script_stacks.empty() )
			return false;

		auto& stack = GetCurrentStack();
		return AbortScriptStack( stack, reason );
	}

	bool ScriptInterpreter::AbortScriptStack( ScriptStack_t& stack, const std::wstring& reason )
	{
		while( !stack.empty() )
		{
			Script_t top = stack.top();
			if( top->in_try )
			{
				top->in_try = false;
				top->pause.Reset();

				// First trace back to the try statement!
				while( top->line_number > 0 && top->file->lines[ top->line_number ].command != CMD_try )
				{
					if( top->file->lines[ top->line_number ].command != CMD_INVALID && IsPush( top->file->lines[ top->line_number ].command ) )
						top->Pop();
					--top->line_number;
				}

				if( top->file->lines[top->line_number].command != CMD_try )
					throw std::runtime_error( "Could not find initial try statement!" );

				// Then move to the next corresponding catch/end tag for this try statement.
				MoveToNextTag( top.get(), CMD_catch );

				const auto& line = top->file->lines[top->line_number];
				if( line.command == CMD_catch )
				{
					const auto find = line.msg.find( L"catch " );
					if( find != line.msg.npos )
						top->root_script->local_variables.Set( TrimString( line.msg.substr( find + 6 ) ), reason );
				}
				else // CMD_end
				{
					top->Pop();
				}

				return true;
			}
			else
				stack.pop();
		}

		PrintMsg( L"<rgb(255,255,0)>{Unhandled exception:} " + reason );
		LOG_CHEAT_INFO( L"Script execution halted by unhandled exception" );

		return false;
	}

	void ScriptInterpreter::StopScriptStack()
	{
		for( auto& stack : script_stacks )
		{
			while( !stack.empty() )
				stack.pop();
		}

		script_stacks.clear();

		LOG_CHEAT_INFO( L"Script execution stopped" );
	}

	void ScriptInterpreter::Update( const unsigned frame_length )
	{
		CHEAT_PROFILE;

		if( !process_handles.empty() )
		{
			if( WaitForCacheLoaded( 0 ) )
				process_handles.clear();
		}

		if( script_stacks.empty() )
		{
			if( !launched )
				Launch();
			else if( ScriptExists( L"loop" ) )
			{
				loop_timer -= static_cast<int>( frame_length );
				if( loop_timer < 0 )
				{
					loop_timer += LoopTimeMs;
					ProcessFile( L"loop" );
				}
			}
		}

		bool paused = false;
		for( auto& stack : script_stacks )
		{
			if( stack.empty() )
				continue;

			auto& top = stack.top();
			if( top->IsPaused( ScriptPause::Duration ) )
			{
				auto& duration = top->pause.duration;
				if( duration <= frame_length )
				{
					LOG_CHEAT_DEBUG( L"Pause Duration finished" );
					top->pause.Reset();
				}
				else
				{
					duration -= frame_length;
					paused = true;
				}
			}
			else if( top->IsPaused( ScriptPause::SingleFrame ) )
			{
				LOG_CHEAT_DEBUG( L"Pause SingleFrame finished" );
				top->pause.Reset();
			}
		}

		if( !paused && !script_stacks.empty() )
			ProcessScriptStacks();
	}

	void ScriptInterpreter::Launch()
	{
		if( process_handles.empty() )
		{
			if( first_launch )
				ProcessFile( L"first_launch" );

			ProcessFile( L"launch" );

			first_launch = false;
			launched = true;
		}

		first_session = false;
	}

	void ScriptInterpreter::SetLast( const std::wstring& new_last )
	{
		CHEAT_PROFILE;
		// We don't want to add any of the items in the exclude list to last.cheat.
		const std::vector<std::wstring> excludes =
		{
			L"/last",
			L"/savelast",
			L"/hotkey",
			L"/say"
		};

		if( std::any_of( excludes.begin(), excludes.end(), [&]( const std::wstring& exclude ) { return BeginsWith( new_last, exclude ); } ) )
			return;

		last = new_last;
	}

	void ScriptInterpreter::Pause( unsigned duration )
	{
		if( script_stacks.empty() )
			return;

		auto script = GetCurrentScript();
		assert( !script->IsPaused() );
		if( script->IsPaused() )
			return;

		script->pause.Reset( ScriptPause::Duration, duration );
		LOG_CHEAT_INFO( L"Execution paused for " << duration << L"ms" );
	}

	void ScriptInterpreter::PauseFor( ScriptPause::PauseType reason )
	{
		auto script = GetCurrentScript();
		if( !script )
			return;
		assert( !script->IsPaused() );
		if( script->IsPaused() )
			return;
		LOG_CHEAT_INFO( L"Execution paused for reason: " << reason );
		script->pause.Reset( reason );
	}

	std::wstring ScriptInterpreter::GetJsonString( const std::wstring& key ) const
	{
		auto found = json_values.find( WstringToString( key ) );
		return found == json_values.end() ? L"" : StringToWstring( found->second );
	}

	const wchar_t* const * GetVariablesBegin()
	{
		return base_prefix;
	}

	const wchar_t* const * GetVariablesEnd()
	{
		return base_prefix + BASE_VARIABLE_MAX;
	}

}
#ifdef NO_OPTIMISE
#pragma optimize( "", on )
#undef NO_OPTIMISE
#endif
#endif
