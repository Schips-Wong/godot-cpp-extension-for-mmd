#pragma once

#include <btBulletDynamicsCommon.h>

#include <BulletCollision/CollisionDispatch/btGhostObject.h>

#include <vector>

#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/variant/array.hpp"
#include "godot_cpp/variant/dictionary.hpp"
#include "godot_cpp/variant/transform3d.hpp"
#include "godot_cpp/variant/vector3.hpp"

#include "bullet_collision_shape.h"

using namespace godot;

class BulletRigidBody;
class BulletConstraint;
class BulletGhostBody;

// Owns a btDiscreteDynamicsWorld and simulates every BulletRigidBody / BulletConstraint below it.
class BulletWorld : public Node3D {
	GDCLASS(BulletWorld, Node3D)

protected:
	static void _bind_methods();

	friend class BulletRigidBody;
	friend class BulletConstraint;
	friend class BulletGhostBody;

	btDefaultCollisionConfiguration *collision_configuration = nullptr;
	btCollisionDispatcher *dispatcher = nullptr;
	btBroadphaseInterface *broadphase = nullptr;
	btSequentialImpulseConstraintSolver *solver = nullptr;
	btDiscreteDynamicsWorld *dynamics_world = nullptr;
	btGhostPairCallback *ghost_pair_callback = nullptr;

	std::vector<BulletRigidBody *> bodies;
	std::vector<BulletConstraint *> constraints;
	std::vector<BulletGhostBody *> ghosts;

	Vector3 gravity = Vector3(0.0, -9.8, 0.0);
	int32_t max_sub_steps = 1;
	double fixed_timestep = 1.0 / 60.0;
	bool auto_step = true;
	bool contact_monitor = false;

	void _create_world();
	void _destroy_world();
	void _process_contacts();
	void _attach_body(BulletRigidBody *p_body);
	void _detach_body(BulletRigidBody *p_body);

public:
	BulletWorld();
	~BulletWorld() override;

	void _ready() override;
	void _physics_process(double p_delta) override;

	// Advances the simulation. Called automatically in _physics_process when auto_step is enabled.
	void step_simulation(double p_delta);

	void add_rigid_body(BulletRigidBody *p_body);
	void remove_rigid_body(BulletRigidBody *p_body);
	Array get_rigid_bodies() const;
	int32_t get_rigid_body_count() const;

	void add_constraint(BulletConstraint *p_constraint);
	void remove_constraint(BulletConstraint *p_constraint);
	Array get_constraints() const;
	int32_t get_constraint_count() const;

	void add_ghost_body(BulletGhostBody *p_ghost);
	void remove_ghost_body(BulletGhostBody *p_ghost);
	Array get_ghost_bodies() const;
	int32_t get_ghost_body_count() const;

	Vector3 get_gravity() const;
	void set_gravity(const Vector3 &p_gravity);

	int32_t get_max_sub_steps() const;
	void set_max_sub_steps(int32_t p_steps);

	double get_fixed_timestep() const;
	void set_fixed_timestep(double p_timestep);

	bool is_auto_step_enabled() const;
	void set_auto_step(bool p_enabled);

	bool is_contact_monitor_enabled() const;
	void set_contact_monitor(bool p_enabled);

	// Casts a ray in world space and returns the closest hit as a Dictionary
	// { hit, position, normal, fraction, collider }.
	Dictionary ray_test(const Vector3 &p_from, const Vector3 &p_to, int32_t p_collision_mask = -1, int32_t p_collision_layer = -1) const;
	// Same as ray_test() but returns every hit along the ray as an Array of Dictionaries.
	Array ray_test_all(const Vector3 &p_from, const Vector3 &p_to, int32_t p_collision_mask = -1, int32_t p_collision_layer = -1) const;

	// Sweeps a convex shape from p_from to p_to. Returns the closest hit as a Dictionary
	// { hit, position, normal, fraction, start, end, collider }. Only convex shapes are supported.
	Dictionary convex_sweep(const Ref<BulletCollisionShape> &p_shape, const Transform3D &p_from, const Transform3D &p_to, int32_t p_collision_mask = -1, int32_t p_collision_layer = -1) const;
	// Same as convex_sweep() but returns every hit as an Array of Dictionaries.
	Array convex_sweep_all(const Ref<BulletCollisionShape> &p_shape, const Transform3D &p_from, const Transform3D &p_to, int32_t p_collision_mask = -1, int32_t p_collision_layer = -1) const;

	// Places the shape at p_transform and reports touching contacts as an Array of Dictionaries
	// { position, normal, distance, collider }.
	Array contact_test(const Ref<BulletCollisionShape> &p_shape, const Transform3D &p_transform, int32_t p_collision_mask = -1, int32_t p_collision_layer = -1) const;
	// Places the shape at p_transform and returns the colliders it overlaps (each once).
	Array overlap_test(const Ref<BulletCollisionShape> &p_shape, const Transform3D &p_transform, int32_t p_collision_mask = -1, int32_t p_collision_layer = -1) const;
};
