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
#include <windows.h>

#include "data.hpp"
#include "quickscope_wrapper.hpp"
#include <memory>
#include <wcxhead.h>
#include <zstd/zstd.h>


archive_data::archive_data(const char * fname) : log("t:/totalcmd-zstd.log", std::ios::app), file(fname), file_shown(false), file_len(0), unpacked_len(0) {}

const char * archive_data::derive_archive_name() const {
	return file.c_str() + file.find_last_of("\\/") + 1;
}

std::string archive_data::derive_contained_name() const {
	auto start              = file.find_last_of("\\/") + 1;
	auto end                = file.rfind('.');
	auto is_zstd_extension  = file.find("zstd", end) != std::string::npos;
	auto is_tzstd_extension = file.find("tzstd", end) != std::string::npos;

	if(is_tzstd_extension)
		return file.substr(start, end + 1 - start) + "tar";
	else if(is_zstd_extension)
		return file.substr(start, end - start);
	else
		return file.substr(start);
}

std::size_t archive_data::size() {
	if(!file_len)
		file_len = std::ifstream(file, std::ios::ate | std::ios::binary).tellg();
	return file_len;
}

std::size_t archive_data::unpacked_size() {
	if(!unpacked_len) {
		load_contents();
		unpacked_len = ZSTD_getFrameContentSize(contents.data(), file_len);
		if(unpacked_len == ZSTD_CONTENTSIZE_UNKNOWN || unpacked_len == ZSTD_CONTENTSIZE_ERROR)
			unpacked_len = 0;
	}
	return unpacked_len;
}

int archive_data::unpack(std::ostream & into) {
	load_contents();
	unpacked_len = 0;

	auto out_buffer = std::make_unique<char[]>(ZSTD_DStreamOutSize());
	ZSTD_inBuffer in_buf{contents.data(), size(), 0};
	ZSTD_outBuffer out_buf{out_buffer.get(), ZSTD_DStreamOutSize(), 0};

	auto ctx = ZSTD_createDStream();
	quickscope_wrapper ctx_dtor{[ctx]() { ZSTD_freeDStream(ctx); }};
	ZSTD_initDStream(ctx);

	for(;;) {
		const auto res = ZSTD_decompressStream(ctx, &out_buf, &in_buf);
		if(ZSTD_isError(res))
			return E_BAD_ARCHIVE;

		into.write(out_buffer.get(), out_buf.pos);
		unpacked_len += out_buf.pos;
		if(data_process_callback)
			if(!data_process_callback(&file[0], out_buf.pos))
				return E_EABORTED;
		out_buf.pos = 0;

		if(res == 0)
			break;
	}

	return 0;
}

void archive_data::load_contents() {
	if(!contents.empty())
		return;

	contents.resize(size());
	std::ifstream(file, std::ios::binary).read(contents.data(), file_len);
}
