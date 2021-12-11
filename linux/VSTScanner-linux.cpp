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

VstScanner::LibraryHandle VstScanner::loadLibrary(const QString& file) const
{
	LibraryHandle handle = os_dlopen(file.toUtf8().data());
	if (handle == nullptr) {
		blog(LOG_WARNING,
		     "Failed trying to load VST from '%s',"
		     "error %d\n",
		     file.toUtf8().data(),
		     errno);
		throw std::runtime_error("Error while loading VST");
	}
    return handle;
}

void VstScanner::closeLibrary(LibraryHandle& handle) const {
    if (!handle) {
        return;
    }
    
    os_dlclose(handle);
}

vstPluginMain VstScanner::getVstMain(LibraryHandle& handle) const {
	if (!handle) {
		throw std::runtime_error("Cannot find vst main method on null handle.");
	}

	vstPluginMain rc;

	if ((rc = (vstPluginMain)os_dlsym(handle, "VSTPluginMain"))) {
        return rc;
    }

	if ((rc = (vstPluginMain)os_dlsym(handle, "VstPluginMain()"))) {
        return rc;
    }

    if ((rc = (vstPluginMain)os_dlsym(handle, "main"))) {
        return rc;
	}

    blog(LOG_WARNING, "Couldn't get a pointer to plug-in's main()");
    throw std::runtime_error("No VST main in library");
}

