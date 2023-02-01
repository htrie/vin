#include "StartupCommand.h"

#include "Common/Utility/WindowsHeaders.h"

#include <string>

namespace Utility
{
	// must use this instead of ::GetCommandLine() to enable argv overriding from the file
	// Implemented in Common/Utility/CommandLine.cpp
	const wchar_t *GetCommandLineEx( );
}

std::wstring commandLineArguments;
std::wstring extraCommandLineArguments;

void SetExtraCommandLineArguments(const wchar_t* commandLineArguments)
{
	assert(extraCommandLineArguments.empty());

	//
	// Without this copy occasionally the extraCommandLineArguments variable will get assigned (incorrectly) some data header
	// as indicated by the string starting with the escape sequence L"\x1" even though the commandLineArguments variable was 
	// was correct.  Not really 100% sure the cause of this, assuming the constructor wstring was doing something unexpected since
	// since after the assignment the commandLineArguments variable would also be incorret.
	//
	wchar_t commandLineParams[MAX_PATH];
	const size_t len = std::wcslen(commandLineArguments);
	std::wmemcpy( commandLineParams, commandLineArguments, len );
	commandLineParams[len] = 0; // EOS
	extraCommandLineArguments.assign(commandLineParams);
}


LPWSTR Device::CommandLine::StartupCommand::GetCommandLine()
{
	commandLineArguments = Utility::GetCommandLineEx( );
	commandLineArguments += (L" " + extraCommandLineArguments);
	return const_cast<LPWSTR>(commandLineArguments.c_str());
}

LPWSTR* Device::CommandLine::StartupCommand::CommandLineToArgv(LPCWSTR lpCmdLine, int* pNumArgs)
{
#if !defined( CONSOLE ) && !defined( __APPLE__ ) && !defined( ANDROID )
	return ::CommandLineToArgvW(lpCmdLine, pNumArgs);
#else
	PWCHAR* argv;
	PWCHAR  _argv;
	ULONG   len;
	ULONG   argc;
	WCHAR   a;
	ULONG   i, j;

	BOOL  in_QM;
	BOOL  in_TEXT;
	BOOL  in_SPACE;

	len = wcslen(lpCmdLine);
	i = ((len + 2) / 2) * sizeof(PVOID) + sizeof(PVOID);

	argv = (PWCHAR*)malloc(i + (len + 2) * sizeof(WCHAR));
	_argv = (PWCHAR)(((PUCHAR)argv) + i);

	argc = 0;
	argv[argc] = _argv;
	in_QM = FALSE;
	in_TEXT = FALSE;
	in_SPACE = TRUE;
	i = 0;
	j = 0;

	while (a = lpCmdLine[i]) {
		if (in_QM) {
			if (a == '\"') {
				in_QM = FALSE;
			}
			else {
				_argv[j] = a;
				j++;
			}
		}
		else {
			switch (a) {
			case '\"':
				in_QM = TRUE;
				in_TEXT = TRUE;
				if (in_SPACE) {
					argv[argc] = _argv + j;
					argc++;
				}
				in_SPACE = FALSE;
				break;
			case ' ':
			case '\t':
			case '\n':
			case '\r':
				if (in_TEXT) {
					_argv[j] = '\0';
					j++;
				}
				in_TEXT = FALSE;
				in_SPACE = TRUE;
				break;
			default:
				in_TEXT = TRUE;
				if (in_SPACE) {
					argv[argc] = _argv + j;
					argc++;
				}
				_argv[j] = a;
				j++;
				in_SPACE = FALSE;
				break;
			}
		}
		i++;
	}
	_argv[j] = '\0';
	argv[argc] = NULL;

	(*pNumArgs) = argc;
	return argv;
#endif
}
