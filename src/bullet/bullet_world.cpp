#include "bullet_world.h"

#include <algorithm>

#include "godot_cpp/classes/object.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/math.hpp"

#include "bullet_constraint.h"
#include "bullet_ghost_body.h"
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

class BulletClosestConvexSweepCallback : public btCollisionWorld::ClosestConvexResultCallback {
public:
	BulletClosestConvexSweepCallback(const btVector3 &p_from, const btVector3 &p_to) :
			btCollisionWorld::ClosestConvexResultCallback(p_from, p_to) {}
};

class BulletAllHitsConvexSweepCallback : public btCollisionWorld::ConvexResultCallback {
public:
	struct Hit {
		btVector3 position;
		btVector3 normal;
		btScalar fraction = btScalar(1.0);
		const btCollisionObject *object = nullptr;
	};

	BulletAllHitsConvexSweepCallback(const btVector3 &p_from, const btVector3 &p_to) :
			from(p_from), to(p_to) {}

	btScalar addSingleResult(btCollisionWorld::LocalConvexResult &p_result, bool p_normal_in_world_space) override {
		Hit hit;
		hit.position = p_result.m_hitPointLocal;
		hit.normal = p_result.m_hitNormalLocal;
		if (!p_normal_in_world_space && p_result.m_hitCollisionObject != nullptr) {
			hit.normal = p_result.m_hitCollisionObject->getWorldTransform().getBasis() * p_result.m_hitNormalLocal;
		}
		hit.fraction = p_result.m_hitFraction;
		hit.object = p_result.m_hitCollisionObject;
		hits.push_back(hit);
		// Keep m_closestHitFraction untouched so every hit along the sweep is reported.
		return m_closestHitFraction;
	}

	btVector3 from;
	btVector3 to;
	std::vector<Hit> hits;
};

class BulletContactCollector : public btCollisionWorld::ContactResultCallback {
public:
	BulletContactCollector(btCollisionObject *p_probe, bool p_unique_objects) :
			probe(p_probe), unique_objects(p_unique_objects) {}

	btScalar addSingleResult(btManifoldPoint &p_point, const btCollisionObjectWrapper *p_object0, int32_t, int32_t, const btCollisionObjectWrapper *p_object1, int32_t, int32_t) override {
		btCollisionObject *const first = p_object0 != nullptr ? const_cast<btCollisionObject *>(p_object0->getCollisionObject()) : nullptr;
		btCollisionObject *const second = p_object1 != nullptr ? const_cast<btCollisionObject *>(p_object1->getCollisionObject()) : nullptr;
		btCollisionObject *other = (first == probe) ? second : first;
		if (other == nullptr || other == probe) {
			return 0.0;
		}

		if (unique_objects) {
			if (std::find(objects.begin(), objects.end(), other) == objects.end()) {
				objects.push_back(other);
			}
			return 0.0;
		}

		Dictionary contact;
		// Report the point that lies on the probe shape.
		if (first == probe) {
			contact["position"] = bullet_util::to_godot(p_point.m_positionWorldOnA);
			contact["normal"] = bullet_util::to_godot(-p_point.m_normalWorldOnB);
		} else {
			contact["position"] = bullet_util::to_godot(p_point.m_positionWorldOnB);
			contact["normal"] = bullet_util::to_godot(p_point.m_normalWorldOnB);
		}
		contact["distance"] = static_cast<real_t>(p_point.m_distance1);
		contact["collider"] = static_cast<Object *>(other->getUserPointer());
		contacts.push_back(contact);
		return 0.0;
	}

	btCollisionObject *probe = nullptr;
	bool unique_objects = false;
	std::vector<Dictionary> contacts;
	std::vector<btCollisionObject *> objects;
};

// Creates a temporary collision object so Bullet can run narrow phase queries against it.
class BulletProbeObject {
public:
	BulletProbeObject(btDiscreteDynamicsWorld *p_world, const btCollisionShape *p_shape, const btTransform &p_transform, int32_t p_layer, int32_t p_mask) :
			world(p_world) {
		object = new btCollisionObject();
		object->setCollisionShape(const_cast<btCollisionShape *>(p_shape));
		object->setWorldTransform(p_transform);
		object->setCollisionFlags(object->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);
		group = static_cast<short>(p_layer < 0 ? btBroadphaseProxy::DefaultFilter : p_layer);
		mask = static_cast<short>(p_mask < 0 ? btBroadphaseProxy::AllFilter : p_mask);
		world->addCollisionObject(object, group, mask);
	}

