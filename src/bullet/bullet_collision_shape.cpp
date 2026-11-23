#include "bullet_collision_shape.h"

#include <algorithm>

#include "godot_cpp/classes/mesh.hpp"
#include "godot_cpp/core/class_db.hpp"
#include "godot_cpp/core/math.hpp"
#include "godot_cpp/variant/array.hpp"

using namespace godot;

VARIANT_ENUM_CAST(BulletCollisionShape::Axis);

// ---------------------------------------------------------------------------
// BulletCollisionShape
// ---------------------------------------------------------------------------

void BulletCollisionShape::_bind_methods() {
	BIND_ENUM_CONSTANT(AXIS_X);
	BIND_ENUM_CONSTANT(AXIS_Y);
	BIND_ENUM_CONSTANT(AXIS_Z);

	ClassDB::bind_method(D_METHOD("mark_dirty"), &BulletCollisionShape::mark_dirty);
	ClassDB::bind_method(D_METHOD("calculate_local_inertia", "mass"), &BulletCollisionShape::calculate_local_inertia);
	ClassDB::bind_method(D_METHOD("get_margin"), &BulletCollisionShape::get_margin);
	ClassDB::bind_method(D_METHOD("set_margin", "margin"), &BulletCollisionShape::set_margin);
	ClassDB::bind_method(D_METHOD("get_local_scaling"), &BulletCollisionShape::get_local_scaling);
	ClassDB::bind_method(D_METHOD("set_local_scaling", "scaling"), &BulletCollisionShape::set_local_scaling);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "margin", PROPERTY_HINT_RANGE, "0,1,0.001,or_greater"), "set_margin", "get_margin");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "local_scaling", PROPERTY_HINT_LINK), "set_local_scaling", "get_local_scaling");
}

BulletCollisionShape::~BulletCollisionShape() {
	_destroy_shape();
}

void BulletCollisionShape::_destroy_shape() {
	if (bt_shape != nullptr) {
		delete bt_shape;
		bt_shape = nullptr;
	}
	_release_shape();
	dirty = true;
}

void BulletCollisionShape::mark_dirty() {
	dirty = true;
}

btCollisionShape *BulletCollisionShape::get_bt_shape() {
	if (dirty || bt_shape == nullptr) {
		_destroy_shape();
		bt_shape = _create_shape();
		if (bt_shape != nullptr) {
			if (bt_shape->isConvex()) {
				bt_shape->setMargin(static_cast<btScalar>(margin));
			}
			bt_shape->setLocalScaling(bullet_util::to_bt(local_scaling));
		}
		dirty = false;
	}
	return bt_shape;
}

float BulletCollisionShape::get_margin() const {
	return margin;
}

void BulletCollisionShape::set_margin(float p_margin) {
	if (!Math::is_equal_approx(margin, p_margin)) {
		margin = p_margin;
		mark_dirty();
	}
}

Vector3 BulletCollisionShape::get_local_scaling() const {
	return local_scaling;
}

void BulletCollisionShape::set_local_scaling(const Vector3 &p_scaling) {
	if (local_scaling != p_scaling) {
		local_scaling = p_scaling;
		mark_dirty();
	}
}

Vector3 BulletCollisionShape::calculate_local_inertia(float p_mass) {
	btVector3 inertia(0, 0, 0);
	btCollisionShape *p_shape = get_bt_shape();
	if (p_shape != nullptr && p_mass > 0.0f) {
		p_shape->calculateLocalInertia(static_cast<btScalar>(p_mass), inertia);
	}
	return bullet_util::to_godot(inertia);
}

// ---------------------------------------------------------------------------
// BulletEmptyShape
// ---------------------------------------------------------------------------

btCollisionShape *BulletEmptyShape::_create_shape() {
	return new btEmptyShape();
}

// ---------------------------------------------------------------------------
// BulletBoxShape
// ---------------------------------------------------------------------------

