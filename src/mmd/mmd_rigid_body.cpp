#include "mmd_rigid_body.h"

#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/math.hpp"

#include "../bullet/bullet_collision_shape.h"
#include "../bullet/bullet_util.h"
#include "mmd_physics_world.h"

using namespace godot;

VARIANT_ENUM_CAST(MMDRigidBody::BoneMode);
VARIANT_ENUM_CAST(MMDRigidBody::ShapeType);

void MMDRigidBody::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_bone_name"), &MMDRigidBody::get_bone_name);
	ClassDB::bind_method(D_METHOD("set_bone_name", "bone_name"), &MMDRigidBody::set_bone_name);
	ClassDB::bind_method(D_METHOD("get_bone_index"), &MMDRigidBody::get_bone_index);
	ClassDB::bind_method(D_METHOD("set_bone_index", "bone_index"), &MMDRigidBody::set_bone_index);
	ClassDB::bind_method(D_METHOD("has_bone"), &MMDRigidBody::has_bone);
	ClassDB::bind_method(D_METHOD("get_bone_mode"), &MMDRigidBody::get_bone_mode);
	ClassDB::bind_method(D_METHOD("set_bone_mode", "bone_mode"), &MMDRigidBody::set_bone_mode);
	ClassDB::bind_method(D_METHOD("get_shape_type"), &MMDRigidBody::get_shape_type);
	ClassDB::bind_method(D_METHOD("set_shape_type", "shape_type"), &MMDRigidBody::set_shape_type);
	ClassDB::bind_method(D_METHOD("get_size"), &MMDRigidBody::get_size);
	ClassDB::bind_method(D_METHOD("set_size", "size"), &MMDRigidBody::set_size);
	ClassDB::bind_method(D_METHOD("rebuild_shape_from_size"), &MMDRigidBody::rebuild_shape_from_size);
	ClassDB::bind_method(D_METHOD("get_group"), &MMDRigidBody::get_group);
	ClassDB::bind_method(D_METHOD("set_group", "group"), &MMDRigidBody::set_group);
	ClassDB::bind_method(D_METHOD("get_group_mask"), &MMDRigidBody::get_group_mask);
	ClassDB::bind_method(D_METHOD("set_group_mask", "mask"), &MMDRigidBody::set_group_mask);
	ClassDB::bind_method(D_METHOD("get_bone_offset"), &MMDRigidBody::get_bone_offset);
	ClassDB::bind_method(D_METHOD("set_bone_offset", "offset"), &MMDRigidBody::set_bone_offset);
	ClassDB::bind_method(D_METHOD("reset_bone_offset"), &MMDRigidBody::reset_bone_offset);
	ClassDB::bind_method(D_METHOD("get_bone_pose"), &MMDRigidBody::get_bone_pose);
	ClassDB::bind_method(D_METHOD("reset_to_bone"), &MMDRigidBody::reset_to_bone);
	ClassDB::bind_method(D_METHOD("clear_bone_override"), &MMDRigidBody::clear_bone_override);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "bone_name"), "set_bone_name", "get_bone_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bone_index"), "set_bone_index", "get_bone_index");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bone_mode", PROPERTY_HINT_ENUM, "Follow Bone,Physics,Physics With Bone Offset"), "set_bone_mode", "get_bone_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "shape_type", PROPERTY_HINT_ENUM, "Sphere,Box,Capsule"), "set_shape_type", "get_shape_type");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "group", PROPERTY_HINT_RANGE, "0,15,1"), "set_group", "get_group");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "group_mask"), "set_group_mask", "get_group_mask");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "bone_offset"), "set_bone_offset", "get_bone_offset");

	BIND_ENUM_CONSTANT(BONE_FOLLOW_BONE);
	BIND_ENUM_CONSTANT(BONE_PHYSICS);
	BIND_ENUM_CONSTANT(BONE_PHYSICS_WITH_BONE_OFFSET);

	BIND_ENUM_CONSTANT(SHAPE_SPHERE);
	BIND_ENUM_CONSTANT(SHAPE_BOX);
	BIND_ENUM_CONSTANT(SHAPE_CAPSULE);
}

MMDRigidBody::MMDRigidBody() = default;

void MMDRigidBody::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_update_shape_from_mmd();
			_apply_bone_mode();
			_apply_mmd_settings();
			skeleton = nullptr;
			resolved_bone = -1;
			bone_warning_printed = false;
		} break;
		case NOTIFICATION_EXIT_TREE: {
			skeleton = nullptr;
			resolved_bone = -1;
		} break;
		default:
			break;
	}

	BulletRigidBody::_notification(p_what);
}

void MMDRigidBody::rebuild() {
	BulletRigidBody::rebuild();

	btRigidBody *created = _get_bt_body();
	if (created == nullptr) {
		return;
	}
	// MMD bodies must never fall asleep, otherwise bones freeze mid animation.
	created->setSleepingThresholds(btScalar(0.0), btScalar(0.0));
	if (bone_mode != BONE_FOLLOW_BONE) {
		created->forceActivationState(DISABLE_DEACTIVATION);
	}
	created->setUserPointer(this);
}

