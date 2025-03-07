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


#include "unpack_data.hpp"
#include <algorithm>
#include <cstring>
#include <memory>


unarchive_data::unarchive_data(const char * fname)
      : file_shown(false), mtime({}), size(0), file(fname), fstream(CreateFileA(fname, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                                                                nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr)),
        iobuf_overlapped({}) {
	if(fstream != INVALID_HANDLE_VALUE) {
		GetFileTime(fstream, nullptr, nullptr, &mtime);
		GetFileSizeEx(fstream, reinterpret_cast<LARGE_INTEGER *>(&size));
		if(!ReadFile(fstream, iobuf, sizeof(iobuf), nullptr, &iobuf_overlapped))
			if(GetLastError() != ERROR_IO_PENDING) {
				CloseHandle(fstream);
				fstream = INVALID_HANDLE_VALUE;
			}
	}
}

unarchive_data::~unarchive_data() {
	CloseHandle(fstream);
}

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

void unarchive_data::await_iobuf() {
	DWORD read;
	iobuf_eof = !GetOverlappedResult(fstream, &iobuf_overlapped, &read, true) && GetLastError() == ERROR_HANDLE_EOF;
	iobuf_len = read;


	std::uint64_t offset = (static_cast<std::uint64_t>(iobuf_overlapped.OffsetHigh) << 32) | static_cast<std::uint64_t>(iobuf_overlapped.Offset);
	offset += iobuf_len;
	iobuf_overlapped.OffsetHigh = offset >> 32;
	iobuf_overlapped.Offset     = offset & 0xFFFFFFFF;
}

std::size_t unarchive_data::unpacked_size() {
	if(!unpacked_len) {
		if(fstream == INVALID_HANDLE_VALUE)
			return 0;

		await_iobuf();

		static_assert(sizeof(iobuf) >= ZSTD_FRAMEHEADERSIZE_MAX);
		unpacked_len = ZSTD_getFrameContentSize(iobuf, iobuf_len);
		if(*unpacked_len == ZSTD_CONTENTSIZE_UNKNOWN || *unpacked_len == ZSTD_CONTENTSIZE_ERROR)
			*unpacked_len = 0;
	}
	return *unpacked_len;
}

int unarchive_data::unpack(std::ostream & into) {
	if(fstream == INVALID_HANDLE_VALUE)
		return E_EREAD;

	if(iobuf_consumed) {
		iobuf_overlapped.OffsetHigh = iobuf_overlapped.Offset = 0;
		if(!ReadFile(fstream, iobuf, sizeof(iobuf), nullptr, &iobuf_overlapped))
			if(GetLastError() != ERROR_IO_PENDING)
				return E_EREAD;
	}
	if(!unpacked_len || iobuf_consumed)
		await_iobuf();
	iobuf_consumed = true;


	unpacked_len = 0;

	const auto in_buf_size  = sizeof(iobuf) * 2;
	const auto out_buf_size = ZSTD_DStreamOutSize() * 2;  // we have * 2 in iobuf
	auto in_buffer          = std::make_unique<char[]>(in_buf_size);
	auto out_buffer         = std::make_unique<char[]>(out_buf_size);
	ZSTD_inBuffer in_buf{in_buffer.get(), 0, 0};

	std::unique_ptr<ZSTD_DStream, decltype(&ZSTD_freeDStream)> ctx{ZSTD_createDStream(), ZSTD_freeDStream};
	ZSTD_initDStream(ctx.get());

	for(;;) {
		std::memmove(const_cast<void *>(in_buf.src), static_cast<const char *>(in_buf.src) + in_buf.pos, in_buf.size - in_buf.pos);
		in_buf.size = in_buf.size - in_buf.pos;
		in_buf.pos  = 0;
		std::memcpy(const_cast<char *>(static_cast<const char *>(in_buf.src)) + in_buf.size, iobuf, iobuf_len);
		in_buf.size += iobuf_len;

		if(!iobuf_eof)
			if(!ReadFile(fstream, iobuf, sizeof(iobuf), nullptr, &iobuf_overlapped))
				if(GetLastError() != ERROR_IO_PENDING)
					return E_EREAD;

		while(in_buf.size != in_buf.pos) {
			ZSTD_outBuffer out_buf{out_buffer.get(), out_buf_size, 0};

			const auto res = ZSTD_decompressStream(ctx.get(), &out_buf, &in_buf);
			if(ZSTD_isError(res))
				return E_BAD_ARCHIVE;

			into.write(static_cast<char *>(out_buf.dst), out_buf.pos);
			if(!into)
				return E_EWRITE;
			*unpacked_len += out_buf.pos;

			if(data_process_callback && !data_process_callback(file.data(), out_buf.pos))
				return E_EABORTED;

			if(out_buf.pos < out_buf.size)
				break;
		}

		if(iobuf_eof)
			break;
		await_iobuf();
	}
	for(;;) {
		ZSTD_outBuffer out_buf{out_buffer.get(), out_buf_size, 0};

		const auto res = ZSTD_decompressStream(ctx.get(), &out_buf, &in_buf);
		if(ZSTD_isError(res))
			return E_BAD_ARCHIVE;

		into.write(static_cast<char *>(out_buf.dst), out_buf.pos);
		if(!into)
			return E_EWRITE;
		*unpacked_len += out_buf.pos;

		if(data_process_callback && !data_process_callback(file.data(), out_buf.pos))
			return E_EABORTED;

		if(in_buf.pos == in_buf.size && out_buf.pos == 0)
			break;
	}

	return 0;
}
