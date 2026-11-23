#pragma once

#include <btBulletDynamicsCommon.h>

#include "godot_cpp/classes/node3d.hpp"
#include "godot_cpp/variant/node_path.hpp"
#include "godot_cpp/variant/transform3d.hpp"
#include "godot_cpp/variant/vector3.hpp"

using namespace godot;

class BulletWorld;
class BulletRigidBody;

// Base class of every Bullet constraint (BulletSharp: TypedConstraint).
//
// By default the constraint's own global transform is used as the joint frame in world space and
// converted into the local space of both linked bodies, which matches how MMD joints are authored.
// Set use_world_transform to false to provide frame_in_a / frame_in_b explicitly.
class BulletConstraint : public Node3D {
	GDCLASS(BulletConstraint, Node3D)

protected:
	static void _bind_methods();
	void _notification(int p_what);

	friend class BulletWorld;

	NodePath body_a_path;
	NodePath body_b_path;
	BulletRigidBody *body_a = nullptr;
	BulletRigidBody *body_b = nullptr;
	btTypedConstraint *constraint = nullptr;
	BulletWorld *world = nullptr;

	bool enabled = true;
	bool disable_collisions = true;
	float breaking_threshold = 0.0f;
	bool use_world_transform = true;
	Transform3D frame_in_a;
	Transform3D frame_in_b;

	BulletWorld *_find_world() const;
	void _resolve_bodies();

	virtual btTypedConstraint *_create_constraint(const btTransform &p_frame_a, const btTransform &p_frame_b) = 0;
	virtual void _apply_parameters();

	bool _has_bodies() const;

public:
	BulletConstraint() = default;
	~BulletConstraint() override;

	// Builds the constraint and inserts it into the world. Safe to call multiple times.
	void setup();
	// Removes and destroys the underlying Bullet constraint.
	void teardown();
	bool is_set_up() const;

	NodePath get_body_a_path() const;
	void set_body_a_path(const NodePath &p_path);
	NodePath get_body_b_path() const;
	void set_body_b_path(const NodePath &p_path);

	BulletRigidBody *get_body_a() const;
	BulletRigidBody *get_body_b() const;

	bool is_enabled() const;
	void set_enabled(bool p_enabled);

	bool are_linked_body_collisions_disabled() const;
	void set_disable_collisions(bool p_disabled);

	float get_breaking_threshold() const;
	void set_breaking_threshold(float p_threshold);

	bool is_using_world_transform() const;
	void set_use_world_transform(bool p_enabled);

	Transform3D get_frame_in_a() const;
	void set_frame_in_a(const Transform3D &p_frame);
	Transform3D get_frame_in_b() const;
	void set_frame_in_b(const Transform3D &p_frame);

	// Re-applies the tweakable parameters (limits, springs, ...) to the live constraint.
	void apply_parameters();
};

class BulletPoint2PointConstraint : public BulletConstraint {
	GDCLASS(BulletPoint2PointConstraint, BulletConstraint)

	Vector3 pivot_in_a;
	Vector3 pivot_in_b;

protected:
	static void _bind_methods();
	btTypedConstraint *_create_constraint(const btTransform &p_frame_a, const btTransform &p_frame_b) override;
	void _apply_parameters() override;

public:
	Vector3 get_pivot_in_a() const;
	void set_pivot_in_a(const Vector3 &p_pivot);
	Vector3 get_pivot_in_b() const;
	void set_pivot_in_b(const Vector3 &p_pivot);
};

class BulletHingeConstraint : public BulletConstraint {
	GDCLASS(BulletHingeConstraint, BulletConstraint)

	bool limit_enabled = false;
	float limit_lower = 0.0f;
	float limit_upper = 0.0f;
	float limit_softness = 0.9f;
	float limit_bias_factor = 0.3f;
	float limit_relaxation_factor = 1.0f;

protected:
	static void _bind_methods();
	btTypedConstraint *_create_constraint(const btTransform &p_frame_a, const btTransform &p_frame_b) override;
	void _apply_parameters() override;

public:
	bool is_limit_enabled() const;
	void set_limit_enabled(bool p_enabled);
	float get_limit_lower() const;
	void set_limit_lower(float p_lower);
	float get_limit_upper() const;
	void set_limit_upper(float p_upper);
	float get_limit_softness() const;
	void set_limit_softness(float p_softness);
	float get_limit_bias_factor() const;
	void set_limit_bias_factor(float p_bias);
	float get_limit_relaxation_factor() const;
	void set_limit_relaxation_factor(float p_relaxation);
};

class BulletSliderConstraint : public BulletConstraint {
	GDCLASS(BulletSliderConstraint, BulletConstraint)

