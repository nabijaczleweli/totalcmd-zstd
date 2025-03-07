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

#include <shellapi.h>

#include "wcxhead.h"
#define WCX_PLUGIN_EXPORTS
#include "wcxapi.h"

#include "config.hpp"
#include "pack_data.hpp"
#include "quickscope_wrapper.hpp"
#include "unpack_data.hpp"
#include "util.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <zstd/zstd.h>


/// SetProcessDataProc() is called with garbage hArcData, so we have to save this statically.
static tProcessDataProc data_process_callback = nullptr;


extern "C" WCX_API HANDLE STDCALL OpenArchive(tOpenArchiveData * ArchiveData) {
	auto out = new(std::nothrow) unarchive_data(ArchiveData->ArcName);
	if(!out)
		ArchiveData->OpenResult = E_NO_MEMORY;
	return out;
}

extern "C" WCX_API int STDCALL ReadHeader(HANDLE hArcData, tHeaderData * HeaderData) {
	if(static_cast<unarchive_data *>(hArcData)->file_shown)
		return E_END_ARCHIVE;

	const auto archtime = file_mod_time(static_cast<unarchive_data *>(hArcData)->file.c_str());
	int unpacked_size   = static_cast<unarchive_data *>(hArcData)->unpacked_size();
	if(unpacked_size == 0)
		unpacked_size = -1;

	memset(HeaderData, 0, sizeof(*HeaderData));
	std::strcpy(HeaderData->ArcName, static_cast<unarchive_data *>(hArcData)->derive_archive_name());
	std::strcpy(HeaderData->FileName, static_cast<unarchive_data *>(hArcData)->derive_contained_name().c_str());
	HeaderData->PackSize                                = static_cast<unarchive_data *>(hArcData)->size();
	HeaderData->UnpSize                                 = unpacked_size;
	HeaderData->FileTime                                = totalcmd_time(*std::localtime(&archtime));
	static_cast<unarchive_data *>(hArcData)->file_shown = true;
	return 0;
}

extern "C" WCX_API int STDCALL ProcessFile(HANDLE hArcData, int Operation, char * DestPath, char * DestName) {
	switch(Operation) {
		case PK_SKIP:
			break;
		case PK_TEST:
			break;
		case PK_EXTRACT: {
			std::string path = DestName;
			if(DestPath)
				path.insert(0, DestPath);

			std::ofstream out(path, std::ios::binary);
			return static_cast<unarchive_data *>(hArcData)->unpack(out);
		} break;
	}

	return 0;
}

extern "C" WCX_API int STDCALL CloseArchive(HANDLE hArcData) {
	delete static_cast<unarchive_data *>(hArcData);
	return 0;
}


extern "C" WCX_API int STDCALL PackFiles(char * PackedFile, char *, char * SrcPath, char * AddList, int Flags) {
	if(/*Flags & PK_PACK_SAVE_PATHS ||*/ Flags & PK_PACK_ENCRYPT)
		return E_NOT_SUPPORTED;

	const auto in_buf_size  = ZSTD_CStreamInSize();
	const auto out_buf_size = ZSTD_CStreamOutSize();
	auto in_buffer          = std::make_unique<char[]>(in_buf_size);
	auto out_buffer         = std::make_unique<char[]>(out_buf_size);
	ZSTD_inBuffer in_buf{in_buffer.get(), in_buf_size, 0};
	ZSTD_outBuffer out_buf{out_buffer.get(), out_buf_size, 0};

	configuration cfg;
	auto ctx = ZSTD_createCStream();
	quickscope_wrapper ctx_dtor{[ctx]() { ZSTD_freeCStream(ctx); }};
	ZSTD_initCStream(ctx, cfg.compression_level);

	// We don't specify PK_CAPS_MULTIPLE in GetPackerCaps() so we'll only ever get one file in AddList.
	std::string path = SrcPath;
	path += AddList;

	{
		std::ifstream in(path, std::ios::binary);
		std::ofstream out(PackedFile, std::ios::binary);

		for(;;) {
			in.read(in_buffer.get(), in_buf_size);
			const auto read = in.gcount();
			if(read == 0)
				break;
			in_buf.size = read;
			in_buf.pos  = 0;

			const auto res = ZSTD_compressStream(ctx, &out_buf, &in_buf);

			if(ZSTD_isError(res))
				return E_ECREATE;

			out.write(out_buffer.get(), out_buf.pos);
			out_buf.pos = 0;

			if(data_process_callback)
				if(!data_process_callback(AddList, read))
					return E_EABORTED;
		}

		for(;;) {
			const auto res = ZSTD_endStream(ctx, &out_buf);
			if(ZSTD_isError(res))
				return E_ECREATE;

			out.write(out_buffer.get(), out_buf.pos);
			out_buf.pos = 0;

			if(res == 0)
				break;
		}
	}

	if(Flags & PK_PACK_MOVE_FILES)
		std::remove(path.c_str());

	return 0;
}

