#include "mmd_joint.h"

#include "godot_cpp/core/class_db.hpp"

using namespace godot;

void MMDJoint::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_linear_spring"), &MMDJoint::get_linear_spring);
	ClassDB::bind_method(D_METHOD("set_linear_spring", "stiffness"), &MMDJoint::set_linear_spring);
	ClassDB::bind_method(D_METHOD("get_angular_spring"), &MMDJoint::get_angular_spring);
	ClassDB::bind_method(D_METHOD("set_angular_spring", "stiffness"), &MMDJoint::set_angular_spring);
	ClassDB::bind_method(D_METHOD("get_spring_damping"), &MMDJoint::get_spring_damping);
	ClassDB::bind_method(D_METHOD("set_spring_damping", "damping"), &MMDJoint::set_spring_damping);
	ClassDB::bind_method(D_METHOD("apply_springs"), &MMDJoint::apply_springs);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "linear_spring"), "set_linear_spring", "get_linear_spring");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "angular_spring"), "set_angular_spring", "get_angular_spring");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "spring_damping", PROPERTY_HINT_RANGE, "0,10,0.01,or_greater"), "set_spring_damping", "get_spring_damping");
}

MMDJoint::MMDJoint() = default;

void MMDJoint::_apply_springs() {
	for (int32_t axis = 0; axis < 3; axis++) {
		const float linear_stiffness = linear_spring[axis];
		if (linear_stiffness != 0.0f) {
			configure_spring(axis, linear_stiffness, spring_damping);
		} else {
			set_spring_enabled(axis, false);
		}

		const float angular_stiffness = angular_spring[axis];
		if (angular_stiffness != 0.0f) {
			configure_spring(3 + axis, angular_stiffness, spring_damping);
		} else {
			set_spring_enabled(3 + axis, false);
		}
	}
}

Vector3 MMDJoint::get_linear_spring() const {
	return linear_spring;
}

void MMDJoint::set_linear_spring(const Vector3 &p_stiffness) {
	linear_spring = p_stiffness;
	_apply_springs();
}

Vector3 MMDJoint::get_angular_spring() const {
	return angular_spring;
}

void MMDJoint::set_angular_spring(const Vector3 &p_stiffness) {
	angular_spring = p_stiffness;
	_apply_springs();
}

float MMDJoint::get_spring_damping() const {
	return spring_damping;
}

void MMDJoint::set_spring_damping(float p_damping) {
	spring_damping = p_damping;
	_apply_springs();
}

void MMDJoint::apply_springs() {
	_apply_springs();
}
