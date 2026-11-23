#pragma once

#include <cstdint>
#include <vector>

#include "godot_cpp/classes/global_constants.hpp"
#include "godot_cpp/variant/color.hpp"
#include "godot_cpp/variant/packed_byte_array.hpp"
#include "godot_cpp/variant/packed_int32_array.hpp"
#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/vector2.hpp"
#include "godot_cpp/variant/vector3.hpp"

using namespace godot;

// PMX 2.0 / 2.1 model data.
//
// Positions, normals and offsets are already converted to Godot's right handed space
// (PMX z is negated, rigid body / joint rotations are negated on x and y), which is the same
// conversion the reference GodotMMD importer uses.

struct PmxVertex {
	Vector3 position;
	Vector3 normal;
	Vector2 uv;
	int32_t bones[4] = { 0, 0, 0, 0 };
	float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

struct PmxMaterial {
	String name;
	Color diffuse = Color(1, 1, 1, 1);
	float specularity = 0.0f;
	Color ambient = Color(0, 0, 0, 1);
	bool double_sided = false;
	int32_t texture_index = -1;
	int32_t sphere_texture_index = -1;
	int32_t sphere_mode = 0;
	int32_t toon_index = -1;
	bool toon_shared = false;
	int32_t index_start = 0;
	int32_t index_count = 0;
};

struct PmxBone {
	String name;
	Vector3 position;
	int32_t parent = -1;
};

struct PmxRigidBody {
	String name;
	int32_t bone = -1;
	int32_t group = 0;
	// PMX stores "groups this body does NOT collide with".
	int32_t non_collision_mask = 0;
	int32_t shape = 1; // 0 sphere, 1 box, 2 capsule
	Vector3 size = Vector3(0.5, 0.5, 0.5);
	Vector3 position;
	Vector3 rotation;
	float mass = 1.0f;
	float linear_damping = 0.0f;
	float angular_damping = 0.0f;
	float restitution = 0.0f;
	float friction = 0.5f;
	int32_t mode = 1; // 0 follow bone, 1 physics, 2 physics + bone offset
};

struct PmxJoint {
	String name;
	int32_t body_a = -1;
	int32_t body_b = -1;
	Vector3 position;
	Vector3 rotation;
	Vector3 linear_lower;
	Vector3 linear_upper;
	Vector3 angular_lower;
	Vector3 angular_upper;
	Vector3 linear_spring;
	Vector3 angular_spring;
};

struct PmxVertexMorphOffset {
	int32_t vertex = 0;
	Vector3 offset;
};

struct PmxMorph {
	String name;
	// 1 = eyebrow, 2 = eye, 3 = mouth, 4 = other.
	int32_t panel = 0;
	// PMX morph type: 0 group, 1 vertex, 2 bone, 3..7 uv, 8 material, 9 flip, 10 impulse.
	int32_t type = 1;
	// Only filled for type 1 (vertex) morphs, which become mesh blend shapes.
	std::vector<PmxVertexMorphOffset> vertex_offsets;
};

struct PmxModel {
	String name;
	String name_en;

	std::vector<PmxVertex> vertices;
	PackedInt32Array indices;
	std::vector<String> textures;
	std::vector<PmxMaterial> materials;
	std::vector<PmxBone> bones;
	std::vector<PmxMorph> morphs;
	std::vector<PmxRigidBody> rigid_bodies;
	std::vector<PmxJoint> joints;
};

// Returns OK on success, ERR_FILE_CORRUPT for bad magic / truncated data and
// ERR_FILE_UNRECOGNIZED for unsupported versions.
Error parse_pmx(const PackedByteArray &p_data, PmxModel &r_model);
