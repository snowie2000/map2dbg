#pragma once

#include <tchar.h>
#include <sstream>
#include <string>
#include <vector>
#include <comutil.h>
#pragma comment(lib, "comsuppw.lib")

std::string ChangeFileExt(const std::string& filename, const std::string& newExtension);
std::string ExtractFileName(const std::string& filename);
bool FileExists(const std::string& filename);
std::vector<std::string> LoadLines(const std::string& filename);

std::string from_tchar(_TCHAR* value);
std::string trim(const std::string &str);

template<class T>
std::string to_s(T  value) {
	std::stringstream str;
	str << value;
	std::string result;
	str >> result;
	return result;
}

void makefullpath(TCHAR* pdbname);
std::wstring s2ws(const std::string& s);
std::string ws2s(const std::wstring& ws);
void fatal(const char *message, ...);

