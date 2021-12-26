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

#pragma once

#include <QString>
#include <QPair>
#include <QList>

#include <obs-module.h>
#include "vst-plugin-callbacks.hpp"

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__linux__)
#include <xcb/xcb.h>
#endif

bool masterCanDo_static(const char* what);

class VstEffectInfo {
public:
	QString       id;
	QString       fileName;
	QString       filePath;
	QString       effectName;
	QString       vendorString;
	bool          shell;
	int32_t       pluginId;

	VstEffectInfo();
	explicit VstEffectInfo(obs_data_t* data);
	virtual ~VstEffectInfo();
	bool less(const VstEffectInfo& o) const;
	obs_data_t *toObsData() const;
};

class VstScanner {
	typedef QPair<QString, QString> NamePathInfo;
	typedef QList<NamePathInfo>     NamePathList;
	typedef QList<VstEffectInfo>    VstPluginList;

#ifdef __APPLE__
	typedef CFBundleRef LibraryHandle;
#elif WIN32
	typedef HINSTANCE   LibraryHandle;
#elif __linux__
	typedef void*       LibraryHandle;
#endif

	QList<VstEffectInfo> effectList;

	VstScanner();

	NamePathList  getLibraryList() const;
	void          readVstInfo(VstPluginList* pluginList, const NamePathInfo& info) const;
	bool          isVstFile(AEffect* effect) const;
	bool          isShellPlugin(AEffect* effect) const;
	void          enumerateShell(VstPluginList* pluginList, AEffect* plugin, const QString &vendorString, const NamePathInfo& info) const;
	void          closeEffect(AEffect* effect) const;
	QString       makeEffectId(const VstEffectInfo &info) const;
	
#pragma region PLATFORM SPECIFIC
	LibraryHandle loadLibrary(const QString& file) const;
	void          closeLibrary(LibraryHandle& handle) const;
	vstPluginMain getVstMain(LibraryHandle& handle) const;
#pragma endregion

	void saveList() const;
	bool loadList();
	void mkdirs() const;

public:
	static VstScanner *getInstance();
	virtual ~VstScanner();
	void                        rescan();
	void                        init();
	const QList<VstEffectInfo> *getEffects() const;
	const VstEffectInfo        *getEffectById(const QString& id) const;
	const VstEffectInfo        *getEffectByPath(const QString& path) const;
};

