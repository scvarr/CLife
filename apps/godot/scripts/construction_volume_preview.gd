extends VBoxContainer

const UNIT_SPHERE_VOLUME := 4.0 * PI / 3.0

var pending_volume := 0.0
var pending_status := ""
var has_valid_volume := false

@onready var preview_surface: SubViewportContainer = $PreviewSurface
@onready var sphere: MeshInstance3D = $PreviewSurface/PreviewViewport/World/VolumeSphere
@onready var status: Label = $Status

func _ready() -> void:
	_apply_state()

func show_volume(volume: float, volume_status: String) -> void:
	has_valid_volume = is_finite(volume) and volume > 0.0
	pending_volume = volume
	pending_status = volume_status
	if is_node_ready(): _apply_state()

func show_unavailable(message: String) -> void:
	has_valid_volume = false
	pending_status = message
	if is_node_ready(): _apply_state()

func _apply_state() -> void:
	status.text = pending_status
	preview_surface.visible = has_valid_volume
	sphere.visible = has_valid_volume
	if not has_valid_volume:
		return
	var radius := pow(pending_volume / UNIT_SPHERE_VOLUME, 1.0 / 3.0)
	sphere.scale = Vector3.ONE * radius
