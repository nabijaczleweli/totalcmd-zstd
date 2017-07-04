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
#include "data.hpp"
#include "quickscope_wrapper.hpp"
#include "util.hpp"
#include <cstdio>
#include <cstring>
#include <memory>
#include <zstd/zstd.h>


/// SetProcessDataProc() is called with garbage hArcData, so we have to save this statically.
static tProcessDataProc data_process_callback = nullptr;


extern "C" WCX_API HANDLE STDCALL OpenArchive(tOpenArchiveData * ArchiveData) {
	auto out = new archive_data(ArchiveData->ArcName);
	out->log << "OpenArchive(" << out << "): ArcName=\"" << ArchiveData->ArcName << "\"; OpenMode=" << ArchiveData->OpenMode
	         << "; OpenResult=" << ArchiveData->OpenResult << '\n';
	return out;
}

extern "C" WCX_API int STDCALL ReadHeader(HANDLE hArcData, tHeaderData * HeaderData) {
	if(static_cast<archive_data *>(hArcData)->file_shown)
		return E_END_ARCHIVE;

	const auto archtime = file_mod_time(static_cast<archive_data *>(hArcData)->file.c_str());
	int unpacked_size   = static_cast<archive_data *>(hArcData)->unpacked_size();
	if(unpacked_size == 0)
		unpacked_size = -1;

	memset(HeaderData, 0, sizeof(*HeaderData));
	std::strcpy(HeaderData->ArcName, static_cast<archive_data *>(hArcData)->derive_archive_name());
	std::strcpy(HeaderData->FileName, static_cast<archive_data *>(hArcData)->derive_contained_name().c_str());
	HeaderData->PackSize = static_cast<archive_data *>(hArcData)->size();
	HeaderData->UnpSize  = unpacked_size;
	HeaderData->FileTime = totalcmd_time(*std::localtime(&archtime));
	static_cast<archive_data *>(hArcData)->log << "ReadHeader(" << hArcData << "): PackSize=" << HeaderData->PackSize << "; UnpSize=" << HeaderData->UnpSize
	                                           << "; FileTime=" << HeaderData->FileTime << "; FileName=" << HeaderData->FileName << '\n';
	static_cast<archive_data *>(hArcData)->file_shown = true;
	return 0;
}

extern "C" WCX_API int STDCALL ProcessFile(HANDLE hArcData, int Operation, char * DestPath, char * DestName) {
	switch(Operation) {
		case PK_SKIP:
			static_cast<archive_data *>(hArcData)->log << "ProcessFile(" << hArcData << ", Operation=PK_SKIP);" << std::endl;
			break;
		case PK_TEST:
			static_cast<archive_data *>(hArcData)->log << "ProcessFile(" << hArcData << ", Operation=PK_TEST);" << std::endl;
			break;
		case PK_EXTRACT: {
			std::string path = DestName;
			if(DestPath)
				path.insert(0, DestPath);

			static_cast<archive_data *>(hArcData)->log << "ProcessFile(" << hArcData << ", Operation=PK_EXTRACT, path=\"" << path
			                                           << "\"): size=" << static_cast<archive_data *>(hArcData)->size() << ";" << std::endl;
			std::ofstream out(path, std::ios::binary);
			return static_cast<archive_data *>(hArcData)->unpack(out);
		} break;
	}

	return 0;
}

extern "C" WCX_API int STDCALL CloseArchive(HANDLE hArcData) {
	static_cast<archive_data *>(hArcData)->log << "CloseArchive(" << hArcData << ");\n\n";
	delete static_cast<archive_data *>(hArcData);
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

		for(int i = 0;; ++i) {
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
		static_cast<archive_data *>(hArcData)->data_process_callback = pProcessDataProc;
}

extern "C" WCX_API BOOL STDCALL CanYouHandleThisFile(char * FileName) {
	return verify_magic(FileName);
}

extern "C" WCX_API int STDCALL GetPackerCaps() {
	return PK_CAPS_NEW | PK_CAPS_OPTIONS | PK_CAPS_BY_CONTENT | PK_CAPS_SEARCHTEXT;
}

extern "C" WCX_API void STDCALL ConfigurePacker(HWND Parent, HINSTANCE) {
	configuration{};  // Force creation if nonexistant

	auto cfg_f = config_file();
	if(ShellExecute(Parent, "edit", cfg_f.c_str(), nullptr, nullptr, SW_SHOWDEFAULT) != reinterpret_cast<HINSTANCE>(0x2A)) {  // 0x2A = OK
		cfg_f[cfg_f.find_last_of("\\/")] = '\0';
		if(ShellExecute(Parent, "explore", cfg_f.c_str(), nullptr, nullptr, SW_SHOWDEFAULT) != reinterpret_cast<HINSTANCE>(0x2Aa)) {
			cfg_f[cfg_f.find('\0')] = '/';
			MessageBox(Parent, ("Failed to open and/or navigate to configuration file\"" + cfg_f + "\".").c_str(), "totalcmd-zstd plugin configuration",
			           MB_ICONWARNING | MB_OK);
		}
	}
}
