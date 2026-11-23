#pragma once

#include <cstdint>
#include <vector>

#include "godot_cpp/classes/global_constants.hpp"
#include "godot_cpp/variant/packed_byte_array.hpp"
#include "godot_cpp/variant/quaternion.hpp"
#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/vector3.hpp"

using namespace godot;

// VMD (Vocaloid Motion Data) parsing.
//
// The data is converted to Godot's space the same way the reference GodotMMD importer does:
// translations negate z, rotations negate x and y, camera / light data keeps the same rules.

struct VmdInterpolator {
	float ax = 0.0f;
	float ay = 0.0f;
	float bx = 1.0f;
	float by = 1.0f;

	// MMD's per channel cubic bezier, evaluated the same way as the reference
	// (newton iterations with a bisection fallback, see CubicBezierCurve.cs).
	float sample(float p_factor) const;
};

struct VmdBoneKeyFrame {
	int32_t frame = 0;
	Vector3 translation;
	Quaternion rotation;
	VmdInterpolator x;
	VmdInterpolator y;
	VmdInterpolator z;
	VmdInterpolator r;
};

struct VmdMorphKeyFrame {
	int32_t frame = 0;
	float weight = 0.0f;
};

struct VmdIkKeyFrame {
	int32_t frame = 0;
	bool enable = true;
};

struct VmdCameraKeyFrame {
	int32_t frame = 0;
	float distance = 0.0f;
	Vector3 position;
	Vector3 rotation;
	VmdInterpolator mx;
	VmdInterpolator my;
	VmdInterpolator mz;
	VmdInterpolator r;
	VmdInterpolator d;
	VmdInterpolator f;
	float fov = 0.0f;
	bool orthographic = false;
};

struct VmdLightKeyFrame {
	int32_t frame = 0;
	Vector3 color;
	Vector3 position;
};

struct VmdBoneTrack {
	String name;
	std::vector<VmdBoneKeyFrame> keys;
};

struct VmdMorphTrack {
	String name;
	std::vector<VmdMorphKeyFrame> keys;
};

struct VmdIkTrack {
	String name;
	std::vector<VmdIkKeyFrame> keys;
};

struct VmdAnimationData {
	String model_name;
	std::vector<VmdBoneTrack> bone_tracks;
	std::vector<VmdMorphTrack> morph_tracks;
	std::vector<VmdIkTrack> ik_tracks;
	std::vector<VmdCameraKeyFrame> camera_frames;
	std::vector<VmdLightKeyFrame> light_frames;
	double duration = 0.0;

	const VmdBoneTrack *find_bone_track(const String &p_name) const;
	const VmdMorphTrack *find_morph_track(const String &p_name) const;
	const VmdIkTrack *find_ik_track(const String &p_name) const;
};

// Returns OK on success, ERR_FILE_CORRUPT for truncated data and
// ERR_FILE_UNRECOGNIZED for files that do not start with the VMD header.
Error parse_vmd(const PackedByteArray &p_data, VmdAnimationData &r_animation);
