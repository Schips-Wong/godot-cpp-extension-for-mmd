#include "mmd_animation_player.h"

#include <cmath>
#include <utility>

#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/classes/mesh_instance3d.hpp"
#include "godot_cpp/classes/node.hpp"
#include "godot_cpp/classes/skeleton3d.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/error_macros.hpp"
#include "godot_cpp/core/math.hpp"
#include "godot_cpp/core/math_defs.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/utility_functions.hpp"

#include "mmd_physics_world.h"

using namespace godot;

namespace {

int32_t dict_int(const Dictionary &p_dict, const char *p_key, int32_t p_default) {
	if (!p_dict.has(p_key)) {
		return p_default;
	}
	return static_cast<int32_t>(static_cast<int64_t>(p_dict[p_key]));
}

float dict_float(const Dictionary &p_dict, const char *p_key, float p_default) {
	if (!p_dict.has(p_key)) {
		return p_default;
	}
	return static_cast<float>(static_cast<double>(p_dict[p_key]));
}

bool dict_bool(const Dictionary &p_dict, const char *p_key, bool p_default) {
	if (!p_dict.has(p_key)) {
		return p_default;
	}
	return static_cast<bool>(p_dict[p_key]);
}

Vector3 dict_vector3(const Dictionary &p_dict, const char *p_key, const Vector3 &p_default) {
	if (!p_dict.has(p_key)) {
		return p_default;
	}
	return static_cast<Vector3>(p_dict[p_key]);
}

Quaternion dict_quaternion(const Dictionary &p_dict, const char *p_key, const Quaternion &p_default) {
	if (!p_dict.has(p_key)) {
		return p_default;
	}
	return static_cast<Quaternion>(p_dict[p_key]);
}

} // namespace

MMDAnimationPlayer::MMDAnimationPlayer() = default;

void MMDAnimationPlayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_vmd", "path"), &MMDAnimationPlayer::load_vmd);
	ClassDB::bind_method(D_METHOD("play"), &MMDAnimationPlayer::play);
	ClassDB::bind_method(D_METHOD("pause"), &MMDAnimationPlayer::pause);
	ClassDB::bind_method(D_METHOD("stop"), &MMDAnimationPlayer::stop);
	ClassDB::bind_method(D_METHOD("seek", "seconds"), &MMDAnimationPlayer::seek);

	ClassDB::bind_method(D_METHOD("get_animation"), &MMDAnimationPlayer::get_animation);
	ClassDB::bind_method(D_METHOD("set_animation", "animation"), &MMDAnimationPlayer::set_animation);
	ClassDB::bind_method(D_METHOD("get_skeleton_path"), &MMDAnimationPlayer::get_skeleton_path);
	ClassDB::bind_method(D_METHOD("set_skeleton_path", "path"), &MMDAnimationPlayer::set_skeleton_path);
	ClassDB::bind_method(D_METHOD("get_physics_path"), &MMDAnimationPlayer::get_physics_path);
	ClassDB::bind_method(D_METHOD("set_physics_path", "path"), &MMDAnimationPlayer::set_physics_path);
	ClassDB::bind_method(D_METHOD("get_mesh_path"), &MMDAnimationPlayer::get_mesh_path);
	ClassDB::bind_method(D_METHOD("set_mesh_path", "path"), &MMDAnimationPlayer::set_mesh_path);
	ClassDB::bind_method(D_METHOD("get_playing"), &MMDAnimationPlayer::get_playing);
	ClassDB::bind_method(D_METHOD("set_playing", "playing"), &MMDAnimationPlayer::set_playing);
	ClassDB::bind_method(D_METHOD("get_autoplay"), &MMDAnimationPlayer::get_autoplay);
	ClassDB::bind_method(D_METHOD("set_autoplay", "autoplay"), &MMDAnimationPlayer::set_autoplay);
	ClassDB::bind_method(D_METHOD("get_loop"), &MMDAnimationPlayer::get_loop);
	ClassDB::bind_method(D_METHOD("set_loop", "loop"), &MMDAnimationPlayer::set_loop);
	ClassDB::bind_method(D_METHOD("get_speed"), &MMDAnimationPlayer::get_speed);
	ClassDB::bind_method(D_METHOD("set_speed", "speed"), &MMDAnimationPlayer::set_speed);
	ClassDB::bind_method(D_METHOD("get_time"), &MMDAnimationPlayer::get_time);
	ClassDB::bind_method(D_METHOD("set_time", "time"), &MMDAnimationPlayer::set_time);
	ClassDB::bind_method(D_METHOD("get_drive_physics"), &MMDAnimationPlayer::get_drive_physics);
	ClassDB::bind_method(D_METHOD("set_drive_physics", "drive_physics"), &MMDAnimationPlayer::set_drive_physics);
	ClassDB::bind_method(D_METHOD("get_use_ik"), &MMDAnimationPlayer::get_use_ik);
	ClassDB::bind_method(D_METHOD("set_use_ik", "use_ik"), &MMDAnimationPlayer::set_use_ik);

	ClassDB::bind_method(D_METHOD("get_duration"), &MMDAnimationPlayer::get_duration);
	ClassDB::bind_method(D_METHOD("get_animated_bone_count"), &MMDAnimationPlayer::get_animated_bone_count);
	ClassDB::bind_method(D_METHOD("get_morph_track_count"), &MMDAnimationPlayer::get_morph_track_count);
	ClassDB::bind_method(D_METHOD("get_ik_count"), &MMDAnimationPlayer::get_ik_count);
	ClassDB::bind_method(D_METHOD("get_vmd_model_name"), &MMDAnimationPlayer::get_vmd_model_name);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "animation", PROPERTY_HINT_RESOURCE_TYPE, "MMDAnimation"), "set_animation", "get_animation");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "skeleton"), "set_skeleton_path", "get_skeleton_path");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "physics"), "set_physics_path", "get_physics_path");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "mesh"), "set_mesh_path", "get_mesh_path");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "playing"), "set_playing", "get_playing");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "autoplay"), "set_autoplay", "get_autoplay");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "loop"), "set_loop", "get_loop");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed"), "set_speed", "get_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "time", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR), "set_time", "get_time");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "drive_physics"), "set_drive_physics", "get_drive_physics");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_ik"), "set_use_ik", "get_use_ik");
}

