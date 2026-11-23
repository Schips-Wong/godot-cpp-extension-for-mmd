#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "bullet/bullet_collision_shape.h"
#include "bullet/bullet_constraint.h"
#include "bullet/bullet_rigid_body.h"
#include "bullet/bullet_world.h"

using namespace godot;

void initialize_gdextension_types(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	// Collision shapes.
	GDREGISTER_ABSTRACT_CLASS(BulletCollisionShape);
	GDREGISTER_CLASS(BulletEmptyShape);
	GDREGISTER_CLASS(BulletBoxShape);
	GDREGISTER_CLASS(BulletSphereShape);
	GDREGISTER_CLASS(BulletCapsuleShape);
	GDREGISTER_CLASS(BulletCylinderShape);
	GDREGISTER_CLASS(BulletConeShape);
	GDREGISTER_CLASS(BulletStaticPlaneShape);
	GDREGISTER_CLASS(BulletConvexHullShape);
	GDREGISTER_CLASS(BulletBvhTriangleMeshShape);
	GDREGISTER_CLASS(BulletCompoundShape);

	// Simulation.
	GDREGISTER_CLASS(BulletWorld);
	GDREGISTER_CLASS(BulletRigidBody);

	// Constraints.
	GDREGISTER_ABSTRACT_CLASS(BulletConstraint);
	GDREGISTER_CLASS(BulletPoint2PointConstraint);
	GDREGISTER_CLASS(BulletHingeConstraint);
	GDREGISTER_CLASS(BulletSliderConstraint);
	GDREGISTER_CLASS(BulletConeTwistConstraint);
	GDREGISTER_CLASS(BulletGeneric6DofConstraint);
	GDREGISTER_CLASS(BulletGeneric6DofSpringConstraint);
	GDREGISTER_CLASS(BulletFixedConstraint);
}

void uninitialize_gdextension_types(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}

extern "C"
{
	// Initialization
	GDExtensionBool GDE_EXPORT bullet3_library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization)
	{
		GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
		init_obj.register_initializer(initialize_gdextension_types);
		init_obj.register_terminator(uninitialize_gdextension_types);
		init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

		return init_obj.init();
	}
}
