#include "bullet_ghost_body.h"

#include <algorithm>

#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/math.hpp"

#include "bullet_rigid_body.h"
#include "bullet_util.h"
#include "bullet_world.h"

using namespace godot;

void BulletGhostBody::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_shape"), &BulletGhostBody::get_shape);
	ClassDB::bind_method(D_METHOD("set_shape", "shape"), &BulletGhostBody::set_shape);
	ClassDB::bind_method(D_METHOD("get_collision_layer"), &BulletGhostBody::get_collision_layer);
	ClassDB::bind_method(D_METHOD("set_collision_layer", "layer"), &BulletGhostBody::set_collision_layer);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &BulletGhostBody::get_collision_mask);
	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &BulletGhostBody::set_collision_mask);
	ClassDB::bind_method(D_METHOD("is_monitoring"), &BulletGhostBody::is_monitoring);
	ClassDB::bind_method(D_METHOD("set_monitoring", "enabled"), &BulletGhostBody::set_monitoring);
	ClassDB::bind_method(D_METHOD("is_overlapping_body", "body"), &BulletGhostBody::is_overlapping_body);
	ClassDB::bind_method(D_METHOD("get_overlapping_bodies"), &BulletGhostBody::get_overlapping_bodies);
	ClassDB::bind_method(D_METHOD("get_overlapping_body_count"), &BulletGhostBody::get_overlapping_body_count);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shape", PROPERTY_HINT_RESOURCE_TYPE, "BulletCollisionShape"), "set_shape", "get_shape");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "monitoring"), "set_monitoring", "is_monitoring");

	ADD_SIGNAL(MethodInfo("body_entered", PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_RESOURCE_TYPE, "BulletRigidBody")));
	ADD_SIGNAL(MethodInfo("body_exited", PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_RESOURCE_TYPE, "BulletRigidBody")));
}

BulletGhostBody::BulletGhostBody() = default;

BulletGhostBody::~BulletGhostBody() {
	_destroy_ghost();
}

void BulletGhostBody::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_ensure_ghost();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_destroy_ghost();
		} break;
		default:
			break;
	}
}

BulletWorld *BulletGhostBody::_find_world() const {
	Node *node = get_parent();
	while (node != nullptr) {
		BulletWorld *found = Object::cast_to<BulletWorld>(node);
		if (found != nullptr) {
			return found;
		}
		node = node->get_parent();
	}
	return nullptr;
}

bool BulletGhostBody::_ensure_ghost() {
	if (ghost != nullptr) {
		return true;
	}
	if (shape.is_null() || shape->get_bt_shape() == nullptr) {
		return false;
	}

	BulletWorld *found = _find_world();
	if (found == nullptr || found->dynamics_world == nullptr) {
		return false;
	}

	ghost = new btGhostObject();
	ghost->setCollisionShape(shape->get_bt_shape());
	ghost->setWorldTransform(bullet_util::to_bt(get_global_transform()));
	ghost->setUserPointer(this);

	world = found;
	world->dynamics_world->addCollisionObject(
			ghost,
			static_cast<short>(MAX(collision_layer, 1)),
			static_cast<short>(collision_mask));
	world->add_ghost_body(this);
	return true;
}

void BulletGhostBody::_destroy_ghost() {
	if (world != nullptr) {
		world->remove_ghost_body(this);
		if (world->dynamics_world != nullptr && ghost != nullptr) {
			world->dynamics_world->removeCollisionObject(ghost);
		}
		world = nullptr;
	}
	if (ghost != nullptr) {
		delete ghost;
		ghost = nullptr;
	}
	overlapping.clear();
	previous_overlapping.clear();
}

void BulletGhostBody::_detach_from_world() {
	if (ghost != nullptr && world != nullptr && world->dynamics_world != nullptr) {
		world->dynamics_world->removeCollisionObject(ghost);
	}
	if (ghost != nullptr) {
		delete ghost;
		ghost = nullptr;
	}
	world = nullptr;
	overlapping.clear();
	previous_overlapping.clear();
}

void BulletGhostBody::_sync_transform() {
	if (ghost == nullptr) {
		return;
	}
	ghost->setWorldTransform(bullet_util::to_bt(get_global_transform()));
}

void BulletGhostBody::_update_overlaps() {
	if (ghost == nullptr) {
		previous_overlapping.clear();
		return;
	}
	if (!monitoring) {
		overlapping.clear();
		previous_overlapping.clear();
		return;
	}

	overlapping.clear();
	const int32_t count = ghost->getNumOverlappingObjects();
	for (int32_t i = 0; i < count; i++) {
		btCollisionObject *object = ghost->getOverlappingObject(i);
		if (object == nullptr) {
			continue;
		}
		BulletRigidBody *body = Object::cast_to<BulletRigidBody>(static_cast<Object *>(object->getUserPointer()));
		if (body == nullptr) {
			continue;
		}
		if (std::find(overlapping.begin(), overlapping.end(), body) == overlapping.end()) {
			overlapping.push_back(body);
		}
	}

	for (BulletRigidBody *body : overlapping) {
		if (std::find(previous_overlapping.begin(), previous_overlapping.end(), body) == previous_overlapping.end()) {
			emit_signal("body_entered", body);
		}
	}
	for (BulletRigidBody *body : previous_overlapping) {
		if (std::find(overlapping.begin(), overlapping.end(), body) == overlapping.end()) {
			emit_signal("body_exited", body);
		}
	}
	previous_overlapping = overlapping;
}

// ---------------------------------------------------------------------------
// Properties
// ---------------------------------------------------------------------------

Ref<BulletCollisionShape> BulletGhostBody::get_shape() const {
	return shape;
}

void BulletGhostBody::set_shape(const Ref<BulletCollisionShape> &p_shape) {
	if (shape == p_shape) {
		return;
	}
	shape = p_shape;
	if (ghost != nullptr) {
		_destroy_ghost();
		_ensure_ghost();
	}
}

int32_t BulletGhostBody::get_collision_layer() const {
	return collision_layer;
}

void BulletGhostBody::set_collision_layer(int32_t p_layer) {
	const int32_t value = MAX(p_layer, 1);
	if (collision_layer == value) {
		return;
	}
	collision_layer = value;
	if (ghost != nullptr) {
		_destroy_ghost();
		_ensure_ghost();
	}
}

int32_t BulletGhostBody::get_collision_mask() const {
	return collision_mask;
}

void BulletGhostBody::set_collision_mask(int32_t p_mask) {
	if (collision_mask == p_mask) {
		return;
	}
	collision_mask = p_mask;
	if (ghost != nullptr) {
		_destroy_ghost();
		_ensure_ghost();
	}
}

bool BulletGhostBody::is_monitoring() const {
	return monitoring;
}

void BulletGhostBody::set_monitoring(bool p_enabled) {
	monitoring = p_enabled;
}

bool BulletGhostBody::is_overlapping_body(const Node *p_body) const {
	if (p_body == nullptr) {
		return false;
	}
	const BulletRigidBody *body = Object::cast_to<BulletRigidBody>(p_body);
	if (body == nullptr) {
		return false;
	}
	return std::find(overlapping.begin(), overlapping.end(), body) != overlapping.end();
}

Array BulletGhostBody::get_overlapping_bodies() const {
	Array result;
	for (BulletRigidBody *body : overlapping) {
		result.push_back(body);
	}
	return result;
}

int32_t BulletGhostBody::get_overlapping_body_count() const {
	return static_cast<int32_t>(overlapping.size());
}
