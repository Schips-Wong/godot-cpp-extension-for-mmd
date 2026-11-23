#include "bullet_constraint.h"

#include "godot_cpp/classes/object.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/error_macros.hpp"

#include "bullet_rigid_body.h"
#include "bullet_util.h"
#include "bullet_world.h"

using namespace godot;

// ---------------------------------------------------------------------------
// BulletConstraint
// ---------------------------------------------------------------------------

void BulletConstraint::_bind_methods() {
	ClassDB::bind_method(D_METHOD("setup"), &BulletConstraint::setup);
	ClassDB::bind_method(D_METHOD("teardown"), &BulletConstraint::teardown);
	ClassDB::bind_method(D_METHOD("is_set_up"), &BulletConstraint::is_set_up);
	ClassDB::bind_method(D_METHOD("apply_parameters"), &BulletConstraint::apply_parameters);

	ClassDB::bind_method(D_METHOD("get_body_a_path"), &BulletConstraint::get_body_a_path);
	ClassDB::bind_method(D_METHOD("set_body_a_path", "path"), &BulletConstraint::set_body_a_path);
	ClassDB::bind_method(D_METHOD("get_body_b_path"), &BulletConstraint::get_body_b_path);
	ClassDB::bind_method(D_METHOD("set_body_b_path", "path"), &BulletConstraint::set_body_b_path);
	ClassDB::bind_method(D_METHOD("get_body_a"), &BulletConstraint::get_body_a);
	ClassDB::bind_method(D_METHOD("get_body_b"), &BulletConstraint::get_body_b);

	ClassDB::bind_method(D_METHOD("is_enabled"), &BulletConstraint::is_enabled);
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &BulletConstraint::set_enabled);
	ClassDB::bind_method(D_METHOD("are_linked_body_collisions_disabled"), &BulletConstraint::are_linked_body_collisions_disabled);
	ClassDB::bind_method(D_METHOD("set_disable_collisions", "disabled"), &BulletConstraint::set_disable_collisions);
	ClassDB::bind_method(D_METHOD("get_breaking_threshold"), &BulletConstraint::get_breaking_threshold);
	ClassDB::bind_method(D_METHOD("set_breaking_threshold", "threshold"), &BulletConstraint::set_breaking_threshold);
	ClassDB::bind_method(D_METHOD("is_using_world_transform"), &BulletConstraint::is_using_world_transform);
	ClassDB::bind_method(D_METHOD("set_use_world_transform", "enabled"), &BulletConstraint::set_use_world_transform);
	ClassDB::bind_method(D_METHOD("get_frame_in_a"), &BulletConstraint::get_frame_in_a);
	ClassDB::bind_method(D_METHOD("set_frame_in_a", "frame"), &BulletConstraint::set_frame_in_a);
	ClassDB::bind_method(D_METHOD("get_frame_in_b"), &BulletConstraint::get_frame_in_b);
	ClassDB::bind_method(D_METHOD("set_frame_in_b", "frame"), &BulletConstraint::set_frame_in_b);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "body_a", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "BulletRigidBody"), "set_body_a_path", "get_body_a_path");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "body_b", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "BulletRigidBody"), "set_body_b_path", "get_body_b_path");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "disable_collisions"), "set_disable_collisions", "are_linked_body_collisions_disabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "breaking_threshold", PROPERTY_HINT_RANGE, "0,100000,0.01,or_greater"), "set_breaking_threshold", "get_breaking_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_world_transform"), "set_use_world_transform", "is_using_world_transform");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "frame_in_a"), "set_frame_in_a", "get_frame_in_a");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "frame_in_b"), "set_frame_in_b", "get_frame_in_b");
}

BulletConstraint::~BulletConstraint() {
	teardown();
}

void BulletConstraint::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (world == nullptr) {
				BulletWorld *found = _find_world();
				if (found != nullptr) {
					found->add_constraint(this);
				}
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (world != nullptr) {
				world->remove_constraint(this);
				world = nullptr;
			}
		} break;
		default:
			break;
	}
}