void MMDAnimationPlayer::_notification(int p_what) {
	if (p_what == NOTIFICATION_EXIT_TREE) {
		if (auto_step_owned && physics_world) {
			physics_world->set_auto_step(true);
			auto_step_owned = false;
		}
		physics_world = nullptr;
		skeleton = nullptr;
		mesh_instance = nullptr;
		initialized = false;
	}
}

void MMDAnimationPlayer::_ready() {
	set_physics_process(true);
	if (autoplay) {
		playing = true;
	}
	_ensure_initialized();
}

void MMDAnimationPlayer::_physics_process(double p_delta) {
	if (!_ensure_initialized()) {
		return;
	}

	if (playing) {
		time += p_delta * speed;
		if (vmd.duration > 0.0) {
			if (loop) {
				time = std::fmod(time, vmd.duration);
			} else {
				time = MIN(time, vmd.duration);
			}
		}
		if (time < 0.0) {
			time = 0.0;
		}
	}

	_apply_animation();

	// Step the world after the skeleton has been posed, so kinematic bodies follow this frame's
	// pose and dynamic bones are written back before rendering.
	if (drive_physics && physics_world) {
		physics_world->step_simulation(p_delta);
	}
}

bool MMDAnimationPlayer::_ensure_initialized() {
	if (initialized) {
		return true;
	}

	if (!vmd_loaded) {
		if (animation.is_null()) {
			return false;
		}
		const PackedByteArray data = animation->get_data();
		if (data.is_empty()) {
			return false;
		}
		VmdAnimationData parsed;
		if (parse_vmd(data, parsed) != OK) {
			ERR_PRINT("MMDAnimationPlayer: the assigned MMDAnimation contains invalid VMD data.");
			animation = Ref<MMDAnimation>();
			return false;
		}
		vmd = std::move(parsed);
		vmd_loaded = true;
	}

	skeleton = Object::cast_to<Skeleton3D>(get_node_or_null(skeleton_path));
	if (skeleton == nullptr) {
		return false;
	}

	physics_world = Object::cast_to<MMDPhysicsWorld>(get_node_or_null(physics_path));
	mesh_instance = Object::cast_to<MeshInstance3D>(get_node_or_null(mesh_path));

	Node *model_root = _find_model_root();
	if (model_root != nullptr && model_root->has_meta("model_scale")) {
		model_scale = static_cast<float>(static_cast<double>(model_root->get_meta("model_scale")));
	}

	bone_count = skeleton->get_bone_count();
	bone_parent.resize(static_cast<size_t>(bone_count));
	bone_rest.resize(static_cast<size_t>(bone_count));
	local_position.assign(static_cast<size_t>(bone_count), Vector3());
	local_rotation.assign(static_cast<size_t>(bone_count), Quaternion());
	append_position.assign(static_cast<size_t>(bone_count), Vector3());
	append_rotation.assign(static_cast<size_t>(bone_count), Quaternion());
	global_transform.resize(static_cast<size_t>(bone_count));

	for (int32_t i = 0; i < bone_count; i++) {
		bone_parent[static_cast<size_t>(i)] = skeleton->get_bone_parent(i);
		bone_rest[static_cast<size_t>(i)] = skeleton->get_bone_rest(i);
	}

	_build_bone_tracks();
	_build_append_bones();
	_build_ik_bones();
	_build_morph_tracks();

	initialized = true;

	if (physics_world != nullptr) {
		if (drive_physics && !auto_step_owned) {
			physics_world->set_auto_step(false);
			auto_step_owned = true;
		} else if (!drive_physics && auto_step_owned) {
			physics_world->set_auto_step(true);
			auto_step_owned = false;
		}
	}

	_apply_animation();
	return true;
}

