@tool
extends EditorPlugin

## GDExtension cannot call EditorPlugins::add_import_plugin() directly (that singleton is not
## exposed to extensions), so this tiny editor plugin registers the C++ importer instances.

var _pmx_importer: EditorImportPlugin
var _vmd_importer: EditorImportPlugin


func _enter_tree() -> void:
	_pmx_importer = PmxImporter.new()
	add_import_plugin(_pmx_importer)
	_vmd_importer = VmdImporter.new()
	add_import_plugin(_vmd_importer)
	print("[pmx_import] PMX and VMD importers registered")


func _exit_tree() -> void:
	if _pmx_importer != null:
		remove_import_plugin(_pmx_importer)
		_pmx_importer = null
	if _vmd_importer != null:
		remove_import_plugin(_vmd_importer)
		_vmd_importer = null
