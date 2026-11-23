#include "bullet_rigid_body.h"

#include "godot_cpp/classes/object.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/math.hpp"

#include "bullet_world.h"

using namespace godot;

VARIANT_ENUM_CAST(BulletRigidBody::Mode);
VARIANT_ENUM_CAST(BulletRigidBody::ActivationState);

BulletRigidBody::BulletRigidBody() = default;

BulletRigidBody::~BulletRigidBody() {
	_unregister_from_world();
	if (body != nullptr) {
		delete body;
		body = nullptr;
	}
	if (motion_state != nullptr) {
		delete motion_state;
		motion_state = nullptr;
	}
}

void BulletRigidBody::_bind_methods() {
	ClassDB::bind_method(D_METHOD("rebuild"), &BulletRigidBody::rebuild);
	ClassDB::bind_method(D_METHOD("get_body_transform"), &BulletRigidBody::get_body_transform);
	ClassDB::bind_method(D_METHOD("set_body_transform", "transform"), &BulletRigidBody::set_body_transform);

	ClassDB::bind_method(D_METHOD("get_shape"), &BulletRigidBody::get_shape);
	ClassDB::bind_method(D_METHOD("set_shape", "shape"), &BulletRigidBody::set_shape);
	ClassDB::bind_method(D_METHOD("get_mode"), &BulletRigidBody::get_mode);
	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &BulletRigidBody::set_mode);
	ClassDB::bind_method(D_METHOD("get_mass"), &BulletRigidBody::get_mass);
	ClassDB::bind_method(D_METHOD("set_mass", "mass"), &BulletRigidBody::set_mass);
	ClassDB::bind_method(D_METHOD("get_friction"), &BulletRigidBody::get_friction);
	ClassDB::bind_method(D_METHOD("set_friction", "friction"), &BulletRigidBody::set_friction);
	ClassDB::bind_method(D_METHOD("get_restitution"), &BulletRigidBody::get_restitution);
	ClassDB::bind_method(D_METHOD("set_restitution", "restitution"), &BulletRigidBody::set_restitution);
	ClassDB::bind_method(D_METHOD("get_rolling_friction"), &BulletRigidBody::get_rolling_friction);
	ClassDB::bind_method(D_METHOD("set_rolling_friction", "friction"), &BulletRigidBody::set_rolling_friction);
	ClassDB::bind_method(D_METHOD("get_linear_damping"), &BulletRigidBody::get_linear_damping);
	ClassDB::bind_method(D_METHOD("set_linear_damping", "damping"), &BulletRigidBody::set_linear_damping);
	ClassDB::bind_method(D_METHOD("get_angular_damping"), &BulletRigidBody::get_angular_damping);
	ClassDB::bind_method(D_METHOD("set_angular_damping", "damping"), &BulletRigidBody::set_angular_damping);
	ClassDB::bind_method(D_METHOD("get_collision_layer"), &BulletRigidBody::get_collision_layer);
	ClassDB::bind_method(D_METHOD("set_collision_layer", "layer"), &BulletRigidBody::set_collision_layer);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &BulletRigidBody::get_collision_mask);
	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &BulletRigidBody::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_inertia"), &BulletRigidBody::get_inertia);
	ClassDB::bind_method(D_METHOD("set_inertia", "inertia"), &BulletRigidBody::set_inertia);
	ClassDB::bind_method(D_METHOD("is_using_custom_gravity"), &BulletRigidBody::is_using_custom_gravity);
	ClassDB::bind_method(D_METHOD("set_use_custom_gravity", "enabled"), &BulletRigidBody::set_use_custom_gravity);
	ClassDB::bind_method(D_METHOD("get_custom_gravity"), &BulletRigidBody::get_custom_gravity);
	ClassDB::bind_method(D_METHOD("set_custom_gravity", "gravity"), &BulletRigidBody::set_custom_gravity);
	ClassDB::bind_method(D_METHOD("get_activation_state"), &BulletRigidBody::get_activation_state);
	ClassDB::bind_method(D_METHOD("set_activation_state", "state"), &BulletRigidBody::set_activation_state);
	ClassDB::bind_method(D_METHOD("get_linear_velocity"), &BulletRigidBody::get_linear_velocity);
	ClassDB::bind_method(D_METHOD("set_linear_velocity", "velocity"), &BulletRigidBody::set_linear_velocity);
	ClassDB::bind_method(D_METHOD("get_angular_velocity"), &BulletRigidBody::get_angular_velocity);
	ClassDB::bind_method(D_METHOD("set_angular_velocity", "velocity"), &BulletRigidBody::set_angular_velocity);
	ClassDB::bind_method(D_METHOD("is_in_world"), &BulletRigidBody::is_in_world);
	ClassDB::bind_method(D_METHOD("is_sleeping"), &BulletRigidBody::is_sleeping);
	ClassDB::bind_method(D_METHOD("set_sleeping", "sleeping"), &BulletRigidBody::set_sleeping);
	ClassDB::bind_method(D_METHOD("apply_central_impulse", "impulse"), &BulletRigidBody::apply_central_impulse);
	ClassDB::bind_method(D_METHOD("apply_impulse", "impulse", "relative_position"), &BulletRigidBody::apply_impulse, DEFVAL(Vector3()));
	ClassDB::bind_method(D_METHOD("apply_torque_impulse", "impulse"), &BulletRigidBody::apply_torque_impulse);
	ClassDB::bind_method(D_METHOD("apply_central_force", "force"), &BulletRigidBody::apply_central_force);
	ClassDB::bind_method(D_METHOD("apply_torque", "torque"), &BulletRigidBody::apply_torque);
	ClassDB::bind_method(D_METHOD("clear_forces"), &BulletRigidBody::clear_forces);
	ClassDB::bind_method(D_METHOD("get_colliding_bodies"), &BulletRigidBody::get_colliding_bodies);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "shape", PROPERTY_HINT_RESOURCE_TYPE, "BulletCollisionShape"), "set_shape", "get_shape");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Static,Kinematic,Dynamic"), "set_mode", "get_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mass", PROPERTY_HINT_RANGE, "0,10000,0.001,or_greater"), "set_mass", "get_mass");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "friction", PROPERTY_HINT_RANGE, "0,10,0.001,or_greater"), "set_friction", "get_friction");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "restitution", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_restitution", "get_restitution");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rolling_friction", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_rolling_friction", "get_rolling_friction");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "linear_damping", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_linear_damping", "get_linear_damping");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "angular_damping", PROPERTY_HINT_RANGE, "0,1,0.001"), "set_angular_damping", "get_angular_damping");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "inertia", PROPERTY_HINT_LINK), "set_inertia", "get_inertia");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_custom_gravity"), "set_use_custom_gravity", "is_using_custom_gravity");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "custom_gravity", PROPERTY_HINT_RANGE, "-100,100,0.001,or_less,or_greater"), "set_custom_gravity", "get_custom_gravity");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "activation_state", PROPERTY_HINT_ENUM, "Active,Island Sleeping,Wants Deactivation,Disable Deactivation,Disable Simulation"), "set_activation_state", "get_activation_state");

	ADD_SIGNAL(MethodInfo("body_entered", PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_RESOURCE_TYPE, "BulletRigidBody")));
	ADD_SIGNAL(MethodInfo("body_exited", PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_RESOURCE_TYPE, "BulletRigidBody")));
	ADD_SIGNAL(MethodInfo("contact_point",
			PropertyInfo(Variant::OBJECT, "body", PROPERTY_HINT_RESOURCE_TYPE, "BulletRigidBody"),
			PropertyInfo(Variant::VECTOR3, "position"),
			PropertyInfo(Variant::VECTOR3, "normal"),
			PropertyInfo(Variant::FLOAT, "impulse")));

	BIND_ENUM_CONSTANT(MODE_STATIC);
	BIND_ENUM_CONSTANT(MODE_KINEMATIC);
	BIND_ENUM_CONSTANT(MODE_DYNAMIC);

	BIND_ENUM_CONSTANT(STATE_ACTIVE);
	BIND_ENUM_CONSTANT(STATE_ISLAND_SLEEPING);
	BIND_ENUM_CONSTANT(STATE_WANTS_DEACTIVATION);
	BIND_ENUM_CONSTANT(STATE_DISABLE_DEACTIVATION);
	BIND_ENUM_CONSTANT(STATE_DISABLE_SIMULATION);
}

