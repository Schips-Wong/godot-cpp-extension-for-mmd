#include "bullet_world.h"

#include <algorithm>

#include "godot_cpp/classes/object.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/math.hpp"

#include "bullet_constraint.h"
#include "bullet_rigid_body.h"
#include "bullet_util.h"

using namespace godot;

namespace {

struct ContactEvent {
	BulletRigidBody *first = nullptr;
	BulletRigidBody *second = nullptr;
	Vector3 position;
	Vector3 normal;
	float impulse = 0.0f;
};

Dictionary make_hit_dictionary(const btVector3 &p_point, const btVector3 &p_normal, btScalar p_fraction, btCollisionObject *p_collision_object) {
	Dictionary hit;
	hit["hit"] = true;
	hit["position"] = bullet_util::to_godot(p_point);
	hit["normal"] = bullet_util::to_godot(p_normal);
	hit["fraction"] = static_cast<real_t>(p_fraction);
	Object *collider = (p_collision_object != nullptr) ? static_cast<Object *>(p_collision_object->getUserPointer()) : nullptr;
	hit["collider"] = collider;
	return hit;
}

class BulletClosestRayCallback : public btCollisionWorld::ClosestRayResultCallback {
public:
	BulletClosestRayCallback(const btVector3 &p_from, const btVector3 &p_to) :
			btCollisionWorld::ClosestRayResultCallback(p_from, p_to) {}
};

class BulletAllHitsRayCallback : public btCollisionWorld::AllHitsRayResultCallback {
public:
	BulletAllHitsRayCallback(const btVector3 &p_from, const btVector3 &p_to) :
			btCollisionWorld::AllHitsRayResultCallback(p_from, p_to) {}
};

} // namespace

BulletWorld::BulletWorld() {
	_create_world();
}

BulletWorld::~BulletWorld() {
	_destroy_world();
}

void BulletWorld::_bind_methods() {
	ClassDB::bind_method(D_METHOD("step_simulation", "delta"), &BulletWorld::step_simulation);

	ClassDB::bind_method(D_METHOD("add_rigid_body", "body"), &BulletWorld::add_rigid_body);
	ClassDB::bind_method(D_METHOD("remove_rigid_body", "body"), &BulletWorld::remove_rigid_body);
	ClassDB::bind_method(D_METHOD("get_rigid_bodies"), &BulletWorld::get_rigid_bodies);
	ClassDB::bind_method(D_METHOD("get_rigid_body_count"), &BulletWorld::get_rigid_body_count);

	ClassDB::bind_method(D_METHOD("add_constraint", "constraint"), &BulletWorld::add_constraint);
	ClassDB::bind_method(D_METHOD("remove_constraint", "constraint"), &BulletWorld::remove_constraint);
	ClassDB::bind_method(D_METHOD("get_constraints"), &BulletWorld::get_constraints);
	ClassDB::bind_method(D_METHOD("get_constraint_count"), &BulletWorld::get_constraint_count);

	ClassDB::bind_method(D_METHOD("get_gravity"), &BulletWorld::get_gravity);
	ClassDB::bind_method(D_METHOD("set_gravity", "gravity"), &BulletWorld::set_gravity);
	ClassDB::bind_method(D_METHOD("get_max_sub_steps"), &BulletWorld::get_max_sub_steps);
	ClassDB::bind_method(D_METHOD("set_max_sub_steps", "steps"), &BulletWorld::set_max_sub_steps);
	ClassDB::bind_method(D_METHOD("get_fixed_timestep"), &BulletWorld::get_fixed_timestep);
	ClassDB::bind_method(D_METHOD("set_fixed_timestep", "timestep"), &BulletWorld::set_fixed_timestep);
	ClassDB::bind_method(D_METHOD("is_auto_step_enabled"), &BulletWorld::is_auto_step_enabled);
	ClassDB::bind_method(D_METHOD("set_auto_step", "enabled"), &BulletWorld::set_auto_step);
	ClassDB::bind_method(D_METHOD("is_contact_monitor_enabled"), &BulletWorld::is_contact_monitor_enabled);
	ClassDB::bind_method(D_METHOD("set_contact_monitor", "enabled"), &BulletWorld::set_contact_monitor);

	ClassDB::bind_method(D_METHOD("ray_test", "from", "to", "collision_mask", "collision_layer"), &BulletWorld::ray_test, DEFVAL(-1), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("ray_test_all", "from", "to", "collision_mask", "collision_layer"), &BulletWorld::ray_test_all, DEFVAL(-1), DEFVAL(-1));

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "gravity", PROPERTY_HINT_RANGE, "-100,100,0.001,or_less,or_greater"), "set_gravity", "get_gravity");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_sub_steps", PROPERTY_HINT_RANGE, "1,64,1,or_greater"), "set_max_sub_steps", "get_max_sub_steps");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fixed_timestep", PROPERTY_HINT_RANGE, "0.0001,0.1,0.0001"), "set_fixed_timestep", "get_fixed_timestep");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "auto_step"), "set_auto_step", "is_auto_step_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "contact_monitor"), "set_contact_monitor", "is_contact_monitor_enabled");
}