	float linear_lower_limit = 1.0f;
	float linear_upper_limit = -1.0f;
	float angular_lower_limit = 1.0f;
	float angular_upper_limit = -1.0f;
	bool use_linear_reference_frame_a = true;

protected:
	static void _bind_methods();
	btTypedConstraint *_create_constraint(const btTransform &p_frame_a, const btTransform &p_frame_b) override;
	void _apply_parameters() override;

public:
	float get_linear_lower_limit() const;
	void set_linear_lower_limit(float p_limit);
	float get_linear_upper_limit() const;
	void set_linear_upper_limit(float p_limit);
	float get_angular_lower_limit() const;
	void set_angular_lower_limit(float p_limit);
	float get_angular_upper_limit() const;
	void set_angular_upper_limit(float p_limit);
	bool is_using_linear_reference_frame_a() const;
	void set_use_linear_reference_frame_a(bool p_enabled);
};

class BulletConeTwistConstraint : public BulletConstraint {
	GDCLASS(BulletConeTwistConstraint, BulletConstraint)

	float swing_span_1 = 1.0f;
	float swing_span_2 = 1.0f;
	float twist_span = 1.0f;
	float limit_softness = 1.0f;
	float limit_bias_factor = 0.3f;
	float limit_relaxation_factor = 1.0f;

protected:
	static void _bind_methods();
	btTypedConstraint *_create_constraint(const btTransform &p_frame_a, const btTransform &p_frame_b) override;
	void _apply_parameters() override;

public:
	float get_swing_span_1() const;
	void set_swing_span_1(float p_span);
	float get_swing_span_2() const;
	void set_swing_span_2(float p_span);
	float get_twist_span() const;
	void set_twist_span(float p_span);
	float get_limit_softness() const;
	void set_limit_softness(float p_softness);
	float get_limit_bias_factor() const;
	void set_limit_bias_factor(float p_bias);
	float get_limit_relaxation_factor() const;
	void set_limit_relaxation_factor(float p_relaxation);
};

class BulletGeneric6DofConstraint : public BulletConstraint {
	GDCLASS(BulletGeneric6DofConstraint, BulletConstraint)

protected:
	Vector3 linear_lower_limit = Vector3(1, 1, 1);
	Vector3 linear_upper_limit = Vector3(-1, -1, -1);
	Vector3 angular_lower_limit = Vector3(1, 1, 1);
	Vector3 angular_upper_limit = Vector3(-1, -1, -1);
	bool use_linear_reference_frame_a = true;

protected:
	static void _bind_methods();
	btTypedConstraint *_create_constraint(const btTransform &p_frame_a, const btTransform &p_frame_b) override;
	void _apply_parameters() override;

	btGeneric6DofConstraint *_get_6dof() const;

public:
	Vector3 get_linear_lower_limit() const;
	void set_linear_lower_limit(const Vector3 &p_limit);
	Vector3 get_linear_upper_limit() const;
	void set_linear_upper_limit(const Vector3 &p_limit);
	Vector3 get_angular_lower_limit() const;
	void set_angular_lower_limit(const Vector3 &p_limit);
	Vector3 get_angular_upper_limit() const;
	void set_angular_upper_limit(const Vector3 &p_limit);
	bool is_using_linear_reference_frame_a() const;
	void set_use_linear_reference_frame_a(bool p_enabled);

	// Axis: 0..2 = linear X/Y/Z, 3..5 = angular X/Y/Z.
	void set_axis_limit(int32_t p_axis, float p_lower, float p_upper);
};

class BulletGeneric6DofSpringConstraint : public BulletGeneric6DofConstraint {
	GDCLASS(BulletGeneric6DofSpringConstraint, BulletGeneric6DofConstraint)

	bool spring_enabled[6] = { false, false, false, false, false, false };
	float spring_stiffness[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	float spring_damping[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

protected:
	static void _bind_methods();
	btTypedConstraint *_create_constraint(const btTransform &p_frame_a, const btTransform &p_frame_b) override;
	void _apply_parameters() override;

public:
	bool is_spring_enabled(int32_t p_axis) const;
	void set_spring_enabled(int32_t p_axis, bool p_enabled);
	float get_spring_stiffness(int32_t p_axis) const;
	void set_spring_stiffness(int32_t p_axis, float p_stiffness);
	float get_spring_damping(int32_t p_axis) const;
	void set_spring_damping(int32_t p_axis, float p_damping);

	// Enables / tunes a spring on one axis in a single call (0..2 linear, 3..5 angular).
	void configure_spring(int32_t p_axis, float p_stiffness, float p_damping);
};

class BulletFixedConstraint : public BulletConstraint {
	GDCLASS(BulletFixedConstraint, BulletConstraint)

protected:
	static void _bind_methods();
	btTypedConstraint *_create_constraint(const btTransform &p_frame_a, const btTransform &p_frame_b) override;
};