BulletWorld *BulletConstraint::_find_world() const {
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

void BulletConstraint::_resolve_bodies() {
	body_a = nullptr;
	body_b = nullptr;

	if (!body_a_path.is_empty()) {
		body_a = Object::cast_to<BulletRigidBody>(get_node_or_null(body_a_path));
	}
	if (!body_b_path.is_empty()) {
		body_b = Object::cast_to<BulletRigidBody>(get_node_or_null(body_b_path));
	}
}

bool BulletConstraint::_has_bodies() const {
	return body_a != nullptr && body_b != nullptr && body_a->_get_bt_body() != nullptr && body_b->_get_bt_body() != nullptr;
}

void BulletConstraint::setup() {
	teardown();

	_resolve_bodies();
	if (body_a == nullptr || body_b == nullptr) {
		WARN_PRINT(vformat("%s: both body_a and body_b must point to a BulletRigidBody.", get_path()));
		return;
	}
	if (!_has_bodies()) {
		return;
	}
	if (body_a->_get_bt_body() == body_b->_get_bt_body()) {
		WARN_PRINT(vformat("%s: body_a and body_b must be different bodies.", get_path()));
		return;
	}

	btTransform frame_a = bullet_util::to_bt(frame_in_a);
	btTransform frame_b = bullet_util::to_bt(frame_in_b);
	if (use_world_transform) {
		const btTransform joint = bullet_util::to_bt(get_global_transform());
		frame_a = body_a->_get_bt_body()->getWorldTransform().inverse() * joint;
		frame_b = body_b->_get_bt_body()->getWorldTransform().inverse() * joint;
	}

	constraint = _create_constraint(frame_a, frame_b);
	if (constraint == nullptr) {
		return;
	}

	_apply_parameters();

	if (world != nullptr && world->dynamics_world != nullptr) {
		world->dynamics_world->addConstraint(constraint, disable_collisions);
	}
}

void BulletConstraint::teardown() {
	if (constraint == nullptr) {
		return;
	}
	if (world != nullptr && world->dynamics_world != nullptr) {
		world->dynamics_world->removeConstraint(constraint);
	}
	delete constraint;
	constraint = nullptr;
}

bool BulletConstraint::is_set_up() const {
	return constraint != nullptr;
}

void BulletConstraint::_apply_parameters() {
	if (constraint == nullptr) {
		return;
	}
	constraint->setEnabled(enabled);
	if (breaking_threshold > 0.0f) {
		constraint->setBreakingImpulseThreshold(static_cast<btScalar>(breaking_threshold));
	} else {
		constraint->setBreakingImpulseThreshold(SIMD_INFINITY);
	}
}

void BulletConstraint::apply_parameters() {
	_apply_parameters();
}

NodePath BulletConstraint::get_body_a_path() const {
	return body_a_path;
}

void BulletConstraint::set_body_a_path(const NodePath &p_path) {
	body_a_path = p_path;
}

NodePath BulletConstraint::get_body_b_path() const {
	return body_b_path;
}

void BulletConstraint::set_body_b_path(const NodePath &p_path) {
	body_b_path = p_path;
}

BulletRigidBody *BulletConstraint::get_body_a() const {
	return body_a;
}

BulletRigidBody *BulletConstraint::get_body_b() const {
	return body_b;
}

bool BulletConstraint::is_enabled() const {
	return enabled;
}

void BulletConstraint::set_enabled(bool p_enabled) {
	enabled = p_enabled;
	if (constraint != nullptr) {
		constraint->setEnabled(enabled);
	}
}

bool BulletConstraint::are_linked_body_collisions_disabled() const {
	return disable_collisions;
}

void BulletConstraint::set_disable_collisions(bool p_disabled) {
	disable_collisions = p_disabled;
}

float BulletConstraint::get_breaking_threshold() const {
	return breaking_threshold;
}

void BulletConstraint::set_breaking_threshold(float p_threshold) {
	breaking_threshold = p_threshold;
	_apply_parameters();
}

bool BulletConstraint::is_using_world_transform() const {
	return use_world_transform;
}

void BulletConstraint::set_use_world_transform(bool p_enabled) {
	use_world_transform = p_enabled;
}

Transform3D BulletConstraint::get_frame_in_a() const {
	return frame_in_a;
}

void BulletConstraint::set_frame_in_a(const Transform3D &p_frame) {
	frame_in_a = p_frame;
}

Transform3D BulletConstraint::get_frame_in_b() const {
	return frame_in_b;
}

void BulletConstraint::set_frame_in_b(const Transform3D &p_frame) {
	frame_in_b = p_frame;
}

// ---------------------------------------------------------------------------
// BulletPoint2PointConstraint
// ---------------------------------------------------------------------------

void BulletPoint2PointConstraint::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_pivot_in_a"), &BulletPoint2PointConstraint::get_pivot_in_a);
	ClassDB::bind_method(D_METHOD("set_pivot_in_a", "pivot"), &BulletPoint2PointConstraint::set_pivot_in_a);
	ClassDB::bind_method(D_METHOD("get_pivot_in_b"), &BulletPoint2PointConstraint::get_pivot_in_b);
	ClassDB::bind_method(D_METHOD("set_pivot_in_b", "pivot"), &BulletPoint2PointConstraint::set_pivot_in_b);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "pivot_in_a"), "set_pivot_in_a", "get_pivot_in_a");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "pivot_in_b"), "set_pivot_in_b", "get_pivot_in_b");
}