void BulletBoxShape::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_half_extents"), &BulletBoxShape::get_half_extents);
	ClassDB::bind_method(D_METHOD("set_half_extents", "half_extents"), &BulletBoxShape::set_half_extents);
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "half_extents", PROPERTY_HINT_LINK), "set_half_extents", "get_half_extents");
}

btCollisionShape *BulletBoxShape::_create_shape() {
	return new btBoxShape(bullet_util::to_bt(half_extents));
}

Vector3 BulletBoxShape::get_half_extents() const {
	return half_extents;
}

void BulletBoxShape::set_half_extents(const Vector3 &p_half_extents) {
	Vector3 extents = p_half_extents.abs();
	if (half_extents != extents) {
		half_extents = extents;
		mark_dirty();
	}
}

// ---------------------------------------------------------------------------
// BulletSphereShape
// ---------------------------------------------------------------------------

void BulletSphereShape::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_radius"), &BulletSphereShape::get_radius);
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &BulletSphereShape::set_radius);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.001,4096,0.001,or_greater"), "set_radius", "get_radius");
}

btCollisionShape *BulletSphereShape::_create_shape() {
	return new btSphereShape(static_cast<btScalar>(radius));
}

float BulletSphereShape::get_radius() const {
	return radius;
}

void BulletSphereShape::set_radius(float p_radius) {
	float value = MAX(p_radius, 0.001f);
	if (!Math::is_equal_approx(radius, value)) {
		radius = value;
		mark_dirty();
	}
}

// ---------------------------------------------------------------------------
// BulletCapsuleShape
// ---------------------------------------------------------------------------

void BulletCapsuleShape::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_radius"), &BulletCapsuleShape::get_radius);
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &BulletCapsuleShape::set_radius);
	ClassDB::bind_method(D_METHOD("get_height"), &BulletCapsuleShape::get_height);
	ClassDB::bind_method(D_METHOD("set_height", "height"), &BulletCapsuleShape::set_height);
	ClassDB::bind_method(D_METHOD("get_axis"), &BulletCapsuleShape::get_axis);
	ClassDB::bind_method(D_METHOD("set_axis", "axis"), &BulletCapsuleShape::set_axis);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.001,4096,0.001,or_greater"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height", PROPERTY_HINT_RANGE, "0,8192,0.001,or_greater"), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "axis", PROPERTY_HINT_ENUM, "X,Y,Z"), "set_axis", "get_axis");
}

btCollisionShape *BulletCapsuleShape::_create_shape() {
	const btScalar r = static_cast<btScalar>(radius);
	const btScalar h = static_cast<btScalar>(height);
	switch (axis) {
		case AXIS_X:
			return new btCapsuleShapeX(r, h);
		case AXIS_Z:
			return new btCapsuleShapeZ(r, h);
		case AXIS_Y:
		default:
			return new btCapsuleShape(r, h);
	}
}

float BulletCapsuleShape::get_radius() const {
	return radius;
}

void BulletCapsuleShape::set_radius(float p_radius) {
	float value = MAX(p_radius, 0.001f);
	if (!Math::is_equal_approx(radius, value)) {
		radius = value;
		mark_dirty();
	}
}

float BulletCapsuleShape::get_height() const {
	return height;
}

void BulletCapsuleShape::set_height(float p_height) {
	float value = MAX(p_height, 0.0f);
	if (!Math::is_equal_approx(height, value)) {
		height = value;
		mark_dirty();
	}
}

BulletCollisionShape::Axis BulletCapsuleShape::get_axis() const {
	return axis;
}

void BulletCapsuleShape::set_axis(Axis p_axis) {
	if (axis != p_axis) {
		axis = p_axis;
		mark_dirty();
	}
}

// ---------------------------------------------------------------------------
// BulletCylinderShape
// ---------------------------------------------------------------------------

