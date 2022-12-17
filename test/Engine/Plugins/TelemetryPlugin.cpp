#include "TelemetryPlugin.h"

#include <regex>
#include <chrono>
#include <fstream>

#include "Common/Network/Tcpstream.h"
#include "Common/Engine/Telemetry.h"
#include "Common/Utility/Logger/Logger.h"
#include "Common/Utility/StringManipulation.h"
#include "Common/Utility/Clipboard.h"
#include "Common/Utility/HighResTimer.h"
#include "Common/Job/JobSystem.h"
#include "Common/File/FileSystem.h"
#include "Visual/GUI2/imgui/imgui.h"
#include "Visual/GUI2/Icons.h"

#include "Include/magic_enum/magic_enum.hpp"

#if !defined(CONSOLE) && !defined(MOBILE) && DEBUG_GUI_ENABLED
#	define REDIS_SUPPORTED
#	ifdef WIN32
#		include <limits.h>
#		include <intrin.h>
#	else
#		include <stdint.h>
#	endif
#endif

#if (defined(DEVELOPMENT_CONFIGURATION) || defined(TESTING_CONFIGURATION)) && defined(REDIS_SUPPORTED) && defined(WIN32)
#	define REDIS_ONLY(A) A
#	define REDIS_ENABLED

#else
#	define REDIS_ONLY(A)
#endif

namespace Engine::Plugins
{
#if DEBUG_GUI_ENABLED
	
	namespace
	{
	#ifdef REDIS_ENABLED
		constexpr std::string_view AuthPassword = "snjdC5YQulryYI28k6PMqZymJqHcJ2rV1METvVXCXzCDtT5";
		const std::regex key_search = std::regex("([0-9a-f]{16})(\\[(\\d{4})\\-(\\d{2})\\-(\\d{2}):(\\d{2})\\-(\\d{2})\\-\\d{2}\\|[0-9a-f]+\\])?:([^:]*):([^:]*):([^:]*)(:([^:]*))?(!D)?", std::regex::optimize);

		int ToInt(const std::string& str)
		{
			if (str.empty())
				return 0;

			try
			{
				return std::stoi(str);
			} catch (...)
			{
				return 0;
			}
		}

		float ToFloat(const std::string& str)
		{
			if (str.empty())
				return 0;

			try
			{
				return std::stof(str);
			} catch (...)
			{
				return 0;
			}
		}

		bool IsSpace(char c)
		{
			// We include " as space char because we want to remove them from the final output
			return c == ' ' || c == '\r' || c == '\n' || c == '\"';
		}

		typedef Memory::SmallVector<std::string_view, 8, Memory::Tag::Profile> ParsedCommand;

		// Split by spaces without splitting strings
		ParsedCommand Parse(const std::string_view& cmd)
		{
			Memory::SmallVector<std::string_view, 16, Memory::Tag::Unknown> cmd_args;
			size_t last_start = 0;
			bool in_block = false;
			for (size_t a = 0; a < cmd.length(); a++)
			{
				if (cmd[a] == '\"' || (!in_block && IsSpace(cmd[a])))
				{
					if (last_start < a)
						cmd_args.push_back(cmd.substr(last_start, a - last_start));

					last_start = a + 1;
				}

				if (cmd[a] == '\"')
					in_block = !in_block;
			}

			if (last_start < cmd.length() && !in_block && !IsSpace(cmd[last_start]))
				cmd_args.push_back(cmd.substr(last_start, cmd.length() - last_start));

			ParsedCommand r;
			for (const auto& a : cmd_args)
				r.emplace_back(a);

			return r;
		}

		std::string GetTime(unsigned day_offset = 0, unsigned hour_offset = 0)
		{
			const auto now = std::chrono::system_clock::now();
			const auto t = std::chrono::system_clock::to_time_t(now - std::chrono::hours((day_offset * 24) + hour_offset));

			std::tm buffer;
		#ifdef WIN32
			gmtime_s(&buffer, &t);
		#else
			gmtime_r(&t, &buffer);
		#endif

			std::stringstream s;
			s << std::put_time(&buffer, "%Y-%m-%d");
			return s.str();
		}

		std::string GetReadableDate(unsigned day_offset = 0)
		{
			const auto now = std::chrono::system_clock::now();
			const auto t = std::chrono::system_clock::to_time_t(now - std::chrono::hours(day_offset * 24));

			std::tm buffer;
		#ifdef WIN32
			gmtime_s(&buffer, &t);
		#else
			gmtime_r(&t, &buffer);
		#endif

			std::stringstream s;
			s << std::put_time(&buffer, "%a, %d/%m/%Y");
			return s.str();
		}

		struct KeyInfo
		{
		private:
			bool valid = false;

			struct KeyIndex
			{
				enum Index : unsigned
				{
					User = 1,
					Year = 3,
					Month = 4,
					Day = 5,
					Hour = 6,
					Minute = 7,
					Config = 8,
					Name = 9,
					Type = 10,
					Unit = 12,
					Debug = 13,
				};
			};

		public:
			enum class Type
			{
				String,
				UInt,
				Float,
			} type;

			std::string user;
			std::string config;
			std::string name;
			std::string unit;
			int year = 0;
			int month = 0;
			int day = 0;
			int hour = 0;
			int minute = 0;
			bool debug = false;

			KeyInfo(const std::string& key = "")
			{
				std::smatch match;
				if (std::regex_match(key, match, key_search))
				{
					valid = true;
					user = match[KeyIndex::User].str();
					config = match[KeyIndex::Config].str();
					name = match[KeyIndex::Name].str();
					unit = match[KeyIndex::Unit].str();
					year = ToInt(match[KeyIndex::Year].str());
					month = ToInt(match[KeyIndex::Month].str());
					day = ToInt(match[KeyIndex::Day].str());
					hour = ToInt(match[KeyIndex::Hour].str());
					minute = ToInt(match[KeyIndex::Minute].str());
					debug = match[KeyIndex::Debug].matched;

					const auto tmp = match[KeyIndex::Type].str();
					if (tmp == "STRING")
						type = Type::String;
					else if (tmp == "UINT")
						type = Type::UInt;
					else if (tmp == "FLOAT")
						type = Type::Float;
					else
						type = Type::String;
				}
			}

			explicit operator bool() const { return valid; }
			bool is_valid() const { return valid; }

			bool HasTime() const { return valid && year > 0; }

			uint64_t GetTimeStamp() const
			{
				if (!valid || year < 1900 || month < 1)
					return 0;

				std::tm t;
				t.tm_year = year - 1900;
				t.tm_mon = month - 1;
				t.tm_mday = day;
				t.tm_hour = hour;
				t.tm_min = minute;
				t.tm_sec = 0;

				auto p = std::chrono::system_clock::from_time_t(std::mktime(&t));
				return std::chrono::duration_cast<std::chrono::seconds>(p.time_since_epoch()).count();
			}
		};
	#endif

		class Protocol
		{
		#ifdef REDIS_ENABLED
		public:
			class Command
			{
			private:
				std::stringstream command_buffer;
				size_t num_commands = 0;

			public:
				void Append(const std::string_view& cmd)
				{
					if (const auto cmd_args = Parse(cmd); cmd_args.size() > 0)
					{
						num_commands++;
						command_buffer << '*' << cmd_args.size() << "\r\n";
						for (const auto& a : cmd_args)
							command_buffer << '$' << a.length() << "\r\n" << a << "\r\n";
					}
				}

				void Append(const char* fmt, ...)
				{
					// Build Command Buffer
					char buffer[1024];

					va_list args;
					va_start(args, fmt);
					int w = vsnprintf(buffer, std::size(buffer), fmt, args);
					va_end(args);

					if (w < 0 || w >= (int)std::size(buffer))
					{
						LOG_WARN(L"Telemetry: Command dropped, buffer too small!");
						return;
					}

					Append(std::string_view(buffer, size_t(w)));
				}

				std::string Get() const { return command_buffer.str(); }
				size_t GetResponseCount() const { return num_commands; }
			};

			class Reply
			{
			private:
				std::string text;

			public:
				Reply(const std::string& text = "+\r\n") : text(text) {}

				bool IsNull() const
				{
					if (text.length() > 0 && text[0] == '_')
						return true;

					if (text.length() < 2)
						return false;

					return (text[0] == '*' || text[0] == '$') && text[1] == '-';
				}

				bool IsError() const { return text.length() > 0 && (text[0] == '-' || text[0] == '!'); }
				bool IsString() const { return text.length() > 0 && !IsNull() && (text[0] == '+' || text[0] == '$' || text[0] == '='); }
				bool IsArray() const { return text.length() > 0 && !IsNull() && (text[0] == '*' || text[0] == '%' || text[0] == '~' || text[0] == '|'); }
				bool IsInt() const { return text.length() > 0 && (text[0] == ':' || text[0] == '('); }
				bool IsDouble() const { return text.length() > 0 && text[0] == ','; }
				bool IsBool() const { return text.length() > 0 && text[0] == '#'; }

				std::string_view AsError() const
				{
					if (!IsError())
						return "";

					if (text[0] == '-')
					{
						const auto f = text.find_first_of("\r\n");
						return std::string_view(text).substr(1, f == text.npos ? f : (f - 1));
					}

					std::stringstream tmp;
					tmp << std::string_view(text).substr(1);

					int count = 0;
					tmp >> count;

					if (const size_t f = text.find_first_of('\n'); f != text.npos && count > 0)
						return std::string_view(text).substr(f + 1, size_t(count));

					return "";
				}

				std::string_view AsString() const
				{
					if (!IsString())
						return "";

					if (text[0] == '+')
					{
						const auto f = text.find_first_of("\r\n");
						return std::string_view(text).substr(1, f == text.npos ? f : (f - 1));
					}

					std::stringstream tmp;
					tmp << std::string_view(text).substr(1);

					int count = 0;
					tmp >> count;

					if (const size_t f = text.find_first_of('\n'); f != text.npos && count > 0)
					{
						const auto r = std::string_view(text).substr(f + 1, size_t(count));
						if (text[0] == '=')
							return r.substr(4);

						return r;
					}

					return "";
				}

				int AsInt() const
				{
					if (!IsInt())
						return 0;

					return ToInt(text.substr(1));
				}

				float AsDouble() const
				{
					if (!IsDouble())
						return 0;

					const auto tmp = std::string_view(text).substr(1);
					if (tmp == "inf")
						return std::numeric_limits<float>::infinity();
					else if (tmp == "-inf")
						return -std::numeric_limits<float>::infinity();

					return ToFloat(text.substr(1));
				}

				bool AsBool() const
				{
					if (!IsBool())
						return false;

					return text[1] == 't';
				}

				Memory::Vector<Reply, Memory::Tag::Profile> AsArray() const;
			};