btTypedConstraint *BulletPoint2PointConstraint::_create_constraint(const btTransform &p_frame_a, const btTransform &p_frame_b) {
	const btVector3 pivot_a = use_world_transform ? p_frame_a.getOrigin() : bullet_util::to_bt(pivot_in_a);
	const btVector3 pivot_b = use_world_transform ? p_frame_b.getOrigin() : bullet_util::to_bt(pivot_in_b);
	return new btPoint2PointConstraint(*body_a->_get_bt_body(), *body_b->_get_bt_body(), pivot_a, pivot_b);
}

void BulletPoint2PointConstraint::_apply_parameters() {
	BulletConstraint::_apply_parameters();
	if (constraint == nullptr) {
		return;
	}
	btPoint2PointConstraint *p2p = static_cast<btPoint2PointConstraint *>(constraint);
	if (!use_world_transform) {
		p2p->setPivotA(bullet_util::to_bt(pivot_in_a));
		p2p->setPivotB(bullet_util::to_bt(pivot_in_b));
	}
}

Vector3 BulletPoint2PointConstraint::get_pivot_in_a() const {
	return pivot_in_a;
}

void BulletPoint2PointConstraint::set_pivot_in_a(const Vector3 &p_pivot) {
	pivot_in_a = p_pivot;
	_apply_parameters();
}

Vector3 BulletPoint2PointConstraint::get_pivot_in_b() const {
	return pivot_in_b;
}

void BulletPoint2PointConstraint::set_pivot_in_b(const Vector3 &p_pivot) {
	pivot_in_b = p_pivot;
	_apply_parameters();
}

// ---------------------------------------------------------------------------
// BulletHingeConstraint
// ---------------------------------------------------------------------------

void BulletHingeConstraint::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_limit_enabled"), &BulletHingeConstraint::is_limit_enabled);
	ClassDB::bind_method(D_METHOD("set_limit_enabled", "enabled"), &BulletHingeConstraint::set_limit_enabled);
	ClassDB::bind_method(D_METHOD("get_limit_lower"), &BulletHingeConstraint::get_limit_lower);
	ClassDB::bind_method(D_METHOD("set_limit_lower", "lower"), &BulletHingeConstraint::set_limit_lower);
	ClassDB::bind_method(D_METHOD("get_limit_upper"), &BulletHingeConstraint::get_limit_upper);
	ClassDB::bind_method(D_METHOD("set_limit_upper", "upper"), &BulletHingeConstraint::set_limit_upper);
	ClassDB::bind_method(D_METHOD("get_limit_softness"), &BulletHingeConstraint::get_limit_softness);
	ClassDB::bind_method(D_METHOD("set_limit_softness", "softness"), &BulletHingeConstraint::set_limit_softness);
	ClassDB::bind_method(D_METHOD("get_limit_bias_factor"), &BulletHingeConstraint::get_limit_bias_factor);
	ClassDB::bind_method(D_METHOD("set_limit_bias_factor", "bias"), &BulletHingeConstraint::set_limit_bias_factor);
	ClassDB::bind_method(D_METHOD("get_limit_relaxation_factor"), &BulletHingeConstraint::get_limit_relaxation_factor);
	ClassDB::bind_method(D_METHOD("set_limit_relaxation_factor", "relaxation"), &BulletHingeConstraint::set_limit_relaxation_factor);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "limit_enabled"), "set_limit_enabled", "is_limit_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "limit_lower", PROPERTY_HINT_RANGE, "-180,180,0.01,or_less,or_greater"), "set_limit_lower", "get_limit_lower");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "limit_upper", PROPERTY_HINT_RANGE, "-180,180,0.01,or_less,or_greater"), "set_limit_upper", "get_limit_upper");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "limit_softness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_limit_softness", "get_limit_softness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "limit_bias_factor", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_limit_bias_factor", "get_limit_bias_factor");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "limit_relaxation_factor", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_limit_relaxation_factor", "get_limit_relaxation_factor");
}