// ---------------------------------------------------------------------------
// Bone binding
// ---------------------------------------------------------------------------

MMDPhysicsWorld *MMDRigidBody::_find_mmd_world() const {
	Node *node = get_parent();
	while (node != nullptr) {
		MMDPhysicsWorld *found = Object::cast_to<MMDPhysicsWorld>(node);
		if (found != nullptr) {
			return found;
		}
		node = node->get_parent();
	}
	return nullptr;
}

bool MMDRigidBody::_ensure_bone() const {
	if (skeleton != nullptr && resolved_bone >= 0) {
		return true;
	}

	MMDPhysicsWorld *physics_world = _find_mmd_world();
	if (physics_world != nullptr) {
		skeleton = physics_world->get_skeleton();
	}
	if (skeleton == nullptr) {
		return false;
	}

	if (bone_index >= 0) {
		resolved_bone = bone_index;
	} else if (!bone_name.is_empty()) {
		resolved_bone = skeleton->find_bone(bone_name);
	}

	if (resolved_bone < 0 || resolved_bone >= skeleton->get_bone_count()) {
		resolved_bone = -1;
		if (!bone_warning_printed) {
			bone_warning_printed = true;
			WARN_PRINT(vformat("%s: bone '%s' was not found in the skeleton.", get_path(), bone_name));
		}
		return false;
	}
	return true;
}

Transform3D MMDRigidBody::_get_bone_world_pose() const {
	if (skeleton == nullptr || resolved_bone < 0) {
		return Transform3D();
	}
	const Transform3D skeleton_transform = skeleton->get_global_transform();
	const Transform3D bone_pose = skeleton->get_bone_global_pose_no_override(resolved_bone);
	return skeleton_transform * bone_pose;
}

void MMDRigidBody::_ensure_bone_offset() {
	if (bone_offset_valid) {
		return;
	}
	btRigidBody *current = _get_bt_body();
	if (current == nullptr) {
		return;
	}
	bone_offset = bullet_util::to_godot(current->getWorldTransform()) * _get_bone_world_pose().affine_inverse();
	bone_offset_valid = true;
}

void MMDRigidBody::_sync_body_from_bone() {
	btRigidBody *current = _get_bt_body();
	if (current == nullptr) {
		return;
	}
	_ensure_bone_offset();

	const btTransform target = bullet_util::to_bt(bone_offset * _get_bone_world_pose());
	motion_state->world_transform = target;
	motion_state->transform_dirty = false;
	current->setWorldTransform(target);
	current->setInterpolationWorldTransform(target);
}

void MMDRigidBody::_prepare_step() {
	btRigidBody *current = _get_bt_body();
	if (current == nullptr) {
		return;
	}

	if (!_ensure_bone()) {
		BulletRigidBody::_prepare_step();
		return;
	}

	if (bone_mode != BONE_FOLLOW_BONE) {
		_ensure_bone_offset();
		return;
	}
	_sync_body_from_bone();
}

void MMDRigidBody::_finish_step() {
	btRigidBody *current = _get_bt_body();
	if (bone_mode == BONE_FOLLOW_BONE || current == nullptr) {
		return;
	}
	if (!_ensure_bone()) {
		BulletRigidBody::_finish_step();
		return;
	}

	_ensure_bone_offset();

	const Transform3D body_world = bullet_util::to_godot(current->getWorldTransform());
	Transform3D bone_world = body_world;
	if (bone_offset_valid) {
		bone_world = bone_offset.affine_inverse() * body_world;
	}
	const Transform3D skeleton_space = skeleton->get_global_transform().affine_inverse() * bone_world;
	skeleton->set_bone_global_pose_override(resolved_bone, skeleton_space, 1.0, true);
}

// ---------------------------------------------------------------------------
// Shape / parameters
// ---------------------------------------------------------------------------

void MMDRigidBody::_update_shape_from_mmd() {
	if (!shape_dirty) {
		return;
	}
	shape_dirty = false;

	Ref<BulletCollisionShape> created;
	switch (shape_type) {
		case SHAPE_SPHERE: {
			Ref<BulletSphereShape> sphere;
			sphere.instantiate();
			sphere->set_radius(MAX(size.x, 0.001f));
			created = sphere;
		} break;
		case SHAPE_CAPSULE: {
			Ref<BulletCapsuleShape> capsule;
			capsule.instantiate();
			capsule->set_radius(MAX(size.x, 0.001f));
			capsule->set_height(MAX(size.y, 0.002f));
			created = capsule;
		} break;
		case SHAPE_BOX:
		default: {
			Ref<BulletBoxShape> box;
			box.instantiate();
			box->set_half_extents(Vector3(MAX(size.x, 0.001f), MAX(size.y, 0.001f), MAX(size.z, 0.001f)));
			created = box;
		} break;
	}

	set_shape(created);
}