void BulletCylinderShape::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_half_extents"), &BulletCylinderShape::get_half_extents);
	ClassDB::bind_method(D_METHOD("set_half_extents", "half_extents"), &BulletCylinderShape::set_half_extents);
	ClassDB::bind_method(D_METHOD("get_axis"), &BulletCylinderShape::get_axis);
	ClassDB::bind_method(D_METHOD("set_axis", "axis"), &BulletCylinderShape::set_axis);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "half_extents", PROPERTY_HINT_LINK), "set_half_extents", "get_half_extents");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "axis", PROPERTY_HINT_ENUM, "X,Y,Z"), "set_axis", "get_axis");
}

btCollisionShape *BulletCylinderShape::_create_shape() {
	const btVector3 extents = bullet_util::to_bt(half_extents);
	switch (axis) {
		case AXIS_X:
			return new btCylinderShapeX(extents);
		case AXIS_Z:
			return new btCylinderShapeZ(extents);
		case AXIS_Y:
		default:
			return new btCylinderShape(extents);
	}
}

Vector3 BulletCylinderShape::get_half_extents() const {
	return half_extents;
}

void BulletCylinderShape::set_half_extents(const Vector3 &p_half_extents) {
	Vector3 extents = p_half_extents.abs();
	if (half_extents != extents) {
		half_extents = extents;
		mark_dirty();
	}
}

BulletCollisionShape::Axis BulletCylinderShape::get_axis() const {
	return axis;
}

void BulletCylinderShape::set_axis(Axis p_axis) {
	if (axis != p_axis) {
		axis = p_axis;
		mark_dirty();
	}
}

// ---------------------------------------------------------------------------
// BulletConeShape
// ---------------------------------------------------------------------------

void BulletConeShape::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_radius"), &BulletConeShape::get_radius);
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &BulletConeShape::set_radius);
	ClassDB::bind_method(D_METHOD("get_height"), &BulletConeShape::get_height);
	ClassDB::bind_method(D_METHOD("set_height", "height"), &BulletConeShape::set_height);
	ClassDB::bind_method(D_METHOD("get_axis"), &BulletConeShape::get_axis);
	ClassDB::bind_method(D_METHOD("set_axis", "axis"), &BulletConeShape::set_axis);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.001,4096,0.001,or_greater"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height", PROPERTY_HINT_RANGE, "0.001,8192,0.001,or_greater"), "set_height", "get_height");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "axis", PROPERTY_HINT_ENUM, "X,Y,Z"), "set_axis", "get_axis");
}

btCollisionShape *BulletConeShape::_create_shape() {
	const btScalar r = static_cast<btScalar>(radius);
	const btScalar h = static_cast<btScalar>(height);
	switch (axis) {
		case AXIS_X:
			return new btConeShapeX(r, h);
		case AXIS_Z:
			return new btConeShapeZ(r, h);
		case AXIS_Y:
		default:
			return new btConeShape(r, h);
	}
}

float BulletConeShape::get_radius() const {
	return radius;
}

void BulletConeShape::set_radius(float p_radius) {
	float value = MAX(p_radius, 0.001f);
	if (!Math::is_equal_approx(radius, value)) {
		radius = value;
		mark_dirty();
	}
}

float BulletConeShape::get_height() const {
	return height;
}

void BulletConeShape::set_height(float p_height) {
	float value = MAX(p_height, 0.001f);
	if (!Math::is_equal_approx(height, value)) {
		height = value;
		mark_dirty();
	}
}

BulletCollisionShape::Axis BulletConeShape::get_axis() const {
	return axis;
}

void BulletConeShape::set_axis(Axis p_axis) {
	if (axis != p_axis) {
		axis = p_axis;
		mark_dirty();
	}
}

// ---------------------------------------------------------------------------
// BulletStaticPlaneShape
// ---------------------------------------------------------------------------

void BulletStaticPlaneShape::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_plane_normal"), &BulletStaticPlaneShape::get_plane_normal);
	ClassDB::bind_method(D_METHOD("set_plane_normal", "normal"), &BulletStaticPlaneShape::set_plane_normal);
	ClassDB::bind_method(D_METHOD("get_plane_constant"), &BulletStaticPlaneShape::get_plane_constant);
	ClassDB::bind_method(D_METHOD("set_plane_constant", "constant"), &BulletStaticPlaneShape::set_plane_constant);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "plane_normal"), "set_plane_normal", "get_plane_normal");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "plane_constant"), "set_plane_constant", "get_plane_constant");
}

