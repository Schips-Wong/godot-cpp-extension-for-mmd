#include "pmx_importer.h"

#include "godot_cpp/classes/editor_plugin.hpp"
#include "godot_cpp/classes/resource_saver.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/packed_string_array.hpp"

#include "godot_cpp/classes/file_access.hpp"

#include "pmx_format.h"
#include "pmx_scene_builder.h"

using namespace godot;

void PmxImporter::_ensure_textures_imported(const String &p_source_file) const {
	const PackedByteArray data = FileAccess::get_file_as_bytes(p_source_file);
	if (data.is_empty()) {
		return;
	}
	PmxModel model;
	if (parse_pmx(data, model) != OK) {
		return;
	}

	// append_import_external_resource() is not const, but it only talks to the editor's
	// EditorFileSystem singleton.
	PmxImporter *self = const_cast<PmxImporter *>(this);
	for (const String &texture : model.textures) {
		const String texture_path = pmx_resolve_texture_path(p_source_file, texture);
		if (texture_path.is_empty() || !FileAccess::file_exists(texture_path)) {
			continue;
		}
		const Error error = self->append_import_external_resource(texture_path);
		if (error != OK) {
			WARN_PRINT(vformat("PMX texture '%s' could not be imported (error %d).", texture_path, static_cast<int32_t>(error)));
		}
	}
}

void PmxImporter::_bind_methods() {
}

String PmxImporter::_get_importer_name() const {
	return "bullet3.pmx";
}

String PmxImporter::_get_visible_name() const {
	return "MMD PMX Model";
}

PackedStringArray PmxImporter::_get_recognized_extensions() const {
	PackedStringArray extensions;
	extensions.push_back("pmx");
	return extensions;
}

String PmxImporter::_get_save_extension() const {
	return "scn";
}

String PmxImporter::_get_resource_type() const {
	return "PackedScene";
}

float PmxImporter::_get_priority() const {
	return 1.0f;
}

int32_t PmxImporter::_get_import_order() const {
	return 0;
}

int32_t PmxImporter::_get_preset_count() const {
	return 1;
}

String PmxImporter::_get_preset_name(int32_t p_preset_index) const {
	return "Default";
}

TypedArray<Dictionary> PmxImporter::_get_import_options(const String &p_path, int32_t p_preset_index) const {
	TypedArray<Dictionary> options;

	Dictionary scale;
	scale["name"] = "scale";
	// The reference importer scales every model to 0.1, which is the unit range its PMX masses and
	// spring stiffnesses were tuned for. 1.0 would make gravity/springs behave ten times slower.
	scale["default_value"] = 0.1f;
	scale["property_hint"] = PROPERTY_HINT_RANGE;
	scale["hint_string"] = "0.001,100,0.001,or_greater";
	options.push_back(scale);

	Dictionary physics;
	physics["name"] = "import_physics";
	physics["default_value"] = true;
	options.push_back(physics);

	return options;
}

bool PmxImporter::_get_option_visibility(const String &p_path, const StringName &p_option_name, const Dictionary &p_options) const {
	return true;
}

Error PmxImporter::_import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options, const TypedArray<String> &p_platform_variants, const TypedArray<String> &p_gen_files) const {
	float scale = 0.1f;
	if (p_options.has("scale")) {
		scale = static_cast<float>(static_cast<double>(p_options["scale"]));
	}
	bool create_physics = true;
	if (p_options.has("import_physics")) {
		create_physics = static_cast<bool>(p_options["import_physics"]);
	}

	// Textures are separate files: make sure the editor has imported them before we reference
	// them from the generated materials (this registers them as import dependencies too).
	_ensure_textures_imported(p_source_file);

	Ref<PmxSceneBuilder> builder;
	builder.instantiate();

	Ref<PackedScene> scene = builder->build_from_file(p_source_file, scale, create_physics);
	if (scene.is_null()) {
		ERR_PRINT(vformat("PMX import failed for '%s'.", p_source_file));
		return ERR_FILE_CORRUPT;
	}

	const String save_path = p_save_path + String(".") + _get_save_extension();
	const Error error = ResourceSaver::get_singleton()->save(scene, save_path, ResourceSaver::FLAG_CHANGE_PATH);
	if (error != OK) {
		ERR_PRINT(vformat("PMX import could not write '%s' (error %d).", save_path, static_cast<int32_t>(error)));
	}
	return error;
}
