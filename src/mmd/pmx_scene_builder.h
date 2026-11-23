#pragma once

#include "godot_cpp/classes/packed_scene.hpp"
#include "godot_cpp/classes/ref_counted.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/string.hpp"

using namespace godot;

// Resolves a PMX texture entry (which may be relative, absolute or use backslashes) against the
// directory of the model file.
String pmx_resolve_texture_path(const String &p_model_path, const String &p_texture_path);

// Turns a PMX file into a ready to use scene:
//   Node3D (model root)
//     Skeleton3D (bones + rest poses)
//       MeshInstance3D (ArrayMesh + Skin, per material surfaces)
//     MMDPhysicsWorld (skeleton = ../Skeleton)
//       MMDRigidBody x N (bone driven)
//       MMDJoint x N
//
// Exposed to scripts so it can be used at runtime, and used by PmxImporter.
class PmxSceneBuilder : public RefCounted {
	GDCLASS(PmxSceneBuilder, RefCounted)

protected:
	static void _bind_methods();

public:
	// Returns an empty (invalid) PackedScene when the file cannot be parsed.
	Ref<PackedScene> build_from_file(const String &p_path, float p_scale = 0.1f, bool p_create_physics = true);
	// Parses only, returning the model summary (useful for tools and tests).
	Dictionary inspect_file(const String &p_path);
};