btCollisionShape *BulletStaticPlaneShape::_create_shape() {
	return new btStaticPlaneShape(bullet_util::to_bt(plane_normal), static_cast<btScalar>(plane_constant));
}

Vector3 BulletStaticPlaneShape::get_plane_normal() const {
	return plane_normal;
}

void BulletStaticPlaneShape::set_plane_normal(const Vector3 &p_normal) {
	if (plane_normal != p_normal) {
		plane_normal = p_normal;
		mark_dirty();
	}
}

float BulletStaticPlaneShape::get_plane_constant() const {
	return plane_constant;
}

void BulletStaticPlaneShape::set_plane_constant(float p_constant) {
	if (!Math::is_equal_approx(plane_constant, p_constant)) {
		plane_constant = p_constant;
		mark_dirty();
	}
}

// ---------------------------------------------------------------------------
// BulletConvexHullShape
// ---------------------------------------------------------------------------

void BulletConvexHullShape::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_point", "point"), &BulletConvexHullShape::add_point);
	ClassDB::bind_method(D_METHOD("add_points", "points"), &BulletConvexHullShape::add_points);
	ClassDB::bind_method(D_METHOD("clear_points"), &BulletConvexHullShape::clear_points);
	ClassDB::bind_method(D_METHOD("get_points"), &BulletConvexHullShape::get_points);
	ClassDB::bind_method(D_METHOD("set_points", "points"), &BulletConvexHullShape::set_points);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR3_ARRAY, "points"), "set_points", "get_points");
}

btCollisionShape *BulletConvexHullShape::_create_shape() {
	btConvexHullShape *hull = new btConvexHullShape();
	const int64_t count = points.size();
	for (int64_t i = 0; i < count; i++) {
		hull->addPoint(bullet_util::to_bt(points[i]), false);
	}
	hull->recalcLocalAabb();
	return hull;
}

void BulletConvexHullShape::add_point(const Vector3 &p_point) {
	points.push_back(p_point);
	mark_dirty();
}

void BulletConvexHullShape::add_points(const PackedVector3Array &p_points) {
	for (int64_t i = 0; i < p_points.size(); i++) {
		points.push_back(p_points[i]);
	}
	mark_dirty();
}

void BulletConvexHullShape::clear_points() {
	points.clear();
	mark_dirty();
}

PackedVector3Array BulletConvexHullShape::get_points() const {
	return points;
}

void BulletConvexHullShape::set_points(const PackedVector3Array &p_points) {
	points = p_points;
	mark_dirty();
}

// ---------------------------------------------------------------------------
// BulletBvhTriangleMeshShape
// ---------------------------------------------------------------------------

void BulletBvhTriangleMeshShape::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_faces"), &BulletBvhTriangleMeshShape::get_faces);
	ClassDB::bind_method(D_METHOD("set_faces", "faces"), &BulletBvhTriangleMeshShape::set_faces);
	ClassDB::bind_method(D_METHOD("set_from_mesh", "mesh"), &BulletBvhTriangleMeshShape::set_from_mesh);
	ClassDB::bind_method(D_METHOD("is_using_quantized_aabb"), &BulletBvhTriangleMeshShape::is_using_quantized_aabb);
	ClassDB::bind_method(D_METHOD("set_use_quantized_aabb", "enabled"), &BulletBvhTriangleMeshShape::set_use_quantized_aabb);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_VECTOR3_ARRAY, "faces"), "set_faces", "get_faces");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_quantized_aabb"), "set_use_quantized_aabb", "is_using_quantized_aabb");
}

BulletBvhTriangleMeshShape::~BulletBvhTriangleMeshShape() {
	_destroy_shape();
}

