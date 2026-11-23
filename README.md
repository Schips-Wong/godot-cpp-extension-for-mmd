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
| `BulletWorld` | owns the `btDiscreteDynamicsWorld`: `gravity`, `max_sub_steps`, `fixed_timestep`, `auto_step`, `contact_monitor`, plus the query API below |
| `BulletRigidBody` | `shape`, `mode` (Static/Kinematic/Dynamic), `mass`, `friction`, `restitution`, `rolling_friction`, `linear_damping`, `angular_damping`, `collision_layer`, `collision_mask`, `inertia`, `activation_state`, velocities, impulses, `get_body_transform()` / `set_body_transform()` |
| `BulletGhostBody` | sensor volume (`btGhostObject`): `shape`, `collision_layer`, `collision_mask`, `monitoring`, `get_overlapping_bodies()`, `is_overlapping_body()`, `body_entered` / `body_exited` signals |
| `BulletConstraint` | base class; its own global transform is the joint frame (like an MMD joint). `use_world_transform = false` + `frame_in_a` / `frame_in_b` uses explicit frames instead |

### Queries

All queries use the same filter arguments as `ray_test()`: `collision_mask` (default `-1` = all)
and `collision_layer` (default `-1` = the default group). Note that a body is only seen when the
two filters are mutually satisfied, so give query-only geometry (ghosts, sensors) its own layer.

| Method | Result |
| --- | --- |
| `ray_test(from, to)` / `ray_test_all(from, to)` | closest hit / every hit as `{ hit, position, normal, fraction, collider }` |
| `convex_sweep(shape, from, to)` / `convex_sweep_all(...)` | sweeps a **convex** shape, same dictionary layout |
| `contact_test(shape, transform)` | touching contacts as `{ position, normal, distance, collider }` |
| `overlap_test(shape, transform)` | colliders overlapping the placed shape (each once) |

`from` / `to` of the sweeps are full `Transform3D`s, so the shape orientation during the sweep is
under control. `BvhTriangleMesh` and `Compound` shapes are rejected by `convex_sweep()` (Bullet
only casts convex shapes); use `contact_test()` / `overlap_test()` or a convex hull instead.

```gdscript
var hit := world.convex_sweep(probe, Transform3D(Basis(), tip), Transform3D(Basis(), tip + Vector3.DOWN * 3.0), ground_layer)
if hit.get("hit", false):
	print("ground distance %.2f" % (3.0 * hit["fraction"]))
```

Constraint subclasses: `BulletPoint2PointConstraint`, `BulletHingeConstraint`,
`BulletSliderConstraint`, `BulletConeTwistConstraint`, `BulletGeneric6DofConstraint`,
`BulletGeneric6DofSpringConstraint`, `BulletFixedConstraint`.

`BulletGeneric6DofSpringConstraint` exposes `set_spring_enabled(axis, enabled)`,
`set_spring_stiffness(axis, value)`, `set_spring_damping(axis, value)` and
`configure_spring(axis, stiffness, damping)` where axis `0..2` is linear X/Y/Z and `3..5` is
angular X/Y/Z.

## MMD layer

`MMDPhysicsWorld`, `MMDRigidBody` and `MMDJoint` put the PMX rigid body / joint model on top of
the generic classes above. Bones are driven directly on a `Skeleton3D`.

| Class | Notes |
| --- | --- |
| `MMDPhysicsWorld` | `BulletWorld` + `skeleton` (NodePath). `simulation_enabled`, `reset_physics()`, `find_bone(name)`. Bones are read before the step and written after it. |
| `MMDRigidBody` | `bone_name` / `bone_index`, `bone_mode`, `shape_type` + `size` (PMX sphere 0 / box 1 / capsule 2), `mass`, damping, `group` 0..15, `group_mask` 16 bits, `bone_offset`. |
| `MMDJoint` | `BulletGeneric6DofSpringConstraint` with PMX spring layout: `linear_spring` / `angular_spring` are one stiffness per axis (0 disables that spring), plus `spring_damping`. |

`bone_mode` follows PMX:

* `BONE_FOLLOW_BONE` (0) – the bone drives the body. The body becomes kinematic.
* `BONE_PHYSICS` (1) – the body drives the bone.
* `BONE_PHYSICS_WITH_BONE_OFFSET` (2) – currently identical to 1, matching the reference
  GodotMMD implementation.

`bone_offset` is the body frame expressed in bone space (`body = bone_offset * bone`). It is
derived automatically on the first simulation step from the node transform and the bone pose,
which is why an importer only has to place `MMDRigidBody` nodes at the PMX rigid body transform.
A body never falls asleep (sleeping thresholds are 0), so bones cannot freeze mid animation.