Node *MMDAnimationPlayer::_find_model_root() const {
	Node *current = get_parent();
	while (current != nullptr) {
		if (current->has_meta("pmx_morphs") || current->has_meta("pmx_ik_bones") || current->has_meta("model_scale")) {
			return current;
		}
		current = current->get_parent();
	}
	return nullptr;
}

void MMDAnimationPlayer::_build_bone_tracks() {
	bone_track.assign(static_cast<size_t>(bone_count), -1);
	for (int32_t i = 0; i < bone_count; i++) {
		const String name = skeleton->get_bone_name(i);
		for (size_t t = 0; t < vmd.bone_tracks.size(); t++) {
			if (vmd.bone_tracks[t].name == name) {
				bone_track[static_cast<size_t>(i)] = static_cast<int32_t>(t);
				break;
			}
		}
	}
}

void MMDAnimationPlayer::_build_append_bones() {
	append_bones.clear();
	Node *model_root = _find_model_root();
	if (model_root == nullptr || !model_root->has_meta("pmx_append_bones")) {
		return;
	}
	const Array meta = model_root->get_meta("pmx_append_bones");
	for (int32_t i = 0; i < meta.size(); i++) {
		const Dictionary dict = meta[i];
		AppendBone append;
		append.bone = dict_int(dict, "bone", -1);
		append.source = dict_int(dict, "source", -1);
		append.rotate = dict_bool(dict, "rotate", false);
		append.translate = dict_bool(dict, "translate", false);
		append.ratio = dict_float(dict, "ratio", 1.0f);
		if (append.bone < 0 || append.bone >= bone_count || append.source < 0 || append.source >= bone_count) {
			continue;
		}
		append_bones.push_back(append);
	}
}

void MMDAnimationPlayer::_build_ik_bones() {
	ik_bones.clear();
	Node *model_root = _find_model_root();
	if (model_root == nullptr || !model_root->has_meta("pmx_ik_bones")) {
		return;
	}
	const Array meta = model_root->get_meta("pmx_ik_bones");
	for (int32_t i = 0; i < meta.size(); i++) {
		const Dictionary dict = meta[i];
		IkBone ik;
		ik.bone = dict_int(dict, "bone", -1);
		ik.target = dict_int(dict, "target", -1);
		ik.loop = dict_int(dict, "loop", 0);
		ik.angle_limit = dict_float(dict, "angle_limit", 0.0f);
		if (ik.bone < 0 || ik.bone >= bone_count || ik.target < 0 || ik.target >= bone_count) {
			continue;
		}

		const String ik_name = skeleton->get_bone_name(ik.bone);
		for (size_t t = 0; t < vmd.ik_tracks.size(); t++) {
			if (vmd.ik_tracks[t].name == ik_name) {
				ik.ik_track = static_cast<int32_t>(t);
				break;
			}
		}

		const Array links = dict["links"];
		for (int32_t j = 0; j < links.size(); j++) {
			const Dictionary link_dict = links[j];
			IkLink link;
			link.bone = dict_int(link_dict, "index", -1);
			link.limited = dict_bool(link_dict, "limited", false);
			link.limit_min = dict_vector3(link_dict, "min", Vector3());
			link.limit_max = dict_vector3(link_dict, "max", Vector3());
			if (link.bone < 0 || link.bone >= bone_count) {
				continue;
			}

			// Same transform order / axis fix selection as the reference implementation.
			if (link.limit_min.x > -Math::PI * 0.5 && link.limit_max.x < Math::PI * 0.5) {
				link.order = ORDER_ZXY;
			} else if (link.limit_min.y > -Math::PI * 0.5 && link.limit_max.y < Math::PI * 0.5) {
				link.order = ORDER_XYZ;
			} else {
				link.order = ORDER_YZX;
			}
			if (link.limited) {
				const float epsilon = 1e-6f;
				uint32_t flags = 0;
				if (std::abs(link.limit_min.x) < epsilon && std::abs(link.limit_max.x) < epsilon) {
					flags |= 1;
				}
				if (std::abs(link.limit_min.y) < epsilon && std::abs(link.limit_max.y) < epsilon) {
					flags |= 2;
				}
				if (std::abs(link.limit_min.z) < epsilon && std::abs(link.limit_max.z) < epsilon) {
					flags |= 4;
				}
				switch (flags) {
					case 7:
						link.fix = AXIS_FIX_ALL;
						break;
					case 6:
						link.fix = AXIS_FIX_X;
						break;
					case 5:
						link.fix = AXIS_FIX_Y;
						break;
					case 3:
						link.fix = AXIS_FIX_Z;
						break;
					default:
						link.fix = AXIS_FIX_NONE;
						break;
				}
			}
			ik.links.push_back(link);
		}
		ik_bones.push_back(ik);
	}
}

