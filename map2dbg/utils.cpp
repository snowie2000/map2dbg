#include "stdafx.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <clocale>
#include <fstream>
#include <locale>
#include <vector>
#include <direct.h>

#include "utils.h"

std::string ChangeFileExt(const std::string& filename, const std::string& newExtension) {
	size_t pos = filename.find_last_of(".", std::string::npos);
	if (pos != std::string::npos) {
		return filename.substr(0, pos) + newExtension;
	} else {
		return filename;
	}
}

std::string ExtractFileName(const std::string& filename) {
	size_t pos = filename.find_last_of("\\", std::string::npos);
	if( pos != std::string::npos ) {
		return filename.substr(pos + 1, std::string::npos);
	} else {
		return filename;
	}
}

bool FileExists(const std::string& filename) {
	struct __stat64 buf;
	int result;

	result = _stat64( filename.c_str(), &buf );
	return result == 0;
}

std::vector<std::string> LoadLines(const std::string& filename) {
	std::vector<std::string> result;
	
	std::ifstream ifs(filename.c_str());
	while( ifs.good() ) {
		char buffer[4096];
		ifs.getline( buffer, 4096 );
		result.push_back( buffer );
	}

	return result;
}

std::string from_tchar(_TCHAR* value) {
#ifdef UNICODE
	std::locale const loc("");
	std::wstring text = std::wstring(value);
	return ws2s(text);

	wchar_t const* from = text.c_str();
	std::size_t const len = text.size();
	std::vector<char> buffer(len + 1);
	std::use_facet<std::ctype<wchar_t> >(loc).narrow(from, from + len, '_', &buffer[0]);
	return std::string(&buffer[0], &buffer[len]);
#else
	return std::string(value);
#endif
}

std::string trim(const std::string &str0)
{
	std::string str = str0;
	size_t at2 = str.find_last_not_of(" \t\r\n\0\a\b\f\v");
	size_t at1 = str.find_first_not_of(" \t\r\n\0\a\b\f\v");
	if (at2 != std::string::npos) str.erase(at2+1);
	if (at1 != std::string::npos) str.erase(0,at1);
	return str;
}


#ifdef UNICODE
#define T_toupper	towupper
#define T_getdcwd	_wgetdcwd
#define T_strlen	wcslen
#define T_strcpy	wcscpy
#define T_strcat	wcscat
#define T_strstr	wcsstr
#define T_strtod	wcstod
#define T_strrchr	wcsrchr
#define T_unlink	_wremove
#define T_main		wmain
#define SARG		"%S"
#define T_stat		_wstat
#else
#define T_toupper	toupper
#define T_getdcwd	_getdcwd
#define T_strlen	strlen
#define T_strcpy	strcpy
#define T_strcat	strcat
#define T_strstr	strstr
#define T_strtod	strtod
#define T_strrchr	strrchr
#define T_unlink	unlink
#define T_main		main
#define SARG		"%s"
#define T_stat		stat
#endif

void makefullpath(TCHAR* pdbname)
{
	TCHAR* pdbstart = pdbname;
	TCHAR fullname[260];
	TCHAR* pfullname = fullname;

	int drive = 0;
	if (pdbname[0] && pdbname[1] == ':')
	{
		if (pdbname[2] == '\\' || pdbname[2] == '/')
			return;
		drive = T_toupper(pdbname[0]) - 'A' + 1;
		pdbname += 2;
	}
	else
	{
		drive = _getdrive();
	}

	if (*pdbname != '\\' && *pdbname != '/')
	{
		T_getdcwd(drive, pfullname, sizeof(fullname) / sizeof(fullname[0]) - 2);
		pfullname += T_strlen(pfullname);
		if (pfullname[-1] != '\\')
			*pfullname++ = '\\';
	}
	else
	{
		*pfullname++ = 'a' - 1 + drive;
		*pfullname++ = ':';
	}
	T_strcpy(pfullname, pdbname);
	T_strcpy(pdbstart, fullname);

	for (TCHAR*p = pdbstart; *p; p++)
		if (*p == '/')
			*p = '\\';

	// remove relative parts "./" and "../"
	while (TCHAR* p = T_strstr(pdbstart, TEXT("\\.\\")))
		T_strcpy(p, p + 2);

	while (TCHAR* p = T_strstr(pdbstart, TEXT("\\..\\")))
	{
		for (TCHAR* q = p - 1; q >= pdbstart; q--)
			if (*q == '\\')
			{
				T_strcpy(q, p + 3);
				break;
			}
	}
}

std::wstring s2ws(const std::string& s)
{
	_bstr_t t = s.c_str();
	wchar_t* pwchar = (wchar_t*)t;
	std::wstring result = pwchar;
	return result;
}

std::string ws2s(const std::wstring& ws)
{
	_bstr_t t = ws.c_str();
	char* pchar = (char*)t;
	std::string result = pchar;
	return result;
}

void fatal(const char *message, ...)
{
	va_list argptr;
	va_start(argptr, message);
	vprintf(message, argptr);
	va_end(argptr);
	printf("\n");
	exit(1);
}