#include "pmx_scene_builder.h"

#include <vector>

#include "godot_cpp/classes/array_mesh.hpp"
#include "godot_cpp/classes/base_material3d.hpp"
#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/classes/image.hpp"
#include "godot_cpp/classes/image_texture.hpp"
#include "godot_cpp/classes/mesh_instance3d.hpp"
#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/classes/resource_loader.hpp"
#include "godot_cpp/classes/skeleton3d.hpp"
#include "godot_cpp/classes/skin.hpp"
#include "godot_cpp/classes/standard_material3d.hpp"
#include "godot_cpp/classes/texture2d.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/core/math.hpp"
#include "godot_cpp/variant/packed_float32_array.hpp"
#include "godot_cpp/variant/packed_string_array.hpp"
#include "godot_cpp/variant/packed_vector2_array.hpp"
#include "godot_cpp/variant/packed_vector3_array.hpp"
#include "godot_cpp/variant/typed_array.hpp"

#include "mmd_joint.h"
#include "mmd_physics_world.h"
#include "mmd_rigid_body.h"
#include "pmx_format.h"

using namespace godot;

namespace {

String sanitize_name(const String &p_name, const String &p_fallback) {
	String name = p_name.strip_edges();
	// Node names cannot contain these characters.
	const String invalid = ".:@/\"%";
	for (int32_t i = 0; i < invalid.length(); i++) {
		name = name.replace(invalid.substr(i, 1), "_");
	}
	if (name.is_empty()) {
		name = p_fallback;
	}
	return name;
}

String make_unique(const String &p_name, std::vector<String> &r_used) {
	String candidate = p_name;
	int32_t suffix = 1;
	bool duplicate = true;
	while (duplicate) {
		duplicate = false;
		for (const String &used : r_used) {
			if (used == candidate) {
				duplicate = true;
				break;
			}
		}
		if (duplicate) {
			candidate = p_name + String("_") + itos(suffix++);
		}
	}
	r_used.push_back(candidate);
	return candidate;
}

Ref<Texture2D> load_texture(const String &p_model_path, const String &p_relative_path) {
	const String path = pmx_resolve_texture_path(p_model_path, p_relative_path);
	if (path.is_empty() || !FileAccess::file_exists(path)) {
		return Ref<Texture2D>();
	}

	Ref<Resource> resource = ResourceLoader::get_singleton()->load(path, "Texture2D");
	Ref<Texture2D> texture = resource;
	if (texture.is_valid()) {
		return texture;
	}

	// During the first import pass the texture may not be in the editor's import cache yet, so
	// fall back to reading the image directly (the texture then lives inside the imported scene).
	Ref<Image> image = Image::load_from_file(path);
	if (image.is_null()) {
		WARN_PRINT(vformat("PMX texture could not be loaded: %s", path));
		return Ref<Texture2D>();
	}
	WARN_PRINT(vformat("PMX texture '%s' is not imported yet, it is embedded in the scene.", path));
	return ImageTexture::create_from_image(image);
}

} // namespace

String pmx_resolve_texture_path(const String &p_model_path, const String &p_texture_path) {
	String relative = p_texture_path.replace("\\", "/").strip_edges();
	if (relative.is_empty()) {
		return String();
	}
	// Some PMX files store absolute paths, keep only the file name in that case.
	if (relative.contains(":")) {
		relative = relative.get_file();
	}
	return p_model_path.get_base_dir().path_join(relative).simplify_path();
}

void PmxSceneBuilder::_bind_methods() {
	ClassDB::bind_method(D_METHOD("build_from_file", "path", "scale", "create_physics"), &PmxSceneBuilder::build_from_file, DEFVAL(1.0f), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("inspect_file", "path"), &PmxSceneBuilder::inspect_file);
}

