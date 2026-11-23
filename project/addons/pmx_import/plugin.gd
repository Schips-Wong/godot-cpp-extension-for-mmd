@tool
extends EditorPlugin

## GDExtension cannot call EditorPlugins::add_import_plugin() directly (that singleton is not
## exposed to extensions), so this tiny editor plugin registers the C++ importer instance.

var _importer: EditorImportPlugin


func _enter_tree() -> void:
	_importer = PmxImporter.new()
	add_import_plugin(_importer)
	print("[pmx_import] PMX importer registered")


func _exit_tree() -> void:
	if _importer != null:
		remove_import_plugin(_importer)
		_importer = null
