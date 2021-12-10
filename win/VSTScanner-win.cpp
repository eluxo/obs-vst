/*****************************************************************************
Copyright (C) 2016-2017 by Colin Edwards.
Additional Code Copyright (C) 2016-2017 by c3r1c3 <c3r1c3@nevermindonline.com>
Additional Code Copyright (C) 2021 by eluxo <eluxo@eluxo.net>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*****************************************************************************/

#include "../headers/VSTScanner.h"
#include <util/platform.h>
#include <Windows.h>

VstScanner::LibraryHandle VstScanner::loadLibrary(const QString& file) const
{
	
	wchar_t* wpath;
	os_utf8_to_wcs_ptr(file.toUtf8().constData(), 0, &wpath);
	auto dllHandle = LoadLibraryW(wpath);
	bfree(wpath);
	if (dllHandle == nullptr) {

		DWORD errorCode = GetLastError();

		// Display the error message and exit the process
		if (errorCode == ERROR_BAD_EXE_FORMAT) {
			blog(LOG_WARNING,
			     "Could not open library, "
			     "wrong architecture.");
			throw std::exception("Invalid library architecture.");
		} else {
			blog(LOG_WARNING,
			     "Failed trying to load VST from '%s'"
			     ", error %d\n",
			     file.toUtf8().constData(),
			     GetLastError());
			throw std::exception("Error while loading VST");
		}
		
	}
	return dllHandle;
}

void VstScanner::closeLibrary(LibraryHandle& handle) const {
	if (!handle) {
		return;
	}
	FreeLibrary(handle);
}

vstPluginMain VstScanner::getVstMain(LibraryHandle& handle) const {
	if (!handle) {
		throw std::exception("Cannot find vst main method on null handle.");
	}

	vstPluginMain rc = nullptr;
	do {
		if (rc = (vstPluginMain)GetProcAddress(handle, "VSTPluginMain"))
			continue;
		if (rc = (vstPluginMain)GetProcAddress(handle, "VstPluginMain()"))
			continue;
		if (rc = (vstPluginMain)GetProcAddress(handle, "main"))
			continue;
		blog(LOG_WARNING, "Couldn't get a pointer to plug-in's main()");
		throw std::exception("No VST main in library");
	} while (false);

	return rc;
}
