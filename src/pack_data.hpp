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


#include <memory>
#include <tuple>
#include <utility>
#include <zstd/zstd.h>


class archive_data {
private:
	std::unique_ptr<ZSTD_CStream, decltype(&ZSTD_freeCStream)> ctx;


public:
	archive_data();

	/// Pack data from the specified buffer into the specified buffer.
	///
	/// Return value: {errorred, {bytes taken, bytes written}}.
	std::pair<bool, std::pair<std::size_t, std::size_t>> add_data(const void * in, std::size_t in_len, void * out, std::size_t out_len);

	/// Flush data and finish the archive.
	///
	/// You might need to call this multiple times if the output buffer is too small.
	///
	/// Return value: {errorred, finished, bytes written}.
	std::tuple<bool, bool, std::size_t> finish(void * out, std::size_t out_len);
};