			//typedef Memory::SmallVector<Reply, 8, Memory::Tag::Profile> Responses;
			typedef Memory::Vector<Reply, Memory::Tag::Profile> Responses;
			typedef std::function<void(const Responses&)> CommandCallback;

		private:
			struct PendingCommand
			{
				size_t responses = 0;
				CommandCallback callback;
			};

			class ResponseBuilder
			{
			private:
				Responses finished_responses;
				size_t pending_lines = 0;
				std::stringstream build;

			public:
				void PushLine(const std::string& line)
				{
					if (line.length() > 0 && (line[0] == '*' || line[0] == '%' || line[0] == '~' || line[0] == '|' || line[0] == '$' || line[0] == '=' || line[0] == '!' || line[0] == '>'))
					{
						std::stringstream tmp;
						tmp << line.substr(1);

						int count = 0;
						tmp >> count;

						if (!tmp)
							return;

						switch (line[0])
						{
							case '*': // Array
							case '~': // Set
								pending_lines += size_t(std::max(0, count));
								break;
							case '%': // Map
							case '|': // Attribute
								pending_lines += size_t(std::max(0, 2 * count));
								break;
							case '$': // String
							case '=': // String with metadata
							case '!': // Error
							case '>': // Push Message
								pending_lines++;
								break;
							default:
								break;
						}
					}

					build << line;
					if (!EndsWith(line, "\r\n"))
						build << "\r\n";

					if (pending_lines == 0)
					{
						const auto final_reply = build.str();
						if (final_reply.length() > 0 && final_reply[0] != '>') // Skip push replies
							finished_responses.push_back(final_reply);

						build.str("");
					}
					else
					{
						pending_lines--;
					}
				}

				const Responses& GetResponses() const { return finished_responses; }

				void Consume(size_t count)
				{
					if (count >= finished_responses.size())
					{
						finished_responses.clear();
					}
					else
					{
						for (size_t a = 0, b = count; b < finished_responses.size(); a++, b++)
							finished_responses[a] = finished_responses[b];

						finished_responses.resize(finished_responses.size() - count);
					}
				}
			};

			enum class State {
				NotConnected,
				Pending,
				Connected,
			};

			std::atomic<State> state = State::NotConnected;
			ResponseBuilder response_builder;
			Memory::Pointer<Network::Tcpstream, Memory::Tag::Profile> stream;
			Memory::Deque<PendingCommand, Memory::Tag::Profile> pending_commands;

			bool Transition(State exp, State next)
			{
				return state.compare_exchange_strong(exp, next, std::memory_order_acq_rel);
			}

			bool WaitForConnection()
			{
				while (true)
				{
					switch (state.load(std::memory_order_consume))
					{
						default:					return false;
						case State::NotConnected:	return false;
						case State::Connected:		return true;
						case State::Pending:		break;
					}
				}
			}

			void UpdatePending()
			{
				while (pending_commands.size() > 0)
				{
					const auto& pending = pending_commands.front();
					const auto& available = response_builder.GetResponses();
					if (available.size() < pending.responses)
						break;

					if (pending.callback)
						pending.callback({ available.begin(), available.begin() + pending.responses });

					response_builder.Consume(pending.responses);
					pending_commands.pop_front();
				}
			}

			void FlushReads()
			{
				stream->fill_read_buffer();
				while (true)
				{
					const auto line = stream->read_string_until_newline();
					if (stream->read_failed())
					{
						// We processed everything that was ready to be recieved
						stream->reset_reads();
						break;
					}

					stream->flush_reads();
					response_builder.PushLine(line);
				}

				UpdatePending();
			}

			void FlushWrites()
			{
				stream->flush_writes(0);
			}

			void SendInternal(const Command& cmd, const CommandCallback& callback)
			{
				const auto command = cmd.Get();
				const auto num_responses = cmd.GetResponseCount();
				if (num_responses == 0 || command.length() == 0)
					return;

				stream->write(command.c_str(), command.length());
				stream->flush_writes(0);

				auto& pending = pending_commands.emplace_back();
				pending.callback = callback;
				pending.responses = num_responses;
			}

		public:
			~Protocol() { Disconnect(); }

			bool IsBusy() const 
			{ 
				return !pending_commands.empty(); 
			}

			void Connect(const std::string& address, int port)
			{
				if (address.empty())
					return;

				if (Transition(State::NotConnected, State::Pending))
				{
					Job2::System::Get().Add(Job2::Type::Idle, Job2::Job(Memory::Tag::Profile, Job2::Profile::Unknown, [address, port, this]()
					{
						stream = decltype(stream)::make(address, std::to_string(port));
						if (stream->CanRead() && stream->CanWrite())
						{
							Command cmd;
							cmd.Append("AUTH \"%.*s\"", AuthPassword.length(), AuthPassword.data());
							cmd.Append("HELLO 3");
							cmd.Append("CLIENT SETNAME EnginePlugin");
							SendInternal(cmd, [](const Responses&) {});

							Transition(State::Pending, State::Connected);
						}
						else
						{
							stream->Finish();
							stream.reset();
							Transition(State::Pending, State::NotConnected);
						}
					}));
				}
			}

			bool IsConnectedAsync() const { return state.load(std::memory_order_consume) != State::NotConnected; }

			bool IsConnected()
			{
				if (WaitForConnection())
				{
					if (!stream)
						return false;

					if (!stream->CanWrite() || !stream->CanRead())
					{
						LOG_WARN(L"Telemetry: Unexpected disconnection");
						Disconnect();
						return false;
					}

					return true;
				}

				return false;
			}

			void Disconnect()
			{
				if (WaitForConnection() && stream)
				{
					stream->Finish();
					stream.reset();
					pending_commands.clear();
				}
			}

			void Flush()
			{
				if (IsConnected())
				{
					FlushWrites();
					FlushReads();
				}
			}

			void FlushAll()
			{
				if (IsConnected())
					while (IsBusy())
						Flush();
			}

			void Send(const Command& cmd, const CommandCallback& callback)
			{
				if (IsConnected())
					SendInternal(cmd, callback);
			}
		#endif
		};

	#ifdef REDIS_ENABLED
		Protocol::Responses Protocol::Reply::AsArray() const
		{
			if (!IsArray())
				return {};

			std::stringstream tmp;
			tmp << std::string_view(text).substr(1);

			int count = 0;
			tmp >> count;

			if (text[0] == '%' || text[0] == '|')
				count *= 2;

			if (const size_t f = text.find_first_of('\n'); f != text.npos && count > 0)
			{
				ResponseBuilder builder;
				for (size_t start = f + 1; builder.GetResponses().size() < size_t(count) && start != text.npos;)
				{
					const auto end = text.find_first_of('\n', start);
					if (end == text.npos)
					{
						builder.PushLine(text.substr(start));
						start = end;
					}
					else
					{
						builder.PushLine(text.substr(start, end + 1 - start));
						start = end + 1;
					}
				}

				return builder.GetResponses();
			}

			return {};
		}

		std::optional<std::string> ReplyToString(const Protocol::Reply& reply)
		{
			if (reply.IsString())
				return std::string(reply.AsString());
			else if (reply.IsInt())
				return std::to_string(reply.AsInt());
			else if (reply.IsDouble())
				return std::to_string(reply.AsDouble());
			else if (reply.IsBool())
				return reply.AsBool() ? "true" : "false";

			return std::nullopt;
		}

		std::optional<float> ReplyToFloat(const Protocol::Reply& reply)
		{
			if (reply.IsInt())
				return float(reply.AsInt());
			else if (reply.IsDouble())
				return reply.AsDouble();
			else if (reply.IsBool())
				return reply.AsBool() ? 1.0f : 0.0f;
			else if (reply.IsString())
				return ToFloat(std::string(reply.AsString()));

			return std::nullopt;
		}
	#endif

		class Scripting : public Protocol
		{
		#ifdef REDIS_ENABLED
		public:
		#if 0
			typedef std::pair<Responses, Responses> ScriptResult;
			typedef std::function<void(const ScriptResult&)> ScriptCallback;
		#endif

			typedef Memory::FlatMap<std::string, Reply, Memory::Tag::Unknown> ScriptResult;
			typedef Memory::SmallVector<std::string, 8, Memory::Tag::Unknown> PackedArgs;
			
			class Callback
			{
			public:
				typedef std::function<void(const ScriptResult&)> Output;
				typedef std::function<ScriptResult(const ScriptResult&)> Filter;
				typedef std::function<void(const ScriptResult&, bool, Engine::Telemetry::DB)> Followup;
				typedef std::function<void(const Followup&, const ScriptResult&, bool, Engine::Telemetry::DB)> Request;

			private:
				Followup next;
				Memory::SmallVector<Filter, 8, Memory::Tag::Unknown> filters;

			public:
				Callback(const Output& result) : next([result](const ScriptResult& input, bool has_input, Engine::Telemetry::DB) { if(has_input) result(input); }) {}

				Callback& Push(const Filter& filter) { filters.push_back(filter); return *this; }
				Callback& Push(Request request)
				{
					next = [request = std::move(request), next = std::move(next), filters = filters](const ScriptResult& input, bool has_input, Engine::Telemetry::DB db) mutable
					{
						auto filter = [next = std::move(next), filters = std::move(filters)](const ScriptResult& input, bool has_input, Engine::Telemetry::DB db)
						{
							if (has_input && input.size() > 0)
							{
								ScriptResult output = input;
								for (size_t a = filters.size(); a > 0; a--)
									output = filters[a - 1](output);

								next(output, has_input, db);
							}
							else
							{
								next({}, has_input, db);
							}
						};

						request(filter, input, has_input, db);
					};

					filters.clear();
					return *this;
				}

				Callback& Execute(Engine::Telemetry::DB db)
				{
					next({}, false, db);
					return *this;
				}
			};

			static PackedArgs PackArgs(const ParsedCommand& command, size_t begin, size_t end)
			{
				PackedArgs res;
				res.reserve(end - begin);
				for (size_t a = begin; a < end; a++)
					res.emplace_back(std::string(command[a]));

				return res;
			}

		private:
			Memory::FlatMap<std::string, std::string, Memory::Tag::Profile> scripts;

			std::optional<int64_t> GetDate(const std::string& key)
			{
				if (const KeyInfo info = key)
				{
					int64_t res = info.year;
					res = (res * 12) + info.month;
					res = (res * 31) + info.day;
					res = (res * 24) + info.hour;
					res = (res * 60) + info.minute;
					return res;
				}

				return std::nullopt;
			}