void BulletRigidBody::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_register_to_world();
			_ensure_body();
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_unregister_from_world();
		} break;
		default:
			break;
	}
}

BulletWorld *BulletRigidBody::_find_world() const {
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

void BulletRigidBody::_register_to_world() {
	if (world != nullptr) {
		return;
	}
	BulletWorld *found = _find_world();
	if (found != nullptr) {
		found->add_rigid_body(this);
	}
}

void BulletRigidBody::_unregister_from_world() {
	if (world != nullptr) {
		world->remove_rigid_body(this);
		world = nullptr;
	}
}

void BulletRigidBody::_ensure_body() {
	if (body != nullptr && !body_dirty) {
		return;
	}
	rebuild();
}

void BulletRigidBody::rebuild() {
	BulletWorld *current_world = world;
	if (current_world != nullptr && body != nullptr) {
		current_world->remove_rigid_body(this);
	}

	if (body != nullptr) {
		delete body;
		body = nullptr;
	}
	if (motion_state != nullptr) {
		delete motion_state;
		motion_state = nullptr;
	}

	body_dirty = false;

	if (shape.is_null()) {
		if (current_world != nullptr) {
			current_world->add_rigid_body(this);
		}
		return;
	}

	btCollisionShape *collision_shape = shape->get_bt_shape();
	if (collision_shape == nullptr) {
		if (current_world != nullptr) {
			current_world->add_rigid_body(this);
		}
		return;
	}

	const btScalar body_mass = (mode == MODE_DYNAMIC) ? static_cast<btScalar>(MAX(mass, 0.0f)) : static_cast<btScalar>(0.0);
	btVector3 inertia(0, 0, 0);
	if (body_mass > 0.0) {
		if (inertia_override != Vector3()) {
			inertia = bullet_util::to_bt(inertia_override);
		} else {
			collision_shape->calculateLocalInertia(body_mass, inertia);
		}
	}

	motion_state = new BulletBodyMotionState(this, bullet_util::to_bt(get_global_transform()));

	btRigidBody::btRigidBodyConstructionInfo construction_info(body_mass, motion_state, collision_shape, inertia);
	btRigidBody *created = new btRigidBody(construction_info);
	created->setUserPointer(this);

	if (mode == MODE_STATIC) {
		created->setCollisionFlags(created->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
	} else if (mode == MODE_KINEMATIC) {
		created->setCollisionFlags(created->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
	}

	body = created;
	_apply_common_properties();

	if (current_world != nullptr) {
		current_world->add_rigid_body(this);
	}
}

Transform3D BulletRigidBody::get_body_transform() const {
	if (body == nullptr) {
		return get_global_transform();
	}
	return bullet_util::to_godot(body->getWorldTransform());
}

void BulletRigidBody::set_body_transform(const Transform3D &p_transform) {
	if (body == nullptr) {
		return;
	}
	const btTransform transform = bullet_util::to_bt(p_transform);
	body->setWorldTransform(transform);
	body->setInterpolationWorldTransform(transform);
	if (motion_state != nullptr) {
		motion_state->world_transform = transform;
		motion_state->transform_dirty = false;
	}
	body->activate(true);
}

void BulletRigidBody::_apply_common_properties() {
	if (body == nullptr) {
		return;
	}
	body->setFriction(static_cast<btScalar>(friction));
	body->setRestitution(static_cast<btScalar>(restitution));
	body->setRollingFriction(static_cast<btScalar>(rolling_friction));
	body->setDamping(static_cast<btScalar>(linear_damping), static_cast<btScalar>(angular_damping));
	body->setGravity(use_custom_gravity ? bullet_util::to_bt(custom_gravity) : btVector3(0, 0, 0));
	body->setLinearVelocity(bullet_util::to_bt(linear_velocity));
	body->setAngularVelocity(bullet_util::to_bt(angular_velocity));
	body->forceActivationState(static_cast<int>(activation_state));
}

void BulletRigidBody::_prepare_step() {
	if (body == nullptr) {
		return;
	}
	if (mode != MODE_DYNAMIC) {
		// Static and kinematic bodies follow the node transform.
		const btTransform node_transform = bullet_util::to_bt(get_global_transform());
		motion_state->world_transform = node_transform;
		body->setWorldTransform(node_transform);
		body->setInterpolationWorldTransform(node_transform);
	}
}

void BulletRigidBody::_finish_step() {
	if (body == nullptr || mode != MODE_DYNAMIC) {
		return;
	}
	if (!motion_state->transform_dirty) {
		return;
	}
	motion_state->transform_dirty = false;
	set_global_transform(bullet_util::to_godot(motion_state->world_transform));
}

Ref<BulletCollisionShape> BulletRigidBody::get_shape() const {
	return shape;
}

void BulletRigidBody::set_shape(const Ref<BulletCollisionShape> &p_shape) {
	if (shape == p_shape) {
		return;
	}
	shape = p_shape;
	body_dirty = true;
	if (world != nullptr) {
		rebuild();
	}
}

BulletRigidBody::Mode BulletRigidBody::get_mode() const {
	return mode;
}

void BulletRigidBody::set_mode(Mode p_mode) {
	if (mode == p_mode) {
		return;
	}
	mode = p_mode;
	body_dirty = true;
	if (world != nullptr) {
		rebuild();
	}
}

float BulletRigidBody::get_mass() const {
	return mass;
}

void BulletRigidBody::set_mass(float p_mass) {
	float value = MAX(p_mass, 0.0f);
	if (Math::is_equal_approx(mass, value)) {
		return;
	}
	mass = value;
	body_dirty = true;
	if (world != nullptr) {
		rebuild();
	}
}

float BulletRigidBody::get_friction() const {
	return friction;
}

void BulletRigidBody::set_friction(float p_friction) {
	friction = p_friction;
	if (body != nullptr) {
		body->setFriction(static_cast<btScalar>(friction));
	}
}

float BulletRigidBody::get_restitution() const {
	return restitution;
}

void BulletRigidBody::set_restitution(float p_restitution) {
	restitution = p_restitution;
	if (body != nullptr) {
		body->setRestitution(static_cast<btScalar>(restitution));
	}
}

float BulletRigidBody::get_rolling_friction() const {
	return rolling_friction;
}

void BulletRigidBody::set_rolling_friction(float p_rolling_friction) {
	rolling_friction = p_rolling_friction;
	if (body != nullptr) {
		body->setRollingFriction(static_cast<btScalar>(rolling_friction));
	}
}

float BulletRigidBody::get_linear_damping() const {
	return linear_damping;
}

void BulletRigidBody::set_linear_damping(float p_linear_damping) {
	linear_damping = p_linear_damping;
	if (body != nullptr) {
		body->setDamping(static_cast<btScalar>(linear_damping), static_cast<btScalar>(angular_damping));
	}
}

float BulletRigidBody::get_angular_damping() const {
	return angular_damping;
}

void BulletRigidBody::set_angular_damping(float p_angular_damping) {
	angular_damping = p_angular_damping;
	if (body != nullptr) {
		body->setDamping(static_cast<btScalar>(linear_damping), static_cast<btScalar>(angular_damping));
	}
}

int32_t BulletRigidBody::get_collision_layer() const {
	return collision_layer;
}

void BulletRigidBody::set_collision_layer(int32_t p_layer) {
	const int32_t value = MAX(p_layer, 1);
	if (collision_layer == value) {
		return;
	}
	collision_layer = value;
	if (world != nullptr && body != nullptr) {
		BulletWorld *current_world = world;
		current_world->remove_rigid_body(this);
		current_world->add_rigid_body(this);
	}
}

int32_t BulletRigidBody::get_collision_mask() const {
	return collision_mask;
}

void BulletRigidBody::set_collision_mask(int32_t p_mask) {
	if (collision_mask == p_mask) {
		return;
	}
	collision_mask = p_mask;
	if (world != nullptr && body != nullptr) {
		BulletWorld *current_world = world;
		current_world->remove_rigid_body(this);
		current_world->add_rigid_body(this);
	}
}

Vector3 BulletRigidBody::get_inertia() const {
	return inertia_override;
}

void BulletRigidBody::set_inertia(const Vector3 &p_inertia) {
	if (inertia_override == p_inertia) {
		return;
	}
	inertia_override = p_inertia;
	body_dirty = true;
	if (world != nullptr) {
		rebuild();
	}
}

bool BulletRigidBody::is_using_custom_gravity() const {
	return use_custom_gravity;
}

void BulletRigidBody::set_use_custom_gravity(bool p_enabled) {
	if (use_custom_gravity == p_enabled) {
		return;
	}
	use_custom_gravity = p_enabled;
	if (body != nullptr) {
		body->setGravity(use_custom_gravity ? bullet_util::to_bt(custom_gravity) : btVector3(0, 0, 0));
	}
}

Vector3 BulletRigidBody::get_custom_gravity() const {
	return custom_gravity;
}

void BulletRigidBody::set_custom_gravity(const Vector3 &p_gravity) {
	custom_gravity = p_gravity;
	if (body != nullptr && use_custom_gravity) {
		body->setGravity(bullet_util::to_bt(custom_gravity));
	}
}

BulletRigidBody::ActivationState BulletRigidBody::get_activation_state() const {
	if (body != nullptr) {
		return static_cast<ActivationState>(body->getActivationState());
	}
	return activation_state;
}

void BulletRigidBody::set_activation_state(ActivationState p_state) {
	activation_state = p_state;
	if (body != nullptr) {
		body->forceActivationState(static_cast<int>(p_state));
		if (p_state == STATE_ACTIVE || p_state == STATE_DISABLE_DEACTIVATION) {
			body->activate(true);
		}
	}
}

Vector3 BulletRigidBody::get_linear_velocity() const {
	if (body != nullptr) {
		return bullet_util::to_godot(body->getLinearVelocity());
	}
	return linear_velocity;
}

void BulletRigidBody::set_linear_velocity(const Vector3 &p_velocity) {
	linear_velocity = p_velocity;
	if (body != nullptr) {
		body->setLinearVelocity(bullet_util::to_bt(p_velocity));
		body->activate(true);
	}
}

Vector3 BulletRigidBody::get_angular_velocity() const {
	if (body != nullptr) {
		return bullet_util::to_godot(body->getAngularVelocity());
	}
	return angular_velocity;
}

void BulletRigidBody::set_angular_velocity(const Vector3 &p_velocity) {
	angular_velocity = p_velocity;
	if (body != nullptr) {
		body->setAngularVelocity(bullet_util::to_bt(p_velocity));
		body->activate(true);
	}
}

bool BulletRigidBody::is_in_world() const {
	return world != nullptr && body != nullptr;
}

bool BulletRigidBody::is_sleeping() const {
	if (body == nullptr) {
		return false;
	}
	return !body->isActive();
}

void BulletRigidBody::set_sleeping(bool p_sleeping) {
	if (body == nullptr) {
		return;
	}
	if (p_sleeping) {
		body->forceActivationState(ISLAND_SLEEPING);
	} else {
		body->forceActivationState(ACTIVE_TAG);
		body->activate(true);
	}
}

void BulletRigidBody::apply_central_impulse(const Vector3 &p_impulse) {
	if (body != nullptr) {
		body->applyCentralImpulse(bullet_util::to_bt(p_impulse));
		body->activate(true);
	}
}

void BulletRigidBody::apply_impulse(const Vector3 &p_impulse, const Vector3 &p_relative_position) {
	if (body != nullptr) {
		body->applyImpulse(bullet_util::to_bt(p_impulse), bullet_util::to_bt(p_relative_position));
		body->activate(true);
	}
}

void BulletRigidBody::apply_torque_impulse(const Vector3 &p_impulse) {
	if (body != nullptr) {
		body->applyTorqueImpulse(bullet_util::to_bt(p_impulse));
		body->activate(true);
	}
}

void BulletRigidBody::apply_central_force(const Vector3 &p_force) {
	if (body != nullptr) {
		body->applyCentralForce(bullet_util::to_bt(p_force));
		body->activate(true);
	}
}

void BulletRigidBody::apply_torque(const Vector3 &p_torque) {
	if (body != nullptr) {
		body->applyTorque(bullet_util::to_bt(p_torque));
		body->activate(true);
	}
}

void BulletRigidBody::clear_forces() {
	if (body != nullptr) {
		body->clearForces();
	}
}

Array BulletRigidBody::get_colliding_bodies() const {
	Array result;
	for (BulletRigidBody *other : colliding_bodies) {
		if (other != nullptr) {
			result.push_back(other);
		}
	}
	return result;
}