Ref<PackedScene> PmxSceneBuilder::build_from_file(const String &p_path, float p_scale, bool p_create_physics) {
	Ref<PackedScene> result;
	if (p_path.is_empty()) {
		ERR_PRINT("PmxSceneBuilder: empty path.");
		return result;
	}

	const PackedByteArray data = FileAccess::get_file_as_bytes(p_path);
	if (data.is_empty()) {
		ERR_PRINT(vformat("PmxSceneBuilder: cannot read '%s'.", p_path));
		return result;
	}

	PmxModel model;
	const Error parse_error = parse_pmx(data, model);
	if (parse_error != OK) {
		ERR_PRINT(vformat("PmxSceneBuilder: '%s' is not a supported PMX file (error %d).", p_path, static_cast<int32_t>(parse_error)));
		return result;
	}

	const float scale = p_scale > 0.0f ? p_scale : 1.0f;

	// --- Root + skeleton ---------------------------------------------------
	Node3D *root = memnew(Node3D);
	root->set_name(sanitize_name(model.name, "PMXModel"));

	Skeleton3D *skeleton = memnew(Skeleton3D);
	skeleton->set_name("Skeleton");
	root->add_child(skeleton);
	skeleton->set_owner(root);

	const int32_t bone_count = static_cast<int32_t>(model.bones.size());
	std::vector<String> bone_names;
	bone_names.reserve(static_cast<size_t>(bone_count));
	std::vector<Transform3D> bone_global;
	bone_global.reserve(static_cast<size_t>(bone_count));

	for (int32_t i = 0; i < bone_count; i++) {
		const PmxBone &bone = model.bones[static_cast<size_t>(i)];
		const String bone_name = make_unique(sanitize_name(bone.name, "Bone" + itos(i)), bone_names);
		skeleton->add_bone(bone_name);

		const int32_t parent = (bone.parent >= 0 && bone.parent < i) ? bone.parent : -1;
		Vector3 parent_position;
		Transform3D parent_global;
		if (parent >= 0) {
			skeleton->set_bone_parent(i, parent);
			parent_position = model.bones[static_cast<size_t>(parent)].position * scale;
			parent_global = bone_global[static_cast<size_t>(parent)];
		}

		const Transform3D rest = Transform3D(Basis(), bone.position * scale - parent_position);
		skeleton->set_bone_rest(i, rest);
		bone_global.push_back(parent >= 0 ? parent_global * rest : rest);
	}
	skeleton->reset_bone_poses();

	// --- Mesh --------------------------------------------------------------
	const PackedInt32Array &indices = model.indices;
	const int32_t vertex_count = static_cast<int32_t>(model.vertices.size());

	Ref<ArrayMesh> mesh;
	mesh.instantiate();

	Ref<Skin> skin;
	skin.instantiate();
	for (int32_t i = 0; i < bone_count; i++) {
		skin->add_bind(i, bone_global[static_cast<size_t>(i)].affine_inverse());
	}

	const int32_t material_count = static_cast<int32_t>(model.materials.size());
	for (int32_t m = 0; m < material_count; m++) {
		const PmxMaterial &material = model.materials[static_cast<size_t>(m)];
		if (material.index_count <= 0) {
			continue;
		}
		if (material.index_start < 0 || material.index_start + material.index_count > indices.size()) {
			WARN_PRINT(vformat("PMX material '%s' has an invalid index range, skipped.", material.name));
			continue;
		}

		// Remap the used vertices of this material into a compact surface.
		std::vector<int32_t> remap(static_cast<size_t>(vertex_count), -1);
		std::vector<int32_t> used;
		PackedInt32Array surface_indices;
		surface_indices.resize(material.index_count);

		for (int32_t k = 0; k < material.index_count; k++) {
			const int32_t vertex_index = indices[material.index_start + k];
			if (vertex_index < 0 || vertex_index >= vertex_count) {
				WARN_PRINT(vformat("PMX material '%s' references an invalid vertex, surface skipped.", material.name));
				used.clear();
				break;
			}
			if (remap[static_cast<size_t>(vertex_index)] < 0) {
				remap[static_cast<size_t>(vertex_index)] = static_cast<int32_t>(used.size());
				used.push_back(vertex_index);
			}
			surface_indices[k] = remap[static_cast<size_t>(vertex_index)];
		}
		if (used.empty()) {
			continue;
		}

		PackedVector3Array positions;
		PackedVector3Array normals;
		PackedVector2Array uvs;
		PackedInt32Array bones;
		PackedFloat32Array weights;
		positions.resize(static_cast<int64_t>(used.size()));
		normals.resize(static_cast<int64_t>(used.size()));
		uvs.resize(static_cast<int64_t>(used.size()));
		bones.resize(static_cast<int64_t>(used.size()) * 4);
		weights.resize(static_cast<int64_t>(used.size()) * 4);

		for (size_t local = 0; local < used.size(); local++) {
			const PmxVertex &vertex = model.vertices[static_cast<size_t>(used[local])];
			positions[static_cast<int64_t>(local)] = vertex.position * scale;
			normals[static_cast<int64_t>(local)] = vertex.normal;
			uvs[static_cast<int64_t>(local)] = vertex.uv;

			float weight_sum = 0.0f;
			for (int32_t b = 0; b < 4; b++) {
				weight_sum += MAX(vertex.weights[b], 0.0f);
			}
			if (weight_sum <= 0.0f) {
				weight_sum = 1.0f;
			}

			for (int32_t b = 0; b < 4; b++) {
				int32_t bone_index = vertex.bones[b];
				if (bone_index < 0 || bone_index >= bone_count) {
					bone_index = 0;
				}
				bones[static_cast<int64_t>(local) * 4 + b] = bone_index;
				weights[static_cast<int64_t>(local) * 4 + b] = MAX(vertex.weights[b], 0.0f) / weight_sum;
			}
		}

		Array arrays;
		arrays.resize(Mesh::ARRAY_MAX);
		arrays[Mesh::ARRAY_VERTEX] = positions;
		arrays[Mesh::ARRAY_NORMAL] = normals;
		arrays[Mesh::ARRAY_TEX_UV] = uvs;
		arrays[Mesh::ARRAY_BONES] = bones;
		arrays[Mesh::ARRAY_WEIGHTS] = weights;
		arrays[Mesh::ARRAY_INDEX] = surface_indices;

		mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);

		Ref<StandardMaterial3D> surface_material;
		surface_material.instantiate();
		surface_material->set_albedo(material.diffuse);
		surface_material->set_roughness(CLAMP(1.0f - material.specularity, 0.0f, 1.0f));
		if (material.double_sided) {
			surface_material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		}
		if (material.texture_index >= 0 && material.texture_index < static_cast<int32_t>(model.textures.size())) {
			const Ref<Texture2D> texture = load_texture(p_path, model.textures[static_cast<size_t>(material.texture_index)]);
			if (texture.is_valid()) {
				surface_material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, texture);
			}
		}
		mesh->surface_set_material(mesh->get_surface_count() - 1, surface_material);
	}

	MeshInstance3D *mesh_instance = memnew(MeshInstance3D);
	mesh_instance->set_name("Mesh");
	mesh_instance->set_mesh(mesh);
	mesh_instance->set_skin(skin);
	skeleton->add_child(mesh_instance);
	mesh_instance->set_owner(root);
	mesh_instance->set_skeleton_path(NodePath(".."));

	// --- Physics -----------------------------------------------------------
	if (p_create_physics && !model.rigid_bodies.empty()) {
		MMDPhysicsWorld *physics_world = memnew(MMDPhysicsWorld);
		physics_world->set_name("Physics");
		physics_world->set_skeleton_path(NodePath("../Skeleton"));
		root->add_child(physics_world);
		physics_world->set_owner(root);

		const int32_t body_count = static_cast<int32_t>(model.rigid_bodies.size());
		std::vector<String> body_node_names;
		body_node_names.reserve(static_cast<size_t>(body_count));

		for (int32_t i = 0; i < body_count; i++) {
			const PmxRigidBody &rigid_body = model.rigid_bodies[static_cast<size_t>(i)];

			MMDRigidBody *body = memnew(MMDRigidBody);
			const String node_name = make_unique(sanitize_name(rigid_body.name, "RigidBody" + itos(i)), body_node_names);
			body->set_name(node_name);

			if (rigid_body.bone >= 0 && rigid_body.bone < bone_count) {
				body->set_bone_name(bone_names[static_cast<size_t>(rigid_body.bone)]);
			}
			body->set_bone_mode(static_cast<MMDRigidBody::BoneMode>(CLAMP(rigid_body.mode, 0, 2)));
			body->set_shape_type(static_cast<MMDRigidBody::ShapeType>(CLAMP(rigid_body.shape, 0, 2)));
			body->set_size(rigid_body.size * scale);
			body->set_mass(rigid_body.mass);
			body->set_linear_damping(rigid_body.linear_damping);
			body->set_angular_damping(rigid_body.angular_damping);
			body->set_restitution(rigid_body.restitution);
			body->set_friction(rigid_body.friction);
			body->set_group(rigid_body.group);
			// PMX stores the groups this body does NOT collide with, Bullet wants the opposite.
			body->set_group_mask((~rigid_body.non_collision_mask) & 0xFFFF);
			body->set_position(rigid_body.position * scale);
			body->set_rotation(rigid_body.rotation);

			physics_world->add_child(body);
			body->set_owner(root);
		}

		const int32_t joint_count = static_cast<int32_t>(model.joints.size());
		std::vector<String> joint_node_names;
		joint_node_names.reserve(static_cast<size_t>(joint_count));

		for (int32_t i = 0; i < joint_count; i++) {
			const PmxJoint &pmx_joint = model.joints[static_cast<size_t>(i)];
			if (pmx_joint.body_a < 0 || pmx_joint.body_a >= body_count ||
					pmx_joint.body_b < 0 || pmx_joint.body_b >= body_count) {
				WARN_PRINT(vformat("PMX joint '%s' references an invalid rigid body, skipped.", pmx_joint.name));
				continue;
			}

			MMDJoint *joint = memnew(MMDJoint);
			joint->set_name(make_unique(sanitize_name(pmx_joint.name, "Joint" + itos(i)), joint_node_names));
			joint->set_body_a_path(NodePath("../" + body_node_names[static_cast<size_t>(pmx_joint.body_a)]));
			joint->set_body_b_path(NodePath("../" + body_node_names[static_cast<size_t>(pmx_joint.body_b)]));
			joint->set_position(pmx_joint.position * scale);
			joint->set_rotation(pmx_joint.rotation);
			joint->set_linear_lower_limit(pmx_joint.linear_lower);
			joint->set_linear_upper_limit(pmx_joint.linear_upper);
			joint->set_angular_lower_limit(pmx_joint.angular_lower);
			joint->set_angular_upper_limit(pmx_joint.angular_upper);
			joint->set_linear_spring(pmx_joint.linear_spring);
			joint->set_angular_spring(pmx_joint.angular_spring);

			physics_world->add_child(joint);
			joint->set_owner(root);
		}
	}

	Ref<PackedScene> scene;
	scene.instantiate();
	const Error pack_error = scene->pack(root);
	memdelete(root);

	if (pack_error != OK) {
		ERR_PRINT(vformat("PmxSceneBuilder: failed to pack '%s' (error %d).", p_path, static_cast<int32_t>(pack_error)));
		return Ref<PackedScene>();
	}
	return scene;
}

Dictionary PmxSceneBuilder::inspect_file(const String &p_path) {
	Dictionary result;
	const PackedByteArray data = FileAccess::get_file_as_bytes(p_path);
	if (data.is_empty()) {
		return result;
	}

	PmxModel model;
	if (parse_pmx(data, model) != OK) {
		return result;
	}

	result["name"] = model.name;
	result["name_en"] = model.name_en;
	result["vertices"] = static_cast<int64_t>(model.vertices.size());
	result["indices"] = static_cast<int64_t>(model.indices.size());
	result["textures"] = static_cast<int64_t>(model.textures.size());
	result["materials"] = static_cast<int64_t>(model.materials.size());
	result["bones"] = static_cast<int64_t>(model.bones.size());
	result["rigid_bodies"] = static_cast<int64_t>(model.rigid_bodies.size());
	result["joints"] = static_cast<int64_t>(model.joints.size());

	PackedStringArray bone_names;
	for (const PmxBone &bone : model.bones) {
		bone_names.push_back(bone.name);
	}
	result["bone_names"] = bone_names;

	PackedStringArray texture_paths;
	for (const String &texture : model.textures) {
		texture_paths.push_back(texture);
	}
	result["texture_paths"] = texture_paths;
	return result;
}