void BulletWorld::_create_world() {
	collision_configuration = new btDefaultCollisionConfiguration();
	dispatcher = new btCollisionDispatcher(collision_configuration);
	broadphase = new btDbvtBroadphase();
	solver = new btSequentialImpulseConstraintSolver();
	dynamics_world = new btDiscreteDynamicsWorld(dispatcher, broadphase, solver, collision_configuration);
	dynamics_world->setGravity(bullet_util::to_bt(gravity));
}

void BulletWorld::_destroy_world() {
	if (dynamics_world != nullptr) {
		for (BulletConstraint *constraint : constraints) {
			if (constraint != nullptr) {
				constraint->teardown();
			}
		}
		delete dynamics_world;
		dynamics_world = nullptr;
	}
	if (solver != nullptr) {
		delete solver;
		solver = nullptr;
	}
	if (broadphase != nullptr) {
		delete broadphase;
		broadphase = nullptr;
	}
	if (dispatcher != nullptr) {
		delete dispatcher;
		dispatcher = nullptr;
	}
	if (collision_configuration != nullptr) {
		delete collision_configuration;
		collision_configuration = nullptr;
	}
	constraints.clear();
	bodies.clear();
}

void BulletWorld::_ready() {
	dynamics_world->setGravity(bullet_util::to_bt(gravity));
	// Bodies are created in their own _enter_tree / _ready, so constraints can be built safely now.
	for (BulletConstraint *constraint : constraints) {
		if (constraint != nullptr) {
			constraint->setup();
		}
	}
}

void BulletWorld::_physics_process(double p_delta) {
	if (auto_step) {
		step_simulation(p_delta);
	}
}

void BulletWorld::step_simulation(double p_delta) {
	if (dynamics_world == nullptr) {
		return;
	}

	for (BulletRigidBody *body : bodies) {
		body->_prepare_step();
	}

	dynamics_world->stepSimulation(
			static_cast<btScalar>(p_delta),
			max_sub_steps,
			static_cast<btScalar>(fixed_timestep));

	for (BulletRigidBody *body : bodies) {
		body->_finish_step();
	}

	if (contact_monitor) {
		_process_contacts();
	}
}

