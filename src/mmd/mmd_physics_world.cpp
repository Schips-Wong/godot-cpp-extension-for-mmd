#include "mmd_physics_world.h"

#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/variant/array.hpp"

#include "mmd_rigid_body.h"

using namespace godot;

void MMDPhysicsWorld::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_skeleton"), &MMDPhysicsWorld::get_skeleton);
	ClassDB::bind_method(D_METHOD("set_skeleton_path", "path"), &MMDPhysicsWorld::set_skeleton_path);
	ClassDB::bind_method(D_METHOD("get_skeleton_path"), &MMDPhysicsWorld::get_skeleton_path);
	ClassDB::bind_method(D_METHOD("find_bone", "bone_name"), &MMDPhysicsWorld::find_bone);
	ClassDB::bind_method(D_METHOD("reset_physics"), &MMDPhysicsWorld::reset_physics);
	ClassDB::bind_method(D_METHOD("set_simulation_enabled", "enabled"), &MMDPhysicsWorld::set_simulation_enabled);
	ClassDB::bind_method(D_METHOD("is_simulation_enabled"), &MMDPhysicsWorld::is_simulation_enabled);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "skeleton", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "Skeleton3D"), "set_skeleton_path", "get_skeleton_path");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "simulation_enabled"), "set_simulation_enabled", "is_simulation_enabled");
}

MMDPhysicsWorld::MMDPhysicsWorld() = default;

void MMDPhysicsWorld::_ready() {
	BulletWorld::_ready();
	_resolve_skeleton();
}

void MMDPhysicsWorld::_physics_process(double p_delta) {
	if (!simulation_enabled) {
		return;
	}
	BulletWorld::_physics_process(p_delta);
}

Skeleton3D *MMDPhysicsWorld::_resolve_skeleton() const {
	if (skeleton != nullptr) {
		return skeleton;
	}
	if (!is_inside_tree()) {
		return nullptr;
	}

	Node *candidate = nullptr;
	if (!skeleton_path.is_empty()) {
		candidate = get_node_or_null(skeleton_path);
	} else {
		// Without an explicit path: the parent, or a Skeleton3D sibling of ours.
		Node *parent = get_parent();
		if (parent != nullptr) {
			candidate = Object::cast_to<Skeleton3D>(parent);
			if (candidate == nullptr) {
				const int32_t count = parent->get_child_count();
				for (int32_t i = 0; i < count && candidate == nullptr; i++) {
					candidate = Object::cast_to<Skeleton3D>(parent->get_child(i));
				}
			}
		}
	}

	skeleton = Object::cast_to<Skeleton3D>(candidate);
	if (skeleton == nullptr) {
		WARN_PRINT(vformat("%s: no Skeleton3D found, bone driven physics is disabled.", get_path()));
	}
	return skeleton;
}

Skeleton3D *MMDPhysicsWorld::get_skeleton() const {
	return _resolve_skeleton();
}

void MMDPhysicsWorld::set_skeleton_path(const NodePath &p_path) {
	if (skeleton_path == p_path) {
		return;
	}
	skeleton_path = p_path;
	skeleton = nullptr;
	_resolve_skeleton();
}

NodePath MMDPhysicsWorld::get_skeleton_path() const {
	return skeleton_path;
}

int32_t MMDPhysicsWorld::find_bone(const String &p_name) const {
	Skeleton3D *resolved = get_skeleton();
	if (resolved == nullptr) {
		return -1;
	}
	return resolved->find_bone(p_name);
}

void MMDPhysicsWorld::reset_physics() {
	const Array bodies = get_rigid_bodies();
	for (int32_t i = 0; i < bodies.size(); i++) {
		MMDRigidBody *body = Object::cast_to<MMDRigidBody>(bodies[i]);
		if (body != nullptr) {
			body->reset_to_bone();
		}
	}
}

void MMDPhysicsWorld::set_simulation_enabled(bool p_enabled) {
	if (simulation_enabled == p_enabled) {
		return;
	}
	simulation_enabled = p_enabled;
	if (!p_enabled) {
		// Hand the bones back to the animation.
		const Array bodies = get_rigid_bodies();
		for (int32_t i = 0; i < bodies.size(); i++) {
			MMDRigidBody *body = Object::cast_to<MMDRigidBody>(bodies[i]);
			if (body != nullptr) {
				body->clear_bone_override();
			}
		}
	}
}

bool MMDPhysicsWorld::is_simulation_enabled() const {
	return simulation_enabled;
}