void MMDAnimationPlayer::_build_morph_tracks() {
	morph_entries.clear();
	morph_tracks.clear();

	Node *model_root = _find_model_root();
	if (model_root == nullptr || !model_root->has_meta("pmx_morphs")) {
		return;
	}
	const Array meta = model_root->get_meta("pmx_morphs");
	morph_entries.resize(meta.size());

	std::vector<String> morph_names(static_cast<size_t>(meta.size()));
	for (int32_t i = 0; i < meta.size(); i++) {
		const Dictionary dict = meta[i];
		MorphEntry &entry = morph_entries[static_cast<size_t>(i)];
		entry.blend_shape = dict_int(dict, "blend_shape", -1);
		morph_names[static_cast<size_t>(i)] = static_cast<String>(dict["name"]);

		if (dict.has("bones")) {
			const Array bones = dict["bones"];
			for (int32_t j = 0; j < bones.size(); j++) {
				const Dictionary bone_dict = bones[j];
				MorphBoneOffset offset;
				offset.bone = dict_int(bone_dict, "bone", -1);
				offset.translation = dict_vector3(bone_dict, "translation", Vector3());
				offset.rotation = dict_quaternion(bone_dict, "rotation", Quaternion());
				if (offset.bone >= 0 && offset.bone < bone_count) {
					entry.bones.push_back(offset);
				}
			}
		}
		if (dict.has("groups")) {
			const Array groups = dict["groups"];
			for (int32_t j = 0; j < groups.size(); j++) {
				const Dictionary group_dict = groups[j];
				MorphGroupOffset offset;
				offset.morph = dict_int(group_dict, "morph", -1);
				offset.rate = dict_float(group_dict, "rate", 0.0f);
				if (offset.morph >= 0 && offset.morph < meta.size()) {
					entry.groups.push_back(offset);
				}
			}
		}
	}

	for (size_t t = 0; t < vmd.morph_tracks.size(); t++) {
		for (size_t m = 0; m < morph_names.size(); m++) {
			if (morph_names[m] == vmd.morph_tracks[t].name) {
				MorphTrackRef ref;
				ref.morph = static_cast<int32_t>(m);
				ref.track = static_cast<int32_t>(t);
				morph_tracks.push_back(ref);
				break;
			}
		}
	}
}

void MMDAnimationPlayer::_apply_animation() {
	const float frame = static_cast<float>(time) * 30.0f;

	_sample_frame(frame);
	_apply_morphs(frame);

	_compute_globals(false);
	_apply_append_bones();
	_compute_globals(true);

	if (use_ik) {
		for (IkBone &ik : ik_bones) {
			_solve_ik(ik, frame);
		}
		_compute_globals(false);
		_apply_append_bones();
		_compute_globals(true);
	}

	_write_skeleton_pose();
}

void MMDAnimationPlayer::_sample_frame(float p_frame) {
	for (int32_t i = 0; i < bone_count; i++) {
		const size_t index = static_cast<size_t>(i);
		local_position[index] = Vector3();
		local_rotation[index] = Quaternion();
		append_position[index] = Vector3();
		append_rotation[index] = Quaternion();

		const int32_t track = bone_track[index];
		if (track >= 0 && track < static_cast<int32_t>(vmd.bone_tracks.size())) {
			_sample_bone(vmd.bone_tracks[static_cast<size_t>(track)], p_frame, local_position[index], local_rotation[index]);
		}
	}
}

void MMDAnimationPlayer::_apply_morphs(float p_frame) {
	if (morph_entries.empty()) {
		return;
	}

	std::vector<float> weights(morph_entries.size(), 0.0f);
	for (const MorphTrackRef &ref : morph_tracks) {
		if (ref.morph < 0 || ref.morph >= static_cast<int32_t>(morph_entries.size()) ||
				ref.track < 0 || ref.track >= static_cast<int32_t>(vmd.morph_tracks.size())) {
			continue;
		}
		const float weight = _sample_morph_weight(vmd.morph_tracks[static_cast<size_t>(ref.track)], p_frame);
		_accumulate_morph(ref.morph, weight, weights, 0);
	}

	for (size_t i = 0; i < morph_entries.size(); i++) {
		const MorphEntry &entry = morph_entries[i];
		const float weight = weights[i];
		if (entry.blend_shape >= 0 && mesh_instance != nullptr) {
			mesh_instance->set_blend_shape_value(entry.blend_shape, weight);
		}
		if (weight == 0.0f) {
			continue;
		}
		for (const MorphBoneOffset &offset : entry.bones) {
			const size_t bone = static_cast<size_t>(offset.bone);
			local_position[bone] += offset.translation * weight;
			local_rotation[bone] = (local_rotation[bone] * Quaternion().slerp(offset.rotation, weight)).normalized();
		}
	}
}

