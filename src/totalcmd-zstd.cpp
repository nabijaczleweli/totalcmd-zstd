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

	// We don't specify PK_CAPS_MULTIPLE in GetPackerCaps() so we'll only ever get one file in AddList.
	std::string path = SrcPath;
	path += AddList;

	{
		archive_data ctx;
		std::ifstream in(path, std::ios::binary);
		std::ofstream out(PackedFile, std::ios::binary | std::ios::trunc);
		if(!out)
			return E_ECREATE;

		for(std::size_t in_buf_off = 0;;) {
			in.read(in_buffer.get() + in_buf_off, in_buf_size - in_buf_off);
			const auto read = in.gcount() + std::exchange(in_buf_off, 0);
			if(read == 0)
				break;

			const auto [errored, taken_written] = ctx.add_data(in_buffer.get(), read, out_buffer.get(), out_buf_size);
			const auto [taken, written]         = taken_written;
			if(errored)
				return E_EWRITE;

			out.write(out_buffer.get(), written);
			if(!out)
				return E_EWRITE;

			if(data_process_callback && !data_process_callback(AddList, taken))
				return E_EABORTED;

			in_buf_off = read - taken;
		}

		for(;;) {
			const auto [errored, finished, written] = ctx.finish(out_buffer.get(), out_buf_size);
			out.write(out_buffer.get(), written);
			if(!out)
				return E_EWRITE;
			if(errored)
				return E_EWRITE;
			else if(finished)
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

	std::string totalcmd_editor, totalcmd_editor_arguments;
	if(const auto totalcmd_cfg_f = totalcmd_config_file(); !totalcmd_cfg_f.empty())
		totalcmd_editor = totalcmd_config_get_editor(totalcmd_cfg_f.c_str());
	auto cfg_f = config_file();

	if(!totalcmd_editor.empty()) {
		if(totalcmd_editor.starts_with("\"\"")) {
			while(totalcmd_editor.back() != '\"')
				totalcmd_editor.pop_back();
			totalcmd_editor.pop_back();
			totalcmd_editor.erase(std::begin(totalcmd_editor));
		}

		bool in_quote{};
		for(std::size_t i = 0; i != totalcmd_editor.size(); ++i) {
			switch(totalcmd_editor[i]) {
				case ' ':
					if(!in_quote) {
						totalcmd_editor_arguments = totalcmd_editor.substr(i);
						totalcmd_editor.resize(i);
						goto break2;
					} else
						break;
				case '"':
					in_quote = !in_quote;
					break;
			}
		}
	break2:;

		if(auto subst = totalcmd_editor_arguments.find("%1"); subst != std::string::npos)
			totalcmd_editor_arguments.replace(subst, 2, cfg_f);
		else
			((totalcmd_editor_arguments += '\"') += cfg_f) += '\"';
	}

	if(totalcmd_editor.empty() ||
	   reinterpret_cast<uintptr_t>(ShellExecute(Parent, "open", totalcmd_editor.c_str(), totalcmd_editor_arguments.c_str(), nullptr, SW_SHOWDEFAULT)) <= 32)
		if(reinterpret_cast<uintptr_t>(ShellExecute(Parent, "edit", cfg_f.c_str(), nullptr, nullptr, SW_SHOWDEFAULT)) <= 32)
			MessageBox(Parent, ("Please edit file \"" + cfg_f + "\".").c_str(), "totalcmd-zstd plugin configuration", MB_ICONWARNING | MB_OK);
}

extern "C" WCX_API HANDLE STDCALL StartMemPack(int, char *) {
	// This has the added benefit of 0=error, so we'll never NPE
	return new(std::nothrow) archive_data;
}

extern "C" WCX_API int STDCALL PackToMem(HANDLE hMemPack, char * BufIn, int InLen, int * Taken, char * BufOut, int OutLen, int * Written, int) {
	if(InLen) {
		const auto [errored, taken_written] = static_cast<archive_data *>(hMemPack)->add_data(BufIn, InLen, BufOut, OutLen);
		std::tie(*Taken, *Written)          = taken_written;
		if(errored)
			return E_EWRITE;
	} else {
		const auto [errored, finished, written] = static_cast<archive_data *>(hMemPack)->finish(BufOut, OutLen);
		std::tie(*Taken, *Written)              = std::make_pair(0, written);
		if(errored)
			return E_EWRITE;
		else if(finished)
			return MEMPACK_DONE;
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