void BulletWorld::_process_contacts() {
	if (dispatcher == nullptr) {
		return;
	}

	for (BulletRigidBody *body : bodies) {
		body->colliding_bodies.clear();
	}

	std::vector<ContactEvent> events;
	const int32_t manifold_count = dispatcher->getNumManifolds();
	for (int32_t i = 0; i < manifold_count; i++) {
		btPersistentManifold *manifold = dispatcher->getManifoldByIndexInternal(i);
		if (manifold == nullptr) {
			continue;
		}
		BulletRigidBody *first = static_cast<BulletRigidBody *>(const_cast<btCollisionObject *>(manifold->getBody0())->getUserPointer());
		BulletRigidBody *second = static_cast<BulletRigidBody *>(const_cast<btCollisionObject *>(manifold->getBody1())->getUserPointer());
		if (first == nullptr || second == nullptr) {
			continue;
		}

		bool touching = false;
		const int32_t contact_count = manifold->getNumContacts();
		for (int32_t c = 0; c < contact_count; c++) {
			const btManifoldPoint &point = manifold->getContactPoint(c);
			if (point.getDistance() > 0.0f) {
				continue;
			}
			touching = true;
			if (point.m_appliedImpulse == btScalar(0.0)) {
				continue;
			}
			ContactEvent event;
			event.first = first;
			event.second = second;
			event.position = bullet_util::to_godot(point.m_positionWorldOnB);
			event.normal = bullet_util::to_godot(point.m_normalWorldOnB);
			event.impulse = static_cast<float>(point.m_appliedImpulse);
			events.push_back(event);
		}

		if (!touching) {
			continue;
		}
		first->colliding_bodies.insert(second);
		second->colliding_bodies.insert(first);
	}

	for (BulletRigidBody *body : bodies) {
		for (BulletRigidBody *other : body->colliding_bodies) {
			if (body->previous_colliding_bodies.find(other) == body->previous_colliding_bodies.end()) {
				body->emit_signal("body_entered", other);
			}
		}
		for (BulletRigidBody *other : body->previous_colliding_bodies) {
			if (body->colliding_bodies.find(other) == body->colliding_bodies.end()) {
				body->emit_signal("body_exited", other);
			}
		}
		body->previous_colliding_bodies = body->colliding_bodies;
	}

	for (const ContactEvent &event : events) {
		if (event.first != nullptr) {
			event.first->emit_signal("contact_point", event.second, event.position, event.normal, event.impulse);
		}
		if (event.second != nullptr) {
			event.second->emit_signal("contact_point", event.first, event.position, event.normal, event.impulse);
		}
	}
}

void BulletWorld::_attach_body(BulletRigidBody *p_body) {
	if (dynamics_world == nullptr || p_body == nullptr || p_body->body == nullptr) {
		return;
	}
	dynamics_world->addRigidBody(
			p_body->body,
			static_cast<short>(p_body->collision_layer),
			static_cast<short>(p_body->collision_mask));
	p_body->body->activate(true);
}

void BulletWorld::_detach_body(BulletRigidBody *p_body) {
	if (dynamics_world == nullptr || p_body == nullptr || p_body->body == nullptr) {
		return;
	}
	dynamics_world->removeRigidBody(p_body->body);
}

void BulletWorld::add_rigid_body(BulletRigidBody *p_body) {
	if (p_body == nullptr) {
		return;
	}
	if (std::find(bodies.begin(), bodies.end(), p_body) != bodies.end()) {
		return;
	}
	bodies.push_back(p_body);
	p_body->world = this;
	p_body->_ensure_body();
	_attach_body(p_body);
}

void BulletWorld::remove_rigid_body(BulletRigidBody *p_body) {
	if (p_body == nullptr) {
		return;
	}
	const auto it = std::find(bodies.begin(), bodies.end(), p_body);
	if (it == bodies.end()) {
		return;
	}
	_detach_body(p_body);
	bodies.erase(it);
	p_body->world = nullptr;
	p_body->previous_colliding_bodies.clear();
}

Array BulletWorld::get_rigid_bodies() const {
	Array result;
	for (BulletRigidBody *body : bodies) {
		result.push_back(body);
	}
	return result;
}

int32_t BulletWorld::get_rigid_body_count() const {
	return static_cast<int32_t>(bodies.size());
}

void BulletWorld::add_constraint(BulletConstraint *p_constraint) {
	if (p_constraint == nullptr) {
		return;
	}
	if (std::find(constraints.begin(), constraints.end(), p_constraint) != constraints.end()) {
		return;
	}
	constraints.push_back(p_constraint);
	p_constraint->world = this;
	// While the scene is still loading the bodies may not exist yet, _ready() will build it later.
	if (is_node_ready()) {
		p_constraint->setup();
	}
}