void MMDAnimationPlayer::_accumulate_morph(int32_t p_morph, float p_weight, std::vector<float> &r_weights, int32_t p_depth) {
	if (p_depth > 8 || p_morph < 0 || p_morph >= static_cast<int32_t>(r_weights.size())) {
		return;
	}
	r_weights[static_cast<size_t>(p_morph)] += p_weight;
	const MorphEntry &entry = morph_entries[static_cast<size_t>(p_morph)];
	for (const MorphGroupOffset &group : entry.groups) {
		_accumulate_morph(group.morph, group.rate * p_weight, r_weights, p_depth + 1);
	}
}

void MMDAnimationPlayer::_apply_append_bones() {
	for (const AppendBone &append : append_bones) {
		const size_t bone = static_cast<size_t>(append.bone);
		const size_t source = static_cast<size_t>(append.source);
		if (append.rotate) {
			append_rotation[bone] = Quaternion().slerp(local_rotation[source], append.ratio).normalized();
		}
		if (append.translate) {
			append_position[bone] = local_position[source] * append.ratio;
		}
	}
}

void MMDAnimationPlayer::_compute_globals(bool p_with_append) {
	for (int32_t i = 0; i < bone_count; i++) {
		const size_t index = static_cast<size_t>(i);
		const Quaternion append_rot = p_with_append ? append_rotation[index] : Quaternion();
		const Vector3 append_pos = p_with_append ? append_position[index] : Vector3();
		const Quaternion rotation = (local_rotation[index] * append_rot).normalized();
		const Vector3 position = local_position[index] + append_pos;
		const Transform3D local(Basis(rotation), position);

		const int32_t parent = bone_parent[index];
		global_transform[index] = local;
		if (parent >= 0 && parent < bone_count) {
			global_transform[index] = global_transform[static_cast<size_t>(parent)] * bone_rest[index] * local;
		} else {
			global_transform[index] = bone_rest[index] * local;
		}
	}
}

void MMDAnimationPlayer::_write_skeleton_pose() {
	for (int32_t i = 0; i < bone_count; i++) {
		skeleton->set_bone_global_pose_override(i, global_transform[static_cast<size_t>(i)], 1.0f, true);
	}
}

