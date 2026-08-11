extends VBoxContainer

const LATITUDE_DIVISIONS := 28
const LONGITUDE_DIVISIONS := 56

var pending_volume := 0.0
var pending_status := ""
var has_valid_volume := false

@onready var preview_surface: SubViewportContainer = $PreviewSurface
@onready var morphology: MeshInstance3D = $PreviewSurface/PreviewViewport/World/MorphologyMesh
@onready var status: Label = $Status

func _ready() -> void:
	_apply_state()

func tessellation_directions() -> PackedVector3Array:
	var directions := PackedVector3Array()
	directions.append(Vector3.UP)
	for latitude in range(1, LATITUDE_DIVISIONS):
		var theta := PI * float(latitude) / float(LATITUDE_DIVISIONS)
		var sin_theta := sin(theta)
		var cos_theta := cos(theta)
		for longitude in range(LONGITUDE_DIVISIONS):
			var phi := TAU * float(longitude) / float(LONGITUDE_DIVISIONS)
			directions.append(Vector3(sin_theta * cos(phi), cos_theta, sin_theta * sin(phi)))
	directions.append(Vector3.DOWN)
	return directions

func show_shape(volume: float, radii: PackedFloat64Array, volume_status: String) -> void:
	var directions := tessellation_directions()
	has_valid_volume = is_finite(volume) and volume > 0.0 and radii.size() == directions.size()
	pending_volume = volume
	pending_status = volume_status
	if has_valid_volume:
		has_valid_volume = _build_mesh(directions, radii)
		if not has_valid_volume: pending_status = tr("ux.volume_preview_mesh_invalid")
	if is_node_ready(): _apply_state()

func show_unavailable(message: String) -> void:
	has_valid_volume = false
	pending_status = message
	if is_node_ready(): _apply_state()

func _apply_state() -> void:
	status.text = pending_status
	preview_surface.visible = has_valid_volume
	morphology.visible = has_valid_volume
	if not has_valid_volume:
		return

func _build_mesh(directions: PackedVector3Array, radii: PackedFloat64Array) -> bool:
	var vertices := PackedVector3Array()
	vertices.resize(directions.size())
	for index in range(directions.size()):
		var radius := float(radii[index])
		if not is_finite(radius) or radius <= 0.0: return false
		vertices[index] = directions[index] * radius
	var indices := PackedInt32Array()
	for longitude in range(LONGITUDE_DIVISIONS):
		var next := (longitude + 1) % LONGITUDE_DIVISIONS
		_add_outward_triangle(indices, vertices, 0, 1 + next, 1 + longitude)
	for latitude in range(LATITUDE_DIVISIONS - 2):
		var upper := 1 + latitude * LONGITUDE_DIVISIONS
		var lower := upper + LONGITUDE_DIVISIONS
		for longitude in range(LONGITUDE_DIVISIONS):
			var next := (longitude + 1) % LONGITUDE_DIVISIONS
			_add_outward_triangle(indices, vertices, upper + longitude, upper + next, lower + longitude)
			_add_outward_triangle(indices, vertices, upper + next, lower + next, lower + longitude)
	var bottom := vertices.size() - 1
	var last_ring := bottom - LONGITUDE_DIVISIONS
	for longitude in range(LONGITUDE_DIVISIONS):
		var next := (longitude + 1) % LONGITUDE_DIVISIONS
		_add_outward_triangle(indices, vertices, bottom, last_ring + longitude, last_ring + next)
	var mesh_volume := _mesh_volume(vertices, indices)
	if not is_finite(mesh_volume) or mesh_volume <= 0.0: return false
	var correction := pow(pending_volume / mesh_volume, 1.0 / 3.0)
	if not is_finite(correction) or correction <= 0.0: return false
	for index in range(vertices.size()): vertices[index] *= correction
	var normals := PackedVector3Array()
	normals.resize(vertices.size())
	for index in range(indices.size() / 3):
		var a := indices[index * 3]; var b := indices[index * 3 + 1]; var c := indices[index * 3 + 2]
		var normal := (vertices[b] - vertices[a]).cross(vertices[c] - vertices[a])
		normals[a] += normal; normals[b] += normal; normals[c] += normal
	for index in range(normals.size()):
		if normals[index].length_squared() <= 0.0: return false
		normals[index] = normals[index].normalized()
	var arrays := []
	arrays.resize(ArrayMesh.ARRAY_MAX)
	arrays[ArrayMesh.ARRAY_VERTEX] = vertices
	arrays[ArrayMesh.ARRAY_NORMAL] = normals
	arrays[ArrayMesh.ARRAY_INDEX] = indices
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	morphology.mesh = mesh
	return true

func _add_outward_triangle(indices: PackedInt32Array, vertices: PackedVector3Array, a: int, b: int, c: int) -> void:
	var normal := (vertices[b] - vertices[a]).cross(vertices[c] - vertices[a])
	if normal.dot(vertices[a] + vertices[b] + vertices[c]) < 0.0:
		var swap := b; b = c; c = swap
	indices.append(a); indices.append(b); indices.append(c)

func _mesh_volume(vertices: PackedVector3Array, indices: PackedInt32Array) -> float:
	var signed_volume := 0.0
	for index in range(indices.size() / 3):
		var a := vertices[indices[index * 3]]; var b := vertices[indices[index * 3 + 1]]; var c := vertices[indices[index * 3 + 2]]
		signed_volume += a.dot(b.cross(c)) / 6.0
	return abs(signed_volume)
