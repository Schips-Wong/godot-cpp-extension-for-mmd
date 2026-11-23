#pragma once

#include <cstdint>
#include <vector>

#include "godot_cpp/classes/mesh_instance3d.hpp"
#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/classes/skeleton3d.hpp"
#include "godot_cpp/variant/node_path.hpp"
#include "godot_cpp/variant/quaternion.hpp"
#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/transform3d.hpp"
#include "godot_cpp/variant/vector3.hpp"

#include "mmd_animation.h"
#include "vmd_format.h"

using namespace godot;

class MMDPhysicsWorld;

// Plays a VMD motion on a PMX model built by PmxSceneBuilder.
//
// The playback order matches the reference MMDModel implementation: sample the bone tracks,
// apply morphs, append (inherit) bones, solve IK, write the resulting global poses and finally
// step the physics world so kinematic bodies follow the new pose before dynamic bones are
// written back. `drive_physics` takes over the world step so this order is guaranteed.
class MMDAnimationPlayer : public Node3D {
	GDCLASS(MMDAnimationPlayer, Node3D)

public:
	enum AxisFixType {
		AXIS_FIX_NONE = 0,
		AXIS_FIX_X = 1,
		AXIS_FIX_Y = 2,
		AXIS_FIX_Z = 3,
		AXIS_FIX_ALL = 4,
	};

	enum TransformOrder {
		ORDER_YZX = 0,
		ORDER_ZXY = 1,
		ORDER_XYZ = 2,
	};

	void _ready() override;
	void _physics_process(double p_delta) override;

protected:
	struct IkLink {
		int32_t bone = -1;
		bool limited = false;
		Vector3 limit_min;
		Vector3 limit_max;
		AxisFixType fix = AXIS_FIX_NONE;
		TransformOrder order = ORDER_YZX;
	};

	struct IkBone {
		int32_t bone = -1;
		int32_t target = -1;
		int32_t loop = 0;
		float angle_limit = 0.0f;
		// Index into vmd.ik_tracks, -1 when the motion does not toggle this IK.
		int32_t ik_track = -1;
		std::vector<IkLink> links;
	};

	struct AppendBone {
		int32_t bone = -1;
		int32_t source = -1;
		bool rotate = false;
		bool translate = false;
		float ratio = 1.0f;
	};

	struct MorphBoneOffset {
		int32_t bone = -1;
		Vector3 translation;
		Quaternion rotation;
	};

	struct MorphGroupOffset {
		int32_t morph = -1;
		float rate = 0.0f;
	};

	// One entry per PMX morph, so group offsets can reference other morphs by index.
	struct MorphEntry {
		int32_t blend_shape = -1;
		std::vector<MorphBoneOffset> bones;
		std::vector<MorphGroupOffset> groups;
	};

	struct MorphTrackRef {
		int32_t morph = -1;
		int32_t track = -1;
	};

	static void _bind_methods();
	void _notification(int p_what);

	bool _ensure_initialized();
	// Rebuilds the bone / morph / append / IK tables from the skeleton and the model meta.
	void _build_bone_tracks();
	void _build_append_bones();
	void _build_ik_bones();
	void _build_morph_tracks();
	Node *_find_model_root() const;

	void _apply_animation();
	void _sample_frame(float p_frame);
	void _apply_morphs(float p_frame);
	void _apply_append_bones();
	void _compute_globals(bool p_with_append);
	void _write_skeleton_pose();
	void _solve_ik(IkBone &p_ik, float p_frame);

	void _sample_bone(const VmdBoneTrack &p_track, float p_frame, Vector3 &r_position, Quaternion &r_rotation) const;
	float _sample_morph_weight(const VmdMorphTrack &p_track, float p_frame) const;
	bool _sample_ik_enable(const VmdIkTrack &p_track, float p_frame) const;
	void _accumulate_morph(int32_t p_morph, float p_weight, std::vector<float> &r_weights, int32_t p_depth);

	static Vector3 _limit_angle(const Vector3 &p_angle, bool p_axis_limit, const Vector3 &p_low, const Vector3 &p_high);
	static Vector3 _quat_to_xyz(const Quaternion &p_quaternion);
	static Vector3 _quat_to_yzx(const Quaternion &p_quaternion);
	static Vector3 _quat_to_zxy(const Quaternion &p_quaternion);

	// Properties.
	Ref<MMDAnimation> animation;
	NodePath skeleton_path = NodePath("../Skeleton");
	NodePath physics_path = NodePath("../Physics");
	NodePath mesh_path = NodePath("../Skeleton/Mesh");
	bool playing = false;
	bool autoplay = true;
	bool loop = true;
	double speed = 1.0;
	double time = 0.0;
	bool drive_physics = true;
	bool use_ik = true;

	// Runtime state.
	VmdAnimationData vmd;
	bool vmd_loaded = false;
	bool initialized = false;
	float model_scale = 0.1f;
	Skeleton3D *skeleton = nullptr;
	MeshInstance3D *mesh_instance = nullptr;
	MMDPhysicsWorld *physics_world = nullptr;
	bool auto_step_owned = false;

	int32_t bone_count = 0;
	std::vector<int32_t> bone_parent;
	std::vector<Transform3D> bone_rest;
	std::vector<int32_t> bone_track;
	std::vector<Vector3> local_position;
	std::vector<Quaternion> local_rotation;
	std::vector<Vector3> append_position;
	std::vector<Quaternion> append_rotation;
	std::vector<Transform3D> global_transform;

	std::vector<IkBone> ik_bones;
	std::vector<AppendBone> append_bones;
	std::vector<MorphEntry> morph_entries;
	std::vector<MorphTrackRef> morph_tracks;

public:
	MMDAnimationPlayer();
	~MMDAnimationPlayer() override = default;

	// Parses a .vmd straight from disk; useful without going through the editor importer.
	Error load_vmd(const String &p_path);

	void play();
	void pause();
	void stop();
	void seek(double p_seconds);

	Ref<MMDAnimation> get_animation() const;
	void set_animation(const Ref<MMDAnimation> &p_animation);

	NodePath get_skeleton_path() const;
	void set_skeleton_path(const NodePath &p_path);
	NodePath get_physics_path() const;
	void set_physics_path(const NodePath &p_path);
	NodePath get_mesh_path() const;
	void set_mesh_path(const NodePath &p_path);

	bool get_playing() const;
	void set_playing(bool p_playing);
	bool get_autoplay() const;
	void set_autoplay(bool p_autoplay);
	bool get_loop() const;
	void set_loop(bool p_loop);
	double get_speed() const;
	void set_speed(double p_speed);
	double get_time() const;
	void set_time(double p_time);
	bool get_drive_physics() const;
	void set_drive_physics(bool p_drive);
	bool get_use_ik() const;
	void set_use_ik(bool p_use);

	double get_duration() const;
	int32_t get_animated_bone_count() const;
	int32_t get_morph_track_count() const;
	int32_t get_ik_count() const;
	String get_vmd_model_name() const;
};