void MMDAnimationPlayer::_solve_ik(IkBone &p_ik, float p_frame) {
	if (p_ik.ik_track >= 0 && p_ik.ik_track < static_cast<int32_t>(vmd.ik_tracks.size())) {
		if (!_sample_ik_enable(vmd.ik_tracks[static_cast<size_t>(p_ik.ik_track)], p_frame)) {
			return;
		}
	}

	const Vector3 ik_target_position = global_transform[static_cast<size_t>(p_ik.bone)].origin;
	Vector3 tip_position = global_transform[static_cast<size_t>(p_ik.target)].origin;
	if (ik_target_position.distance_squared_to(tip_position) < 1e-6f) {
		return;
	}

	const int32_t half_limit = p_ik.loop / 2;
	for (int32_t iteration = 0; iteration < p_ik.loop; iteration++) {
		const bool axis_limit = iteration < half_limit;
		for (const IkLink &link : p_ik.links) {
			const size_t link_index = static_cast<size_t>(link.bone);
			const Vector3 it_position = global_transform[link_index].origin;
			const Vector3 target_direction = (ik_target_position - it_position).normalized();
			const Vector3 ik_direction = (tip_position - it_position).normalized();
			const float dot_v = CLAMP(target_direction.dot(ik_direction), -1.0f, 1.0f);
			const float angle1 = std::acos(dot_v);
			if (std::abs(angle1) < 6e-4f) {
				continue;
			}

			const Basis inverse_basis = global_transform[link_index].basis.inverse();
			Vector3 rotate_axis = inverse_basis.xform(target_direction.cross(ik_direction));

			if (axis_limit) {
				switch (link.fix) {
					case AXIS_FIX_X:
						rotate_axis = Vector3(rotate_axis.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
						break;
					case AXIS_FIX_Y:
						rotate_axis = Vector3(0.0f, rotate_axis.y >= 0.0f ? 1.0f : -1.0f, 0.0f);
						break;
					case AXIS_FIX_Z:
						rotate_axis = Vector3(0.0f, 0.0f, rotate_axis.z >= 0.0f ? 1.0f : -1.0f);
						break;
					default:
						break;
				}
			}
			if (rotate_axis.length_squared() < 1e-12f) {
				continue;
			}

			const float limit = p_ik.angle_limit * static_cast<float>(iteration + 1);
			Quaternion result = (local_rotation[link_index] * Quaternion(rotate_axis.normalized(), -CLAMP(angle1, -limit, limit))).normalized();

			if (link.limited) {
				Vector3 angles;
				switch (link.order) {
					case ORDER_ZXY: {
						angles = _limit_angle(_quat_to_zxy(result), axis_limit, link.limit_min, link.limit_max);
						result = Quaternion(Vector3(0, 0, 1), angles.z) * Quaternion(Vector3(1, 0, 0), angles.x) * Quaternion(Vector3(0, 1, 0), angles.y);
					} break;
					case ORDER_XYZ: {
						angles = _limit_angle(_quat_to_xyz(result), axis_limit, link.limit_min, link.limit_max);
						result = Quaternion(Vector3(1, 0, 0), angles.x) * Quaternion(Vector3(0, 1, 0), angles.y) * Quaternion(Vector3(0, 0, 1), angles.z);
					} break;
					default: {
						angles = _limit_angle(_quat_to_yzx(result), axis_limit, link.limit_min, link.limit_max);
						result = Quaternion(Vector3(0, 1, 0), angles.y) * Quaternion(Vector3(0, 0, 1), angles.z) * Quaternion(Vector3(1, 0, 0), angles.x);
					} break;
				}
			}

			local_rotation[link_index] = result.normalized();
			// Recompute the chain so the next link uses the updated tip position.
			_compute_globals(false);
			tip_position = global_transform[static_cast<size_t>(p_ik.target)].origin;
		}

		tip_position = global_transform[static_cast<size_t>(p_ik.target)].origin;
		if (ik_target_position.distance_squared_to(tip_position) < 1e-6f) {
			return;
		}
	}
}

void MMDAnimationPlayer::_sample_bone(const VmdBoneTrack &p_track, float p_frame, Vector3 &r_position, Quaternion &r_rotation) const {
	const std::vector<VmdBoneKeyFrame> &keys = p_track.keys;
	if (keys.empty()) {
		return;
	}

	int32_t left = 0;
	int32_t right = static_cast<int32_t>(keys.size()) - 1;
	const VmdBoneKeyFrame *lf = &keys[static_cast<size_t>(left)];
	const VmdBoneKeyFrame *rf = &keys[static_cast<size_t>(right)];

	while (left + 1 < right) {
		const int32_t mid = (left + right) / 2;
		const VmdBoneKeyFrame &middle = keys[static_cast<size_t>(mid)];
		if (middle.frame > p_frame) {
			rf = &middle;
			right = mid;
		} else {
			lf = &middle;
			left = mid;
		}
	}

	if (lf->frame == rf->frame) {
		r_position = rf->translation * model_scale;
		r_rotation = rf->rotation.normalized();
		return;
	}

	float factor = (p_frame - static_cast<float>(lf->frame)) / static_cast<float>(rf->frame - lf->frame);
	factor = CLAMP(factor, 0.0f, 1.0f);

	const float fx = rf->x.sample(factor);
	const float fy = rf->y.sample(factor);
	const float fz = rf->z.sample(factor);
	const float fr = rf->r.sample(factor);

	const Vector3 translation(
			lf->translation.x * (1.0f - fx) + rf->translation.x * fx,
			lf->translation.y * (1.0f - fy) + rf->translation.y * fy,
			lf->translation.z * (1.0f - fz) + rf->translation.z * fz);
	r_position = translation * model_scale;
	r_rotation = lf->rotation.slerp(rf->rotation, fr).normalized();
}

float MMDAnimationPlayer::_sample_morph_weight(const VmdMorphTrack &p_track, float p_frame) const {
	const std::vector<VmdMorphKeyFrame> &keys = p_track.keys;
	if (keys.empty()) {
		return 0.0f;
	}

	int32_t left = 0;
	int32_t right = static_cast<int32_t>(keys.size()) - 1;
	const VmdMorphKeyFrame *lf = &keys[static_cast<size_t>(left)];
	const VmdMorphKeyFrame *rf = &keys[static_cast<size_t>(right)];

	while (left + 1 < right) {
		const int32_t mid = (left + right) / 2;
		const VmdMorphKeyFrame &middle = keys[static_cast<size_t>(mid)];
		if (middle.frame > p_frame) {
			rf = &middle;
			right = mid;
		} else {
			lf = &middle;
			left = mid;
		}
	}

	if (lf->frame == rf->frame) {
		return rf->weight;
	}
	float factor = (p_frame - static_cast<float>(lf->frame)) / static_cast<float>(rf->frame - lf->frame);
	factor = CLAMP(factor, 0.0f, 1.0f);
	return lf->weight * (1.0f - factor) + rf->weight * factor;
}

bool MMDAnimationPlayer::_sample_ik_enable(const VmdIkTrack &p_track, float p_frame) const {
	const std::vector<VmdIkKeyFrame> &keys = p_track.keys;
	if (keys.empty()) {
		return true;
	}

	int32_t left = 0;
	int32_t right = static_cast<int32_t>(keys.size()) - 1;
	const VmdIkKeyFrame *lf = &keys[static_cast<size_t>(left)];
	const VmdIkKeyFrame *rf = &keys[static_cast<size_t>(right)];

	while (left + 1 < right) {
		const int32_t mid = (left + right) / 2;
		const VmdIkKeyFrame &middle = keys[static_cast<size_t>(mid)];
		if (middle.frame > p_frame) {
			rf = &middle;
			right = mid;
		} else {
			lf = &middle;
			left = mid;
		}
	}

	if (p_frame > static_cast<float>(rf->frame)) {
		return rf->enable;
	}
	return lf->enable;
}

Vector3 MMDAnimationPlayer::_limit_angle(const Vector3 &p_angle, bool p_axis_limit, const Vector3 &p_low, const Vector3 &p_high) {
	if (!p_axis_limit) {
		return p_angle.clamp(p_low, p_high);
	}

	Vector3 angle = p_angle;
	const Vector3 vec_l1 = p_low * 2.0f - angle;
	const Vector3 vec_h1 = p_high * 2.0f - angle;

	if (angle.x < p_low.x) {
		angle.x = (vec_l1.x <= p_high.x) ? vec_l1.x : p_low.x;
	} else if (angle.x > p_high.x) {
		angle.x = (vec_h1.x >= p_low.x) ? vec_h1.x : p_high.x;
	}
	if (angle.y < p_low.y) {
		angle.y = (vec_l1.y <= p_high.y) ? vec_l1.y : p_low.y;
	} else if (angle.y > p_high.y) {
		angle.y = (vec_h1.y >= p_low.y) ? vec_h1.y : p_high.y;
	}
	if (angle.z < p_low.z) {
		angle.z = (vec_l1.z <= p_high.z) ? vec_l1.z : p_low.z;
	} else if (angle.z > p_high.z) {
		angle.z = (vec_h1.z >= p_low.z) ? vec_h1.z : p_high.z;
	}
	return angle;
}

Vector3 MMDAnimationPlayer::_quat_to_xyz(const Quaternion &p_quaternion) {
	const float ii = p_quaternion.x * p_quaternion.x;
	const float jj = p_quaternion.y * p_quaternion.y;
	const float kk = p_quaternion.z * p_quaternion.z;
	const float ei = p_quaternion.w * p_quaternion.x;
	const float ej = p_quaternion.w * p_quaternion.y;
	const float ek = p_quaternion.w * p_quaternion.z;
	const float ij = p_quaternion.x * p_quaternion.y;
	const float ik = p_quaternion.x * p_quaternion.z;
	const float jk = p_quaternion.y * p_quaternion.z;

	Vector3 result;
	result.x = std::atan2(2.0f * (ei - jk), 1.0f - 2.0f * (ii + jj));
	result.y = std::asin(CLAMP(2.0f * (ej + ik), -1.0f, 1.0f));
	result.z = std::atan2(2.0f * (ek - ij), 1.0f - 2.0f * (jj + kk));
	return result;
}

Vector3 MMDAnimationPlayer::_quat_to_yzx(const Quaternion &p_quaternion) {
	const float ii = p_quaternion.x * p_quaternion.x;
	const float jj = p_quaternion.y * p_quaternion.y;
	const float kk = p_quaternion.z * p_quaternion.z;
	const float ei = p_quaternion.w * p_quaternion.x;
	const float ej = p_quaternion.w * p_quaternion.y;
	const float ek = p_quaternion.w * p_quaternion.z;
	const float ij = p_quaternion.x * p_quaternion.y;
	const float ik = p_quaternion.x * p_quaternion.z;
	const float jk = p_quaternion.y * p_quaternion.z;

	Vector3 result;
	result.x = std::atan2(2.0f * (ei - jk), 1.0f - 2.0f * (ii + kk));
	result.y = std::atan2(2.0f * (ej - ik), 1.0f - 2.0f * (jj + kk));
	result.z = std::asin(CLAMP(2.0f * (ek + ij), -1.0f, 1.0f));
	return result;
}

Vector3 MMDAnimationPlayer::_quat_to_zxy(const Quaternion &p_quaternion) {
	const float ii = p_quaternion.x * p_quaternion.x;
	const float jj = p_quaternion.y * p_quaternion.y;
	const float kk = p_quaternion.z * p_quaternion.z;
	const float ei = p_quaternion.w * p_quaternion.x;
	const float ej = p_quaternion.w * p_quaternion.y;
	const float ek = p_quaternion.w * p_quaternion.z;
	const float ij = p_quaternion.x * p_quaternion.y;
	const float ik = p_quaternion.x * p_quaternion.z;
	const float jk = p_quaternion.y * p_quaternion.z;

	Vector3 result;
	result.x = std::asin(CLAMP(2.0f * (ei + jk), -1.0f, 1.0f));
	result.y = std::atan2(2.0f * (ej - ik), 1.0f - 2.0f * (ii + jj));
	result.z = std::atan2(2.0f * (ek - ij), 1.0f - 2.0f * (ii + kk));
	return result;
}

Error MMDAnimationPlayer::load_vmd(const String &p_path) {
	const PackedByteArray data = FileAccess::get_file_as_bytes(p_path);
	if (data.is_empty()) {
		ERR_PRINT(vformat("MMDAnimationPlayer: cannot read '%s'.", p_path));
		return ERR_FILE_CANT_READ;
	}

	VmdAnimationData parsed;
	const Error error = parse_vmd(data, parsed);
	if (error != OK) {
		ERR_PRINT(vformat("MMDAnimationPlayer: '%s' is not a supported VMD file (error %d).", p_path, static_cast<int32_t>(error)));
		return error;
	}

	vmd = std::move(parsed);
	vmd_loaded = true;
	animation = Ref<MMDAnimation>();
	time = 0.0;
	initialized = false;
	if (is_inside_tree()) {
		_ensure_initialized();
	}
	return OK;
}

void MMDAnimationPlayer::play() {
	playing = true;
}

void MMDAnimationPlayer::pause() {
	playing = false;
}

void MMDAnimationPlayer::stop() {
	playing = false;
	time = 0.0;
}

void MMDAnimationPlayer::seek(double p_seconds) {
	time = MAX(p_seconds, 0.0);
	if (vmd.duration > 0.0) {
		if (loop) {
			time = std::fmod(time, vmd.duration);
		} else {
			time = MIN(time, vmd.duration);
		}
	}
}

Ref<MMDAnimation> MMDAnimationPlayer::get_animation() const {
	return animation;
}

void MMDAnimationPlayer::set_animation(const Ref<MMDAnimation> &p_animation) {
	animation = p_animation;
	vmd = VmdAnimationData();
	vmd_loaded = false;
	initialized = false;
}

NodePath MMDAnimationPlayer::get_skeleton_path() const {
	return skeleton_path;
}

void MMDAnimationPlayer::set_skeleton_path(const NodePath &p_path) {
	skeleton_path = p_path;
	initialized = false;
}

NodePath MMDAnimationPlayer::get_physics_path() const {
	return physics_path;
}

void MMDAnimationPlayer::set_physics_path(const NodePath &p_path) {
	physics_path = p_path;
	initialized = false;
}

NodePath MMDAnimationPlayer::get_mesh_path() const {
	return mesh_path;
}

void MMDAnimationPlayer::set_mesh_path(const NodePath &p_path) {
	mesh_path = p_path;
	initialized = false;
}

bool MMDAnimationPlayer::get_playing() const {
	return playing;
}

void MMDAnimationPlayer::set_playing(bool p_playing) {
	playing = p_playing;
}

bool MMDAnimationPlayer::get_autoplay() const {
	return autoplay;
}

void MMDAnimationPlayer::set_autoplay(bool p_autoplay) {
	autoplay = p_autoplay;
}

bool MMDAnimationPlayer::get_loop() const {
	return loop;
}

void MMDAnimationPlayer::set_loop(bool p_loop) {
	loop = p_loop;
}

double MMDAnimationPlayer::get_speed() const {
	return speed;
}

void MMDAnimationPlayer::set_speed(double p_speed) {
	speed = p_speed;
}

double MMDAnimationPlayer::get_time() const {
	return time;
}

void MMDAnimationPlayer::set_time(double p_time) {
	seek(p_time);
}

bool MMDAnimationPlayer::get_drive_physics() const {
	return drive_physics;
}

void MMDAnimationPlayer::set_drive_physics(bool p_drive) {
	drive_physics = p_drive;
	if (initialized && physics_world != nullptr) {
		if (drive_physics && !auto_step_owned) {
			physics_world->set_auto_step(false);
			auto_step_owned = true;
		} else if (!drive_physics && auto_step_owned) {
			physics_world->set_auto_step(true);
			auto_step_owned = false;
		}
	}
}

bool MMDAnimationPlayer::get_use_ik() const {
	return use_ik;
}

void MMDAnimationPlayer::set_use_ik(bool p_use) {
	use_ik = p_use;
}

double MMDAnimationPlayer::get_duration() const {
	if (vmd_loaded) {
		return vmd.duration;
	}
	if (animation.is_valid()) {
		return animation->get_duration();
	}
	return 0.0;
}

int32_t MMDAnimationPlayer::get_animated_bone_count() const {
	if (!initialized) {
		return 0;
	}
	int32_t count = 0;
	for (int32_t track : bone_track) {
		if (track >= 0) {
			count++;
		}
	}
	return count;
}

int32_t MMDAnimationPlayer::get_morph_track_count() const {
	return static_cast<int32_t>(morph_tracks.size());
}

int32_t MMDAnimationPlayer::get_ik_count() const {
	return static_cast<int32_t>(ik_bones.size());
}

String MMDAnimationPlayer::get_vmd_model_name() const {
	return vmd.model_name;
}
