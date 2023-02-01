#pragma once

#include "Visual/Device/Constants.h"

namespace Device
{
	namespace CommandLine
	{
		class StartupCommand
		{
		public:
			static LPWSTR  GetCommandLine();
			static LPWSTR* CommandLineToArgv(LPCWSTR lpCmdLine, int* pNumArgs);
		};
	}
}