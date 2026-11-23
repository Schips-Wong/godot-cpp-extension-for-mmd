#pragma once

#include "godot_cpp/classes/editor_import_plugin.hpp"
#include "godot_cpp/variant/typed_array.hpp"

using namespace godot;

// Imports .pmx files as PackedScene through the editor import pipeline.
//
// GDExtension cannot call EditorPlugins::add_import_plugin() (not exposed), so the editor
// plugin shipped in project/addons/pmx_import registers an instance of this class with
// EditorPlugin::add_import_plugin().
class PmxImporter : public EditorImportPlugin {
	GDCLASS(PmxImporter, EditorImportPlugin)

protected:
	static void _bind_methods();

	// Imports the external textures referenced by the model so the materials can reference them.
	void _ensure_textures_imported(const String &p_source_file) const;

public:
	String _get_importer_name() const override;
	String _get_visible_name() const override;
	PackedStringArray _get_recognized_extensions() const override;
	String _get_save_extension() const override;
	String _get_resource_type() const override;
	float _get_priority() const override;
	int32_t _get_import_order() const override;
	int32_t _get_preset_count() const override;
	String _get_preset_name(int32_t p_preset_index) const override;
	TypedArray<Dictionary> _get_import_options(const String &p_path, int32_t p_preset_index) const override;
	bool _get_option_visibility(const String &p_path, const StringName &p_option_name, const Dictionary &p_options) const override;
	Error _import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options, const TypedArray<String> &p_platform_variants, const TypedArray<String> &p_gen_files) const override;
};
