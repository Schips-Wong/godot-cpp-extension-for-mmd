#pragma once

#include "godot_cpp/classes/skeleton3d.hpp"

#include "../bullet/bullet_world.h"

using namespace godot;

// A BulletWorld whose rigid bodies are bound to the bones of a Skeleton3D.
//
// The per frame order is the one MMD expects:
//   bones (animation) -> bodies -> solver -> bones
// which is exactly what BulletWorld::step_simulation() does, because the MMD bodies
// override the pre/post step hooks of BulletRigidBody.
class MMDPhysicsWorld : public BulletWorld {
	GDCLASS(MMDPhysicsWorld, BulletWorld)

protected:
	static void _bind_methods();

	NodePath skeleton_path;
	mutable Skeleton3D *skeleton = nullptr;
	bool simulation_enabled = true;

	Skeleton3D *_resolve_skeleton() const;

public:
	MMDPhysicsWorld();
	~MMDPhysicsWorld() override = default;

	void _ready() override;
	void _physics_process(double p_delta) override;

	// Resolved lazily so bodies can query it while the scene is still being built.
	Skeleton3D *get_skeleton() const;
	void set_skeleton_path(const NodePath &p_path);
	NodePath get_skeleton_path() const;

	// Returns -1 when the bone is unknown.
	int32_t find_bone(const String &p_name) const;

	// Drops every bound body back onto its bone and clears its velocities.
	void reset_physics();

	void set_simulation_enabled(bool p_enabled);
	bool is_simulation_enabled() const;
};