	~BulletProbeObject() {
		world->removeCollisionObject(object);
		delete object;
	}

	btCollisionObject *get_object() const { return object; }
	short get_group() const { return group; }
	short get_mask() const { return mask; }

private:
	btDiscreteDynamicsWorld *world = nullptr;
	btCollisionObject *object = nullptr;
	short group = 0;
	short mask = 0;
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
	ClassDB::bind_method(D_METHOD("convex_sweep", "shape", "from", "to", "collision_mask", "collision_layer"), &BulletWorld::convex_sweep, DEFVAL(-1), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("convex_sweep_all", "shape", "from", "to", "collision_mask", "collision_layer"), &BulletWorld::convex_sweep_all, DEFVAL(-1), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("contact_test", "shape", "transform", "collision_mask", "collision_layer"), &BulletWorld::contact_test, DEFVAL(-1), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("overlap_test", "shape", "transform", "collision_mask", "collision_layer"), &BulletWorld::overlap_test, DEFVAL(-1), DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_ghost_bodies"), &BulletWorld::get_ghost_bodies);
	ClassDB::bind_method(D_METHOD("get_ghost_body_count"), &BulletWorld::get_ghost_body_count);

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

	// Required for btGhostObject overlap tracking (BulletGhostBody).
	ghost_pair_callback = new btGhostPairCallback();
	broadphase->getOverlappingPairCache()->setInternalGhostPairCallback(ghost_pair_callback);
}

void BulletWorld::_destroy_world() {
	if (dynamics_world != nullptr) {
		for (BulletConstraint *constraint : constraints) {
			if (constraint != nullptr) {
				constraint->teardown();
			}
		}
		for (BulletGhostBody *ghost : ghosts) {
			if (ghost != nullptr) {
				ghost->_detach_from_world();
			}
		}
		ghosts.clear();
		delete dynamics_world;
		dynamics_world = nullptr;
	}
	if (solver != nullptr) {
		delete solver;
		solver = nullptr;
	}
	if (broadphase != nullptr) {
		broadphase->getOverlappingPairCache()->setInternalGhostPairCallback(nullptr);
	}
	if (ghost_pair_callback != nullptr) {
		delete ghost_pair_callback;
		ghost_pair_callback = nullptr;
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
	ghosts.clear();
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
	for (BulletGhostBody *ghost : ghosts) {
		ghost->_sync_transform();
	}

	dynamics_world->stepSimulation(
			static_cast<btScalar>(p_delta),
			max_sub_steps,
			static_cast<btScalar>(fixed_timestep));

	for (BulletRigidBody *body : bodies) {
		body->_finish_step();
	}
	for (BulletGhostBody *ghost : ghosts) {
		ghost->_update_overlaps();
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
		// The user pointer may belong to a ghost body or another non rigid body object.
		BulletRigidBody *first = Object::cast_to<BulletRigidBody>(static_cast<Object *>(const_cast<btCollisionObject *>(manifold->getBody0())->getUserPointer()));
		BulletRigidBody *second = Object::cast_to<BulletRigidBody>(static_cast<Object *>(const_cast<btCollisionObject *>(manifold->getBody1())->getUserPointer()));
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

Dictionary BulletWorld::convex_sweep(const Ref<BulletCollisionShape> &p_shape, const Transform3D &p_from, const Transform3D &p_to, int32_t p_collision_mask, int32_t p_collision_layer) const {
	Dictionary result;
	result["hit"] = false;
	if (dynamics_world == nullptr || p_shape.is_null()) {
		return result;
	}

	btCollisionShape *bt_shape = p_shape->get_bt_shape();
	if (bt_shape == nullptr || !bt_shape->isConvex()) {
		WARN_PRINT("BulletWorld::convex_sweep() needs a convex shape (box, sphere, capsule, cylinder, cone, convex hull).");
		return result;
	}

	const btTransform from = bullet_util::to_bt(p_from);
	const btTransform to = bullet_util::to_bt(p_to);
	BulletClosestConvexSweepCallback callback(from.getOrigin(), to.getOrigin());
	callback.m_collisionFilterMask = static_cast<short>(p_collision_mask < 0 ? btBroadphaseProxy::AllFilter : p_collision_mask);
	callback.m_collisionFilterGroup = static_cast<short>(p_collision_layer < 0 ? btBroadphaseProxy::DefaultFilter : p_collision_layer);

	dynamics_world->convexSweepTest(static_cast<const btConvexShape *>(bt_shape), from, to, callback);

	if (callback.hasHit()) {
		result = make_hit_dictionary(callback.m_hitPointWorld, callback.m_hitNormalWorld, callback.m_closestHitFraction, const_cast<btCollisionObject *>(callback.m_hitCollisionObject));
	}
	return result;
}

Array BulletWorld::convex_sweep_all(const Ref<BulletCollisionShape> &p_shape, const Transform3D &p_from, const Transform3D &p_to, int32_t p_collision_mask, int32_t p_collision_layer) const {
	Array result;
	if (dynamics_world == nullptr || p_shape.is_null()) {
		return result;
	}

	btCollisionShape *bt_shape = p_shape->get_bt_shape();
	if (bt_shape == nullptr || !bt_shape->isConvex()) {
		WARN_PRINT("BulletWorld::convex_sweep_all() needs a convex shape (box, sphere, capsule, cylinder, cone, convex hull).");
		return result;
	}

	const btTransform from = bullet_util::to_bt(p_from);
	const btTransform to = bullet_util::to_bt(p_to);
	BulletAllHitsConvexSweepCallback callback(from.getOrigin(), to.getOrigin());
	callback.m_collisionFilterMask = static_cast<short>(p_collision_mask < 0 ? btBroadphaseProxy::AllFilter : p_collision_mask);
	callback.m_collisionFilterGroup = static_cast<short>(p_collision_layer < 0 ? btBroadphaseProxy::DefaultFilter : p_collision_layer);

	dynamics_world->convexSweepTest(static_cast<const btConvexShape *>(bt_shape), from, to, callback);

	for (const BulletAllHitsConvexSweepCallback::Hit &hit : callback.hits) {
		result.push_back(make_hit_dictionary(hit.position, hit.normal, hit.fraction, const_cast<btCollisionObject *>(hit.object)));
	}
	return result;
}

Array BulletWorld::contact_test(const Ref<BulletCollisionShape> &p_shape, const Transform3D &p_transform, int32_t p_collision_mask, int32_t p_collision_layer) const {
	Array result;
	if (dynamics_world == nullptr || p_shape.is_null()) {
		return result;
	}

	btCollisionShape *bt_shape = p_shape->get_bt_shape();
	if (bt_shape == nullptr) {
		return result;
	}

	BulletProbeObject probe(dynamics_world, bt_shape, bullet_util::to_bt(p_transform), p_collision_layer, p_collision_mask);
	BulletContactCollector collector(probe.get_object(), false);
	collector.m_collisionFilterGroup = probe.get_group();
	collector.m_collisionFilterMask = probe.get_mask();

	dynamics_world->contactTest(probe.get_object(), collector);

	for (const Dictionary &contact : collector.contacts) {
		result.push_back(contact);
	}
	return result;
}

Array BulletWorld::overlap_test(const Ref<BulletCollisionShape> &p_shape, const Transform3D &p_transform, int32_t p_collision_mask, int32_t p_collision_layer) const {
	Array result;
	if (dynamics_world == nullptr || p_shape.is_null()) {
		return result;
	}

	btCollisionShape *bt_shape = p_shape->get_bt_shape();
	if (bt_shape == nullptr) {
		return result;
	}

	BulletProbeObject probe(dynamics_world, bt_shape, bullet_util::to_bt(p_transform), p_collision_layer, p_collision_mask);
	BulletContactCollector collector(probe.get_object(), true);
	collector.m_collisionFilterGroup = probe.get_group();
	collector.m_collisionFilterMask = probe.get_mask();

	dynamics_world->contactTest(probe.get_object(), collector);

	for (btCollisionObject *object : collector.objects) {
		Object *collider = static_cast<Object *>(object->getUserPointer());
		if (collider != nullptr) {
			result.push_back(collider);
		}
	}
	return result;
}

void BulletWorld::add_ghost_body(BulletGhostBody *p_ghost) {
	if (p_ghost == nullptr) {
		return;
	}
	if (std::find(ghosts.begin(), ghosts.end(), p_ghost) == ghosts.end()) {
		ghosts.push_back(p_ghost);
	}
}

void BulletWorld::remove_ghost_body(BulletGhostBody *p_ghost) {
	ghosts.erase(std::remove(ghosts.begin(), ghosts.end(), p_ghost), ghosts.end());
}

Array BulletWorld::get_ghost_bodies() const {
	Array result;
	for (BulletGhostBody *ghost : ghosts) {
		result.push_back(ghost);
	}
	return result;
}

int32_t BulletWorld::get_ghost_body_count() const {
	return static_cast<int32_t>(ghosts.size());
}
