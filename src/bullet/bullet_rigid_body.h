#pragma once

#include <btBulletDynamicsCommon.h>

#include <unordered_set>
#include <vector>

#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/transform3d.hpp"
#include "godot_cpp/variant/vector3.hpp"

#include "bullet_collision_shape.h"

using namespace godot;

class BulletWorld;
class BulletConstraint;
class BulletRigidBody;

// Bridges a Bullet rigid body with the transform of a Godot node.
class BulletBodyMotionState : public btMotionState {
public:
	BulletBodyMotionState(BulletRigidBody *p_owner, const btTransform &p_initial) :
			owner(p_owner), world_transform(p_initial) {}

	void getWorldTransform(btTransform &p_world_trans) const override {
		p_world_trans = world_transform;
	}

	void setWorldTransform(const btTransform &p_world_trans) override {
		world_transform = p_world_trans;
		transform_dirty = true;
	}

	BulletRigidBody *owner = nullptr;
	btTransform world_transform;
	bool transform_dirty = false;
};

// A Bullet rigid body living inside a BulletWorld.
// Mirrors BulletSharp's RigidBody: mass, friction, restitution, damping, velocities, activation state.
class BulletRigidBody : public Node3D {
	GDCLASS(BulletRigidBody, Node3D)

public:
	enum Mode {
		MODE_STATIC,
		MODE_KINEMATIC,
		MODE_DYNAMIC,
	};

	enum ActivationState {
		STATE_ACTIVE = 1,
		STATE_ISLAND_SLEEPING = 2,
		STATE_WANTS_DEACTIVATION = 3,
		STATE_DISABLE_DEACTIVATION = 4,
		STATE_DISABLE_SIMULATION = 5,
	};

protected:
	static void _bind_methods();
	void _notification(int p_what);

	friend class BulletWorld;
	friend class BulletConstraint;

	Ref<BulletCollisionShape> shape;
	Mode mode = MODE_DYNAMIC;
	float mass = 1.0f;
	float friction = 0.5f;
	float restitution = 0.0f;
	float rolling_friction = 0.0f;
	float linear_damping = 0.0f;
	float angular_damping = 0.0f;
	int32_t collision_layer = 1;
	int32_t collision_mask = 0xFFFFFFFF;
	Vector3 inertia_override;
	bool use_custom_gravity = false;
	Vector3 custom_gravity;
	ActivationState activation_state = STATE_ACTIVE;

	btRigidBody *body = nullptr;
	BulletBodyMotionState *motion_state = nullptr;
	BulletWorld *world = nullptr;
	bool body_dirty = true;

	// Cached values, applied to the body whenever it is (re)created.
	Vector3 linear_velocity;
	Vector3 angular_velocity;

	mutable std::unordered_set<BulletRigidBody *> colliding_bodies;
	std::unordered_set<BulletRigidBody *> previous_colliding_bodies;

	BulletWorld *_find_world() const;
	void _register_to_world();
	void _unregister_from_world();

	void _ensure_body();
	void _apply_common_properties();

	// Simulation hooks, overridden by bone driven bodies (see MMDRigidBody).
	// _prepare_step() runs before the world is stepped, _finish_step() afterwards.
	virtual void _prepare_step();
	virtual void _finish_step();

public:
	BulletRigidBody();
	~BulletRigidBody() override;

	// Internal use only (deliberately not exposed to scripts): the raw Bullet body.
	btRigidBody *_get_bt_body() const { return body; }

	// Destroys and recreates the underlying btRigidBody using the current properties.
	virtual void rebuild();

	Ref<BulletCollisionShape> get_shape() const;
	void set_shape(const Ref<BulletCollisionShape> &p_shape);

	Mode get_mode() const;
	void set_mode(Mode p_mode);

	float get_mass() const;
	void set_mass(float p_mass);

	float get_friction() const;
	void set_friction(float p_friction);

	float get_restitution() const;
	void set_restitution(float p_restitution);

	float get_rolling_friction() const;
	void set_rolling_friction(float p_rolling_friction);

	float get_linear_damping() const;
	void set_linear_damping(float p_linear_damping);

	float get_angular_damping() const;
	void set_angular_damping(float p_angular_damping);

	int32_t get_collision_layer() const;
	void set_collision_layer(int32_t p_layer);

	int32_t get_collision_mask() const;
	void set_collision_mask(int32_t p_mask);

	Vector3 get_inertia() const;
	void set_inertia(const Vector3 &p_inertia);

	bool is_using_custom_gravity() const;
	void set_use_custom_gravity(bool p_enabled);

	Vector3 get_custom_gravity() const;
	void set_custom_gravity(const Vector3 &p_gravity);

	ActivationState get_activation_state() const;
	void set_activation_state(ActivationState p_state);

	Vector3 get_linear_velocity() const;
	void set_linear_velocity(const Vector3 &p_velocity);

	Vector3 get_angular_velocity() const;
	void set_angular_velocity(const Vector3 &p_velocity);

	bool is_in_world() const;
	bool is_sleeping() const;
	void set_sleeping(bool p_sleeping);

	void apply_central_impulse(const Vector3 &p_impulse);
	void apply_impulse(const Vector3 &p_impulse, const Vector3 &p_relative_position = Vector3());
	void apply_torque_impulse(const Vector3 &p_impulse);
	void apply_central_force(const Vector3 &p_force);
	void apply_torque(const Vector3 &p_torque);
	void clear_forces();

	Array get_colliding_bodies() const;
};
