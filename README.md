# Bullet 3 physics for Godot 4 (GDExtension)

Bullet 3.27 is compiled directly into this GDExtension, and exposed to Godot as a small set of
nodes whose API follows [BulletSharp](https://github.com/AndresTraks/BulletSharp) naming and
semantics. No external shared library is required at runtime: the extension DLL/so/dylib is
self-contained.

This is the "standalone node" flavour: Bullet lives next to Godot's own physics engine, with its
own world, bodies and constraints, instead of replacing `PhysicsServer3D`.

## Contents

* `src/bullet/` – the binding layer (`BulletWorld`, `BulletRigidBody`, shapes, constraints)
* `thirdparty/bullet3/src/` – vendored Bullet 3.27 sources (BulletCollision, BulletDynamics, LinearMath)
* `godot-cpp/` – the C++ bindings submodule
* `project/` – a Godot project with a runnable demo (`bullet_demo.tscn`)

## Classes

Shapes are `Resource`s, so they can be shared and stored inside scenes.

| Shape | Notes |
| --- | --- |
| `BulletBoxShape` | `half_extents` |
| `BulletSphereShape` | `radius` |
| `BulletCapsuleShape` | `radius`, `height`, `axis` |
| `BulletCylinderShape` | `half_extents`, `axis` |
| `BulletConeShape` | `radius`, `height`, `axis` |
| `BulletStaticPlaneShape` | `plane_normal`, `plane_constant` |
| `BulletConvexHullShape` | `points` / `add_point()` |
| `BulletBvhTriangleMeshShape` | static triangle mesh, `set_faces()` or `set_from_mesh()` |
| `BulletCompoundShape` | `add_child_shape(shape, transform)` |
| `BulletEmptyShape` | placeholder |

Every shape exposes `margin`, `local_scaling` and `calculate_local_inertia(mass)`.

| Node | Notes |
| --- | --- |
| `BulletWorld` | owns the `btDiscreteDynamicsWorld`: `gravity`, `max_sub_steps`, `fixed_timestep`, `auto_step`, `contact_monitor`, `ray_test()`, `ray_test_all()` |
| `BulletRigidBody` | `shape`, `mode` (Static/Kinematic/Dynamic), `mass`, `friction`, `restitution`, `rolling_friction`, `linear_damping`, `angular_damping`, `collision_layer`, `collision_mask`, `inertia`, `activation_state`, velocities, impulses |
| `BulletConstraint` | base class; its own global transform is the joint frame (like an MMD joint). `use_world_transform = false` + `frame_in_a` / `frame_in_b` uses explicit frames instead |

Constraint subclasses: `BulletPoint2PointConstraint`, `BulletHingeConstraint`,
`BulletSliderConstraint`, `BulletConeTwistConstraint`, `BulletGeneric6DofConstraint`,
`BulletGeneric6DofSpringConstraint`, `BulletFixedConstraint`.

`BulletGeneric6DofSpringConstraint` exposes `set_spring_enabled(axis, enabled)`,
`set_spring_stiffness(axis, value)`, `set_spring_damping(axis, value)` and
`configure_spring(axis, stiffness, damping)` where axis `0..2` is linear X/Y/Z and `3..5` is
angular X/Y/Z.

## Usage

```gdscript
var world := BulletWorld.new()
world.gravity = Vector3(0, -9.8, 0)
world.contact_monitor = true
add_child(world)

var shape := BulletSphereShape.new()
shape.radius = 0.5

var body := BulletRigidBody.new()
body.mode = BulletRigidBody.MODE_DYNAMIC
body.mass = 1.0
body.shape = shape
body.position = Vector3(0, 5, 0)
body.body_entered.connect(func(other): print("touched ", other.name))
world.add_child(body)

var hit := world.ray_test(Vector3(0, 10, 0), Vector3(0, -10, 0))
```

* Any `BulletRigidBody` / `BulletConstraint` below a `BulletWorld` registers itself automatically.
* Dynamic bodies write their transform back to the node every physics tick; static and kinematic
  bodies are driven by the node transform.
* `BulletWorld.contact_monitor` must be enabled for `body_entered`, `body_exited` and
  `contact_point` to be emitted (the contact scan costs time, so it is off by default).

## Building

```shell
# SCons
scons -j8 target=editor platform=windows arch=x86_64

# CMake
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The library is copied to `project/bin/<platform>/` automatically. Replace `target=editor` with
`target=template_debug` / `target=template_release` for export templates.

The first build compiles ~130 Bullet translation units. They are compiled one by one on purpose:
several Bullet files define `BT_USE_SSE_IN_API` locally, which does not survive the amalgamated
`btBullet*All.cpp` files on MSVC. Nothing inside `thirdparty/` has been modified.

`BT_USE_DOUBLE_PRECISION` is defined automatically when the bindings are built with
`precision=double` (SCons) or `-DGODOTCPP_PRECISION=double` (CMake), so Bullet and `real_t` always
match.

## Not implemented yet

* Overlap/ghost queries (`btGhostObject`) and convex sweeps.
* Concave `GImpact` shapes for moving meshes.
* A `PhysicsServer3D` based back end (this extension only provides its own nodes).
* MMD specific helpers (PMX rigid body / joint import).
