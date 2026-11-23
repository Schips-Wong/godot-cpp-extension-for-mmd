#pragma once

#include <btBulletDynamicsCommon.h>

#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#include <vector>

#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/variant/array.hpp"

#include "bullet_collision_shape.h"

using namespace godot;

class BulletWorld;
class BulletRigidBody;

// A sensor volume (btGhostObject) that reports which bodies overlap it.
//
// It follows its own node transform, does not affect the simulation, and refreshes its
// overlap list after every world step (so it needs a BulletWorld above it).
class BulletGhostBody : public Node3D {
	GDCLASS(BulletGhostBody, Node3D)

protected:
	static void _bind_methods();
	void _notification(int p_what);

	friend class BulletWorld;

	Ref<BulletCollisionShape> shape;
	int32_t collision_layer = 1;
	int32_t collision_mask = 0xFFFFFFFF;
	bool monitoring = true;

	btGhostObject *ghost = nullptr;
	BulletWorld *world = nullptr;

	std::vector<BulletRigidBody *> overlapping;
	std::vector<BulletRigidBody *> previous_overlapping;

	BulletWorld *_find_world() const;
	bool _ensure_ghost();
	void _destroy_ghost();
	// Called by BulletWorld while it tears itself down, so the node never touches it again.
	void _detach_from_world();
	void _sync_transform();
	void _update_overlaps();

public:
	BulletGhostBody();
	~BulletGhostBody() override;

	Ref<BulletCollisionShape> get_shape() const;
	void set_shape(const Ref<BulletCollisionShape> &p_shape);

	int32_t get_collision_layer() const;
	void set_collision_layer(int32_t p_layer);
	int32_t get_collision_mask() const;
	void set_collision_mask(int32_t p_mask);

	bool is_monitoring() const;
	void set_monitoring(bool p_enabled);

	bool is_overlapping_body(const Node *p_body) const;
	Array get_overlapping_bodies() const;
	int32_t get_overlapping_body_count() const;
};