btTypedConstraint *BulletHingeConstraint::_create_constraint(const btTransform &p_frame_a, const btTransform &p_frame_b) {
	return new btHingeConstraint(*body_a->_get_bt_body(), *body_b->_get_bt_body(), p_frame_a, p_frame_b, false);
}

void BulletHingeConstraint::_apply_parameters() {
	BulletConstraint::_apply_parameters();
	if (constraint == nullptr) {
		return;
	}
	btHingeConstraint *hinge = static_cast<btHingeConstraint *>(constraint);
	if (limit_enabled) {
		hinge->setLimit(
				static_cast<btScalar>(limit_lower),
				static_cast<btScalar>(limit_upper),
				static_cast<btScalar>(limit_softness),
				static_cast<btScalar>(limit_bias_factor),
				static_cast<btScalar>(limit_relaxation_factor));
	} else {
		// Bullet treats lower > upper as "no limit".
		hinge->setLimit(btScalar(1.0), btScalar(-1.0));
	}
}

bool BulletHingeConstraint::is_limit_enabled() const {
	return limit_enabled;
}

void BulletHingeConstraint::set_limit_enabled(bool p_enabled) {
	limit_enabled = p_enabled;
	_apply_parameters();
}

float BulletHingeConstraint::get_limit_lower() const {
	return limit_lower;
}

void BulletHingeConstraint::set_limit_lower(float p_lower) {
	limit_lower = p_lower;
	_apply_parameters();
}

float BulletHingeConstraint::get_limit_upper() const {
	return limit_upper;
}

void BulletHingeConstraint::set_limit_upper(float p_upper) {
	limit_upper = p_upper;
	_apply_parameters();
}

float BulletHingeConstraint::get_limit_softness() const {
	return limit_softness;
}

void BulletHingeConstraint::set_limit_softness(float p_softness) {
	limit_softness = p_softness;
	_apply_parameters();
}

float BulletHingeConstraint::get_limit_bias_factor() const {
	return limit_bias_factor;
}

void BulletHingeConstraint::set_limit_bias_factor(float p_bias) {
	limit_bias_factor = p_bias;
	_apply_parameters();
}

float BulletHingeConstraint::get_limit_relaxation_factor() const {
	return limit_relaxation_factor;
}

void BulletHingeConstraint::set_limit_relaxation_factor(float p_relaxation) {
	limit_relaxation_factor = p_relaxation;
	_apply_parameters();
}

// ---------------------------------------------------------------------------
// BulletSliderConstraint
// ---------------------------------------------------------------------------

void BulletSliderConstraint::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_linear_lower_limit"), &BulletSliderConstraint::get_linear_lower_limit);
	ClassDB::bind_method(D_METHOD("set_linear_lower_limit", "limit"), &BulletSliderConstraint::set_linear_lower_limit);
	ClassDB::bind_method(D_METHOD("get_linear_upper_limit"), &BulletSliderConstraint::get_linear_upper_limit);
	ClassDB::bind_method(D_METHOD("set_linear_upper_limit", "limit"), &BulletSliderConstraint::set_linear_upper_limit);
	ClassDB::bind_method(D_METHOD("get_angular_lower_limit"), &BulletSliderConstraint::get_angular_lower_limit);
	ClassDB::bind_method(D_METHOD("set_angular_lower_limit", "limit"), &BulletSliderConstraint::set_angular_lower_limit);
	ClassDB::bind_method(D_METHOD("get_angular_upper_limit"), &BulletSliderConstraint::get_angular_upper_limit);
	ClassDB::bind_method(D_METHOD("set_angular_upper_limit", "limit"), &BulletSliderConstraint::set_angular_upper_limit);
	ClassDB::bind_method(D_METHOD("is_using_linear_reference_frame_a"), &BulletSliderConstraint::is_using_linear_reference_frame_a);
	ClassDB::bind_method(D_METHOD("set_use_linear_reference_frame_a", "enabled"), &BulletSliderConstraint::set_use_linear_reference_frame_a);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "linear_lower_limit", PROPERTY_HINT_RANGE, "-1000,1000,0.01,or_less,or_greater"), "set_linear_lower_limit", "get_linear_lower_limit");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "linear_upper_limit", PROPERTY_HINT_RANGE, "-1000,1000,0.01,or_less,or_greater"), "set_linear_upper_limit", "get_linear_upper_limit");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "angular_lower_limit", PROPERTY_HINT_RANGE, "-180,180,0.01,or_less,or_greater"), "set_angular_lower_limit", "get_angular_lower_limit");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "angular_upper_limit", PROPERTY_HINT_RANGE, "-180,180,0.01,or_less,or_greater"), "set_angular_upper_limit", "get_angular_upper_limit");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_linear_reference_frame_a"), "set_use_linear_reference_frame_a", "is_using_linear_reference_frame_a");
}

