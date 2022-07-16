#pragma once
#include <string>

#define MakeErrorInfo(what) \
	ErrorInfo(what, __FILE__, std::to_string(__LINE__));

struct ErrorInfo
{
	std::string what;
	std::string file;
	std::string line;

	ErrorInfo(std::string what, std::string file, std::string line)
	{
		this->what = what;
		this->file = file;
		this->line = line;
	}
};