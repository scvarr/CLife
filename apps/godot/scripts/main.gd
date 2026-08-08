extends Node3D

const VISUAL_SCALE_BASE := 1.0

var runtime: CLifeDemoRuntime
var object_views: Dictionary = {}
var tick_label: Label
var light_value_label: Label
var energy_label: Label
var used_energy_label: Label
var temperature_label: Label
var status_label: Label
var light_slider: HSlider


func _ready() -> void:
	runtime = CLifeDemoRuntime.new()
	object_views[runtime.get_cell_object_id()] = $Cell
	_build_ui()
	_refresh_view()


func _process(delta: float) -> void:
	runtime.advance_time(delta)
	_refresh_view()


func _build_ui() -> void:
	var root := $UI/Root as Control

	var title := Label.new()
	title.text = "CLife / simulation controls"
	title.add_theme_font_size_override("font_size", 24)
	title.position = Vector2(20, 12)
	root.add_child(title)

	var left := VBoxContainer.new()
	left.position = Vector2(20, 58)
	left.size = Vector2(315, 535)
	root.add_child(left)
	_add_section(left, "World", "First cellular preset\nCell ObjectId -> Godot Node3D")
	_add_section(left, "Values", runtime.get_values_summary())
	_add_section(left, "Genome", runtime.get_genome_summary())
	_add_section(left, "World Rules", runtime.get_world_rule_summary())
	_add_section(left, "Bindings", runtime.get_binding_summary())

	var inspector := VBoxContainer.new()
	inspector.set_anchors_preset(Control.PRESET_TOP_RIGHT)
	inspector.position = Vector2(-315, 58)
	inspector.size = Vector2(295, 360)
	root.add_child(inspector)
	_add_heading(inspector, "Inspector")
	light_value_label = _add_value(inspector, "Light")
	energy_label = _add_value(inspector, "Energy")
	used_energy_label = _add_value(inspector, "UsedEnergy")
	temperature_label = _add_value(inspector, "Temperature")
	_add_section(inspector, "Visualization", "scale = 1.0 + Temperature")

	var controls := HBoxContainer.new()
	controls.anchor_right = 1.0
	controls.anchor_top = 1.0
	controls.anchor_bottom = 1.0
	controls.offset_left = 20.0
	controls.offset_top = -96.0
	controls.offset_right = -20.0
	controls.offset_bottom = -24.0
	controls.add_theme_constant_override("separation", 10)
	root.add_child(controls)
	controls.add_child(_button("Play", _on_play))
	controls.add_child(_button("Pause", _on_pause))
	controls.add_child(_button("Step", _on_step))
	controls.add_child(_button("Reset", _on_reset))
	tick_label = Label.new()
	tick_label.custom_minimum_size = Vector2(100, 0)
	controls.add_child(tick_label)
	var light_caption := Label.new()
	light_caption.text = "Light"
	controls.add_child(light_caption)
	light_slider = HSlider.new()
	light_slider.min_value = 0.0
	light_slider.max_value = 2.0
	light_slider.step = 0.05
	light_slider.value = runtime.get_light()
	light_slider.custom_minimum_size = Vector2(250, 0)
	light_slider.value_changed.connect(_on_light_changed)
	controls.add_child(light_slider)
	status_label = Label.new()
	status_label.custom_minimum_size = Vector2(120, 0)
	controls.add_child(status_label)


func _add_heading(parent: VBoxContainer, text: String) -> void:
	var label := Label.new()
	label.text = text
	label.add_theme_font_size_override("font_size", 19)
	parent.add_child(label)


func _add_section(parent: VBoxContainer, heading: String, body: String) -> void:
	_add_heading(parent, heading)
	var label := Label.new()
	label.text = body
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	parent.add_child(label)


func _add_value(parent: VBoxContainer, name: String) -> Label:
	var label := Label.new()
	label.text = name + ": 0"
	parent.add_child(label)
	return label


func _button(text: String, callback: Callable) -> Button:
	var button := Button.new()
	button.text = text
	button.pressed.connect(callback)
	return button


func _on_play() -> void:
	runtime.set_running(true)


func _on_pause() -> void:
	runtime.set_running(false)


func _on_step() -> void:
	runtime.step_once()


func _on_reset() -> void:
	runtime.reset()
	object_views.clear()
	object_views[runtime.get_cell_object_id()] = $Cell
	light_slider.value = runtime.get_light()
	_refresh_view()


func _on_light_changed(value: float) -> void:
	runtime.set_light(value)


func _refresh_view() -> void:
	if runtime == null:
		return
	var temperature := runtime.get_temperature()
	var visual_scale := VISUAL_SCALE_BASE + temperature
	for object_id in object_views:
		var view := object_views[object_id] as Node3D
		if is_instance_valid(view):
			view.scale = Vector3.ONE * visual_scale
	tick_label.text = "Tick: %d" % runtime.get_tick()
	light_value_label.text = "Light: %.3f" % runtime.get_light()
	energy_label.text = "Energy: %.3f" % runtime.get_energy()
	used_energy_label.text = "UsedEnergy: %.3f" % runtime.get_used_energy()
	temperature_label.text = "Temperature: %.3f" % temperature
	status_label.text = "Running" if runtime.is_running() else "Paused"
