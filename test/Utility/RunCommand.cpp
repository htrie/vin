#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <string>

/// RunCommandGetStdOut
/// runs the given command in a win32 shell
/// @param command the command to execute, including all arguments
/// @param output if successfull, on return output contains all of stdout
/// @returns true if the command was executed, else false
bool RunCommandGetStdOut(const char *command, std::string &output)
{
	SECURITY_ATTRIBUTES saAttr;
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	// Create a pipe for the child process's STDOUT.
	HANDLE child_stdout_read = 0;
	HANDLE child_stdout_write = 0;
	if (!CreatePipe(&child_stdout_read, &child_stdout_write, &saAttr, 0))
		return false;

	PROCESS_INFORMATION proc_info; 
	ZeroMemory(&proc_info, sizeof(PROCESS_INFORMATION));

	// specify the STDIN and STDOUT handles for redirection.
	STARTUPINFOA start_info;
	ZeroMemory(&start_info, sizeof(STARTUPINFOA));
	start_info.cb = sizeof(STARTUPINFOA);
	start_info.hStdOutput = child_stdout_write;
	start_info.dwFlags |= STARTF_USESTDHANDLES;

	// Create the child process, supplying our redirection to anonymous pipes
	BOOL success = CreateProcessA(NULL, 
		(LPSTR)command,     // command line 
		NULL,          // process security attributes 
		NULL,          // primary thread security attributes 
		TRUE,          // handles are inherited 
		0,             // creation flags 
		NULL,          // use parent's environment 
		NULL,          // use parent's current directory 
		&start_info,  // STARTUPINFO pointer 
		&proc_info);  // receives PROCESS_INFORMATION
	if (!success)
	{
		CloseHandle(child_stdout_read);
		CloseHandle(child_stdout_write);
		return false;
	}

	// Close the write end of the pipe before reading from the 
	// read end of the pipe, to control child process execution.
	// The pipe is assumed to have enough buffer space to hold the
	// data the child process has already written to it.
	if (!CloseHandle(child_stdout_write))
	{
		CloseHandle(child_stdout_read);
		return false;
	}

	// read stdout from child process till it is empty
	DWORD num_read = 0;
	const int BUFSIZE = 10*1024;
	CHAR buffer[BUFSIZE]; 
	for (;;)
	{ 
		BOOL success = ReadFile(child_stdout_read, buffer, BUFSIZE - 1, &num_read, NULL);
		if (!success || num_read == 0)
			break; 
		buffer[num_read] = 0;
		output += buffer;
	}
	CloseHandle(child_stdout_read);
	return true;
}

//EOF