void MMDRigidBody::_apply_bone_mode() {
	const BulletRigidBody::Mode target = (bone_mode == BONE_FOLLOW_BONE) ? MODE_KINEMATIC : MODE_DYNAMIC;
	if (get_mode() != target) {
		set_mode(target);
	}
}

void MMDRigidBody::_apply_mmd_settings() {
	set_collision_layer(1 << (CLAMP(group, 0, 15)));
	set_collision_mask(group_mask);
}

void MMDRigidBody::rebuild_shape_from_size() {
	shape_dirty = true;
	_update_shape_from_mmd();
}

// ---------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------

String MMDRigidBody::get_bone_name() const {
	return bone_name;
}

void MMDRigidBody::set_bone_name(const String &p_name) {
	if (bone_name == p_name) {
		return;
	}
	bone_name = p_name;
	skeleton = nullptr;
	resolved_bone = -1;
	bone_warning_printed = false;
	bone_offset_valid = false;
}

int32_t MMDRigidBody::get_bone_index() const {
	return bone_index;
}

void MMDRigidBody::set_bone_index(int32_t p_index) {
	if (bone_index == p_index) {
		return;
	}
	bone_index = p_index;
	skeleton = nullptr;
	resolved_bone = -1;
	bone_warning_printed = false;
	bone_offset_valid = false;
}

bool MMDRigidBody::has_bone() const {
	return _ensure_bone();
}

MMDRigidBody::BoneMode MMDRigidBody::get_bone_mode() const {
	return bone_mode;
}

void MMDRigidBody::set_bone_mode(BoneMode p_mode) {
	if (bone_mode == p_mode) {
		return;
	}
	bone_mode = p_mode;
	// set_mode() rebuilds the body when we are already registered.
	_apply_bone_mode();
}

MMDRigidBody::ShapeType MMDRigidBody::get_shape_type() const {
	return shape_type;
}

void MMDRigidBody::set_shape_type(ShapeType p_type) {
	if (shape_type == p_type) {
		return;
	}
	shape_type = p_type;
	rebuild_shape_from_size();
}

Vector3 MMDRigidBody::get_size() const {
	return size;
}

void MMDRigidBody::set_size(const Vector3 &p_size) {
	if (size == p_size) {
		return;
	}
	size = p_size;
	rebuild_shape_from_size();
}

int32_t MMDRigidBody::get_group() const {
	return group;
}

void MMDRigidBody::set_group(int32_t p_group) {
	const int32_t value = CLAMP(p_group, 0, 15);
	if (group == value) {
		return;
	}
	group = value;
	set_collision_layer(1 << group);
}

int32_t MMDRigidBody::get_group_mask() const {
	return group_mask;
}

void MMDRigidBody::set_group_mask(int32_t p_mask) {
	const int32_t value = p_mask & 0xFFFF;
	if (group_mask == value) {
		return;
	}
	group_mask = value;
	set_collision_mask(group_mask);
}

Transform3D MMDRigidBody::get_bone_offset() const {
	return bone_offset;
}

void MMDRigidBody::set_bone_offset(const Transform3D &p_offset) {
	bone_offset = p_offset;
	bone_offset_valid = true;
}

void MMDRigidBody::reset_bone_offset() {
	if (!_ensure_bone()) {
		return;
	}
	btRigidBody *current = _get_bt_body();
	if (current == nullptr) {
		return;
	}
	bone_offset = bullet_util::to_godot(current->getWorldTransform()) * _get_bone_world_pose().affine_inverse();
	bone_offset_valid = true;
}

Transform3D MMDRigidBody::get_bone_pose() const {
	if (skeleton == nullptr || resolved_bone < 0) {
		return Transform3D();
	}
	return skeleton->get_bone_global_pose(resolved_bone);
}

void MMDRigidBody::clear_bone_override() {
	if (skeleton != nullptr && resolved_bone >= 0) {
		skeleton->set_bone_global_pose_override(resolved_bone, Transform3D(), 0.0, false);
	}
}

void MMDRigidBody::reset_to_bone() {
	btRigidBody *current = _get_bt_body();
	if (current == nullptr || !_ensure_bone()) {
		return;
	}
	if (!bone_offset_valid) {
		_ensure_bone_offset();
	}

	const btTransform target = bullet_util::to_bt(bone_offset * _get_bone_world_pose());
	current->setWorldTransform(target);
	current->setInterpolationWorldTransform(target);
	current->setLinearVelocity(btVector3(0, 0, 0));
	current->setAngularVelocity(btVector3(0, 0, 0));
	current->clearForces();
	current->activate(true);
	motion_state->world_transform = target;
	motion_state->transform_dirty = false;
}