extern "C" WCX_API void STDCALL SetChangeVolProc(HANDLE, tChangeVolProc) {}

extern "C" WCX_API void STDCALL SetProcessDataProc(HANDLE hArcData, tProcessDataProc pProcessDataProc) {
	if(hArcData == nullptr || hArcData == reinterpret_cast<HANDLE>(0xffffffffffffffffull))
		data_process_callback = pProcessDataProc;
	else
		static_cast<unarchive_data *>(hArcData)->data_process_callback = pProcessDataProc;
}

extern "C" WCX_API BOOL STDCALL CanYouHandleThisFile(char * FileName) {
	return verify_magic(FileName);
}

extern "C" WCX_API int STDCALL GetPackerCaps() {
	return PK_CAPS_NEW | PK_CAPS_OPTIONS | PK_CAPS_MEMPACK | PK_CAPS_BY_CONTENT | PK_CAPS_SEARCHTEXT;
}

extern "C" WCX_API void STDCALL ConfigurePacker(HWND Parent, HINSTANCE) {
	configuration{};  // Force creation if nonexistant

	const auto totalcmd_cfg_f = totalcmd_config_file();
	std::string totalcmd_editor;
	if(!totalcmd_cfg_f.empty())
		totalcmd_editor = totalcmd_config_get_editor(totalcmd_cfg_f.c_str());
	auto cfg_f = config_file();

	if(totalcmd_editor.empty() ||
	   ShellExecute(Parent, "open", totalcmd_editor.c_str(), cfg_f.c_str(), nullptr, SW_SHOWDEFAULT) != reinterpret_cast<HINSTANCE>(0x2A))  // 0x2A = OK
		if(ShellExecute(Parent, "edit", cfg_f.c_str(), nullptr, nullptr, SW_SHOWDEFAULT) != reinterpret_cast<HINSTANCE>(0x2A))
			if(totalcmd_cfg_f.empty() ||
			   ShellExecute(Parent, "open", totalcmd_cfg_f.c_str(), nullptr, nullptr, SW_SHOWDEFAULT) != reinterpret_cast<HINSTANCE>(0x2A)) {
				cfg_f[cfg_f.find_last_of("\\/")] = '\0';
				if(ShellExecute(Parent, "explore", cfg_f.c_str(), nullptr, nullptr, SW_SHOWDEFAULT) != reinterpret_cast<HINSTANCE>(0x2A)) {
					cfg_f[cfg_f.find('\0')] = '/';
					MessageBox(Parent, ("Failed to open and/or navigate to configuration file\"" + cfg_f + "\".").c_str(), "totalcmd-zstd plugin configuration",
					           MB_ICONWARNING | MB_OK);
				}
			}
}

extern "C" WCX_API HANDLE STDCALL StartMemPack(int, char *) {
	// This has the added benefit of 0=error, so we'll never NPE
	return new(std::nothrow) archive_data;
}

extern "C" WCX_API int STDCALL PackToMem(HANDLE hMemPack, char * BufIn, int InLen, int * Taken, char * BufOut, int OutLen, int * Written, int) {
	if(InLen) {
		const auto out = static_cast<archive_data *>(hMemPack)->add_data(BufIn, InLen, BufOut, OutLen);
		if(out.first)
			return E_EWRITE;
		else
			std::tie(*Taken, *Written) = out.second;
	} else {
		const auto [errored, finished, written] = static_cast<archive_data *>(hMemPack)->finish(BufOut, OutLen);
		if(errored)
			return E_EWRITE;
		else if(finished) {
			std::tie(*Taken, *Written) = std::make_pair(0, 0);
			return MEMPACK_DONE;
		} else
			std::tie(*Taken, *Written) = std::make_pair(0, written);
	}
	return MEMPACK_OK;
}

extern "C" WCX_API int STDCALL DoneMemPack(HANDLE hMemPack) {
	delete static_cast<archive_data *>(hMemPack);
	return 0;
}

extern "C" WCX_API int STDCALL GetBackgroundFlags(void) {
	return BACKGROUND_UNPACK | BACKGROUND_PACK | BACKGROUND_MEMPACK;
}
