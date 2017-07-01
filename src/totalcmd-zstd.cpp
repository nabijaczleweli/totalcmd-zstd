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

#include "wcxhead.h"
#define WCX_PLUGIN_EXPORTS
#include "wcxapi.h"

#include "data.hpp"
#include "util.hpp"
#include <cstring>


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

	memset(HeaderData, 0, sizeof(*HeaderData));
	std::strcpy(HeaderData->ArcName, static_cast<archive_data *>(hArcData)->derive_archive_name());
	std::strcpy(HeaderData->FileName, static_cast<archive_data *>(hArcData)->derive_contained_name().c_str());
	HeaderData->PackSize = static_cast<archive_data *>(hArcData)->size();
	HeaderData->UnpSize  = static_cast<archive_data *>(hArcData)->unpacked_size();
	HeaderData->FileTime = totalcmd_time(*std::localtime(&archtime));
	static_cast<archive_data *>(hArcData)->log << "ReadHeader(" << hArcData << "): PackSize=" << HeaderData->PackSize << "; UnpSize=" << HeaderData->UnpSize
	                                           << "; FileTime=" << HeaderData->FileTime << "; FileName=" << HeaderData->FileName << '\n';
	static_cast<archive_data *>(hArcData)->file_shown = true;
	return 0;
}

extern "C" WCX_API int STDCALL ProcessFile(HANDLE hArcData, int Operation, char * DestPath, char * DestName) {
	switch(Operation) {
		case PK_SKIP:
			static_cast<archive_data *>(hArcData)->log << "ProcessFile(" << hArcData << ", Operation=PK_SKIP);\n";
			break;
		case PK_TEST:
			static_cast<archive_data *>(hArcData)->log << "ProcessFile(" << hArcData << ", Operation=PK_TEST);\n";
			break;
		case PK_EXTRACT: {
			std::string path = DestName;
			if(DestPath)
				path.insert(0, DestPath);

			static_cast<archive_data *>(hArcData)->log << "ProcessFile(" << hArcData << ", Operation=PK_EXTRACT, path=\"" << path << "\");\n";
			std::ofstream out(path, std::ios::binary);
			static_cast<archive_data *>(hArcData)->unpack(out);
		} break;
	}

	return 0;
}

extern "C" WCX_API int STDCALL CloseArchive(HANDLE hArcData) {
	static_cast<archive_data *>(hArcData)->log << "CloseArchive(" << hArcData << ");\n\n";
	delete static_cast<archive_data *>(hArcData);
	return 0;
}

extern "C" WCX_API void STDCALL SetChangeVolProc(HANDLE hArcData, tChangeVolProc pChangeVolProc1) {
	static_cast<archive_data *>(hArcData)->log << "SetChangeVolProc(" << hArcData << ", pChangeVolProc1=" << reinterpret_cast<void *>(pChangeVolProc1) << ");\n";
}

extern "C" WCX_API void STDCALL SetProcessDataProc(HANDLE hArcData, tProcessDataProc pProcessDataProc1) {
	static_cast<archive_data *>(hArcData)->log << "SetProcessDataProc(" << hArcData << ", pProcessDataProc1=" << reinterpret_cast<void *>(pProcessDataProc1)
	                                           << ");\n";
}

extern "C" WCX_API BOOL STDCALL CanYouHandleThisFile(char * FileName) {
	std::ofstream("t:/asdf.log", std::ios::app) << "CanYouHandleThisFile(" << FileName << ")=" << verify_magic(FileName) << ";" << std::endl;
	return verify_magic(FileName);
}

extern "C" WCX_API int STDCALL GetPackerCaps() {
	std::ofstream("t:/fdsa.log", std::ios::app) << "GetPackerCaps();" << std::endl;
	return PK_CAPS_BY_CONTENT;
}
