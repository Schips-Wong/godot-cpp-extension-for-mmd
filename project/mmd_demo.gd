extends Node3D

## MMD style physics demo: a three bone chain (Skeleton3D) driven by MMDRigidBody /
## MMDJoint nodes. Bone 0 is kinematic (follows the bone), bones 1 and 2 are dynamic and
## write their simulation result back onto the skeleton.

const BONE_NAMES := ["root", "chain1", "chain2"]
const LINK_LENGTH := 1.0
const BALL_RADIUS := 0.15

var skeleton: Skeleton3D
var world: MMDPhysicsWorld
var bodies: Array[MMDRigidBody] = []

const GROUND_LAYER := 2

var probe_shape: BulletSphereShape

var _report_timer := 0.0
var _elapsed := 0.0
var _swing := 0.0


func _ready() -> void:
	_build_skeleton()

	world = MMDPhysicsWorld.new()
	world.name = "Physics"
	world.gravity = Vector3(0, -9.8, 0)
	add_child(world)
	world.skeleton = world.get_path_to(skeleton)

	_build_ground()
	_build_bodies()
	_build_joints()
	_build_visuals()

	probe_shape = BulletSphereShape.new()
	probe_shape.radius = 0.2

	print("[mmd] chain2 height before simulation = %.3f" % _bone_height(2))


func _build_skeleton() -> void:
	skeleton = Skeleton3D.new()
	skeleton.name = "Skeleton"
	add_child(skeleton)

	for index in BONE_NAMES.size():
		skeleton.add_bone(BONE_NAMES[index])
		if index == 0:
			skeleton.set_bone_rest(index, Transform3D())
		else:
			skeleton.set_bone_parent(index, index - 1)
			skeleton.set_bone_rest(index, Transform3D(Basis(), Vector3(LINK_LENGTH, 0, 0)))
	skeleton.reset_bone_poses()


func _build_ground() -> void:
	var shape := BulletBoxShape.new()
	shape.half_extents = Vector3(4.0, 0.2, 4.0)

	var ground := BulletRigidBody.new()
	ground.name = "Ground"
	ground.mode = BulletRigidBody.MODE_STATIC
	ground.shape = shape
	ground.collision_layer = GROUND_LAYER # PMX group 1
	ground.position = Vector3(0, -2.0, 0)
	world.add_child(ground)


func _build_bodies() -> void:
	for index in BONE_NAMES.size():
		var body := MMDRigidBody.new()
		body.name = "Body%d" % index
		body.bone_name = BONE_NAMES[index]
		body.bone_mode = MMDRigidBody.BONE_FOLLOW_BONE if index == 0 else MMDRigidBody.BONE_PHYSICS
		body.shape_type = MMDRigidBody.SHAPE_SPHERE
		body.size = Vector3(BALL_RADIUS, 0, 0)
		body.mass = 1.0
		body.linear_damping = 0.1
		body.angular_damping = 0.1
		body.friction = 0.5
		body.restitution = 0.0
		body.group = 0
		# PMX group mask: collide with every group (the links are 1 m apart, so they never touch).
		body.group_mask = 0xFFFF
		body.position = Vector3(index * LINK_LENGTH, 0, 0)
		world.add_child(body)
		bodies.append(body)


func _build_joints() -> void:
	for index in range(1, BONE_NAMES.size()):
		var joint := MMDJoint.new()
		joint.name = "Joint%d" % index
		joint.body_a = bodies[index - 1].get_path()
		joint.body_b = bodies[index].get_path()
		joint.position = Vector3(index * LINK_LENGTH, 0, 0)
		# Translation is locked, rotation is free within +/- 0.9 rad (about 51 degrees).
		joint.linear_lower_limit = Vector3.ZERO
		joint.linear_upper_limit = Vector3.ZERO
		joint.angular_lower_limit = Vector3(-0.9, -0.9, -0.9)
		joint.angular_upper_limit = Vector3(0.9, 0.9, 0.9)
		world.add_child(joint)


func _build_visuals() -> void:
	for bone_name in BONE_NAMES:
		var attachment := BoneAttachment3D.new()
		attachment.name = "Attach_%s" % bone_name
		attachment.bone_name = bone_name
		skeleton.add_child(attachment)

		var sphere := SphereMesh.new()
		sphere.radius = BALL_RADIUS
		sphere.height = BALL_RADIUS * 2.0

		var material := StandardMaterial3D.new()
		material.albedo_color = Color(0.9, 0.6, 0.25)

		var instance := MeshInstance3D.new()
		instance.mesh = sphere
		instance.material_override = material
		attachment.add_child(instance)

	var camera := Camera3D.new()
	add_child(camera)
	camera.look_at_from_position(Vector3(2.0, 1.5, 5.0), Vector3(1.0, -0.5, 0.0))

	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-50, -35, 0)
	add_child(light)


func _bone_height(index: int) -> float:
	var pose := skeleton.get_global_transform() * skeleton.get_bone_global_pose(index)
	return pose.origin.y


func _bone_position(index: int) -> Vector3:
	return (skeleton.get_global_transform() * skeleton.get_bone_global_pose(index)).origin


func _physics_process(delta: float) -> void:
	_elapsed += delta

	# Swing the root bone: the kinematic body has to follow it, and the chain follows the body.
	if _elapsed > 1.0:
		_swing += delta
		var angle := sin(_swing * 2.0) * 0.5
		skeleton.set_bone_pose_rotation(0, Quaternion(Vector3(0, 0, 1), angle))

	_report_timer += delta
	if _report_timer < 1.0:
		return
	_report_timer = 0.0
	var tip := _bone_position(2)

	# Sweep the tip downwards. collision layer 2 is the ground group, so the chain bodies
	# (which sit on layer 1) are ignored and the fraction is the real distance to the floor.
	var sweep_from := Transform3D(Basis(), tip)
	var sweep_to := Transform3D(Basis(), tip + Vector3(0, -4.0, 0))
	var sweep := world.convex_sweep(probe_shape, sweep_from, sweep_to, GROUND_LAYER)
	var overlapping := world.overlap_test(probe_shape, sweep_from)

	print("[mmd] bone heights: root=%.3f chain1=%.3f chain2=%.3f | tip=(%.2f, %.2f) | ground_hit=%s dist=%.2f overlaps=%d" % [
		_bone_height(0), _bone_height(1), _bone_height(2), tip.x, tip.y,
		sweep.get("hit", false), 4.0 * float(sweep.get("fraction", 1.0)), overlapping.size()])