btCollisionShape *BulletBvhTriangleMeshShape::_create_shape() {
	triangle_mesh = new btTriangleMesh(true, false);
	const int64_t count = faces.size();
	for (int64_t i = 0; i + 2 < count; i += 3) {
		triangle_mesh->addTriangle(
				bullet_util::to_bt(faces[i]),
				bullet_util::to_bt(faces[i + 1]),
				bullet_util::to_bt(faces[i + 2]),
				true);
	}
	return new btBvhTriangleMeshShape(triangle_mesh, use_quantized_aabb, true);
}

void BulletBvhTriangleMeshShape::_release_shape() {
	if (triangle_mesh != nullptr) {
		delete triangle_mesh;
		triangle_mesh = nullptr;
	}
}

PackedVector3Array BulletBvhTriangleMeshShape::get_faces() const {
	return faces;
}

void BulletBvhTriangleMeshShape::set_faces(const PackedVector3Array &p_faces) {
	faces = p_faces;
	mark_dirty();
}

void BulletBvhTriangleMeshShape::set_from_mesh(const Ref<Mesh> &p_mesh) {
	if (p_mesh.is_null()) {
		faces.clear();
		mark_dirty();
		return;
	}
	set_faces(p_mesh->get_faces());
}

bool BulletBvhTriangleMeshShape::is_using_quantized_aabb() const {
	return use_quantized_aabb;
}

void BulletBvhTriangleMeshShape::set_use_quantized_aabb(bool p_enabled) {
	if (use_quantized_aabb != p_enabled) {
		use_quantized_aabb = p_enabled;
		mark_dirty();
	}
}

// ---------------------------------------------------------------------------
// BulletCompoundShape
// ---------------------------------------------------------------------------

void BulletCompoundShape::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_child_shape", "shape", "transform"), &BulletCompoundShape::add_child_shape, DEFVAL(Transform3D()));
	ClassDB::bind_method(D_METHOD("clear_child_shapes"), &BulletCompoundShape::clear_child_shapes);
	ClassDB::bind_method(D_METHOD("get_child_shape_count"), &BulletCompoundShape::get_child_shape_count);
	ClassDB::bind_method(D_METHOD("get_child_shape", "index"), &BulletCompoundShape::get_child_shape);
	ClassDB::bind_method(D_METHOD("get_child_transform", "index"), &BulletCompoundShape::get_child_transform);
}

btCollisionShape *BulletCompoundShape::_create_shape() {
	btCompoundShape *compound = new btCompoundShape();
	const int64_t count = std::min<int64_t>(child_shapes.size(), child_transforms.size());
	for (int64_t i = 0; i < count; i++) {
		Ref<BulletCollisionShape> child = child_shapes[i];
		if (child.is_null()) {
			continue;
		}
		btCollisionShape *child_shape = child->get_bt_shape();
		if (child_shape == nullptr) {
			continue;
		}
		Transform3D transform = child_transforms[i];
		compound->addChildShape(bullet_util::to_bt(transform), child_shape);
	}
	return compound;
}

void BulletCompoundShape::add_child_shape(const Ref<BulletCollisionShape> &p_shape, const Transform3D &p_transform) {
	if (p_shape.is_null()) {
		return;
	}
	child_shapes.push_back(p_shape);
	child_transforms.push_back(p_transform);
	mark_dirty();
}

void BulletCompoundShape::clear_child_shapes() {
	child_shapes.clear();
	child_transforms.clear();
	mark_dirty();
}

int32_t BulletCompoundShape::get_child_shape_count() const {
	return static_cast<int32_t>(std::min<int64_t>(child_shapes.size(), child_transforms.size()));
}

Ref<BulletCollisionShape> BulletCompoundShape::get_child_shape(int32_t p_index) const {
	if (p_index < 0 || p_index >= get_child_shape_count()) {
		return Ref<BulletCollisionShape>();
	}
	return child_shapes[p_index];
}

Transform3D BulletCompoundShape::get_child_transform(int32_t p_index) const {
	if (p_index < 0 || p_index >= get_child_shape_count()) {
		return Transform3D();
	}
	return child_transforms[p_index];
}