btTypedConstraint *BulletSliderConstraint::_create_constraint(const btTransform &p_frame_a, const btTransform &p_frame_b) {
	return new btSliderConstraint(*body_a->_get_bt_body(), *body_b->_get_bt_body(), p_frame_a, p_frame_b, use_linear_reference_frame_a);
}

void BulletSliderConstraint::_apply_parameters() {
	BulletConstraint::_apply_parameters();
	if (constraint == nullptr) {
		return;
	}
	btSliderConstraint *slider = static_cast<btSliderConstraint *>(constraint);
	slider->setLowerLinLimit(static_cast<btScalar>(linear_lower_limit));
	slider->setUpperLinLimit(static_cast<btScalar>(linear_upper_limit));
	slider->setLowerAngLimit(static_cast<btScalar>(angular_lower_limit));
	slider->setUpperAngLimit(static_cast<btScalar>(angular_upper_limit));
}

float BulletSliderConstraint::get_linear_lower_limit() const {
	return linear_lower_limit;
}

void BulletSliderConstraint::set_linear_lower_limit(float p_limit) {
	linear_lower_limit = p_limit;
	_apply_parameters();
}

float BulletSliderConstraint::get_linear_upper_limit() const {
	return linear_upper_limit;
}

void BulletSliderConstraint::set_linear_upper_limit(float p_limit) {
	linear_upper_limit = p_limit;
	_apply_parameters();
}

float BulletSliderConstraint::get_angular_lower_limit() const {
	return angular_lower_limit;
}

void BulletSliderConstraint::set_angular_lower_limit(float p_limit) {
	angular_lower_limit = p_limit;
	_apply_parameters();
}

float BulletSliderConstraint::get_angular_upper_limit() const {
	return angular_upper_limit;
}

void BulletSliderConstraint::set_angular_upper_limit(float p_limit) {
	angular_upper_limit = p_limit;
	_apply_parameters();
}

bool BulletSliderConstraint::is_using_linear_reference_frame_a() const {
	return use_linear_reference_frame_a;
}

void BulletSliderConstraint::set_use_linear_reference_frame_a(bool p_enabled) {
	use_linear_reference_frame_a = p_enabled;
}

// ---------------------------------------------------------------------------
// BulletConeTwistConstraint
// ---------------------------------------------------------------------------

void BulletConeTwistConstraint::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_swing_span_1"), &BulletConeTwistConstraint::get_swing_span_1);
	ClassDB::bind_method(D_METHOD("set_swing_span_1", "span"), &BulletConeTwistConstraint::set_swing_span_1);
	ClassDB::bind_method(D_METHOD("get_swing_span_2"), &BulletConeTwistConstraint::get_swing_span_2);
	ClassDB::bind_method(D_METHOD("set_swing_span_2", "span"), &BulletConeTwistConstraint::set_swing_span_2);
	ClassDB::bind_method(D_METHOD("get_twist_span"), &BulletConeTwistConstraint::get_twist_span);
	ClassDB::bind_method(D_METHOD("set_twist_span", "span"), &BulletConeTwistConstraint::set_twist_span);
	ClassDB::bind_method(D_METHOD("get_limit_softness"), &BulletConeTwistConstraint::get_limit_softness);
	ClassDB::bind_method(D_METHOD("set_limit_softness", "softness"), &BulletConeTwistConstraint::set_limit_softness);
	ClassDB::bind_method(D_METHOD("get_limit_bias_factor"), &BulletConeTwistConstraint::get_limit_bias_factor);
	ClassDB::bind_method(D_METHOD("set_limit_bias_factor", "bias"), &BulletConeTwistConstraint::set_limit_bias_factor);
	ClassDB::bind_method(D_METHOD("get_limit_relaxation_factor"), &BulletConeTwistConstraint::get_limit_relaxation_factor);
	ClassDB::bind_method(D_METHOD("set_limit_relaxation_factor", "relaxation"), &BulletConeTwistConstraint::set_limit_relaxation_factor);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "swing_span_1", PROPERTY_HINT_RANGE, "-180,180,0.01,or_less,or_greater"), "set_swing_span_1", "get_swing_span_1");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "swing_span_2", PROPERTY_HINT_RANGE, "-180,180,0.01,or_less,or_greater"), "set_swing_span_2", "get_swing_span_2");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "twist_span", PROPERTY_HINT_RANGE, "-180,180,0.01,or_less,or_greater"), "set_twist_span", "get_twist_span");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "limit_softness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_limit_softness", "get_limit_softness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "limit_bias_factor", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_limit_bias_factor", "get_limit_bias_factor");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "limit_relaxation_factor", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_limit_relaxation_factor", "get_limit_relaxation_factor");
}

