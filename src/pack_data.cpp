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


#include "pack_data.hpp"
#include "config.hpp"


archive_data::archive_data() : ctx(ZSTD_createCStream(), ZSTD_freeCStream) {
	configuration cfg;
	ZSTD_initCStream(ctx.get(), cfg.compression_level);
}

std::pair<bool, std::pair<std::size_t, std::size_t>> archive_data::add_data(const void * in, std::size_t in_len, void * out, std::size_t out_len) {
	ZSTD_inBuffer in_buf{in, in_len, 0};
	ZSTD_outBuffer out_buf{out, out_len, 0};

	const auto res = ZSTD_compressStream(ctx.get(), &out_buf, &in_buf);
	return {static_cast<bool>(ZSTD_isError(res)), {in_buf.pos, out_buf.pos}};
}

std::tuple<bool, bool, std::size_t> archive_data::finish(void * out, std::size_t out_len) {
	ZSTD_outBuffer out_buf{out, out_len, 0};
	const auto res = ZSTD_endStream(ctx.get(), &out_buf);
	return {static_cast<bool>(ZSTD_isError(res)), res == 0, out_buf.pos};
}
