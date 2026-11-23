extends Node3D

## Loads a PMX model and reports the generated structure.
##
## The scene comes from the editor import plugin (PmxImporter -> bullet3.pmx) when the model has
## already been imported, and from PmxSceneBuilder otherwise.

const MODEL_PATH := "res://models/托特.pmx"
const MODEL_SCALE := 1.0

var _skeleton: Skeleton3D
var _physics: MMDPhysicsWorld
var _report_timer := 0.0
var _reports := 0


func _ready() -> void:
	# Optional override: godot --path project res://pmx_demo.tscn -- res://models/other.pmx
	var arguments := OS.get_cmdline_user_args()
	var model_path := MODEL_PATH
	if arguments.size() > 0:
		model_path = arguments[0]

	var builder := PmxSceneBuilder.new()

	var info := builder.inspect_file(model_path)
	if info.is_empty():
		push_error("[pmx] cannot parse %s" % model_path)
		return
	print("[pmx] %s: vertices=%d indices=%d textures=%d materials=%d bones=%d rigid_bodies=%d joints=%d morphs=%d vertex_morphs=%d" % [
		info["name"], info["vertices"], info["indices"], info["textures"],
		info["materials"], info["bones"], info["rigid_bodies"], info["joints"],
		info["morphs"], info["vertex_morphs"]])

	# Prefer the resource produced by the editor import plugin, fall back to the builder.
	var scene: PackedScene = ResourceLoader.load(model_path)
	var source := "imported"
	if scene == null:
		scene = builder.build_from_file(model_path, MODEL_SCALE, true)
		source = "builder"
	if scene == null:
		push_error("[pmx] build failed")
		return

	var model := scene.instantiate()
	model.name = "Model"
	add_child(model)

	_skeleton = model.get_node_or_null("Skeleton") as Skeleton3D
	var mesh_instance := model.get_node_or_null("Skeleton/Mesh") as MeshInstance3D
	_physics = model.get_node_or_null("Physics") as MMDPhysicsWorld

	var surface_count := 0
	var textured_surfaces := 0
	var blend_shape_count := 0
	var morph_names := ""
	if mesh_instance != null and mesh_instance.mesh != null:
		surface_count = mesh_instance.mesh.get_surface_count()
		blend_shape_count = mesh_instance.mesh.get_blend_shape_count()
		for surface in surface_count:
			var surface_material := mesh_instance.mesh.surface_get_material(surface) as StandardMaterial3D
			if surface_material != null and surface_material.albedo_texture != null:
				textured_surfaces += 1
		for index in mini(blend_shape_count, 5):
			if index > 0:
				morph_names += ", "
			morph_names += mesh_instance.mesh.get_blend_shape_name(index)

	print("[pmx] built (%s): bones=%d surfaces=%d textured=%d bodies=%d joints=%d blend_shapes=%d skeleton=%s skin=%s" % [
		source,
		_skeleton.get_bone_count() if _skeleton != null else -1,
		surface_count,
		textured_surfaces,
		_physics.get_rigid_body_count() if _physics != null else -1,
		_physics.get_constraint_count() if _physics != null else -1,
		blend_shape_count,
		_skeleton != null,
		mesh_instance != null and mesh_instance.skin != null])
	print("[pmx] morphs: %s%s" % [morph_names, " ..." if blend_shape_count > 5 else ""])
	print("[pmx] morph metadata entries: %d" % (model.get_meta("pmx_morphs", []).size()))

	# Drive the first morph to show the blend shapes are usable.
	if mesh_instance != null and blend_shape_count > 0:
		mesh_instance.set_blend_shape_value(0, 1.0)
		print("[pmx] blend shape '%s' set to %.1f" % [
			mesh_instance.mesh.get_blend_shape_name(0), mesh_instance.get_blend_shape_value(0)])

	_add_camera_and_light()


func _add_camera_and_light() -> void:
	var camera := Camera3D.new()
	add_child(camera)
	camera.look_at_from_position(Vector3(4.0, 8.0, 14.0), Vector3(0, 7.0, 0))

	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-45, -30, 0)
	light.shadow_enabled = true
	add_child(light)


func _physics_process(delta: float) -> void:
	if _skeleton == null or _skeleton.get_bone_count() == 0:
		return

	_report_timer += delta
	if _report_timer < 1.0 or _reports >= 3:
		return
	_report_timer = 0.0
	_reports += 1

	# The last bone is a good canary: it hangs off a physics driven chain in most models.
	var last_bone := _skeleton.get_bone_count() - 1
	var pose := _skeleton.get_bone_global_pose(last_bone)
	var finite := not (is_nan(pose.origin.x) or is_nan(pose.origin.y) or is_nan(pose.origin.z))
	print("[pmx] t=%.1fs bodies=%d last_bone(%s)=(%.3f, %.3f, %.3f) finite=%s" % [
		_reports, _physics.get_rigid_body_count() if _physics != null else -1,
		_skeleton.get_bone_name(last_bone),
		pose.origin.x, pose.origin.y, pose.origin.z, finite])