btTypedConstraint *BulletConeTwistConstraint::_create_constraint(const btTransform &p_frame_a, const btTransform &p_frame_b) {
	return new btConeTwistConstraint(*body_a->_get_bt_body(), *body_b->_get_bt_body(), p_frame_a, p_frame_b);
}

void BulletConeTwistConstraint::_apply_parameters() {
	BulletConstraint::_apply_parameters();
	if (constraint == nullptr) {
		return;
	}
	static_cast<btConeTwistConstraint *>(constraint)->setLimit(
			static_cast<btScalar>(swing_span_1),
			static_cast<btScalar>(swing_span_2),
			static_cast<btScalar>(twist_span),
			static_cast<btScalar>(limit_softness),
			static_cast<btScalar>(limit_bias_factor),
			static_cast<btScalar>(limit_relaxation_factor));
}

float BulletConeTwistConstraint::get_swing_span_1() const {
	return swing_span_1;
}

void BulletConeTwistConstraint::set_swing_span_1(float p_span) {
	swing_span_1 = p_span;
	_apply_parameters();
}

float BulletConeTwistConstraint::get_swing_span_2() const {
	return swing_span_2;
}

void BulletConeTwistConstraint::set_swing_span_2(float p_span) {
	swing_span_2 = p_span;
	_apply_parameters();
}

float BulletConeTwistConstraint::get_twist_span() const {
	return twist_span;
}

void BulletConeTwistConstraint::set_twist_span(float p_span) {
	twist_span = p_span;
	_apply_parameters();
}

float BulletConeTwistConstraint::get_limit_softness() const {
	return limit_softness;
}

void BulletConeTwistConstraint::set_limit_softness(float p_softness) {
	limit_softness = p_softness;
	_apply_parameters();
}

float BulletConeTwistConstraint::get_limit_bias_factor() const {
	return limit_bias_factor;
}

void BulletConeTwistConstraint::set_limit_bias_factor(float p_bias) {
	limit_bias_factor = p_bias;
	_apply_parameters();
}

float BulletConeTwistConstraint::get_limit_relaxation_factor() const {
	return limit_relaxation_factor;
}

void BulletConeTwistConstraint::set_limit_relaxation_factor(float p_relaxation) {
	limit_relaxation_factor = p_relaxation;
	_apply_parameters();
}

// ---------------------------------------------------------------------------
// BulletGeneric6DofConstraint
// ---------------------------------------------------------------------------

void BulletGeneric6DofConstraint::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_linear_lower_limit"), &BulletGeneric6DofConstraint::get_linear_lower_limit);
	ClassDB::bind_method(D_METHOD("set_linear_lower_limit", "limit"), &BulletGeneric6DofConstraint::set_linear_lower_limit);
	ClassDB::bind_method(D_METHOD("get_linear_upper_limit"), &BulletGeneric6DofConstraint::get_linear_upper_limit);
	ClassDB::bind_method(D_METHOD("set_linear_upper_limit", "limit"), &BulletGeneric6DofConstraint::set_linear_upper_limit);
	ClassDB::bind_method(D_METHOD("get_angular_lower_limit"), &BulletGeneric6DofConstraint::get_angular_lower_limit);
	ClassDB::bind_method(D_METHOD("set_angular_lower_limit", "limit"), &BulletGeneric6DofConstraint::set_angular_lower_limit);
	ClassDB::bind_method(D_METHOD("get_angular_upper_limit"), &BulletGeneric6DofConstraint::get_angular_upper_limit);
	ClassDB::bind_method(D_METHOD("set_angular_upper_limit", "limit"), &BulletGeneric6DofConstraint::set_angular_upper_limit);
	ClassDB::bind_method(D_METHOD("is_using_linear_reference_frame_a"), &BulletGeneric6DofConstraint::is_using_linear_reference_frame_a);
	ClassDB::bind_method(D_METHOD("set_use_linear_reference_frame_a", "enabled"), &BulletGeneric6DofConstraint::set_use_linear_reference_frame_a);
	ClassDB::bind_method(D_METHOD("set_axis_limit", "axis", "lower", "upper"), &BulletGeneric6DofConstraint::set_axis_limit);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "linear_lower_limit", PROPERTY_HINT_LINK), "set_linear_lower_limit", "get_linear_lower_limit");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "linear_upper_limit", PROPERTY_HINT_LINK), "set_linear_upper_limit", "get_linear_upper_limit");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "angular_lower_limit", PROPERTY_HINT_LINK), "set_angular_lower_limit", "get_angular_lower_limit");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "angular_upper_limit", PROPERTY_HINT_LINK), "set_angular_upper_limit", "get_angular_upper_limit");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_linear_reference_frame_a"), "set_use_linear_reference_frame_a", "is_using_linear_reference_frame_a");
}

