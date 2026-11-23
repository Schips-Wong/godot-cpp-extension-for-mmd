#pragma once

#include "../bullet/bullet_constraint.h"

using namespace godot;

// PMX joint convenience wrapper: the same 6DOF spring constraint, but with the PMX
// spring layout (one stiffness per linear axis, one per angular axis).
class MMDJoint : public BulletGeneric6DofSpringConstraint {
	GDCLASS(MMDJoint, BulletGeneric6DofSpringConstraint)

protected:
	static void _bind_methods();

	Vector3 linear_spring;
	Vector3 angular_spring;
	float spring_damping = 1.0f;

	void _apply_springs();

public:
	MMDJoint();
	~MMDJoint() override = default;

	Vector3 get_linear_spring() const;
	void set_linear_spring(const Vector3 &p_stiffness);
	Vector3 get_angular_spring() const;
	void set_angular_spring(const Vector3 &p_stiffness);
	float get_spring_damping() const;
	void set_spring_damping(float p_damping);

	// Re-applies the PMX springs to every axis.
	void apply_springs();
};
