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

#include "headers/VSTScanner.h"
#include "headers/vst-plugin-callbacks.hpp"
#include "aeffectx.h"

#include <QDir>
#include <QDirIterator>

bool masterCanDo_static(const char* what)
{
	if (!strcmp("shellCategory", what)) {
		return true;
	}
	return false;
}

static intptr_t
hostCallback_static(AEffect* effect, int32_t opcode, int32_t index, intptr_t value, void* ptr, float opt)
{
	switch (opcode) {
	case audioMasterVersion:
		return (intptr_t)2400;

	case audioMasterCanDo:
		return masterCanDo_static(static_cast<const char*>(ptr));

	case audioMasterCurrentId:
		return 0;

	default:
		return 0;
	}
}

VstScanner::VstScanner() {}

VstScanner::~VstScanner() {}

VstScanner* VstScanner::getInstance() {
	static VstScanner instance;
	return &instance;
}

void VstScanner::rescan()
{
	NamePathList libraries = getLibraryList();
	VstPluginList newEffectList;

	for(auto it = libraries.constBegin(); it != libraries.constEnd(); ++it) {
		blog(LOG_WARNING, "reading %s", (*it).second.toUtf8().constData());
		readVstInfo(&newEffectList, *it);
	}

	for (auto it = newEffectList.constBegin(); it != newEffectList.constEnd(); ++it) {
		blog(LOG_INFO,
		     "VST: %s (%s) %d %s",
		     it->effectName.toUtf8().data(),
		     it->vendorString.toUtf8().data(),
		     it->pluginId,
		     it->fileName.toUtf8().data());
	}

	std::stable_sort(newEffectList.begin(), newEffectList.end(), [](auto a, auto b) -> bool { return a.less(b); });
	effectList = newEffectList;
}

const QList<VstEffectInfo>* VstScanner::getEffects() const {
	return &effectList;
}

const VstEffectInfo* VstScanner::getEffectById(const QString& id) const {
	for (auto it = effectList.constBegin(); it != effectList.constEnd(); ++it) {
		if (it->id == id) {
			return &(*it);
		}
	}
	return nullptr;
}

void VstScanner::readVstInfo(VstPluginList* pluginList, const NamePathInfo& info) const
{
	LibraryHandle handle = nullptr;
	AEffect*      effect = nullptr;
	try {
		handle = loadLibrary(info.second);
		auto mainEntry   = getVstMain(handle);
		effect      = mainEntry(hostCallback_static);
		if (!effect) {
			blog(LOG_WARNING,
			     "Initialization of plugin failed, "
			     "%s",
			     info.second.toUtf8().constData());
			return;
		}

		if (!isVstFile(effect)) {
			return;
		}

		QByteArray buffer(64, '\0');
		effect->dispatcher(effect, effGetVendorString, 0, 0, buffer.data(), 0);
		QString vendorString = QString::fromUtf8(buffer);

		if (isShellPlugin(effect)) {
			enumerateShell(pluginList, effect, vendorString, info);
		} else {
			VstEffectInfo entry;
			entry.vendorString = vendorString;
			entry.filePath     = info.second;
			entry.fileName     = info.first;
			
			buffer.fill('\0');
			effect->dispatcher(effect, effGetEffectName, 0, 0, buffer.data(), 0);
			entry.effectName = QString::fromUtf8(buffer);

			entry.id = makeEffectId(entry);
			pluginList->push_back(entry);
		}
	} catch (...) {
	}
	closeEffect(effect);
	closeLibrary(handle);
}

bool VstScanner::isVstFile(AEffect* plugin) const
{
	if (!plugin) {
		throw std::exception("Got null instead of effect instance.");
	}

	if (plugin->magic != kEffectMagic) {
		blog(LOG_WARNING, "VST Plug-in's magic number is bad");
		return false;
	}

	return true;
}

bool VstScanner::isShellPlugin(AEffect* plugin) const
{
	auto category = plugin->dispatcher(plugin, effGetPlugCategory, 0, 0, nullptr, 0);
	return (category == kPlugCategShell);
}

void VstScanner::enumerateShell(VstPluginList* pluginList, AEffect* plugin, const QString &vendorString, const NamePathInfo& info) const {
	QByteArray buffer(64, '\0');

	while (true) {
		VstEffectInfo entry;
		entry.filePath     = info.second;
		entry.fileName     = info.first;
		entry.vendorString = vendorString;
		entry.shell   = true;

		entry.pluginId = plugin->dispatcher(plugin, effShellGetNextPlugin, 0, 0, buffer.data(), 0);
		if (entry.pluginId == 0 || buffer[0] == '\0') {
			break;
		}
		entry.effectName = QString::fromUtf8(buffer);

		entry.id = makeEffectId(entry);
		pluginList->push_back(entry);
	}
}