btGeneric6DofConstraint *BulletGeneric6DofConstraint::_get_6dof() const {
	return static_cast<btGeneric6DofConstraint *>(constraint);
}

btTypedConstraint *BulletGeneric6DofConstraint::_create_constraint(const btTransform &p_frame_a, const btTransform &p_frame_b) {
	return new btGeneric6DofConstraint(*body_a->_get_bt_body(), *body_b->_get_bt_body(), p_frame_a, p_frame_b, use_linear_reference_frame_a);
}

void BulletGeneric6DofConstraint::_apply_parameters() {
	BulletConstraint::_apply_parameters();
	if (constraint == nullptr) {
		return;
	}
	btGeneric6DofConstraint *dof = _get_6dof();
	dof->setLinearLowerLimit(bullet_util::to_bt(linear_lower_limit));
	dof->setLinearUpperLimit(bullet_util::to_bt(linear_upper_limit));
	dof->setAngularLowerLimit(bullet_util::to_bt(angular_lower_limit));
	dof->setAngularUpperLimit(bullet_util::to_bt(angular_upper_limit));
}

Vector3 BulletGeneric6DofConstraint::get_linear_lower_limit() const {
	return linear_lower_limit;
}

void BulletGeneric6DofConstraint::set_linear_lower_limit(const Vector3 &p_limit) {
	linear_lower_limit = p_limit;
	_apply_parameters();
}

Vector3 BulletGeneric6DofConstraint::get_linear_upper_limit() const {
	return linear_upper_limit;
}

void BulletGeneric6DofConstraint::set_linear_upper_limit(const Vector3 &p_limit) {
	linear_upper_limit = p_limit;
	_apply_parameters();
}

Vector3 BulletGeneric6DofConstraint::get_angular_lower_limit() const {
	return angular_lower_limit;
}

void BulletGeneric6DofConstraint::set_angular_lower_limit(const Vector3 &p_limit) {
	angular_lower_limit = p_limit;
	_apply_parameters();
}

Vector3 BulletGeneric6DofConstraint::get_angular_upper_limit() const {
	return angular_upper_limit;
}

void BulletGeneric6DofConstraint::set_angular_upper_limit(const Vector3 &p_limit) {
	angular_upper_limit = p_limit;
	_apply_parameters();
}

bool BulletGeneric6DofConstraint::is_using_linear_reference_frame_a() const {
	return use_linear_reference_frame_a;
}

void BulletGeneric6DofConstraint::set_use_linear_reference_frame_a(bool p_enabled) {
	use_linear_reference_frame_a = p_enabled;
}

void BulletGeneric6DofConstraint::set_axis_limit(int32_t p_axis, float p_lower, float p_upper) {
	if (p_axis < 0 || p_axis > 5) {
		return;
	}
	switch (p_axis) {
		case 0:
			linear_lower_limit.x = p_lower;
			linear_upper_limit.x = p_upper;
			break;
		case 1:
			linear_lower_limit.y = p_lower;
			linear_upper_limit.y = p_upper;
			break;
		case 2:
			linear_lower_limit.z = p_lower;
			linear_upper_limit.z = p_upper;
			break;
		case 3:
			angular_lower_limit.x = p_lower;
			angular_upper_limit.x = p_upper;
			break;
		case 4:
			angular_lower_limit.y = p_lower;
			angular_upper_limit.y = p_upper;
			break;
		case 5:
			angular_lower_limit.z = p_lower;
			angular_upper_limit.z = p_upper;
			break;
		default:
			break;
	}
	_apply_parameters();
}

// ---------------------------------------------------------------------------
// BulletGeneric6DofSpringConstraint
// ---------------------------------------------------------------------------

