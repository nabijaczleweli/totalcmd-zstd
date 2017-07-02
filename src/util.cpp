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
#include <sys/stat.h>
#include <fstream>
#include <sys/types.h>
#include <zstd/zstd.h>
#ifndef _WIN32
#include <unistd.h>
#endif


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
