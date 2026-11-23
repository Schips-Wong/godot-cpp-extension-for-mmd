#pragma once

#include <btBulletDynamicsCommon.h>

#include "godot_cpp/classes/mesh.hpp"
#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/variant/packed_vector3_array.hpp"
#include "godot_cpp/variant/string.hpp"
#include "godot_cpp/variant/transform3d.hpp"
#include "godot_cpp/variant/typed_array.hpp"
#include "godot_cpp/variant/vector3.hpp"

#include "bullet_util.h"

using namespace godot;

// Base class of every Bullet collision shape.
// Shapes are Resources so they can be stored inside scenes and shared between bodies.
// The underlying btCollisionShape is created lazily and rebuilt whenever a parameter changes.
class BulletCollisionShape : public Resource {
	GDCLASS(BulletCollisionShape, Resource)

public:
	enum Axis {
		AXIS_X,
		AXIS_Y,
		AXIS_Z,
	};

protected:
	static void _bind_methods();

	btCollisionShape *bt_shape = nullptr;
	bool dirty = true;

	float margin = 0.04f;
	Vector3 local_scaling = Vector3(1.0, 1.0, 1.0);

	// Creates a brand new btCollisionShape from the current parameters.
	virtual btCollisionShape *_create_shape() = 0;
	// Hook called right before the shape is destroyed, for subclasses owning helper data.
	virtual void _release_shape() {}

	void _destroy_shape();

public:
	BulletCollisionShape() = default;
	~BulletCollisionShape() override;

	// Invalidates the shape; it will be recreated on the next get_bt_shape() call.
	void mark_dirty();

	// Returns the cached btCollisionShape, creating it if needed. Never null.
	btCollisionShape *get_bt_shape();

	float get_margin() const;
	void set_margin(float p_margin);

	Vector3 get_local_scaling() const;
	void set_local_scaling(const Vector3 &p_scaling);

	// Local inertia tensor of this shape for the given mass.
	Vector3 calculate_local_inertia(float p_mass);
};

class BulletEmptyShape : public BulletCollisionShape {
	GDCLASS(BulletEmptyShape, BulletCollisionShape)

protected:
	static void _bind_methods() {}
	btCollisionShape *_create_shape() override;
};

class BulletBoxShape : public BulletCollisionShape {
	GDCLASS(BulletBoxShape, BulletCollisionShape)

	Vector3 half_extents = Vector3(0.5, 0.5, 0.5);

protected:
	static void _bind_methods();
	btCollisionShape *_create_shape() override;

public:
	Vector3 get_half_extents() const;
	void set_half_extents(const Vector3 &p_half_extents);
};

class BulletSphereShape : public BulletCollisionShape {
	GDCLASS(BulletSphereShape, BulletCollisionShape)

	float radius = 0.5f;

protected:
	static void _bind_methods();
	btCollisionShape *_create_shape() override;

public:
	float get_radius() const;
	void set_radius(float p_radius);
};

class BulletCapsuleShape : public BulletCollisionShape {
	GDCLASS(BulletCapsuleShape, BulletCollisionShape)

	float radius = 0.5f;
	float height = 1.0f;
	Axis axis = AXIS_Y;

protected:
	static void _bind_methods();
	btCollisionShape *_create_shape() override;

public:
	float get_radius() const;
	void set_radius(float p_radius);

	// Height of the cylindrical part, caps excluded (same convention as Bullet).
	float get_height() const;
	void set_height(float p_height);

	Axis get_axis() const;
	void set_axis(Axis p_axis);
};

class BulletCylinderShape : public BulletCollisionShape {
	GDCLASS(BulletCylinderShape, BulletCollisionShape)

	Vector3 half_extents = Vector3(0.5, 0.5, 0.5);
	Axis axis = AXIS_Y;

protected:
	static void _bind_methods();
	btCollisionShape *_create_shape() override;

public:
	Vector3 get_half_extents() const;
	void set_half_extents(const Vector3 &p_half_extents);

	Axis get_axis() const;
	void set_axis(Axis p_axis);
};

class BulletConeShape : public BulletCollisionShape {
	GDCLASS(BulletConeShape, BulletCollisionShape)

	float radius = 0.5f;
	float height = 1.0f;
	Axis axis = AXIS_Y;

protected:
	static void _bind_methods();
	btCollisionShape *_create_shape() override;

public:
	float get_radius() const;
	void set_radius(float p_radius);

	float get_height() const;
	void set_height(float p_height);

	Axis get_axis() const;
	void set_axis(Axis p_axis);
};

class BulletStaticPlaneShape : public BulletCollisionShape {
	GDCLASS(BulletStaticPlaneShape, BulletCollisionShape)

	Vector3 plane_normal = Vector3(0, 1, 0);
	float plane_constant = 0.0f;

protected:
	static void _bind_methods();
	btCollisionShape *_create_shape() override;

public:
	Vector3 get_plane_normal() const;
	void set_plane_normal(const Vector3 &p_normal);

	float get_plane_constant() const;
	void set_plane_constant(float p_constant);
};

class BulletConvexHullShape : public BulletCollisionShape {
	GDCLASS(BulletConvexHullShape, BulletCollisionShape)

	PackedVector3Array points;

protected:
	static void _bind_methods();
	btCollisionShape *_create_shape() override;

public:
	void add_point(const Vector3 &p_point);
	void add_points(const PackedVector3Array &p_points);
	void clear_points();

	PackedVector3Array get_points() const;
	void set_points(const PackedVector3Array &p_points);
};

// Static triangle mesh (BVH), built from a triangle soup or directly from a Godot Mesh.
// Only usable by static / kinematic bodies: dynamic bodies cannot collide with it.
class BulletBvhTriangleMeshShape : public BulletCollisionShape {
	GDCLASS(BulletBvhTriangleMeshShape, BulletCollisionShape)

	PackedVector3Array faces;
	bool use_quantized_aabb = true;
	btTriangleMesh *triangle_mesh = nullptr;

protected:
	static void _bind_methods();
	btCollisionShape *_create_shape() override;
	void _release_shape() override;

public:
	~BulletBvhTriangleMeshShape() override;

	PackedVector3Array get_faces() const;
	void set_faces(const PackedVector3Array &p_faces);

	// Fills the shape with every triangle of the given mesh (uses Mesh::get_faces()).
	void set_from_mesh(const Ref<Mesh> &p_mesh);

	bool is_using_quantized_aabb() const;
	void set_use_quantized_aabb(bool p_enabled);
};

// Collection of shapes, each one with its own local transform.
class BulletCompoundShape : public BulletCollisionShape {
	GDCLASS(BulletCompoundShape, BulletCollisionShape)

	// Parallel arrays: child_shapes[i] is placed at child_transforms[i].
	Array child_shapes;
	Array child_transforms;

protected:
	static void _bind_methods();
	btCollisionShape *_create_shape() override;

public:
	void add_child_shape(const Ref<BulletCollisionShape> &p_shape, const Transform3D &p_transform = Transform3D());
	void clear_child_shapes();

	int32_t get_child_shape_count() const;
	Ref<BulletCollisionShape> get_child_shape(int32_t p_index) const;
	Transform3D get_child_transform(int32_t p_index) const;
};