void BulletWorld::remove_constraint(BulletConstraint *p_constraint) {
	if (p_constraint == nullptr) {
		return;
	}
	const auto it = std::find(constraints.begin(), constraints.end(), p_constraint);
	if (it == constraints.end()) {
		return;
	}
	p_constraint->teardown();
	constraints.erase(it);
	p_constraint->world = nullptr;
}

Array BulletWorld::get_constraints() const {
	Array result;
	for (BulletConstraint *constraint : constraints) {
		result.push_back(constraint);
	}
	return result;
}

int32_t BulletWorld::get_constraint_count() const {
	return static_cast<int32_t>(constraints.size());
}

Vector3 BulletWorld::get_gravity() const {
	return gravity;
}

void BulletWorld::set_gravity(const Vector3 &p_gravity) {
	gravity = p_gravity;
	if (dynamics_world != nullptr) {
		dynamics_world->setGravity(bullet_util::to_bt(gravity));
		for (BulletRigidBody *body : bodies) {
			body->body->activate(true);
		}
	}
}

int32_t BulletWorld::get_max_sub_steps() const {
	return max_sub_steps;
}

void BulletWorld::set_max_sub_steps(int32_t p_steps) {
	max_sub_steps = MAX(p_steps, 1);
}

double BulletWorld::get_fixed_timestep() const {
	return fixed_timestep;
}

void BulletWorld::set_fixed_timestep(double p_timestep) {
	fixed_timestep = MAX(p_timestep, 0.0001);
}

bool BulletWorld::is_auto_step_enabled() const {
	return auto_step;
}

void BulletWorld::set_auto_step(bool p_enabled) {
	auto_step = p_enabled;
}

bool BulletWorld::is_contact_monitor_enabled() const {
	return contact_monitor;
}

void BulletWorld::set_contact_monitor(bool p_enabled) {
	contact_monitor = p_enabled;
}

Dictionary BulletWorld::ray_test(const Vector3 &p_from, const Vector3 &p_to, int32_t p_collision_mask, int32_t p_collision_layer) const {
	Dictionary result;
	result["hit"] = false;
	if (dynamics_world == nullptr) {
		return result;
	}

	const btVector3 from = bullet_util::to_bt(p_from);
	const btVector3 to = bullet_util::to_bt(p_to);
	BulletClosestRayCallback callback(from, to);
	callback.m_collisionFilterMask = static_cast<short>(p_collision_mask < 0 ? btBroadphaseProxy::AllFilter : p_collision_mask);
	callback.m_collisionFilterGroup = static_cast<short>(p_collision_layer < 0 ? btBroadphaseProxy::DefaultFilter : p_collision_layer);

	dynamics_world->rayTest(from, to, callback);

	if (callback.hasHit()) {
		result = make_hit_dictionary(callback.m_hitPointWorld, callback.m_hitNormalWorld, callback.m_closestHitFraction, const_cast<btCollisionObject *>(callback.m_collisionObject));
	}
	return result;
}

Array BulletWorld::ray_test_all(const Vector3 &p_from, const Vector3 &p_to, int32_t p_collision_mask, int32_t p_collision_layer) const {
	Array result;
	if (dynamics_world == nullptr) {
		return result;
	}

	const btVector3 from = bullet_util::to_bt(p_from);
	const btVector3 to = bullet_util::to_bt(p_to);
	BulletAllHitsRayCallback callback(from, to);
	callback.m_collisionFilterMask = static_cast<short>(p_collision_mask < 0 ? btBroadphaseProxy::AllFilter : p_collision_mask);
	callback.m_collisionFilterGroup = static_cast<short>(p_collision_layer < 0 ? btBroadphaseProxy::DefaultFilter : p_collision_layer);

	dynamics_world->rayTest(from, to, callback);

	const int32_t hit_count = static_cast<int32_t>(callback.m_hitPointWorld.size());
	for (int32_t i = 0; i < hit_count; i++) {
		Dictionary hit = make_hit_dictionary(
				callback.m_hitPointWorld[i],
				callback.m_hitNormalWorld[i],
				callback.m_hitFractions[i],
				const_cast<btCollisionObject *>(callback.m_collisionObjects[i]));
		result.push_back(hit);
	}
	return result;
}
