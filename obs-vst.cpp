/*****************************************************************************
Copyright (C) 2016-2017 by Colin Edwards.
Additional Code Copyright (C) 2016-2017 by c3r1c3 <c3r1c3@nevermindonline.com>

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

#include "headers/VSTPlugin.h"
#include "headers/VSTScanner.h"

#define OPEN_VST_SETTINGS "open_vst_settings"
#define CLOSE_VST_SETTINGS "close_vst_settings"
#define OPEN_WHEN_ACTIVE_VST_SETTINGS "open_when_active_vst_settings"

#define PLUG_IN_NAME obs_module_text("VstPlugin")
#define OPEN_VST_TEXT obs_module_text("OpenPluginInterface")
#define CLOSE_VST_TEXT obs_module_text("ClosePluginInterface")
#define OPEN_WHEN_ACTIVE_VST_TEXT obs_module_text("OpenInterfaceWhenActive")

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-vst", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "VST 2.x Plug-in filter";
}

static bool open_editor_button_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	VSTPlugin *vstPlugin = (VSTPlugin *)data;

	QMetaObject::invokeMethod(vstPlugin, "openEditor");

	obs_property_set_visible(obs_properties_get(props, OPEN_VST_SETTINGS), false);
	obs_property_set_visible(obs_properties_get(props, CLOSE_VST_SETTINGS), true);

	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(property);
	UNUSED_PARAMETER(data);

	return true;
}

static bool close_editor_button_clicked(obs_properties_t *props, obs_property_t *property, void *data)
{
	VSTPlugin *vstPlugin = (VSTPlugin *)data;

	QMetaObject::invokeMethod(vstPlugin, "closeEditor");

	obs_property_set_visible(obs_properties_get(props, OPEN_VST_SETTINGS), true);
	obs_property_set_visible(obs_properties_get(props, CLOSE_VST_SETTINGS), false);

	UNUSED_PARAMETER(property);

	return true;
}

static const char *vst_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return PLUG_IN_NAME;
}

static void vst_destroy(void *data)
{
	VSTPlugin *vstPlugin = (VSTPlugin *)data;
	QMetaObject::invokeMethod(vstPlugin, "closeEditor");
	vstPlugin->deleteLater();
}

static void vst_update(void *data, obs_data_t *settings)
{
	VSTPlugin *vstPlugin = (VSTPlugin *)data;
	const VstEffectInfo *effectInfo;

	vstPlugin->openInterfaceWhenActive = obs_data_get_bool(settings, OPEN_WHEN_ACTIVE_VST_SETTINGS);

	const char *pluginPath = obs_data_get_string(settings,   "plugin_path");
	const char *pluginId     = obs_data_get_string(settings, "plugin_id");

	auto        scanner      = VstScanner::getInstance();
	
	if (pluginId && strcmp(pluginId, "")) {
		effectInfo = scanner->getEffectById(pluginId);
	} else {
		effectInfo = scanner->getEffectByPath(pluginPath);
		if (effectInfo) {
			obs_data_set_string(settings, "plugin_id", effectInfo->id.toUtf8().data());
		}
	}

	if (effectInfo == nullptr) {
		return;
	}
	vstPlugin->loadEffectFromInfo(*effectInfo);

	const char *chunkData = obs_data_get_string(settings, "chunk_data");
	if (chunkData && strlen(chunkData) > 0) {
		vstPlugin->setChunk(std::string(chunkData));
	}
}

static void *vst_create(obs_data_t *settings, obs_source_t *filter)
{
	VSTPlugin *vstPlugin = new VSTPlugin(filter);
	vst_update(vstPlugin, settings);

	return vstPlugin;
}

static void vst_save(void *data, obs_data_t *settings)
{
	VSTPlugin *vstPlugin = (VSTPlugin *)data;

	obs_data_set_string(settings, "chunk_data", vstPlugin->getChunk().c_str());
}

static struct obs_audio_data *vst_filter_audio(void *data, struct obs_audio_data *audio)
{
	VSTPlugin *vstPlugin = (VSTPlugin *)data;
	vstPlugin->process(audio);

	/*
	 * OBS can only guarantee getting the filter source's parent and own name
	 * in this call, so we grab it and return the results for processing
	 * by the EditorWidget.
	 */
	vstPlugin->getSourceNames();

	return audio;
}

static void fill_out_plugins(obs_property_t *list) {
	obs_property_list_add_string(list, "{Please select a plug-in}", nullptr);

	auto scanner = VstScanner::getInstance();
	auto effects  = scanner->getEffects();
	for (auto it = effects->constBegin(); it != effects->constEnd(); ++it) {
		QString label = QString("%1 (%2)").arg(it->effectName, it->vendorString);
		QString id    = it->id;
		obs_property_list_add_string(list, label.toUtf8().data(), id.toUtf8().data());
	}
}

static obs_properties_t *vst_properties(void *data)
{
	VSTPlugin *       vstPlugin = (VSTPlugin *)data;
	obs_properties_t *props     = obs_properties_create();
	obs_property_t *  list      = obs_properties_add_list(
                props, "plugin_id", PLUG_IN_NAME, OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	fill_out_plugins(list);

	obs_properties_add_button(props, OPEN_VST_SETTINGS, OPEN_VST_TEXT, open_editor_button_clicked);
	obs_properties_add_button(props, CLOSE_VST_SETTINGS, CLOSE_VST_TEXT, close_editor_button_clicked);

	if (vstPlugin->isEditorOpen()) {
		obs_property_set_visible(obs_properties_get(props, OPEN_VST_SETTINGS), false);
	} else {
		obs_property_set_visible(obs_properties_get(props, CLOSE_VST_SETTINGS), false);
	}

	obs_properties_add_bool(props, OPEN_WHEN_ACTIVE_VST_SETTINGS, OPEN_WHEN_ACTIVE_VST_TEXT);

	return props;
}

bool obs_module_load(void)
{
	struct obs_source_info vst_filter = {};
	vst_filter.id                     = "vst_filter";
	vst_filter.type                   = OBS_SOURCE_TYPE_FILTER;
	vst_filter.output_flags           = OBS_SOURCE_AUDIO;
	vst_filter.get_name               = vst_name;
	vst_filter.create                 = vst_create;
	vst_filter.destroy                = vst_destroy;
	vst_filter.update                 = vst_update;
	vst_filter.filter_audio           = vst_filter_audio;
	vst_filter.get_properties         = vst_properties;
	vst_filter.save                   = vst_save;

	VstScanner::getInstance()->init();

	obs_register_source(&vst_filter);
	return true;
}
