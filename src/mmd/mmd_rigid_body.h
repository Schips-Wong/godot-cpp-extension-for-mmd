#pragma once

#include "godot_cpp/classes/skeleton3d.hpp"

#include "../bullet/bullet_rigid_body.h"

using namespace godot;

class MMDPhysicsWorld;

// A rigid body that belongs to a bone of a PMX model.
//
// Placement follows the PMX convention: the node transform is the rigid body frame in
// model space, and `bone_offset` (body frame expressed in bone space, i.e.
// `body = bone_offset * bone`) is derived from it the first time the body is simulated.
// Importers can also set `bone_offset` explicitly.
class MMDRigidBody : public BulletRigidBody {
	GDCLASS(MMDRigidBody, BulletRigidBody)

public:
	enum BoneMode {
		BONE_FOLLOW_BONE = 0, // PMX mode 0: the bone drives the body (kinematic)
		BONE_PHYSICS = 1, // PMX mode 1: the body drives the bone
		BONE_PHYSICS_WITH_BONE_OFFSET = 2, // PMX mode 2: same as 1 for now
	};

	enum ShapeType {
		SHAPE_SPHERE = 0, // size.x = radius
		SHAPE_BOX = 1, // size = half extents
		SHAPE_CAPSULE = 2, // size.x = radius, size.y = height
	};

protected:
	static void _bind_methods();
	void _notification(int p_what);
	void _prepare_step() override;
	void _finish_step() override;
	void rebuild() override;

	String bone_name;
	int32_t bone_index = -1;
	BoneMode bone_mode = BONE_PHYSICS;

	ShapeType shape_type = SHAPE_BOX;
	Vector3 size = Vector3(0.5, 0.5, 0.5);
	bool shape_dirty = true;

	int32_t group = 0;
	int32_t group_mask = 0xFFFF;

	Transform3D bone_offset;
	bool bone_offset_valid = false;

	mutable Skeleton3D *skeleton = nullptr;
	mutable int32_t resolved_bone = -1;
	mutable bool bone_warning_printed = false;

	MMDPhysicsWorld *_find_mmd_world() const;
	bool _ensure_bone() const;
	void _ensure_bone_offset();
	Transform3D _get_bone_world_pose() const;
	void _update_shape_from_mmd();
	void _apply_bone_mode();
	void _apply_mmd_settings();
	void _sync_body_from_bone();

public:
	MMDRigidBody();
	~MMDRigidBody() override = default;

	String get_bone_name() const;
	void set_bone_name(const String &p_name);
	int32_t get_bone_index() const;
	void set_bone_index(int32_t p_index);
	bool has_bone() const;

	BoneMode get_bone_mode() const;
	void set_bone_mode(BoneMode p_mode);

	ShapeType get_shape_type() const;
	void set_shape_type(ShapeType p_type);
	Vector3 get_size() const;
	void set_size(const Vector3 &p_size);
	// Rebuilds the Bullet shape from shape_type/size (PMX rigid body data).
	void rebuild_shape_from_size();

	// PMX group 0..15 -> collision layer, PMX group mask -> collision mask.
	int32_t get_group() const;
	void set_group(int32_t p_group);
	int32_t get_group_mask() const;
	void set_group_mask(int32_t p_mask);

	Transform3D get_bone_offset() const;
	void set_bone_offset(const Transform3D &p_offset);
	// Derives the offset from the current bone pose and body transform.
	void reset_bone_offset();

	// Bone pose currently driven by this body, in skeleton space.
	Transform3D get_bone_pose() const;

	// Puts the body back on its bone and clears velocities.
	void reset_to_bone();

	// Removes the physics override from the bone (the animation drives it again).
	void clear_bone_override();
};