			static std::optional<int64_t> ArgsToDate(const std::string& arg)
			{
				bool success = false;
				int year = 0;
				int month = 0;
				int day = 0;
				int hour = 0;
				int minute = 0;

				// Format YYYY-MM-DD:hh-mm
				if(!success)
				{
					std::regex search("(\\d{4})(\\-(\\d{1,2}))?(\\-(\\d{1,2}))?(:(\\d{1,2})\\-(\\d{1,2}))?");
					std::smatch match;
					if (std::regex_match(arg, match, search))
					{
						success = true;
						year = ToInt(match[1]);
						month = ToInt(match[3]);
						day = ToInt(match[5]);
						hour = ToInt(match[7]);
						minute = ToInt(match[8]);
					}
				}

				// Format DD/MM/YYYY:hh-mm
				if (!success)
				{
					std::regex search("(\\d{1,2})/(\\d{1,2})/(\\d{2}(\\d{2})?)(:(\\d{1,2})\\-(\\d{1,2}))?");
					std::smatch match;
					if (std::regex_match(arg, match, search))
					{
						success = true;
						day = ToInt(match[1]);
						month = ToInt(match[2]);
						year = ToInt(match[3]) + (match[4].matched ? 0 : 2000);
						hour = ToInt(match[6]);
						minute = ToInt(match[7]);
					}
				}

				if (success)
				{
					int64_t res = year;
					res = (res * 12) + month;
					res = (res * 31) + day;
					res = (res * 24) + hour;
					res = (res * 60) + minute;
					return res;
				}

				return std::nullopt;
			}

			void LoadScript(const std::string& name, const std::string_view& script)
			{
				if (!IsConnected())
					return;

				Command cmd;
				std::stringstream tmp;
				tmp << "SCRIPT LOAD \"" << script << "\"";
				cmd.Append(tmp.str());

				Send(cmd, [this, name](const Responses& rsp)
				{
					if (rsp.front().IsString())
						scripts[name] = rsp.front().AsString();
					else if (rsp.front().IsError())
						LOG_WARN(L"Failed to load script '" << StringToWstring(name) << L"': " << StringToWstring(std::string(rsp.front().AsError())));
				});
			}

			void CallScript(const std::string& name, const std::string& final_args, Engine::Telemetry::DB db, const CommandCallback& callback)
			{
				if (name.empty())
					return;

				const auto script = scripts.find(name);
				if (script == scripts.end())
				{
					if (callback)
						callback({ Reply("-ERR No Script named '" + name + "' found") });

					return;
				}

				std::stringstream tmp;
				tmp << "EVALSHA \"" << script->second << "\" " << final_args;

				Command cmd;
				cmd.Append("MULTI");
				cmd.Append("SELECT %u", unsigned(db));
				cmd.Append(tmp.str());
				cmd.Append("EXEC");

				Send(cmd, callback);
			}

			ScriptResult SendError(const Reply& msg)
			{
				ScriptResult res;
				res["Error"] = msg;
				return res;
			}

			void RequestGet(const Callback::Followup& next, const ScriptResult& input, bool has_input, const PackedArgs& args, Engine::Telemetry::DB db, unsigned offset = 0)
			{
				if (has_input && input.empty())
				{
					next(input, has_input, db);
					return;
				}

				std::stringstream final_args;
				final_args << "0 " << offset << " 100";
				if (args.size() > 0)
					final_args << " \"*" << args[0] << "*\"";

				CallScript("get", final_args.str(), db, [this, next, input, has_input, args, db](const Responses& rsp)
				{
					if (rsp.back().IsError())
					{
						next(SendError(rsp.back()), true, db);
						return;
					}

					if (const auto r = rsp.back().AsArray(); r.size() >= 2)
					{
						if (const auto result = r[1].AsArray(); result.size() >= 3)
						{
							const auto next_offset = result[0].IsInt() ? result[0].AsInt() : ToInt(std::string(result[0].AsString()));
							const auto keys = result[1].AsArray();
							const auto vals = result[2].AsArray();
							const auto count = std::min(keys.size(), vals.size());

							ScriptResult output;
							for (const auto& a : input)
								if (a.second.IsError())
									output[a.first] = a.second;

							for (size_t a = 0; a < count; a++)
								if (const auto k = ReplyToString(keys[a]); k && (!has_input || input.find(*k) != input.end()))
									output[*k] = vals[a];

							next(output, true, db);

							if (next_offset != 0)
								RequestGet(next, input, has_input, args, db, next_offset);
						}
					}
				});
			}

			void RequestSelect(const Callback::Followup& next, const ScriptResult& input, bool has_input, const PackedArgs& args, Engine::Telemetry::DB db)
			{
				if (args.empty())
				{
					next(SendError(Reply("-ERR Missing arguments for 'select'")), true, db);
					return;
				}

				if (const auto ndb = magic_enum::enum_cast<Engine::Telemetry::DB>(args[0]))
					next(input, has_input, *ndb);
				else
					next(SendError(Reply("-ERR 'select': No database named '" + args[0] + "' found")), true, db);
			}

			void RequestHelp(const Callback::Followup& next, const ScriptResult& input, bool has_input, const PackedArgs& args, Engine::Telemetry::DB db)
			{
				if (has_input)
				{
					next(input, has_input, db);
					return;
				}

				auto MakeHelpText = [](const std::string_view& text, const std::string_view& example = "")
				{
					std::stringstream s;
					if (example.length() > 0)
						s << "*2\r\n";
					else
						s << "*1\r\n";

					s << "$" << text.length() << "\r\n" << text << "\r\n";
					if (example.length() > 0)
						s << "$" << (9 + example.length()) << "\r\nExample: " << example << "\r\n";

					return s.str();
				};

				ScriptResult output;
				output["get"] = MakeHelpText("Recieve values from the current database. Values are filtered by the first (optional) argument", "get \"LOADING 1_1_town\"");
				output["select"] = MakeHelpText("Select the current database. The default database is always 'Stats'", "select Hardware");
				output["help"] = MakeHelpText("Shows this help text");
				output["values"] = MakeHelpText("Filters the shown data points by value", "values > 3000");
				output["since"] = MakeHelpText("Filters the shown data points by date", "since 4/6/2021");
				output["before"] = MakeHelpText("Filters the shown data points by date", "before 4/6/2021");
				output["type"] = MakeHelpText("Filters the shown data points by their type", "type Float");
				output["user"] = MakeHelpText("Filters the shown data points by user", "user 52839c1e6feaf500");
				output["unit"] = MakeHelpText("Filters the shown data points by unit", "unit ms");
				output["badkey"] = MakeHelpText("Only show keys with an invaid key format");
				output["valid"] = MakeHelpText("Only show keys with an valid key format");
				output["config"] = MakeHelpText("Only show keys from the specified configuration", "config testing");
				output["release"] = MakeHelpText("Do not show data points from debug builds");
				output["debug"] = MakeHelpText("Only show data points from debug builds");
				output[""] = MakeHelpText("You can chain commands using |", "get | values < 3000 | delete");
				next(output, true, db);
			}

			ScriptResult FilterValues(const ScriptResult& input, const PackedArgs& args)
			{
				if (args.size() < 2)
					return SendError(Reply("-ERR Missing arguments for 'values'"));

				ScriptResult output;
				float minV = -FLT_MAX;
				float maxV = FLT_MAX;

				if (args[0] == "<" || args[0] == "<=" || args[0] == "less")
				{
					if (const auto v = ReplyToFloat("+" + args[1]))
						maxV = *v;
					else
						return SendError(Reply("-ERR 'values' expected numeric value as second argument"));
				}
				else if (args[0] == ">" || args[0] == ">=" || args[0] == "greater")
				{
					if (const auto v = ReplyToFloat("+" + args[1]))
						minV = *v;
					else
						return SendError(Reply("-ERR 'values' expected numeric value as second argument"));
				}
				else if (args[0] == "==" || args[0] == "equal")
				{
					if (const auto v = ReplyToFloat("+" + args[1]))
					{
						minV = *v - 1e-5f;
						maxV = *v + 1e-5f;
					}
					else
						return SendError(Reply("-ERR 'values' expected numeric value as second argument"));
				}
				else if (args[0] == "range")
				{
					if (args.size() < 3)
						return SendError(Reply("-ERR Missing arguments for 'values'"));

					if (const auto v = ReplyToFloat("+" + args[1]))
						minV = *v;
					else
						return SendError(Reply("-ERR 'values' expected numeric value as second argument"));

					if (const auto v = ReplyToFloat("+" + args[2]))
						maxV = *v;
					else
						return SendError(Reply("-ERR 'values' expected numeric value as thrid argument"));
				}

				for (const auto& a : input)
				{
					if (a.second.IsError())
						output[a.first] = a.second;
					else if (const auto v = ReplyToFloat(a.second); v && *v >= minV && *v <= maxV)
						output[a.first] = a.second;
				}

				return output;
			}

			ScriptResult FilterConfig(const ScriptResult& input, const PackedArgs& args)
			{
				if (args.size() < 1)
					return SendError(Reply("-ERR Missing arguments for 'config'"));

				Memory::SmallVector<std::string, 4, Memory::Tag::Unknown> targets;
				for (const auto& a : args)
					targets.push_back(LowercaseString(a));

				ScriptResult output;

				for (const auto& a : input)
				{
					if (a.second.IsError())
						output[a.first] = a.second;
					else if (const auto info = KeyInfo(a.first); info && std::find(targets.begin(), targets.end(), LowercaseString(info.config)) != targets.end())
						output[a.first] = a.second;
				}

				return output;
			}

			ScriptResult FilterSince(const ScriptResult& input, const PackedArgs& args)
			{
				if (args.size() < 1)
					return SendError(Reply("-ERR Missing arguments for 'since'"));

				std::string arg = args[0];
				if (arg == "today")
					arg = GetTime();
				else if (arg == "last" && args.size() > 1)
				{
					if (args[1] == "hour")
						arg = GetTime(0, 1);
					else if(args[1] == "day")
						arg = GetTime(1);
					else if (args[1] == "week")
						arg = GetTime(7);
					else if (args[1] == "month")
						arg = GetTime(31);
					else if (args[1] == "year")
						arg = GetTime(365);
				}
				else if (arg == "days" && args.size() > 1)
				{
					arg = GetTime(ToInt(args[1]));
				}
				else if (arg == "hours" && args.size() > 1)
				{
					arg = GetTime(0, ToInt(args[1]));
				}

				if (auto target = ArgsToDate(arg))
				{
					ScriptResult output;

					for (const auto& a : input)
					{
						if (a.second.IsError())
							output[a.first] = a.second;
						else if (const auto v = GetDate(a.first); v && *v >= *target)
							output[a.first] = a.second;
					}

					return output;
				}
				
				return SendError(Reply("-ERR 'since' expects argument format YYYY-MM-DD"));
			}