void BulletGeneric6DofSpringConstraint::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_spring_enabled", "axis"), &BulletGeneric6DofSpringConstraint::is_spring_enabled);
	ClassDB::bind_method(D_METHOD("set_spring_enabled", "axis", "enabled"), &BulletGeneric6DofSpringConstraint::set_spring_enabled);
	ClassDB::bind_method(D_METHOD("get_spring_stiffness", "axis"), &BulletGeneric6DofSpringConstraint::get_spring_stiffness);
	ClassDB::bind_method(D_METHOD("set_spring_stiffness", "axis", "stiffness"), &BulletGeneric6DofSpringConstraint::set_spring_stiffness);
	ClassDB::bind_method(D_METHOD("get_spring_damping", "axis"), &BulletGeneric6DofSpringConstraint::get_spring_damping);
	ClassDB::bind_method(D_METHOD("set_spring_damping", "axis", "damping"), &BulletGeneric6DofSpringConstraint::set_spring_damping);
	ClassDB::bind_method(D_METHOD("configure_spring", "axis", "stiffness", "damping"), &BulletGeneric6DofSpringConstraint::configure_spring);
}

btTypedConstraint *BulletGeneric6DofSpringConstraint::_create_constraint(const btTransform &p_frame_a, const btTransform &p_frame_b) {
	return new btGeneric6DofSpringConstraint(*body_a->_get_bt_body(), *body_b->_get_bt_body(), p_frame_a, p_frame_b, use_linear_reference_frame_a);
}

void BulletGeneric6DofSpringConstraint::_apply_parameters() {
	BulletGeneric6DofConstraint::_apply_parameters();
	if (constraint == nullptr) {
		return;
	}
	btGeneric6DofSpringConstraint *spring = static_cast<btGeneric6DofSpringConstraint *>(constraint);
	for (int32_t axis = 0; axis < 6; axis++) {
		spring->enableSpring(axis, spring_enabled[axis]);
		if (!spring_enabled[axis]) {
			continue;
		}
		spring->setStiffness(axis, static_cast<btScalar>(spring_stiffness[axis]));
		spring->setDamping(axis, static_cast<btScalar>(spring_damping[axis]));
		// The reference importer never calls setEquilibriumPoint(), so Bullet's default
		// equilibrium (0 in the constraint frame) is what MMD depends on.
	}
}

bool BulletGeneric6DofSpringConstraint::is_spring_enabled(int32_t p_axis) const {
	if (p_axis < 0 || p_axis > 5) {
		return false;
	}
	return spring_enabled[p_axis];
}

void BulletGeneric6DofSpringConstraint::set_spring_enabled(int32_t p_axis, bool p_enabled) {
	if (p_axis < 0 || p_axis > 5) {
		return;
	}
	spring_enabled[p_axis] = p_enabled;
	_apply_parameters();
}

float BulletGeneric6DofSpringConstraint::get_spring_stiffness(int32_t p_axis) const {
	if (p_axis < 0 || p_axis > 5) {
		return 0.0f;
	}
	return spring_stiffness[p_axis];
}

void BulletGeneric6DofSpringConstraint::set_spring_stiffness(int32_t p_axis, float p_stiffness) {
	if (p_axis < 0 || p_axis > 5) {
		return;
	}
	spring_stiffness[p_axis] = p_stiffness;
	_apply_parameters();
}

float BulletGeneric6DofSpringConstraint::get_spring_damping(int32_t p_axis) const {
	if (p_axis < 0 || p_axis > 5) {
		return 0.0f;
	}
	return spring_damping[p_axis];
}

void BulletGeneric6DofSpringConstraint::set_spring_damping(int32_t p_axis, float p_damping) {
	if (p_axis < 0 || p_axis > 5) {
		return;
	}
	spring_damping[p_axis] = p_damping;
	_apply_parameters();
}

void BulletGeneric6DofSpringConstraint::configure_spring(int32_t p_axis, float p_stiffness, float p_damping) {
	if (p_axis < 0 || p_axis > 5) {
		return;
	}
	spring_enabled[p_axis] = true;
	spring_stiffness[p_axis] = p_stiffness;
	spring_damping[p_axis] = p_damping;
	_apply_parameters();
}

// ---------------------------------------------------------------------------
// BulletFixedConstraint
// ---------------------------------------------------------------------------

void BulletFixedConstraint::_bind_methods() {
	// Nothing extra to bind: bodies, frames and damping are inherited.
}

btTypedConstraint *BulletFixedConstraint::_create_constraint(const btTransform &p_frame_a, const btTransform &p_frame_b) {
	return new btFixedConstraint(*body_a->_get_bt_body(), *body_b->_get_bt_body(), p_frame_a, p_frame_b);
}
