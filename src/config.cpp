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


#include "config.hpp"
#include "util.hpp"
#include <algorithm>
#include <fstream>
#include <jsonpp/parser.hpp>
#include <map>
#include <zstd/zstd.h>


configuration::configuration() {
	std::ifstream in(config_file());
	if(in.is_open()) {
		json::value cfg;
		try {
			json::parse(in, cfg);
		} catch(...) {
			return;
		}

		auto && clvl = cfg["compression‐level"];
		if(clvl.is<std::size_t>())
			compression_level = std::min(clvl.as<std::size_t>(), static_cast<std::size_t>(ZSTD_maxCLevel()));
	}
}

configuration::~configuration() {
	std::size_t max_clevel = ZSTD_maxCLevel();
	compression_level = std::min(compression_level, max_clevel);

	std::ofstream out(config_file());
	std::map<std::string, json::value> obj{
	    {"compression‐level", static_cast<double>(compression_level)},
	    {"compression-level-comment",
	     "Integer between 0 (store) and " + std::to_string(max_clevel) + " (ultra). Values ≥20 should be used with caution, as they require more memory."},
	    {"‌", ""},
	    {"‌totalcmd-zstd", "version " TOTALCMD_ZSTD_VERSION ", found at https://github.com/nabijaczleweli/totalcmd-zstd"},
	    {"‌‌zstd", "version " ZSTD_VERSION_STRING ", found at https://github.com/facebook/zstd"},
	    {"‌‌‌jsonpp", "version " JSONPP_VERSION ", found at https://github.com/Rapptz/jsonpp"},
	    {"‌‌‌‌whereami-cpp", "version " WHEREAMI_CPP_VERSION ", found at https://github.com/nabijaczleweli/whereami-cpp"},
	};
	json::dump(out, obj);
}
