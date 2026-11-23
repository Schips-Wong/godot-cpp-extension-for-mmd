#pragma once

// Godot <-> Bullet math conversions.
// Bullet is compiled with btScalar == float by default (BT_USE_DOUBLE_PRECISION switches it to double).

#include <btBulletDynamicsCommon.h>

#include "godot_cpp/variant/basis.hpp"
#include "godot_cpp/variant/quaternion.hpp"
#include "godot_cpp/variant/transform3d.hpp"
#include "godot_cpp/variant/vector3.hpp"

namespace bullet_util {

inline btVector3 to_bt(const godot::Vector3 &p_v) {
	return btVector3(static_cast<btScalar>(p_v.x), static_cast<btScalar>(p_v.y), static_cast<btScalar>(p_v.z));
}

inline godot::Vector3 to_godot(const btVector3 &p_v) {
	return godot::Vector3(static_cast<godot::real_t>(p_v.x()), static_cast<godot::real_t>(p_v.y()), static_cast<godot::real_t>(p_v.z()));
}

inline btQuaternion to_bt(const godot::Quaternion &p_q) {
	return btQuaternion(static_cast<btScalar>(p_q.x), static_cast<btScalar>(p_q.y), static_cast<btScalar>(p_q.z), static_cast<btScalar>(p_q.w));
}

inline godot::Quaternion to_godot(const btQuaternion &p_q) {
	return godot::Quaternion(static_cast<godot::real_t>(p_q.x()), static_cast<godot::real_t>(p_q.y()), static_cast<godot::real_t>(p_q.z()), static_cast<godot::real_t>(p_q.w()));
}

// Godot's Basis stores its rows the same way btMatrix3x3 does (element [row][column]).
inline btMatrix3x3 to_bt(const godot::Basis &p_basis) {
	return btMatrix3x3(
			static_cast<btScalar>(p_basis.rows[0].x), static_cast<btScalar>(p_basis.rows[0].y), static_cast<btScalar>(p_basis.rows[0].z),
			static_cast<btScalar>(p_basis.rows[1].x), static_cast<btScalar>(p_basis.rows[1].y), static_cast<btScalar>(p_basis.rows[1].z),
			static_cast<btScalar>(p_basis.rows[2].x), static_cast<btScalar>(p_basis.rows[2].y), static_cast<btScalar>(p_basis.rows[2].z));
}

inline godot::Basis to_godot(const btMatrix3x3 &p_matrix) {
	godot::Basis basis;
	for (int column = 0; column < 3; column++) {
		basis.set_column(column, godot::Vector3(
										 static_cast<godot::real_t>(p_matrix[0][column]),
										 static_cast<godot::real_t>(p_matrix[1][column]),
										 static_cast<godot::real_t>(p_matrix[2][column])));
	}
	return basis;
}

inline btTransform to_bt(const godot::Transform3D &p_transform) {
	return btTransform(to_bt(p_transform.basis), to_bt(p_transform.origin));
}

inline godot::Transform3D to_godot(const btTransform &p_transform) {
	return godot::Transform3D(to_godot(p_transform.getBasis()), to_godot(p_transform.getOrigin()));
}

} // namespace bullet_util