```gdscript
var world := MMDPhysicsWorld.new()
world.skeleton = ^"../Skeleton3D"
add_child(world)

var body := MMDRigidBody.new()
body.bone_name = "左腕"
body.bone_mode = MMDRigidBody.BONE_PHYSICS
body.shape_type = MMDRigidBody.SHAPE_CAPSULE
body.size = Vector3(0.1, 0.6, 0.0)
body.mass = 1.0
body.position = Vector3(0.0, 1.0, 0.0) # PMX rigid body transform, model space
world.add_child(body)

var joint := MMDJoint.new()
joint.body_a = body.get_path()
joint.body_b = other_body.get_path()
joint.position = Vector3(0.0, 1.0, 0.0)
joint.angular_lower_limit = Vector3(-0.5, -0.5, -0.5)
joint.angular_upper_limit = Vector3(0.5, 0.5, 0.5)
joint.linear_spring = Vector3(0.0, 0.0, 0.0)
joint.angular_spring = Vector3(0.0, 5.0, 0.0)
world.add_child(joint)
```

`project/mmd_demo.tscn` builds a three bone chain at runtime, swings the root bone and prints the
bone heights plus a downward `convex_sweep()` distance to a static ground body (filtered to the
ground's collision layer). `project/bullet_demo.tscn` shows the generic API, including a
`BulletGhostBody` sensor that slides through the box stack and a convex sweep.

## PMX import

`.pmx` files are imported as `PackedScene` by `PmxImporter` (an `EditorImportPlugin` written in
C++). The generated scene is:

```
Node3D (model name)
  Skeleton3D (bones + rest poses)
    MeshInstance3D (ArrayMesh + Skin, one surface per PMX material)
  MMDPhysicsWorld (skeleton = ../Skeleton)
    MMDRigidBody x N (bone_name bound, PMX mode/shape/mass/damping/group/mask)
    MMDJoint x N (body_a / body_b, limits, PMX springs)
```

Import options: `scale` (uniform, applies to the mesh, bones and physics) and `import_physics`.

Because `EditorPlugins::add_import_plugin()` is not exposed to GDExtension, a 12 line editor plugin
(`project/addons/pmx_import/plugin.gd`) calls `add_import_plugin(PmxImporter.new())`. It is enabled
in `project/project.godot`; keep that entry (or re-enable the plugin) in your own project.

`PmxSceneBuilder` is the same conversion exposed to scripts, which is handy for runtime loading or
for tools:

```gdscript
var builder := PmxSceneBuilder.new()
print(builder.inspect_file("res://models/model.pmx")) # counts / bone names, no scene built
var scene := builder.build_from_file("res://models/model.pmx", 0.08, true)
if scene != null:
	add_child(scene.instantiate())
```

Details that differ from a naive implementation:

* Positions and normals are converted to Godot's right handed space (`z` negated), rigid body and
  joint rotations have `x`/`y` negated, and node euler order is Godot's default `YXZ`, which is
  what MMD uses.
* PMX stores the groups a body does **not** collide with; `group_mask` is the inverted 16 bit mask
  handed to Bullet.
* `BDEF1/2/4` and `QDEF` become 4 bone weights (normalised); `SDEF` is imported as `BDEF2` (the
  spherical correction is dropped).
* Vertices are remapped per material, so each surface only carries the vertices it uses.

Textures are referenced as imported resources (`ResourceLoader.load()`); the importer first calls
`append_import_external_resource()` for every referenced file, and if a texture is still missing
from the import cache (first pass) the image is embedded in the scene instead, so a freshly
imported model is never untextured.

Not imported yet: morphs (blend shapes), toon/sphere textures, display frames, IK, and VMD
animations.

`project/pmx_demo.tscn` loads `res://models/托特.pmx` (a 498 bone / 376 body / 625 joint model from
the reference project), prefers the imported scene and reports the live physics state. Delete
`project/models/` if you do not want the sample assets.

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

* Concave `GImpact` shapes for moving meshes.
* A `PhysicsServer3D` based back end (this extension only provides its own nodes).
* VMD animation import, and PMX morphs / toon / sphere textures / IK.
* PMX mode 2 (`物理演算+ボーン位置合わせ`) does not yet keep the animated bone position.
* Runtime scaling of a model: scale at import time (the `scale` option) instead of scaling the
  skeleton node, because the Bullet bodies live in world space.