void VstScanner::closeEffect(AEffect* effect) const {
	if (!effect) {
		return;
	}
	effect->dispatcher(effect, effMainsChanged, 0, 0, nullptr, 0);
	effect->dispatcher(effect, effClose, 0, 0, nullptr, 0.0f);
}

QString VstScanner::makeEffectId(const VstEffectInfo &info) const {
	return QString("%1:%2").arg(
		info.filePath,
		QString::number(info.pluginId));
}

VstScanner::NamePathList VstScanner::getLibraryList() const
{
	// TODO: just taken from obs-vst.cpp
	// could be cleaned up into platform specific files
	QStringList dir_list;

#ifdef __APPLE__
	dir_list << "/Library/Audio/Plug-Ins/VST/"
	         << "~/Library/Audio/Plug-ins/VST/";
#elif WIN32
#ifndef _WIN64
	HANDLE hProcess = GetCurrentProcess();

	BOOL isWow64;
	IsWow64Process(hProcess, &isWow64);

	if (!isWow64) {
#endif
		dir_list << qEnvironmentVariable("ProgramFiles") + "/Steinberg/VstPlugins/"
		         << qEnvironmentVariable("CommonProgramFiles") + "/Steinberg/Shared Components/"
		         << qEnvironmentVariable("CommonProgramFiles") + "/VST2"
		         << qEnvironmentVariable("CommonProgramFiles") + "/Steinberg/VST2"
		         << qEnvironmentVariable("CommonProgramFiles") + "/VSTPlugins/"
		         << qEnvironmentVariable("ProgramFiles") + "/VSTPlugins/";
#ifndef _WIN64
	} else {
		dir_list << qEnvironmentVariable("ProgramFiles(x86)") + "/Steinberg/VstPlugins/"
		         << qEnvironmentVariable("CommonProgramFiles(x86)") + "/Steinberg/Shared Components/"
		         << qEnvironmentVariable("CommonProgramFiles(x86)") + "/VST2"
		         << qEnvironmentVariable("CommonProgramFiles(x86)") + "/VSTPlugins/"
		         << qEnvironmentVariable("ProgramFiles(x86)") + "/VSTPlugins/";
	}
#endif
#elif __linux__
	// If the user has set the VST_PATH environmental
	// variable, then use it. Else default to a list
	// of common locations.
	QString vstPathEnv(getenv("VST_PATH"));
	if (!vstPathEnv.isNull()) {
		dir_list.append(vstPathEnv.split(":"));
	} else {
		QString home(getenv("HOME"));
		// Choose the most common locations
		// clang-format off
		dir_list << "/usr/lib/vst/"
		         << "/usr/lib/lxvst/"
		         << "/usr/lib/linux_vst/"
		         << "/usr/lib64/vst/"
		         << "/usr/lib64/lxvst/"
		         << "/usr/lib64/linux_vst/"
		         << "/usr/local/lib/vst/"
		         << "/usr/local/lib/lxvst/"
		         << "/usr/local/lib/linux_vst/"
		         << "/usr/local/lib64/vst/"
		         << "/usr/local/lib64/lxvst/"
		         << "/usr/local/lib64/linux_vst/"
		         << home + "/.vst/"
		         << home + "/.lxvst/";
		// clang-format on
	}
#endif

	QStringList filters;

#ifdef __APPLE__
	filters << "*.vst";
#elif WIN32
	filters << "*.dll";
#elif __linux__
	filters << "*.so"
	        << "*.o";
#endif

	NamePathList vst_list;

	// Read all plugins into a list...
	for (int a = 0; a < dir_list.size(); ++a) {
		QDir search_dir(dir_list[a]);
		search_dir.setNameFilters(filters);
		QDirIterator it(search_dir, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			QString path = it.next();
			QString name = it.fileName();

#ifdef __APPLE__
			name.remove(".vst", Qt::CaseInsensitive);
#elif WIN32
			name.remove(".dll", Qt::CaseInsensitive);
#elif __linux__
			name.remove(".so", Qt::CaseInsensitive);
			name.remove(".o", Qt::CaseInsensitive);
#endif

			vst_list << NamePathInfo(name, path);
		}
	}

	// Now sort list alphabetically (still case-sensitive though).
	// std::stable_sort(vst_list.begin(), vst_list.end(), std::less<QString>());
	return vst_list;
}


