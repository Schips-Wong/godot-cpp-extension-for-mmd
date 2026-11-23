#include "vmd_importer.h"

#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/classes/resource_saver.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/packed_string_array.hpp"

#include "mmd_animation.h"
#include "vmd_format.h"

using namespace godot;

void VmdImporter::_bind_methods() {
}

String VmdImporter::_get_importer_name() const {
	return "bullet3.vmd";
}

String VmdImporter::_get_visible_name() const {
	return "MMD VMD Motion";
}

PackedStringArray VmdImporter::_get_recognized_extensions() const {
	PackedStringArray extensions;
	extensions.push_back("vmd");
	return extensions;
}

String VmdImporter::_get_save_extension() const {
	return "res";
}

String VmdImporter::_get_resource_type() const {
	return "MMDAnimation";
}

float VmdImporter::_get_priority() const {
	return 1.0f;
}

int32_t VmdImporter::_get_import_order() const {
	return 0;
}

int32_t VmdImporter::_get_preset_count() const {
	return 1;
}

String VmdImporter::_get_preset_name(int32_t p_preset_index) const {
	return "Default";
}

TypedArray<Dictionary> VmdImporter::_get_import_options(const String &p_path, int32_t p_preset_index) const {
	TypedArray<Dictionary> options;
	return options;
}

bool VmdImporter::_get_option_visibility(const String &p_path, const StringName &p_option_name, const Dictionary &p_options) const {
	return true;
}

Error VmdImporter::_import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options, const TypedArray<String> &p_platform_variants, const TypedArray<String> &p_gen_files) const {
	const PackedByteArray data = FileAccess::get_file_as_bytes(p_source_file);
	if (data.is_empty()) {
		ERR_PRINT(vformat("VMD import could not read '%s'.", p_source_file));
		return ERR_FILE_CANT_READ;
	}

	VmdAnimationData parsed;
	const Error parse_error = parse_vmd(data, parsed);
	if (parse_error != OK) {
		ERR_PRINT(vformat("VMD import failed for '%s' (error %d).", p_source_file, static_cast<int32_t>(parse_error)));
		return parse_error;
	}

	Ref<MMDAnimation> animation;
	animation.instantiate();
	animation->set_data(data);
	animation->set_duration(parsed.duration);
	animation->set_model_name(parsed.model_name);
	animation->set_motion_name(p_source_file.get_file().get_basename());

	const String save_path = p_save_path + String(".") + _get_save_extension();
	const Error error = ResourceSaver::get_singleton()->save(animation, save_path, ResourceSaver::FLAG_CHANGE_PATH);
	if (error != OK) {
		ERR_PRINT(vformat("VMD import could not write '%s' (error %d).", save_path, static_cast<int32_t>(error)));
	}
	return error;
}
