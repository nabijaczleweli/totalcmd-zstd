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


#pragma once


#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <zstd/zstd.h>
#include <wcxhead.h>


class unarchive_data {
public:
	bool file_shown;
	tProcessDataProc data_process_callback;

	FILETIME mtime;
	std::uint64_t size;

private:
	std::string file;
	HANDLE fstream;
	char iobuf[(ZSTD_BLOCKSIZE_MAX + 10) * 2];  // ZSTD_DStreamInSize() is ZSTD_BLOCKSIZE_MAX + ZSTD_blockHeaderSize (private 3)
	OVERLAPPED iobuf_overlapped;
	std::size_t iobuf_len;
	bool iobuf_consumed, iobuf_eof;
	std::optional<std::size_t> unpacked_len;

	void await_iobuf();


public:
	unarchive_data(const char * fname);
	~unarchive_data();
	unarchive_data(const unarchive_data &) = delete;
	unarchive_data(unarchive_data &&) = delete;

	const char * derive_archive_name() const;
	std::string derive_contained_name() const;
	std::size_t unpacked_size();
	int unpack(std::ostream & into);
};
