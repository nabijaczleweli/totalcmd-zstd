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


#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "quickscope_wrapper.hpp"
#include "unpack_data.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <memory>
#include <wcxhead.h>
#include <zstd/zstd.h>


unarchive_data::unarchive_data(const char * fname) : file(fname), file_shown(false), file_len(0), unpacked_len(0) {}

const char * unarchive_data::derive_archive_name() const {
	return file.c_str() + file.find_last_of("\\/") + 1;
}

std::string unarchive_data::derive_contained_name() const {
	std::string lowercase_file;
	lowercase_file.reserve(file.size());
	std::transform(file.begin(), file.end(), std::back_inserter(lowercase_file), [](char c) { return std::tolower(c); });

	auto start              = file.find_last_of("\\/") + 1;
	auto end                = file.rfind('.');
	auto is_zstd_extension  = lowercase_file.find("zst", end) != std::string::npos;
	auto is_tzstd_extension = lowercase_file.find("tzst", end) != std::string::npos;

	if(is_tzstd_extension)
		return file.substr(start, end + 1 - start) + "tar";
	else if(is_zstd_extension)
		return file.substr(start, end - start);
	else
		return file.substr(start);
}

std::size_t unarchive_data::size() {
	if(!file_len)
		file_len = std::ifstream(file, std::ios::ate | std::ios::binary).tellg();
	return file_len;
}

std::size_t unarchive_data::unpacked_size() {
	if(!unpacked_len) {
		char frame_header[ZSTD_FRAMEHEADERSIZE_MAX];
		std::ifstream(file, std::ios::binary).read(frame_header, sizeof frame_header / sizeof *frame_header);
		unpacked_len = ZSTD_getFrameContentSize(frame_header, sizeof frame_header / sizeof *frame_header);
		if(unpacked_len == ZSTD_CONTENTSIZE_UNKNOWN || unpacked_len == ZSTD_CONTENTSIZE_ERROR)
			unpacked_len = 0;
	}
	return unpacked_len;
}

int unarchive_data::unpack(std::ostream & into) {
	unpacked_len = 0;

	const auto in_buf_size = ZSTD_DStreamInSize();
	auto in_buffer         = std::make_unique<char[]>(in_buf_size);
	auto out_buffer        = std::make_unique<char[]>(ZSTD_DStreamOutSize());
	ZSTD_inBuffer in_buf{in_buffer.get(), in_buf_size, in_buf_size};
	ZSTD_outBuffer out_buf{out_buffer.get(), ZSTD_DStreamOutSize(), 0};

	auto ctx = ZSTD_createDStream();
	quickscope_wrapper ctx_dtor{[ctx]() { ZSTD_freeDStream(ctx); }};
	ZSTD_initDStream(ctx);

	std::ifstream from(file, std::ios::binary);

	for(;;) {
		const auto overlap = in_buf.size - in_buf.pos;
		if(overlap)
			std::memmove(in_buffer.get(), in_buffer.get() + in_buf.pos, overlap);

		from.read(in_buffer.get() + overlap, in_buf_size - overlap);
		in_buf.size = from.gcount() + overlap;
		in_buf.pos  = 0;

		const auto res = ZSTD_decompressStream(ctx, &out_buf, &in_buf);
		if(ZSTD_isError(res))
			return E_BAD_ARCHIVE;

		into.write(out_buffer.get(), out_buf.pos);
		unpacked_len += out_buf.pos;
		if(data_process_callback)
			if(!data_process_callback(&file[0], out_buf.pos))
				return E_EABORTED;

		if(res == 0 || out_buf.pos == 0)
			break;

		out_buf.pos = 0;
	}

	return 0;
}
