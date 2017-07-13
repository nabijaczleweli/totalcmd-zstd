// The MIT License (MIT)

// Copyright (c) 2017 nabijaczleweli

// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "util.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ini.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <whereami++.hpp>
#include <zstd/zstd.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>


static std::string try_regval(HKEY genkey) {
	DWORD len;
	if(RegGetValue(genkey,
	               R"(Software\Ghisler\Total Commander)", "IniFileName", RRF_RT_REG_EXPAND_SZ | RRF_RT_REG_SZ, nullptr, nullptr, &len) != ERROR_SUCCESS)
		return {};

	std::string out(len - 1, '\0');
	if(RegGetValue(genkey,
	               R"(Software\Ghisler\Total Commander)", "IniFileName", RRF_RT_REG_EXPAND_SZ | RRF_RT_REG_SZ, nullptr, &out[0], &len) != ERROR_SUCCESS)
		return {};

	out.resize(len - 1);
	std::ofstream("t:/totalcmd-zstd.log", std::ios::app) << genkey << ": " << out.size() << "; \"" << out << "\""
	                                                     << "; " << len << "\n\n";
	return out;
}

static std::string maybe_expand(std::string whom) {
	if(whom.find("%") == std::string::npos)
		return whom;

	const auto size = ExpandEnvironmentStrings(whom.c_str(), nullptr, 0);
	if(size == 0)
		return whom;

	std::string out(size - 1, '\0');
	const auto written = ExpandEnvironmentStrings(whom.c_str(), &out[0], out.size() + 1);
	if(written == 0)
		return whom;

	return out;
}


int totalcmd_time(std::tm from) {
	return ((from.tm_year + 1900) - 1980) << 25 | (from.tm_mon + 1) << 21 | from.tm_mday << 16 | from.tm_hour << 11 | from.tm_min << 5 | from.tm_sec / 2;
}

std::time_t file_mod_time(const char * fname) {
	struct stat buf;
	stat(fname, &buf);
	return buf.st_mtime;
}

bool verify_magic(const char * fname) {
	char buf[4];
	std::ifstream(fname, std::ios::binary).read(buf, sizeof buf);
	return ZSTD_isFrame(buf, sizeof buf);
}

bool file_exists(const char * path) {
	auto f = std::fopen(path, "r");
	if(f)
		std::fclose(f);
	return f;
}

std::string config_file() {
	return whereami::module_dir() + "/totalcmd-zstd.json";
}

/// Based on http://www.ghisler.ch/wiki/index.php?title=Finding_the_paths_of_Total_Commander_files#Finding_the_TC_main_config_file
std::string totalcmd_config_file() {
	if(const auto cini = getenv("COMMANDER_INI"))
		return maybe_expand(cini);

	std::string out;
	if(!(out = try_regval(HKEY_CURRENT_USER)).empty())
		return maybe_expand(out);
	if(!(out = try_regval(HKEY_LOCAL_MACHINE)).empty())
		return maybe_expand(out);

	out = maybe_expand(R"(%Windir%\wincmd.ini)");
	if(file_exists(out.c_str()))
		return out;
	out = maybe_expand(R"(%AppData%\Ghisler\wincmd.ini)");
	if(file_exists(out.c_str()))
		return out;

	return {};
}

std::string totalcmd_config_get_editor(const char * totalcmd_cfg_f) {
	std::string editor;
	if(ini_parse(totalcmd_cfg_f,
	             [](void * user, const char * section, const char * name, const char * value) -> int {
		             if(std::strcmp(section, "Configuration") == 0 && std::strcmp(name, "Editor") == 0)
			             *static_cast<std::string *>(user) = value;
		             return true;
		           },
	             &editor) != 0)
		return {};
	return maybe_expand(editor);
}