			ScriptResult FilterBefore(const ScriptResult& input, const PackedArgs& args)
			{
				if (args.size() < 1)
					return SendError(Reply("-ERR Missing arguments for 'before'"));

				std::string arg = args[0];
				if (arg == "today")
					arg = GetTime();
				else if (arg == "last" && args.size() > 1)
				{
					if (args[1] == "hour")
						arg = GetTime(0, 1);
					else if (args[1] == "day")
						arg = GetTime(1);
					else if (args[1] == "week")
						arg = GetTime(7);
					else if (args[1] == "month")
						arg = GetTime(31);
					else if (args[1] == "year")
						arg = GetTime(365);
				}
				else if (arg == "days" && args.size() > 1)
				{
					arg = GetTime(ToInt(args[1]));
				}
				else if (arg == "hours" && args.size() > 1)
				{
					arg = GetTime(0, ToInt(args[1]));
				}

				if (auto target = ArgsToDate(arg))
				{
					ScriptResult output;

					for (const auto& a : input)
					{
						if (a.second.IsError())
							output[a.first] = a.second;
						else if (const auto v = GetDate(a.first); v && *v <= *target)
							output[a.first] = a.second;
					}

					return output;
				}

				return SendError(Reply("-ERR 'before' expects argument format YYYY-MM-DD"));
			}

			ScriptResult FilterType(const ScriptResult& input, const PackedArgs& args)
			{
				if (args.size() < 1)
					return SendError(Reply("-ERR Missing arguments for 'type'"));

				const auto target = LowercaseString(args[0]);
				ScriptResult output;

				for (const auto& a : input)
				{
					if (a.second.IsError())
						output[a.first] = a.second;
					else if (const auto info = KeyInfo(a.first); info && LowercaseString(std::string(magic_enum::enum_name(info.type))) == target)
						output[a.first] = a.second;
				}

				return output;
			}

			ScriptResult FilterUser(const ScriptResult& input, const PackedArgs& args)
			{
				if (args.size() < 1)
					return SendError(Reply("-ERR Missing arguments for 'user'"));

				ScriptResult output;

				for (const auto& a : input)
				{
					if (a.second.IsError())
						output[a.first] = a.second;
					else if (const auto info = KeyInfo(a.first); info && info.user == args[0])
						output[a.first] = a.second;
				}

				return output;
			}

			ScriptResult FilterUnit(const ScriptResult& input, const PackedArgs& args)
			{
				if (args.size() < 1)
					return SendError(Reply("-ERR Missing arguments for 'unit'"));

				ScriptResult output;

				for (const auto& a : input)
				{
					if (a.second.IsError())
						output[a.first] = a.second;
					else if (const auto info = KeyInfo(a.first); info && info.unit == args[0])
						output[a.first] = a.second;
				}

				return output;
			}

			ScriptResult FilterBadKey(const ScriptResult& input, const PackedArgs& args)
			{
				ScriptResult output;

				for (const auto& a : input)
				{
					if (a.second.IsError())
						output[a.first] = a.second;
					else if (const auto info = KeyInfo(a.first); !info)
						output[a.first] = a.second;
				}

				return output;
			}

			ScriptResult FilterValid(const ScriptResult& input, const PackedArgs& args)
			{
				ScriptResult output;

				for (const auto& a : input)
				{
					if (a.second.IsError())
						output[a.first] = a.second;
					else if (KeyInfo(a.first))
						output[a.first] = a.second;
				}

				return output;
			}

			ScriptResult FilterRelease(const ScriptResult& input, const PackedArgs& args)
			{
				ScriptResult output;

				for (const auto& a : input)
				{
					if (a.second.IsError())
						output[a.first] = a.second;
					else if (const auto info = KeyInfo(a.first); info && !info.debug)
						output[a.first] = a.second;
				}

				return output;
			}

			ScriptResult FilterDebug(const ScriptResult& input, const PackedArgs& args)
			{
				ScriptResult output;

				for (const auto& a : input)
				{
					if (a.second.IsError())
						output[a.first] = a.second;
					else if (const auto info = KeyInfo(a.first); info && info.debug)
						output[a.first] = a.second;
				}

				return output;
			}

			void PushCommand(Callback& call, const ParsedCommand& command, size_t begin, size_t end)
			{
				if (begin >= end)
					return;

				const auto cmd = LowercaseString(std::string(command[begin]));
				bool found_command = false;

			#define REGISTER_REQUEST(Name, Func) if (!found_command && cmd == Name) { call.Push([this, args = PackArgs(command, begin + 1, end)](const Callback::Followup& next, const ScriptResult& input, bool has_input, Engine::Telemetry::DB db) { Func(next, input, has_input, args, db); }); found_command = true; }
			#define REGISTER_FILTER(Name, Func) if (!found_command && cmd == Name) { call.Push([this, args = PackArgs(command, begin + 1, end)](const ScriptResult& input){ return Func(input, args); }); found_command = true; }

				REGISTER_REQUEST("get", RequestGet);
				REGISTER_REQUEST("select", RequestSelect);
				REGISTER_REQUEST("help", RequestHelp);

				REGISTER_FILTER("values", FilterValues);
				REGISTER_FILTER("since", FilterSince);
				REGISTER_FILTER("before", FilterBefore);
				REGISTER_FILTER("type", FilterType);
				REGISTER_FILTER("user", FilterUser);
				REGISTER_FILTER("unit", FilterUnit);
				REGISTER_FILTER("badkey", FilterBadKey);
				REGISTER_FILTER("valid", FilterValid);
				REGISTER_FILTER("config", FilterConfig);
				REGISTER_FILTER("release", FilterRelease);
				REGISTER_FILTER("debug", FilterDebug);

				if(!found_command)
					call.Push([this, cmd](const ScriptResult& input) { return SendError("-ERR No Command named '" + cmd + "' found"); });

			#undef REGISTER_FILTER
			#undef REGISTER_REQUEST
			}

		public:
			void LoadScripts()
			{
				if (!IsConnected())
					return;

				LoadScript("get", 
						   "local keys = {} "
						   "if #ARGV > 2 then "
						   "	keys = redis.call('SCAN', ARGV[1], 'MATCH', ARGV[3], 'COUNT', ARGV[2]) "
						   "elseif #ARGV > 1 then "
						   "	keys = redis.call('SCAN', ARGV[1], 'COUNT', ARGV[2]) "
						   "else "
						   "	keys = redis.call('SCAN', ARGV[1]) "
						   "end "
						   "if #keys[2] == 0 then "
						   "	return { keys[1], {}, {} } "
						   "end "
						   "local vals = redis.call('MGET', unpack(keys[2])) "
						   "return { keys[1], keys[2], vals } "
				);
			}

			void PushRequest(const std::string_view& command, const Callback::Output& callback, Engine::Telemetry::DB db = Engine::Telemetry::DB::Stats)
			{
				if (!IsConnected() || !callback)
					return;

				const auto args = Parse(command);
				size_t end = args.size();

				Callback call(callback);

				// Pipeline commands:
				for (size_t a = args.size(); a > 0; a--)
				{
					if (args[a - 1] == "|")
					{
						PushCommand(call, args, a, end);
						end = a - 1;
					}
				}

				PushCommand(call, args, 0, end);
				call.Execute(db);
			}

		#endif
		};

		void DrawPlot(const char* label, std::optional<float>(*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, float scale_min, float scale_max, ImVec2 frame_size, const char* overlay_text = nullptr, const char* tooltip_text = "%8.4g")
		{
			ImGuiContext& g = *GImGui;
			ImGuiWindow* window = ImGui::GetCurrentWindow();
			if (window->SkipItems)
				return;

			const ImGuiStyle& style = g.Style;
			const ImGuiID id = window->GetID(label);

			const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
			if (frame_size.x == 0.0f)
				frame_size.x = ImGui::CalcItemWidth();
			if (frame_size.y == 0.0f)
				frame_size.y = label_size.y + (style.FramePadding.y * 2);

			const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);
			const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
			const ImRect total_bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0));
			ImGui::ItemSize(total_bb, style.FramePadding.y);
			if (!ImGui::ItemAdd(total_bb, 0, &frame_bb))
				return;

			const bool hovered = ImGui::ItemHoverable(frame_bb, id);

