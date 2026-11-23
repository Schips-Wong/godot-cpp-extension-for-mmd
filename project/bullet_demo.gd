extends Node3D

## Minimal Bullet3 GDExtension demo.
## Everything is built from script so the scene has no dependency on resource serialization.

const BOX_COUNT := 6

var world: BulletWorld
var _report_timer := 0.0


func _ready() -> void:
	world = BulletWorld.new()
	world.name = "World"
	world.gravity = Vector3(0, -9.8, 0)
	world.contact_monitor = true
	add_child(world)

	_add_ground()
	_add_box_stack()
	_add_spring_box()
	_add_camera_and_light()


func _add_ground() -> void:
	var shape := BulletBoxShape.new()
	shape.half_extents = Vector3(6.0, 0.5, 6.0)

	var body := _create_body("Ground", shape, Vector3(0, -0.5, 0), BulletRigidBody.MODE_STATIC, 0.0)
	world.add_child(body)
	_attach_mesh(body, BoxMesh.new(), Vector3(12, 1, 12), Color(0.35, 0.4, 0.45))


func _add_box_stack() -> void:
	var shape := BulletBoxShape.new()
	shape.half_extents = Vector3(0.5, 0.5, 0.5)

	for index in BOX_COUNT:
		var body := _create_body(
			"Box%d" % index,
			shape,
			Vector3(0, 0.5 + index * 1.02, 0),
			BulletRigidBody.MODE_DYNAMIC,
			1.0)
		body.friction = 0.6
		body.restitution = 0.05
		world.add_child(body)
		_attach_mesh(body, BoxMesh.new(), Vector3(1, 1, 1), Color(0.8, 0.55, 0.3))

	# A falling sphere that reports its contacts.
	var sphere_shape := BulletSphereShape.new()
	sphere_shape.radius = 0.6

	var sphere := _create_body("Sphere", sphere_shape, Vector3(2.5, 6.0, 0.0), BulletRigidBody.MODE_DYNAMIC, 2.0)
	sphere.restitution = 0.45
	sphere.body_entered.connect(_on_sphere_entered)
	world.add_child(sphere)
	_attach_mesh(sphere, SphereMesh.new(), Vector3(0.6, 1.2, 0), Color(0.9, 0.75, 0.2))


func _add_spring_box() -> void:
	var shape := BulletBoxShape.new()
	shape.half_extents = Vector3(0.4, 0.4, 0.4)

	var body := _create_body("SpringBox", shape, Vector3(-2.5, 3.0, 0.0), BulletRigidBody.MODE_DYNAMIC, 1.0)
	world.add_child(body)
	_attach_mesh(body, BoxMesh.new(), Vector3(0.8, 0.8, 0.8), Color(0.4, 0.8, 0.5))

	var joint := BulletGeneric6DofSpringConstraint.new()
	joint.name = "SpringJoint"
	joint.body_a = world.get_node("Ground").get_path()
	joint.body_b = body.get_path()
	# Free movement, but only the vertical axis is limited to +/- 1.5 m.
	joint.linear_lower_limit = Vector3(0, -1.5, 0)
	joint.linear_upper_limit = Vector3(0, 1.5, 0)
	joint.angular_lower_limit = Vector3(0, 0, 0)
	joint.angular_upper_limit = Vector3(0, 0, 0)
	joint.position = Vector3(-2.5, 4.5, 0.0)
	world.add_child(joint)

	# Axis 1 is linear Y: pull the box back to its equilibrium point.
	joint.configure_spring(1, 25.0, 2.0)


func _create_body(name: String, shape: BulletCollisionShape, position: Vector3, mode: int, mass: float) -> BulletRigidBody:
	var body := BulletRigidBody.new()
	body.name = name
	body.mode = mode
	body.mass = mass
	body.shape = shape
	body.position = position
	return body


func _attach_mesh(body: BulletRigidBody, mesh: Mesh, size: Vector3, color: Color) -> void:
	if mesh is BoxMesh:
		mesh.size = size
	elif mesh is SphereMesh:
		mesh.radius = size.x
		mesh.height = size.y

	var material := StandardMaterial3D.new()
	material.albedo_color = color

	var instance := MeshInstance3D.new()
	instance.mesh = mesh
	instance.material_override = material
	body.add_child(instance)


func _add_camera_and_light() -> void:
	var camera := Camera3D.new()
	camera.fov = 60.0
	add_child(camera)
	camera.look_at_from_position(Vector3(7, 5, 9), Vector3(0, 2, 0))

	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-50, -35, 0)
	light.shadow_enabled = true
	add_child(light)


func _on_sphere_entered(body: BulletRigidBody) -> void:
	print("[bullet] sphere touched %s" % body.name)


func _physics_process(delta: float) -> void:
	_report_timer += delta
	if _report_timer < 1.0:
		return
	_report_timer = 0.0

	var hit := world.ray_test(Vector3(0.0, 6.0, 0.0), Vector3(0.0, -6.0, 0.0))
	var collider_name := "none"
	if hit.get("hit", false) and hit.get("collider") != null:
		collider_name = hit["collider"].name

	var spring_box := world.get_node_or_null("SpringBox") as Node3D
	var spring_height: float = 0.0
	if spring_box != null:
		spring_height = spring_box.position.y
	print("[bullet] bodies=%d ray_hit=%s collider=%s spring_y=%.2f" % [world.get_rigid_body_count(), hit.get("hit", false), collider_name, spring_height])