			// Determine scale from values if not specified
			if (scale_min == FLT_MAX || scale_max == FLT_MAX)
			{
				float v_min = FLT_MAX;
				float v_max = -FLT_MAX;
				for (int i = 0; i < values_count; i++)
				{
					if (auto v = values_getter(data, i))
					{
						if (*v != *v) // Ignore NaN values
							continue;
						v_min = ImMin(v_min, *v);
						v_max = ImMax(v_max, *v);
					}
				}
				if (scale_min == FLT_MAX)
					scale_min = v_min;
				if (scale_max == FLT_MAX)
					scale_max = v_max;
			}

			ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);

			if (scale_min != FLT_MAX && scale_max != FLT_MAX && scale_min < scale_max)
			{
				const auto step = (scale_max - scale_min) / 3.0f;
				auto RenderBack = [&](float v)
				{
					char buffer[20] = { '\0' };
					ImFormatString(buffer, std::size(buffer), "%.4g", v);

					const auto buffer_size = ImGui::CalcTextSize(buffer);
					const auto y = ImLerp(inner_bb.Min.y, inner_bb.Max.y, 1.0f - ImSaturate((v - scale_min) / (scale_max - scale_min)));
					const auto x = (inner_bb.Min.x + inner_bb.Max.x - buffer_size.x) / 2.0f;

					const auto xp0 = inner_bb.Min.x + 5;
					const auto xp1 = x - 5;
					const auto xp2 = x + buffer_size.x + 5;
					const auto xp3 = inner_bb.Max.x - 5;

					const auto col = ImGui::GetColorU32(ImGuiCol_PlotLines, 0.3f);
					ImGui::PushStyleColor(ImGuiCol_Text, col);
					ImGui::RenderTextClipped(ImVec2(inner_bb.Min.x, y - (ImGui::GetFontSize() / 2)), ImVec2(inner_bb.Max.x, y + (ImGui::GetFontSize() / 2)), buffer, nullptr, &buffer_size, ImVec2(0.5f, 0.5f), &inner_bb);

					if (xp0 < xp1)
						window->DrawList->AddLine(ImVec2(xp0, y), ImVec2(xp1, y), col);

					if (xp2 < xp3)
						window->DrawList->AddLine(ImVec2(xp2, y), ImVec2(xp3, y), col);

					ImGui::PopStyleColor();
				};

				RenderBack(scale_min + step);
				RenderBack(scale_min + step + step);
			}

			if (values_count >= 2)
			{
				float closest_d = FLT_MAX;
				float closest_v = 0.0f;

				const bool mouse_hovered = hovered && inner_bb.Contains(g.IO.MousePos);
				const float mouse_t = mouse_hovered ? ImClamp((g.IO.MousePos.x - inner_bb.Min.x) / (inner_bb.Max.x - inner_bb.Min.x), 0.0f, 0.9999f) : 0.0f;
				const float mouse_v = mouse_hovered ? ImClamp((g.IO.MousePos.y - inner_bb.Min.y) / (inner_bb.Max.y - inner_bb.Min.y), 0.0f, 0.9999f) : 0.0f;
				const ImVec2 mtp = ImVec2(mouse_t, mouse_v);

				const float t_step = 1.0f / (values_count - 1);
				const float inv_scale = (scale_min == scale_max) ? 0.0f : (1.0f / (scale_max - scale_min));
				const ImU32 col_base = ImGui::GetColorU32(ImGuiCol_PlotLines);

				float v0 = scale_min;
				if (auto v = values_getter(data, (0 + values_offset) % values_count))
					v0 = *v;

				float t0 = 0.0f;
				ImVec2 tp0 = ImVec2(t0, 1.0f - ImSaturate((v0 - scale_min) * inv_scale));

				if (auto d = ImLengthSqr(tp0 - mtp); d < closest_d)
				{
					closest_d = d;
					closest_v = v0;
				}

				for (int i = 1; i < values_count; i++)
				{
					float v1 = v0;
					if (auto v = values_getter(data, (i + values_offset) % values_count))
						v1 = *v;
					else if (i + 1 < values_count)
						continue;

					const float t1 = i * t_step;
					ImVec2 tp1 = ImVec2(t1, 1.0f - ImSaturate((v1 - scale_min) * inv_scale));

					ImVec2 pos0 = ImLerp(inner_bb.Min, inner_bb.Max, tp0);
					ImVec2 pos1 = ImLerp(inner_bb.Min, inner_bb.Max, tp1);
					window->DrawList->AddLine(pos0, pos1, col_base);

					if (auto d = ImLengthSqr(tp1 - mtp); d < closest_d)
					{
						closest_d = d;
						closest_v = v1;
					}

					v0 = v1;
					t0 = t1;
					tp0 = tp1;
				}

				const auto hover_distance = 20.0f / ImMax(inner_bb.GetWidth(), inner_bb.GetHeight());
				if (mouse_hovered && tooltip_text && closest_d < hover_distance * hover_distance)
					ImGui::SetTooltip(tooltip_text, closest_v);
			}

			// Text overlay
			if (overlay_text)
				ImGui::RenderTextClipped(ImVec2(frame_bb.Min.x, frame_bb.Min.y + style.FramePadding.y), frame_bb.Max, overlay_text, NULL, NULL, ImVec2(0.5f, 0.0f));

			if (label_size.x > 0.0f)
				ImGui::RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, inner_bb.Min.y), label);
		}
	}

	class TelemetryPlugin::Impl : public Scripting
	{
	#ifdef REDIS_ENABLED
	private:
		static constexpr float DefaultPlotWidth = 200.0f;
		static constexpr float DefaultPlotHeight = 150.0f;

		struct DataPoint
		{
			std::string key;
			KeyInfo info;
			Reply data;

			DataPoint(const std::string& key, const Reply& data) : key(key), info(key), data(data) {}
		};

		struct PlotPoint
		{
			double total = 0;
			double count = 0;
		};

		class AutomaticData
		{
		protected:
			const std::string label;
			const std::string request;
			const std::string tooltip;

		private:
			size_t request_id = 1;
			size_t rendered_id = 0;

		protected:
			static constexpr size_t NumIDs = 2;
			virtual void Reset(size_t id) = 0;
			virtual void Update(size_t id, const ScriptResult& input) = 0;
			virtual void Render(size_t id, bool cap_value) = 0;

		public:
			AutomaticData(const std::string& label, const std::string& request, const std::string& tooltip) : label(label), request(request), tooltip(tooltip) {}

			void Update(Scripting* scripting, bool first_update)
			{
				request_id++;
				rendered_id = (first_update ? request_id : (request_id + 1)) % NumIDs;
				const auto bucket_id = request_id % NumIDs;

				Reset(bucket_id);

				scripting->PushRequest(request, [this, id = request_id](const ScriptResult& input)
				{
					if (id != request_id)
						return;

					const auto bucket_id = id % NumIDs;
					Update(bucket_id, input);
				});
			}

			void Render(Scripting* scripting, bool cap_values)
			{
				ImGui::PushID(label.c_str());
				ImGui::BeginGroup();

				const auto text_w = ImGui::CalcTextSize(label.c_str()).x;
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ((DefaultPlotWidth - text_w) / 2.0f));
				ImGui::Text(label.c_str());

				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", request.c_str());

				Render(rendered_id, cap_values);

				ImGui::EndGroup();

				if (ImGui::BeginPopupContextItem("##PlotContextMenu"))
				{
					if (ImGui::Selectable(IMGUI_ICON_RELOAD " Refresh"))
						Update(scripting, cap_values);

					if (ImGui::MenuItem(IMGUI_ICON_EXPORT " Copy search to clipboard"))
						CopyTextToClipboard(StringToWstring(request));

					ImGui::EndPopup();
				}

				ImGui::PopID();
			}
		};

		class AutomaticPlot : public AutomaticData
		{
		public:
			static constexpr size_t MinUsers = 20; // Data Points with less than this number of users will not be shown in "compressed" view mode

		private:
			static constexpr size_t NumBuckets = 30;

			uint64_t min_time = 0;
			uint64_t max_time = 0;

			struct SamplePoint {
				float min_value = 0;
				float max_value = 0;
				float mean_value = 0;
				size_t users = 0;
			};

			struct DataPoint {
				float min_value = 0;
				float max_value = 0;
				double sum_value = 0;
				double sum_sqr = 0;
				size_t users = 0;

				SamplePoint GetSamplePoint(bool cap_value) const
				{
					if (users == 0)
						return { 0.0f, 0.0f, 0.0f, 0 };

					const auto u = double(users);
					const auto m = sum_value / u;
					float l = min_value;
					float h = max_value;
					
					if (cap_value)
					{
						const auto d = float(std::sqrt(std::abs((sum_sqr / u) - (m * m))));
						l = std::max(l, float(m - d));
						h = std::min(h, float(m + d));
					}

					return { l, h, float(m), users };
				}
			};

			DataPoint plot[NumIDs][NumBuckets]; // One data point for every day

			void Reset(size_t id) override final
			{
				for (auto& a : plot[id])
				{
					a.users = 0;
					a.min_value = 0.0f;
					a.max_value = 0.0f;
					a.sum_value = 0.0;
				}

				max_time = std::chrono::duration_cast<std::chrono::hours>(std::chrono::system_clock::now().time_since_epoch()).count();
				if (max_time % 24 != 0)
					max_time += 24 - (max_time % 24);

				min_time = max_time - (24 * NumBuckets);
			}

			void Update(size_t id, const ScriptResult& input) override final
			{
				for (const auto& a : input)
				{
					const auto key = KeyInfo(a.first);
					const auto value = ReplyToFloat(a.second);
					if (key && key.HasTime() && value)
					{
						const auto t = key.GetTimeStamp();
						if (t == 0)
							continue;

						const auto d = std::chrono::duration_cast<std::chrono::hours>(std::chrono::seconds(t)).count();
						if (d < min_time || d > max_time)
							continue;

						const auto bucket = (d + 23 - min_time) / 24;
						if (bucket < NumBuckets)
						{
							if (plot[id][bucket].users == 0 || *value < plot[id][bucket].min_value)
								plot[id][bucket].min_value = *value;

							if (plot[id][bucket].users == 0 || *value > plot[id][bucket].max_value)
								plot[id][bucket].max_value = *value;

							plot[id][bucket].users++;
							plot[id][bucket].sum_value += *value;
							plot[id][bucket].sum_sqr += *value * *value;
						}
					}
				}
			}

			void Render(size_t data_id, bool cap_value) override final
			{
				ImGuiContext& g = *GImGui;
				ImGuiWindow* window = ImGui::GetCurrentWindow();
				if (window->SkipItems)
					return;

				const auto& data = plot[data_id];
				const auto frame_size = ImVec2(DefaultPlotWidth, DefaultPlotHeight);
				const ImGuiStyle& style = g.Style;
				const ImGuiID id = window->GetID("##Plot");

				const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);
				const ImRect inner_bb(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding);
				const ImRect total_bb = frame_bb;
				ImGui::ItemSize(total_bb, style.FramePadding.y);
				if (!ImGui::ItemAdd(total_bb, 0, &frame_bb))
					return;

				const bool hovered = ImGui::ItemHoverable(frame_bb, id);
				const bool mouse_hovered = hovered && inner_bb.Contains(g.IO.MousePos);
				const float v_min = 0.0f;
				float v_max = 0.0f;

				const size_t min_users = cap_value ? MinUsers : 0;
				SamplePoint samples[NumBuckets] = { };
				for (size_t a = 0; a < NumBuckets; a++)
					samples[a] = data[a].GetSamplePoint(cap_value);

				float max_sum = 0;
				float sample_count = 0;
				for (size_t a = 0; a < NumBuckets; a++)
				{
					if (samples[a].users > min_users || a + 1 >= NumBuckets)
					{
						v_max = std::max(v_max, samples[a].max_value);
						max_sum += samples[a].max_value;
						sample_count += 1.0f;
					}
				}

				if (sample_count > 0.0f)
				{
					const auto m = max_sum / sample_count;
					v_max = std::min(v_max, m + m);
				}

				size_t hovered_item = NumBuckets;

				ImGui::RenderFrame(frame_bb.Min, frame_bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
				if (v_max > v_min)
				{
					ImGui::PushClipRect(inner_bb.Min, inner_bb.Max, true);
					const auto scale_y = 1.0f / (v_max - v_min);
					const auto scale_x = 1.0f / (NumBuckets - 1);
					
					const float background_step = (v_max - v_min) / 3.0f;
					for (size_t a = 1; a < 3; a++)
					{
						const auto v = v_min + (a * background_step);
						char buffer[20] = { '\0' };
						ImFormatString(buffer, std::size(buffer), "%.0f", v);

						const auto buffer_size = ImGui::CalcTextSize(buffer);
						const auto y = ImLerp(inner_bb.Min.y, inner_bb.Max.y, 1.0f - ((v - v_min) * scale_y));
						const auto x = (inner_bb.Min.x + inner_bb.Max.x - buffer_size.x) / 2.0f;

						const auto xp0 = inner_bb.Min.x + 5;
						const auto xp1 = x - 5;
						const auto xp2 = x + buffer_size.x + 5;
						const auto xp3 = inner_bb.Max.x - 5;

						const auto col = ImGui::GetColorU32(ImGuiCol_PlotLines, 0.3f);
						ImGui::PushStyleColor(ImGuiCol_Text, col);
						ImGui::RenderTextClipped(ImVec2(inner_bb.Min.x, y - (ImGui::GetFontSize() / 2)), ImVec2(inner_bb.Max.x, y + (ImGui::GetFontSize() / 2)), buffer, nullptr, &buffer_size, ImVec2(0.5f, 0.5f), &inner_bb);

						if (xp0 < xp1)
							window->DrawList->AddLine(ImVec2(xp0, y), ImVec2(xp1, y), col);

						if (xp2 < xp3)
							window->DrawList->AddLine(ImVec2(xp2, y), ImVec2(xp3, y), col);

						ImGui::PopStyleColor();
					}

					const auto line_col = ImGui::GetColorU32(ImGuiCol_PlotLines);
					for (size_t a = 1; a < NumBuckets; a++)
					{
						if (samples[a].users <= min_users && a + 1 < NumBuckets)
							continue;

						size_t prev = a - 1;
						for (size_t b = a; b > 0; b--)
						{
							if (samples[b - 1].users > min_users)
							{
								prev = b - 1;
								break;
							}
						}

						if (samples[prev].users <= min_users)
							continue;

						const auto lx = ImLerp(inner_bb.Min.x, inner_bb.Max.x, prev * scale_x);
						const auto rx = ImLerp(inner_bb.Min.x, inner_bb.Max.x, a * scale_x);
						const bool h = mouse_hovered && g.IO.MousePos.x >= lx && g.IO.MousePos.x <= rx;
						const auto col = ImGui::GetColorU32(h ? ImGuiCol_PlotHistogramHovered : ImGuiCol_PlotHistogram, 0.5f);

						if (lx < rx)
						{
							const auto lpy = ImLerp(inner_bb.Max.y, inner_bb.Min.y, (samples[prev].mean_value - v_min) * scale_y);
							const auto rpy = ImLerp(inner_bb.Max.y, inner_bb.Min.y, (samples[a].mean_value - v_min) * scale_y);
							const auto lhy = ImLerp(inner_bb.Max.y, inner_bb.Min.y, (samples[prev].max_value - v_min) * scale_y);
							const auto lly = ImLerp(inner_bb.Max.y, inner_bb.Min.y, (samples[prev].min_value - v_min) * scale_y);
							const auto rhy = ImLerp(inner_bb.Max.y, inner_bb.Min.y, (samples[a].max_value - v_min) * scale_y);
							const auto rly = ImLerp(inner_bb.Max.y, inner_bb.Min.y, (samples[a].min_value - v_min) * scale_y);

							if (lhy < lly && rhy < rly)
							{
								ImVec2 points[] = { ImVec2(lx, lhy), ImVec2(rx, rhy), ImVec2(rx, rly), ImVec2(lx, lly) };
								window->DrawList->AddConvexPolyFilled(points, int(std::size(points)), col);
							}
							else if (lhy < lly)
							{
								ImVec2 points[] = { ImVec2(lx, lhy), ImVec2(rx, rpy), ImVec2(lx, lly) };
								window->DrawList->AddConvexPolyFilled(points, int(std::size(points)), col);
							}
							else if (rhy < rly)
							{
								ImVec2 points[] = { ImVec2(lx, lpy), ImVec2(rx, rhy), ImVec2(rx, rly) };
								window->DrawList->AddConvexPolyFilled(points, int(std::size(points)), col);
							}

							window->DrawList->AddLine(ImVec2(lx, lpy), ImVec2(rx, rpy), line_col);
						}

						if (h)
							hovered_item = (g.IO.MousePos.x - lx) < (rx - g.IO.MousePos.x) ? prev : a;
					}

					ImGui::PopClipRect();
				}

				if (hovered_item < NumBuckets)
				{
					const auto date = GetReadableDate(unsigned(NumBuckets - (hovered_item + 1)));
					ImGui::BeginTooltipEx(ImGuiWindowFlags_None, ImGuiTooltipFlags_OverridePreviousTooltip);
					ImGui::Text("%s", date.c_str());
					ImGui::Separator();
					ImGui::Text(tooltip.c_str(), data[hovered_item].users > 0 ? float(data[hovered_item].sum_value / data[hovered_item].users) : 0.0f, unsigned(data[hovered_item].users), data[hovered_item].min_value, data[hovered_item].max_value);
					ImGui::EndTooltip();
				}
			}

			static std::string BuildRequest(const std::string& request, const std::string& filter)
			{
				std::stringstream r;
				r << request << " | valid | since days " << (NumBuckets + 1) << " | release | config testing | " << filter;
				return r.str();
			}

		public:
			AutomaticPlot(const std::string& label, const std::string& request, const std::string& filter, const std::string& tooltip = "%8.4g") : AutomaticData(label, BuildRequest(request, filter), tooltip) {}
		};

		class AutomaticHistory : public AutomaticData
		{
		private:
			Memory::Map<unsigned, unsigned, Memory::Tag::Unknown> history[NumIDs];

			void Reset(size_t id) override final
			{
				history[id].clear();
			}

			void Update(size_t id, const ScriptResult& input) override final
			{
				for (const auto& a : input)
				{
					const auto key = KeyInfo(a.first);
					const auto value = ReplyToFloat(a.second);
					if (key && value)
					{
						const auto v = unsigned(*value);
						if (const auto f = history[id].find(v); f != history[id].end())
							f->second++;
						else
							history[id][v] = 1;
					}
				}
			}

			static float ValueGetter(void* data, int idx)
			{
				return reinterpret_cast<float*>(data)[idx];
			}

			void Render(size_t id, bool cap_value) override final
			{
				Memory::SmallVector<unsigned, 32, Memory::Tag::Unknown> keys;
				Memory::SmallVector<float, 32, Memory::Tag::Unknown> values;
				for (const auto& a : history[id])
				{
					keys.push_back(a.first);
					values.push_back(float(a.second));
				}

				const auto hovered = ImGui::PlotEx(ImGuiPlotType_Histogram, "##Plot", ValueGetter, values.data(), int(values.size()), 0, nullptr, 0.0f, FLT_MAX, ImVec2(DefaultPlotWidth, DefaultPlotHeight));
				if (hovered >= 0 && hovered < ( int )keys.size())
				{
					ImGui::BeginTooltipEx(ImGuiWindowFlags_None, ImGuiTooltipFlags_OverridePreviousTooltip);
					ImGui::Text(tooltip.c_str(), keys[hovered], unsigned(values[hovered]));
					ImGui::EndTooltip();
				}
			}

			static std::string BuildRequest(const std::string& request, const std::string& filter)
			{
				std::stringstream r;
				r << request << " | valid | release | config testing | " << filter;
				return r.str();
			}

		public:
			AutomaticHistory(const std::string& label, const std::string& request, const std::string& filter, const std::string& tooltip = "%u") : AutomaticData(label, BuildRequest(request, filter), tooltip) {}
		};

		Memory::Vector<DataPoint, Memory::Tag::Unknown> current_data;
		Memory::SmallVector<Memory::Pointer<AutomaticData, Memory::Tag::Unknown>, 8, Memory::Tag::Unknown> automatic_plots;
		PlotPoint current_plot[512];
		size_t request_id = 0;
		size_t next_automatic_update = 0;
		int64_t automatic_timer = 0;
		char request_buffer[2 * Memory::KB];
		KeyInfo plot_first_point;
		KeyInfo plot_last_point;
		bool has_plot = false;
		bool has_automatic_timer = false;
		bool automatic_all_values = false;
		bool first_frame = true;

		template<typename T, typename... Args> void AddAutomaticData(Args&&... args)
		{
			automatic_plots.emplace_back(Memory::Pointer<T, Memory::Tag::Unknown>::make(std::forward<Args>(args)...).release());
		}

		void UpdatePlot()
		{
			for (auto& a : current_plot)
			{
				a.total = 0.0f;
				a.count = 0.0f;
			}

			has_plot = false;
			uint64_t min_time = ~uint64_t(0);
			uint64_t max_time = 0;
			plot_first_point = KeyInfo();
			plot_last_point = KeyInfo();

			for (const auto& a : current_data)
			{
				if (!a.info.is_valid() || !ReplyToFloat(a.data))
					continue;

				const auto t = a.info.GetTimeStamp();
				if (t == 0)
					continue;

				if (t < min_time)
				{
					min_time = t;
					plot_first_point = a.info;
				}

				if (t > max_time)
				{
					max_time = t;
					plot_last_point = a.info;
				}
			}

			if (min_time >= max_time || min_time + 120 > max_time)
				return;

			auto GetBucket = [min_time, max_time, size = std::size(current_plot)](const KeyInfo& key) -> std::optional<size_t>
			{
				if (!key.is_valid())
					return std::nullopt;

				const auto t = key.GetTimeStamp();
				if (t == 0)
					return std::nullopt;

				if (t < min_time)
					return 0;

				if (t > max_time)
					return size - 1;

				return std::min( static_cast<size_t>( (size * (t - min_time)) / (max_time - min_time) ), size - 1);
			};

			has_plot = true;
			for (const auto& a : current_data)
			{
				if (auto bucket = GetBucket(a.info))
				{
					if (auto value = ReplyToFloat(a.data))
					{
						current_plot[*bucket].count += 1.0;
						current_plot[*bucket].total += *value;
					}
				}
			}
		}

		void Refresh()
		{
			current_data.clear();
			request_id++;
			PushRequest(request_buffer, [this, id = request_id](const ScriptResult& input)
			{
				if (id != request_id)
					return;

				for (const auto& a : input)
					current_data.emplace_back(a.first, a.second);

				UpdatePlot();
			});
		}

		static std::optional<float> PlotGetter(void* data, int idx)
		{
			PlotPoint* points = reinterpret_cast<PlotPoint*>(data);
			const auto& point = points[idx];
			if (point.count > 0.0f)
				return float(point.total / point.count);

			return std::nullopt;
		}

		void SaveManualSearch()
		{
			static size_t next_file_id = 0;
			constexpr std::wstring_view file_prefix = L"telemetry_search";
			constexpr std::wstring_view file_suffix = L".csv";

			auto GetFileName = [&]()
			{
				std::wstringstream s;
				s << file_prefix;
				if (next_file_id > 0)
					s << L" (" << next_file_id << L")";

				s << file_suffix;
				return s.str();
			};

			std::wstring file_name = GetFileName();
			while (FileSystem::FileExists(file_name))
			{
				next_file_id++;
				file_name = GetFileName();
			}

			std::ofstream file(file_name, std::ios::trunc | std::ios::out);
			if (!file)
				return;

			file << "key,config,tag,user,date,time,type,unit,value" << std::endl;

			auto SanitizeString = [](const std::string_view& str)
			{
				if (str.find(',') == str.npos)
					return std::string(str);

				std::stringstream s;
				s << "\"";
				for (size_t a = 0; true;)
				{
					const auto f = str.find_first_of('\"', a);
					if (f == str.npos)
					{
						s << str.substr(a);
						break;
					}
					else
					{
						if(f > a)
							s << str.substr(a, f - a);

						s << "\"\"";
						a = f + 1;
					}
				}

				s << "\"";
				return s.str();
			};

			std::function<void(const Reply& reply)> OutputReply;
			OutputReply = [&file, &SanitizeString, &OutputReply](const Reply& reply)
			{
				if (reply.IsError())
					file << SanitizeString(reply.AsError());
				else if (reply.IsNull())
					return;
				else if (reply.IsInt())
					file << reply.AsInt();
				else if (reply.IsDouble())
					file << reply.AsDouble();
				else if (reply.IsBool())
					file << reply.AsBool() ? "true" : "false";
				else if (reply.IsString())
					file << SanitizeString(reply.AsString());
				else if (reply.IsArray())
				{
					const auto tmp = reply.AsArray();
					if (tmp.empty())
						file << "[empty list]";
					else if (tmp.size() == 1)
						OutputReply(tmp[0]);
					else
						file << "[list with " << tmp.size() << " elements]";
				}
			};

			for (const auto& data : current_data)
			{
				file << SanitizeString(data.key) << ",";
				if (data.info)
				{
					file << SanitizeString(data.info.config) << ",";
					file << SanitizeString(data.info.name) << ",";
					file << SanitizeString(data.info.user) << ",";
					file << data.info.day << "/" << data.info.month << "/" << data.info.year << ",";
					file << data.info.hour << ":" << data.info.minute << ",";
					file << magic_enum::enum_name(data.info.type) << ",";
					file << data.info.unit << ",";
				}
				else
				{
					file << ",,,,,,,";
				}

				OutputReply(data.data);
				file << std::endl;
			}
		}

		void RenderManualSearch()
		{
			ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);

			const auto reload_button_width = ImGui::CalcTextSize(IMGUI_ICON_RELOAD).x + (2 * ImGui::GetStyle().FramePadding.x);
			const auto export_button_width = ImGui::CalcTextSize(IMGUI_ICON_SAVE).x + (2 * ImGui::GetStyle().FramePadding.x);

			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - (reload_button_width + export_button_width + (2 * ImGui::GetStyle().ItemSpacing.x)));
			if (ImGui::InputText("##ManualRequest", request_buffer, std::size(request_buffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
				Refresh();

			ImGui::SameLine();
			if (ImGui::Button(IMGUI_ICON_RELOAD, ImVec2(reload_button_width, 0)))
			{
				Engine::Telemetry::Get().Flush();
				Refresh();
			}

			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Refresh");

			ImGui::SameLine();
			if (ImGui::Button(IMGUI_ICON_SAVE, ImVec2(export_button_width, 0)))
				SaveManualSearch();

			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text("Save search result");
				ImGui::Separator();
				ImGui::Text("Search Result will be saved into telemetry_search.csv");
				ImGui::EndTooltip();
			}
			
			if (ImGui::BeginTabBar("##Tabs"))
			{
				if (ImGui::BeginTabItem("Table"))
				{
					enum StatTableID : ImU32
					{
						ID_Key,
						ID_Value,
					};

					constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;

					if (ImGui::BeginTable("##Stats", int(magic_enum::enum_count<StatTableID>()), tableFlags))
					{
						ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_None, 0.0f, ID_Key);
						ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_None, 0.0f, ID_Value);
						ImGui::TableSetupScrollFreeze(0, 1); // Make row always visible
						ImGui::TableHeadersRow();

						std::function<void(const Reply& reply, const KeyInfo& key, bool bullet)> OutputReply;
						OutputReply = [&OutputReply](const Reply& reply, const KeyInfo& key, bool bullet)
						{
							if (bullet)
							{
								ImGui::Bullet();
								ImGui::SameLine();
							}

							if (reply.IsError())
							{
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
								auto tmp = std::string(reply.AsError());
								ImGui::TextUnformatted(tmp.c_str());
								ImGui::PopStyleColor();
							}
							else if (reply.IsNull())
								ImGui::Text("Nil");
							else if (reply.IsInt())
								ImGui::Text("%d %s", reply.AsInt(), key.unit.c_str());
							else if (reply.IsDouble())
								ImGui::Text("%f %s", reply.AsDouble(), key.unit.c_str());
							else if (reply.IsBool())
								ImGui::Text("%s %s", reply.AsBool() ? "true" : "false", key.unit.c_str());
							else if (reply.IsString())
							{
								auto tmp = std::string(reply.AsString());
								if (key.type == KeyInfo::Type::UInt || key.type == KeyInfo::Type::Float)
									ImGui::Text("%s %s", tmp.c_str(), key.unit.c_str());
								else
									ImGui::Text("\"%s\"", tmp.c_str());
							}
							else if (reply.IsArray())
							{
								ImGui::Text("List");
								ImGui::Indent();
								const auto tmp = reply.AsArray();
								for (const auto& a : tmp)
									OutputReply(a, key, true);

								ImGui::Unindent();
							}
						};

						int id = 0;
						for (const auto& data : current_data)
						{
							ImGui::TableNextRow();
							ImGui::PushID(id++);

							// Key
							{
								ImGui::TableNextColumn();
								if (data.info)
								{
									if (data.info.year > 0)
										ImGui::Text("%s at %d/%d/%d - %02d:%02d", data.info.name.c_str(), data.info.day, data.info.month, data.info.year, data.info.hour, data.info.minute);
									else
										ImGui::Text("%s from %s", data.info.name.c_str(), data.info.user.c_str());

									if (ImGui::BeginPopupContextItem("##KeyItemPopup"))
									{
										ImGui::Text("%s", data.info.name.c_str());
										ImGui::Separator();

										char buffer[256] = { '\0' };
										ImFormatString(buffer, std::size(buffer), "User: %s", data.info.user.c_str());
										if (ImGui::MenuItem(buffer))
											CopyTextToClipboard(StringToWstring(data.info.user));

										if (data.info.year > 0)
										{
											ImFormatString(buffer, std::size(buffer), "From %d/%d/%d at %02d:%02d", data.info.day, data.info.month, data.info.year, data.info.hour, data.info.minute);
											if (ImGui::MenuItem(buffer))
											{
												ImFormatString(buffer, std::size(buffer), "%04d-%02d-%02d:%02d-%02d", data.info.year, data.info.month, data.info.day, data.info.hour, data.info.minute);
												CopyTextToClipboard(StringToWstring(buffer));
											}
										}

										const auto type = std::string(magic_enum::enum_name(data.info.type));
										ImFormatString(buffer, std::size(buffer), "Type: %s", type.c_str());
										if (ImGui::MenuItem(buffer))
											CopyTextToClipboard(StringToWstring(type));

										ImFormatString(buffer, std::size(buffer), "Config: %s", data.info.config.c_str());
										if (ImGui::MenuItem(buffer))
											CopyTextToClipboard(StringToWstring(data.info.config));

										if (data.info.unit.length() > 0)
										{
											ImFormatString(buffer, std::size(buffer), "Unit: %s", data.info.unit.c_str());
											if (ImGui::MenuItem(buffer))
												CopyTextToClipboard(StringToWstring(data.info.unit));
										}

										if (ImGui::MenuItem(IMGUI_ICON_EXPORT " Copy key to clipboard"))
											CopyTextToClipboard(StringToWstring(data.key));

										ImGui::EndPopup();
									}
								}
								else
									ImGui::TextUnformatted(data.key.c_str());
							}

							// Value
							{
								ImGui::TableNextColumn();
								ImGui::BeginGroup();

								OutputReply(data.data, data.info, false);

								ImGui::EndGroup();
							}

							ImGui::PopID();
						}

						ImGui::EndTable();
					}

					ImGui::EndTabItem();
				}

				if (has_plot && ImGui::BeginTabItem("Plot"))
				{
					char buffer[256] = { '\0' };
					ImFormatString(buffer, std::size(buffer), "%d/%d/%d  %02d:%02d - %d/%d/%d  %02d:%02d", plot_first_point.day, plot_first_point.month, plot_first_point.year, plot_first_point.hour, plot_first_point.minute, plot_last_point.day, plot_last_point.month, plot_last_point.year, plot_last_point.hour, plot_last_point.minute);

					DrawPlot("##DataPlot", PlotGetter, current_plot, int(std::size(current_plot)), 0, 0, FLT_MAX, ImGui::GetContentRegionAvail(), buffer);
					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
			}

			ImGui::PopItemWidth();
		}

		void RenderDefaultPlots()
		{
			ImGui::BeginGroup();

			ImGui::Checkbox("Show all values", &automatic_all_values);
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(20.0f * ImGui::GetFontSize());
				ImGui::TextWrapped("Show all values");
				ImGui::Separator();
				ImGui::TextWrapped("By default, these graphs only show values that lie within the standard deviation "
					"of values at each day, while days with fewer than %u samples are ignored. This is done to make the graphs smoother and the overal tendency more "
					"clearly visible. Enabling this option shows all values, including exceptional spikes or days with very few samples.", unsigned(AutomaticPlot::MinUsers + 1));
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}

			ImGui::EndGroup();

			ImGui::BeginGroup();
			const auto plots_per_row = std::max(size_t(1), size_t(ImGui::GetContentRegionAvail().x / (DefaultPlotWidth + ImGui::GetStyle().ItemSpacing.x)));

			for (size_t a = 0; a < automatic_plots.size();)
			{
				for (size_t b = 0; a < automatic_plots.size() && b < plots_per_row; a++, b++)
				{
					if (b > 0)
						ImGui::SameLine();

					automatic_plots[a]->Render(this, !automatic_all_values);
				}
			}

			ImGui::EndGroup();
		}

	#ifdef DEVELOPMENT_CONFIGURATION
		Memory::Vector<std::string, Memory::Tag::Unknown> server_log;
		bool server_log_auto_scroll = true;
		bool server_log_refresh = false;
		bool scroll_to_end = false;
		char server_cmd_buffer[2048] = { '\0' };

		void PushToServerLog(const Reply& rply, size_t index, size_t tabs, const std::string& pre = "")
		{
			std::stringstream s;
			if (pre.length() > 0)
				s << pre;
			else if (tabs > 0)
				s << std::string(tabs, '\t');

			s << index << ") ";

			if (rply.IsError())
				s << "(Error) " << rply.AsError();
			else if (rply.IsInt())
				s << "(Int) " << rply.AsInt();
			else if (rply.IsDouble())
				s << "(Double) " << rply.AsDouble();
			else if (rply.IsBool())
				s << "(Bool) " << (rply.AsBool() ? "true" : "false");
			else if (rply.IsString())
			{
				const auto str = rply.AsString();
				size_t start = 0;
				s << "\"";
				while (true)
				{
					const auto f = str.find_first_of('\n', start);
					if (f == str.npos)
						break;

					auto end = f;
					if (end > start && str[end - 1] == '\r')
						end--;

					s << str.substr(start, end - start);
					start = f + 1;

					if (start < str.length())
					{
						server_log.push_back(s.str());
						s.str("");
					}
				}

				s << str.substr(start) << "\"";
			}
			else if (rply.IsNull())
				s << "nil";
			else if (rply.IsArray())
			{
				const auto arr = rply.AsArray();
				if (arr.empty())
					s << "(empty array or set)";
				else
				{
					PushToServerLog(arr[0], 0, tabs + 1, s.str());
					for (size_t a = 1; a < arr.size(); a++)
						PushToServerLog(arr[a], a, tabs + 1, "");

					return;
				}
			}

			server_log.push_back(s.str());
		}

		void RenderServerCLI()
		{
			const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
			ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), true, ImGuiWindowFlags_HorizontalScrollbar);
			if (ImGui::BeginPopupContextWindow())
			{
				if (ImGui::Selectable("Clear"))
				{
					server_log.clear();
					server_log_refresh = true;
				}

				if (ImGui::MenuItem("Auto-Scroll", nullptr, &server_log_auto_scroll) && server_log_auto_scroll)
					scroll_to_end = true;

				if (ImGui::Selectable("Scroll to end"))
					scroll_to_end = true;

				ImGui::EndPopup();
			}

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
			if ((server_log_refresh && server_log_auto_scroll) || scroll_to_end)
			{
				for(const auto& a : server_log)
					ImGui::TextUnformatted(a.c_str());

				if (scroll_to_end || server_log_auto_scroll)
					ImGui::SetScrollHereY(1.0f);
			}
			else
			{
				ImGuiListClipper clipper(int(server_log.size()));
				while (clipper.Step())
				{
					for (int a = clipper.DisplayStart; a < clipper.DisplayEnd; a++)
						ImGui::TextUnformatted(server_log[a].c_str());
				}
			}

			scroll_to_end = false;
			server_log_refresh = false;
			ImGui::PopStyleVar();
			ImGui::EndChild();

			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			if (ImGui::InputText("##CLI", server_cmd_buffer, std::size(server_cmd_buffer), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				server_log.push_back("> " + std::string(server_cmd_buffer));
				server_log_refresh = true;
				scroll_to_end = true;

				Command cmd;
				cmd.Append(server_cmd_buffer);
				Send(cmd, [this](const Responses& rsp) 
				{
					for (size_t a = 0; a < rsp.size(); a++)
						PushToServerLog(rsp[a], a, 0);

					server_log_refresh = true;
				});

				server_cmd_buffer[0] = { '\0' };
				ImGui::SetKeyboardFocusHere(-1);
			}
		}
	#endif

		void UpdateAllAutomatic(bool first_update)
		{
			for (auto& a : automatic_plots)
				a->Update(this, first_update);
		}

		void OnFirstFrame()
		{
			LoadScripts();
			FlushAll(); // Make sure all scripts are loaded before we try to do any command

			// Initialise all automatic plots:
			UpdateAllAutomatic(true);
		}

		void BuildAutomaticDatas()
		{
			AddAutomaticData<AutomaticPlot>("Login Loading Time (ms)", "select Stats | get \":0 LOADING Login:\"", "type uint | unit ms", "Avg: %.0f ms (% u Samples)\nMin: %.0f ms\nMax : %.0f ms");
			AddAutomaticData<AutomaticPlot>("First Loading Time (ms)", "select Stats | get \":1 LOADING \"", "type uint | unit ms", "Avg: %.0f ms (%u Samples)\nMin: %.0f ms\nMax: %.0f ms");
			AddAutomaticData<AutomaticPlot>("Global Loading Time (ms)", "select Stats | get \":[0-9]* LOADING \"", "type uint | unit ms", "Avg: %.0f ms (%u Samples)\nMin: %.0f ms\nMax: %.0f ms");

			AddAutomaticData<AutomaticPlot>("Login Memory Usage (MB)", "select Stats | get \":0 MEMORY:\"", "type uint | unit MB", "Avg: %.0f MB (%u Samples)\nMin: %.0f MB\nMax: %.0f MB");
			AddAutomaticData<AutomaticPlot>("First Loading Memory Usage (MB)", "select Stats | get \":1 MEMORY:\"", "type uint | unit MB", "Avg: %.0f MB (%u Samples)\nMin: %.0f MB\nMax: %.0f MB");
			AddAutomaticData<AutomaticPlot>("Global Loading Memory Usage (MB)", "select Stats | get \":[0-9]* MEMORY:\"", "type uint | unit MB", "Avg: %.0f MB (%u Samples)\nMin: %.0f MB\nMax: %.0f MB");

			AddAutomaticData<AutomaticPlot>("Login Allocation Count", "select Stats | get \":0 ALLOCS:\"", "type uint", "Avg: %.0f (%u Samples)\nMin: %.0f\nMax: %.0f");
			AddAutomaticData<AutomaticPlot>("First Loading Allocation Count", "select Stats | get \":1 ALLOCS:\"", "type uint", "Avg: %.0f (%u Samples)\nMin: %.0f\nMax: %.0f");
			AddAutomaticData<AutomaticPlot>("Global Loading Allocation Count", "select Stats | get \":[0-9]* ALLOCS:\"", "type uint", "Avg: %.0f (%u Samples)\nMin: %.0f\nMax: %.0f");

			AddAutomaticData<AutomaticPlot>("Bandwidth", "select Stats | get \":[0-9]* BANDWIDTH:\"", "type float", "Avg: %.3f Kb/sec (%u Samples)\nMin: %.3f Kb/sec\nMax: %.3f Kb/sec");
			AddAutomaticData<AutomaticHistory>("VRAM", "select Hardware | get \":VRAM:\"", "type uint | unit MB", "%u MB\n(%u Users)");
			AddAutomaticData<AutomaticHistory>("Logical Threads", "select Hardware | get \":Cores:\"", "type uint", "%u Threads\n(%u Users)");
		}

	public:
		Impl()
		{
			request_buffer[0] = { '\0' };

			const auto address = Engine::Telemetry::Get().GetAddress();
			Connect(address.first, address.second);
			BuildAutomaticDatas();
		}

		void OnUpdate(float elapsed_time, bool is_visible)
		{
			Flush();

			if (first_frame)
				return;

			if (IsBusy())
			{
				has_automatic_timer = false;
			}
			else if (!has_automatic_timer)
			{
				has_automatic_timer = true;
				automatic_timer = HighResTimer::Get().GetTime();
			}
			else if (is_visible && automatic_plots.size() > 0 && HighResTimer::Get().GetTime() - automatic_timer > 1000 * 60)
			{
				automatic_plots[next_automatic_update]->Update(this, false);
				next_automatic_update = (next_automatic_update + 1) % automatic_plots.size();
				automatic_timer = HighResTimer::Get().GetTime();
			}
		}

		bool OnRender(float elapsed_time, bool visible)
		{
			if (!visible)
				return false;

			if (!IsConnectedAsync())
				return true;

			if (ImGui::Begin("Telemetry", &visible))
			{
				if (!first_frame && ImGui::IsWindowAppearing())
					UpdateAllAutomatic(false);

				if (first_frame)
				{
					first_frame = false;
					OnFirstFrame();
				}

				if (ImGui::BeginTabBar("##MainTabs"))
				{
					const bool show_default = ImGui::BeginTabItem("Default", nullptr, ImGuiTabItemFlags_NoTooltip);

					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(20.0f * ImGui::GetFontSize());
						ImGui::TextWrapped("Default Graphs");
						ImGui::Separator();
						ImGui::TextWrapped("Shows a set of default graphs. All graphs show the data of the past 7 days and refresh automatically. Shown data might have an delay of multiple minutes.");
						ImGui::PopTextWrapPos();
						ImGui::EndTooltip();
					}

					if (show_default)
					{
						RenderDefaultPlots();
						ImGui::EndTabItem();
					}

					const bool show_manual = ImGui::BeginTabItem("Manual Search", nullptr, ImGuiTabItemFlags_NoTooltip);

					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(20.0f * ImGui::GetFontSize());
						ImGui::TextWrapped("Manual Search");
						ImGui::Separator();
						ImGui::TextWrapped("Interface to issue custom searches. Use 'help' to get a list of available commands.");
						ImGui::PopTextWrapPos();
						ImGui::EndTooltip();
					}

					if (show_manual)
					{
						RenderManualSearch();
						ImGui::EndTabItem();
					}

				#ifdef DEVELOPMENT_CONFIGURATION
					const bool show_server = ImGui::BeginTabItem("Server", nullptr, ImGuiTabItemFlags_NoTooltip);

					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::PushTextWrapPos(20.0f * ImGui::GetFontSize());
						ImGui::TextWrapped("Server CLI");
						ImGui::Separator();
						ImGui::TextWrapped("Allows direct access to the Redis database and all Redis commands. Careful! Only use this if you know what you are doing!\nThis tab is only visible on Development builds.");
						ImGui::PopTextWrapPos();
						ImGui::EndTooltip();
					}

					if (show_server)
					{
						RenderServerCLI();
						ImGui::EndTabItem();
					}
				#endif

					if (IsBusy())
					{
						ImGui::TabItemButton(IMGUI_ICON_NETWORK, ImGuiTabItemFlags_Trailing);
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip(IMGUI_ICON_NETWORK " Data is currently being fetched from the server");
					}

					ImGui::EndTabBar();
				}
			}

			ImGui::End();

			return visible;
		}
	#endif
	};

	TelemetryPlugin::TelemetryPlugin() : Plugin(REDIS_ONLY(Feature_Visible)), impl(nullptr)
	{
		AddAction("Toggle", L'M', Input::Modifier_Ctrl, [this]() { SetVisible(!IsVisible()); });
	}

	TelemetryPlugin::~TelemetryPlugin() { REDIS_ONLY(delete impl;) }
	void TelemetryPlugin::OnUpdate(float elapsed_time) 
	{
	#ifdef REDIS_ENABLED
		if (impl->IsConnectedAsync())
			impl->OnUpdate(elapsed_time, IsVisible());
		else
			SetEnabled(false);
	#endif
	}

	void TelemetryPlugin::OnRender(float elapsed_time) { REDIS_ONLY(SetVisible(impl->OnRender(elapsed_time, IsVisible()))); }
	void TelemetryPlugin::OnEnabled() 
	{
	#ifdef REDIS_ENABLED
		delete impl;
		impl = new Impl();
	#endif
	}

	void TelemetryPlugin::OnDisabled() 
	{
	#ifdef REDIS_ENABLED
		delete impl;
		impl = nullptr;
	#endif
	}
#endif
}
