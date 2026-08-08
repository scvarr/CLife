extends Node3D

const INPUT_DIRECTION := 0
const OUTPUT_DIRECTION := 1
const GEOMETRY_VOLUME_CHANNEL := "geometry.volume"

var editor: CLifeWorldEditor
var object_views: Dictionary = {}
var selected_kind := ""
var selected_identity := -1
var runtime_object_selected := false

var world_tree: Tree
var inspector: VBoxContainer
var mode_label: Label
var tick_label: Label
var play_button: Button
var pause_button: Button
var step_button: Button
var reset_button: Button
var run_button: Button
var stop_button: Button
var new_name: LineEdit
var add_value_button: Button
var add_template_button: Button
var add_rule_button: Button
var host_inputs_box: HBoxContainer
var status_label: Label
var runtime_value_labels: Dictionary = {}


func _ready() -> void:
	editor = CLifeWorldEditor.new()
	_build_editor_ui()
	_rebuild_world_tree()
	_refresh_mode()
	_show_welcome_inspector()


func _process(delta: float) -> void:
	editor.advance_time(delta)
	_apply_runtime_to_views()
	tick_label.text = "Tick: %d" % editor.get_tick()
	if runtime_object_selected:
		_refresh_runtime_values()
	_show_facade_error_if_any()


func _input(event: InputEvent) -> void:
	if not editor.is_run_active() or not event is InputEventMouseButton:
		return
	var mouse_event := event as InputEventMouseButton
	if mouse_event.button_index != MOUSE_BUTTON_LEFT or not mouse_event.pressed:
		return
	var camera := $Camera3D as Camera3D
	var ray_origin := camera.project_ray_origin(mouse_event.position)
	var ray_end := ray_origin + camera.project_ray_normal(mouse_event.position) * 1000.0
	var query := PhysicsRayQueryParameters3D.create(ray_origin, ray_end)
	query.collide_with_areas = true
	query.collide_with_bodies = false
	var hit := get_world_3d().direct_space_state.intersect_ray(query)
	if not hit.is_empty() and hit.get("collider") == $Cell/SelectionArea:
		runtime_object_selected = true
		_show_runtime_inspector()
		get_viewport().set_input_as_handled()


func _build_editor_ui() -> void:
	var root := $UI/Root as Control
	var margin := MarginContainer.new()
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	margin.add_theme_constant_override("margin_left", 12)
	margin.add_theme_constant_override("margin_right", 12)
	margin.add_theme_constant_override("margin_top", 10)
	margin.add_theme_constant_override("margin_bottom", 10)
	root.add_child(margin)

	var page := VBoxContainer.new()
	page.add_theme_constant_override("separation", 8)
	margin.add_child(page)
	page.add_child(_build_header())
	page.add_child(_build_workspace())
	page.add_child(_build_status_area())


func _build_header() -> Control:
	var header := HBoxContainer.new()
	header.custom_minimum_size.y = 42.0
	var title := Label.new()
	title.text = "CLife World Editor"
	title.add_theme_font_size_override("font_size", 24)
	header.add_child(title)
	mode_label = Label.new()
	mode_label.add_theme_font_size_override("font_size", 18)
	mode_label.custom_minimum_size.x = 120.0
	header.add_child(mode_label)
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(spacer)
	run_button = _button("Run", _on_run)
	stop_button = _button("Stop", _on_stop)
	header.add_child(run_button)
	header.add_child(stop_button)
	return header


func _build_workspace() -> Control:
	var split := HSplitContainer.new()
	split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	split.split_offset = 240
	split.add_child(_build_world_panel())
	var detail_split := HSplitContainer.new()
	detail_split.split_offset = 470
	detail_split.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	split.add_child(detail_split)

	var viewport_slot := Control.new()
	viewport_slot.custom_minimum_size.x = 300.0
	viewport_slot.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	viewport_slot.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var viewport_caption := Label.new()
	viewport_caption.text = "VIEWPORT — click the runtime sphere to inspect"
	viewport_caption.position = Vector2(16, 12)
	viewport_caption.mouse_filter = Control.MOUSE_FILTER_IGNORE
	viewport_slot.add_child(viewport_caption)
	detail_split.add_child(viewport_slot)
	detail_split.add_child(_build_inspector_panel())
	return split


func _build_world_panel() -> Control:
	var panel := PanelContainer.new()
	panel.custom_minimum_size.x = 230.0
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 10)
	margin.add_theme_constant_override("margin_right", 10)
	margin.add_theme_constant_override("margin_top", 8)
	margin.add_theme_constant_override("margin_bottom", 8)
	panel.add_child(margin)
	var column := VBoxContainer.new()
	margin.add_child(column)
	var heading := Label.new()
	heading.text = "WORLD"
	heading.add_theme_font_size_override("font_size", 18)
	column.add_child(heading)
	world_tree = Tree.new()
	world_tree.hide_root = true
	world_tree.size_flags_vertical = Control.SIZE_EXPAND_FILL
	world_tree.item_selected.connect(_on_world_item_selected)
	column.add_child(world_tree)
	new_name = LineEdit.new()
	new_name.placeholder_text = "New value or template name"
	column.add_child(new_name)
	var buttons := HBoxContainer.new()
	add_value_button = _button("+ Value", _on_add_value)
	add_template_button = _button("+ Template", _on_add_template)
	add_rule_button = _button("+ Rule", _on_add_rule)
	buttons.add_child(add_value_button)
	buttons.add_child(add_template_button)
	buttons.add_child(add_rule_button)
	column.add_child(buttons)
	return panel


func _build_inspector_panel() -> Control:
	var panel := PanelContainer.new()
	panel.custom_minimum_size.x = 300.0
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 10)
	margin.add_theme_constant_override("margin_right", 10)
	margin.add_theme_constant_override("margin_top", 8)
	margin.add_theme_constant_override("margin_bottom", 8)
	panel.add_child(margin)
	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	margin.add_child(scroll)
	inspector = VBoxContainer.new()
	inspector.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(inspector)
	return panel


func _build_status_area() -> Control:
	var column := VBoxContainer.new()
	status_label = Label.new()
	status_label.text = "Ready"
	status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	column.add_child(status_label)
	var controls := HBoxContainer.new()
	controls.add_theme_constant_override("separation", 8)
	play_button = _button("Play", _on_play)
	pause_button = _button("Pause", _on_pause)
	step_button = _button("Step", _on_step)
	reset_button = _button("Reset", _on_reset)
	controls.add_child(play_button)
	controls.add_child(pause_button)
	controls.add_child(step_button)
	controls.add_child(reset_button)
	tick_label = Label.new()
	tick_label.custom_minimum_size.x = 90.0
	controls.add_child(tick_label)
	var separator := VSeparator.new()
	controls.add_child(separator)
	var inputs_caption := Label.new()
	inputs_caption.text = "Host Inputs"
	controls.add_child(inputs_caption)
	host_inputs_box = HBoxContainer.new()
	host_inputs_box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	controls.add_child(host_inputs_box)
	column.add_child(controls)
	return column


func _rebuild_world_tree() -> void:
	world_tree.clear()
	var root := world_tree.create_item()
	var values_root := world_tree.create_item(root)
	values_root.set_text(0, "Values")
	values_root.set_metadata(0, {"kind": "section"})
	for value in editor.get_values():
		var item := world_tree.create_item(values_root)
		item.set_text(0, "%s  [#%d]" % [value.name, value.key])
		item.set_metadata(0, {"kind": "value", "id": int(value.key)})
	values_root.collapsed = false

	var templates_root := world_tree.create_item(root)
	templates_root.set_text(0, "Templates")
	templates_root.set_metadata(0, {"kind": "section"})
	for object_template in editor.get_templates():
		var item := world_tree.create_item(templates_root)
		item.set_text(0, "%s  [#%d]" % [object_template.name, object_template.id])
		item.set_metadata(0, {"kind": "template", "id": int(object_template.id)})
	templates_root.collapsed = false

	var rules_root := world_tree.create_item(root)
	rules_root.set_text(0, "World Rules")
	rules_root.set_metadata(0, {"kind": "section"})
	for rule in editor.get_world_rules():
		var item := world_tree.create_item(rules_root)
		item.set_text(0, "%s → %s  × %.3f" % [
			_value_name(int(rule.source_key)),
			_value_name(int(rule.target_key)),
			float(rule.target_per_source),
		])
		item.set_metadata(0, {"kind": "rule", "id": int(rule.index)})
	rules_root.collapsed = false


func _on_world_item_selected() -> void:
	if editor.is_run_active():
		return
	var item := world_tree.get_selected()
	if item == null:
		return
	var metadata: Dictionary = item.get_metadata(0)
	selected_kind = str(metadata.get("kind", ""))
	selected_identity = int(metadata.get("id", -1))
	match selected_kind:
		"value":
			_show_value_inspector(selected_identity)
		"template":
			if editor.select_template(selected_identity):
				_show_template_inspector(selected_identity)
			else:
				_show_facade_error_if_any()
		"rule":
			_show_rule_inspector(selected_identity, false)
		_:
			_show_welcome_inspector()


func _show_welcome_inspector() -> void:
	_clear_children(inspector)
	_add_heading(inspector, "INSPECTOR")
	_add_wrapped_label(inspector, "Select a value, template, or world rule. The first-world preset is the editable startup document.")


func _show_value_inspector(key: int) -> void:
	var value := _find_by(editor.get_values(), "key", key)
	if value.is_empty():
		_show_welcome_inspector()
		return
	_clear_children(inspector)
	_add_heading(inspector, "Value")
	_add_wrapped_label(inspector, "Stable ValueKey: %d" % key)
	var name_edit := LineEdit.new()
	name_edit.text = str(value.name)
	inspector.add_child(name_edit)
	inspector.add_child(_button("Rename", func() -> void:
		_finish_edit(editor.rename_value(key, name_edit.text), "Value renamed")
	))
	var delete_button := _button("Delete Value", func() -> void:
		_finish_edit(editor.remove_value(key), "Value deleted")
	)
	inspector.add_child(delete_button)


func _show_template_inspector(template_id: int) -> void:
	var object_template := _find_by(editor.get_templates(), "id", template_id)
	if object_template.is_empty():
		_show_welcome_inspector()
		return
	_clear_children(inspector)
	_add_heading(inspector, "Template")
	_add_wrapped_label(inspector, "Stable TemplateId: %d" % template_id)
	var name_edit := LineEdit.new()
	name_edit.text = str(object_template.name)
	inspector.add_child(name_edit)
	inspector.add_child(_button("Rename Template", func() -> void:
		_finish_edit(editor.rename_template(template_id, name_edit.text), "Template renamed")
	))
	inspector.add_child(_button("Delete Template", func() -> void:
		_finish_edit(editor.remove_template(template_id), "Template deleted")
	))
	_add_separator(inspector)
	_build_initial_values_editor(template_id)
	_add_separator(inspector)
	_build_genome_editor(template_id)
	_add_separator(inspector)
	_build_bindings_editor(template_id)


func _build_initial_values_editor(template_id: int) -> void:
	_add_heading(inspector, "Initial Values")
	for initial in editor.get_initial_values(template_id):
		var key := int(initial.value_key)
		var row := HBoxContainer.new()
		var label := Label.new()
		label.text = "%s [#%d]" % [_value_name(key), key]
		label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		row.add_child(label)
		var amount := _amount_spin(float(initial.amount))
		row.add_child(amount)
		row.add_child(_button("Set", func() -> void:
			_finish_edit(editor.set_initial_value(template_id, key, amount.value), "Initial value updated")
		))
		row.add_child(_button("×", func() -> void:
			_finish_edit(editor.remove_initial_value(template_id, key), "Initial value removed")
		))
		inspector.add_child(row)
	var add_row := HBoxContainer.new()
	var value_option := _value_option()
	value_option.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var amount := _amount_spin(0.0)
	add_row.add_child(value_option)
	add_row.add_child(amount)
	add_row.add_child(_button("Set / Add", func() -> void:
		_finish_edit(editor.set_initial_value(template_id, _selected_option_id(value_option), amount.value), "Initial value set")
	))
	inspector.add_child(add_row)


func _build_genome_editor(template_id: int) -> void:
	_add_heading(inspector, "Genome")
	for function in editor.get_genome(template_id):
		var index := int(function.index)
		var input_option := _value_option(int(function.input_key))
		var output_option := _value_option(int(function.output_key))
		var throughput := _positive_spin(float(function.throughput))
		var result_factor := _amount_spin(float(function.result_per_input))
		_add_wrapped_label(inspector, "Function #%d" % index)
		inspector.add_child(_labeled_control("Input", input_option))
		inspector.add_child(_labeled_control("Output", output_option))
		inspector.add_child(_labeled_control("Throughput", throughput))
		inspector.add_child(_labeled_control("Result / input", result_factor))
		var actions := HBoxContainer.new()
		actions.add_child(_button("Update", func() -> void:
			_finish_edit(editor.change_genome_function(template_id, index, _selected_option_id(input_option),
				_selected_option_id(output_option), throughput.value, result_factor.value), "Genome function updated")
		))
		actions.add_child(_button("Remove", func() -> void:
			_finish_edit(editor.remove_genome_function(template_id, index), "Genome function removed")
		))
		inspector.add_child(actions)
	var new_input := _value_option()
	var new_output := _value_option()
	var new_throughput := _positive_spin(1.0)
	var new_result := _amount_spin(1.0)
	_add_wrapped_label(inspector, "New function")
	inspector.add_child(_labeled_control("Input", new_input))
	inspector.add_child(_labeled_control("Output", new_output))
	inspector.add_child(_labeled_control("Throughput", new_throughput))
	inspector.add_child(_labeled_control("Result / input", new_result))
	inspector.add_child(_button("Add Function", func() -> void:
		_finish_edit(editor.add_genome_function(template_id, _selected_option_id(new_input),
			_selected_option_id(new_output), new_throughput.value, new_result.value), "Genome function added")
	))


func _build_bindings_editor(template_id: int) -> void:
	_add_heading(inspector, "Host Bindings")
	for binding in editor.get_bindings(template_id):
		var index := int(binding.index)
		var binding_direction := int(binding.direction_id)
		var binding_channel := str(binding.channel)
		var capability := _find_capability(binding_channel, binding_direction)
		var value_option := _value_option(int(binding.value_key))
		if capability.is_empty():
			_add_wrapped_label(inspector, "Binding #%d — unsupported/legacy\n%s (%s)" % [
				index, binding_channel, binding.direction,
			])
			inspector.add_child(_labeled_control("Value", value_option))
			inspector.add_child(_button("Remove Legacy Binding", func() -> void:
				_finish_edit(editor.remove_host_binding(template_id, index), "Legacy binding removed")
			))
			continue
		var direction := _direction_option(binding_direction)
		var channel := _capability_option(binding_direction, binding_channel)
		direction.item_selected.connect(func(_selected_index: int) -> void:
			_populate_capability_option(channel, _selected_option_id(direction), "")
		)
		_add_wrapped_label(inspector, "Binding #%d" % index)
		inspector.add_child(_labeled_control("Direction", direction))
		inspector.add_child(_labeled_control("Capability", channel))
		inspector.add_child(_labeled_control("Value", value_option))
		var actions := HBoxContainer.new()
		actions.add_child(_button("Update", func() -> void:
			_finish_edit(editor.change_host_binding(template_id, index, _selected_capability_channel(channel),
				_selected_option_id(direction), _selected_option_id(value_option)), "Binding updated")
		))
		actions.add_child(_button("Remove", func() -> void:
			_finish_edit(editor.remove_host_binding(template_id, index), "Binding removed")
		))
		inspector.add_child(actions)
	var new_direction := _direction_option(INPUT_DIRECTION)
	var new_channel := _capability_option(INPUT_DIRECTION)
	new_direction.item_selected.connect(func(_selected_index: int) -> void:
		_populate_capability_option(new_channel, _selected_option_id(new_direction), "")
	)
	var new_value := _value_option()
	_add_wrapped_label(inspector, "New binding")
	inspector.add_child(_labeled_control("Direction", new_direction))
	inspector.add_child(_labeled_control("Capability", new_channel))
	inspector.add_child(_labeled_control("Value", new_value))
	inspector.add_child(_button("Add Binding", func() -> void:
		_finish_edit(editor.add_host_binding(template_id, _selected_capability_channel(new_channel),
			_selected_option_id(new_direction),
			_selected_option_id(new_value)), "Binding added")
	))


func _show_rule_inspector(index: int, is_new: bool) -> void:
	var rules := editor.get_world_rules()
	var rule: Dictionary = {} if is_new else _find_by(rules, "index", index)
	if not is_new and rule.is_empty():
		_show_welcome_inspector()
		return
	_clear_children(inspector)
	_add_heading(inspector, "New World Rule" if is_new else "World Rule #%d" % index)
	var source := _value_option(0 if is_new else int(rule.source_key))
	var target := _value_option(0 if is_new else int(rule.target_key))
	if is_new and target.item_count > 1:
		target.select(1)
	var factor := _amount_spin(1.0 if is_new else float(rule.target_per_source))
	inspector.add_child(_labeled_control("Source", source))
	inspector.add_child(_labeled_control("Target", target))
	inspector.add_child(_labeled_control("Target / source", factor))
	if is_new:
		inspector.add_child(_button("Add Rule", func() -> void:
			_finish_edit(editor.add_world_rule(_selected_option_id(source), _selected_option_id(target), factor.value),
				"World rule added")
		))
	else:
		inspector.add_child(_button("Update Rule", func() -> void:
			_finish_edit(editor.change_world_rule(index, _selected_option_id(source), _selected_option_id(target),
				factor.value), "World rule updated")
		))
		inspector.add_child(_button("Remove Rule", func() -> void:
			_finish_edit(editor.remove_world_rule(index), "World rule removed")
		))


func _show_runtime_inspector() -> void:
	_clear_children(inspector)
	runtime_value_labels.clear()
	_add_heading(inspector, "Runtime Object #%d" % editor.get_preview_object_id())
	_add_wrapped_label(inspector, "Live values from RuntimeWorld, addressed by stable ValueKey.")
	for value in editor.get_runtime_values():
		var key := int(value.key)
		var label := Label.new()
		label.text = "%s [#%d]: %.6f" % [value.name, key, float(value.amount)]
		inspector.add_child(label)
		runtime_value_labels[key] = label


func _refresh_runtime_values() -> void:
	for value in editor.get_runtime_values():
		var key := int(value.key)
		if runtime_value_labels.has(key):
			var label := runtime_value_labels[key] as Label
			label.text = "%s [#%d]: %.6f" % [value.name, key, float(value.amount)]


func _rebuild_host_inputs() -> void:
	_clear_children(host_inputs_box)
	for input in editor.get_host_inputs():
		var channel := str(input.channel)
		var label := Label.new()
		label.text = channel
		host_inputs_box.add_child(label)
		var amount := _amount_spin(float(input.amount))
		amount.custom_minimum_size.x = 100.0
		amount.value_changed.connect(func(value: float) -> void:
			if not editor.set_host_input(channel, value):
				_show_facade_error_if_any()
		)
		host_inputs_box.add_child(amount)


func _on_add_value() -> void:
	var key := editor.add_value(new_name.text)
	if key == 0:
		_show_facade_error_if_any()
		return
	new_name.clear()
	selected_kind = "value"
	selected_identity = key
	status_label.text = "Value added with stable key #%d" % key
	_rebuild_world_tree()
	_show_value_inspector(key)


func _on_add_template() -> void:
	var template_id := editor.add_template(new_name.text)
	if template_id == 0:
		_show_facade_error_if_any()
		return
	new_name.clear()
	selected_kind = "template"
	selected_identity = template_id
	status_label.text = "Template added with stable id #%d" % template_id
	_rebuild_world_tree()
	_show_template_inspector(template_id)


func _on_add_rule() -> void:
	if editor.get_values().size() < 2:
		status_label.text = "Add at least two values before creating a world rule"
		return
	_show_rule_inspector(-1, true)


func _on_run() -> void:
	if not editor.run():
		_show_facade_error_if_any()
		return
	object_views.clear()
	$Cell.scale = Vector3.ONE
	object_views[editor.get_preview_object_id()] = $Cell
	runtime_object_selected = false
	status_label.text = "Compiled current WorldDefinition and entered RUN"
	_rebuild_host_inputs()
	_refresh_mode()
	_clear_children(inspector)
	_add_heading(inspector, "RUN Inspector")
	_add_wrapped_label(inspector, "Click the sphere to inspect its live values.")


func _on_stop() -> void:
	editor.stop()
	object_views.clear()
	runtime_object_selected = false
	$Cell.scale = Vector3.ONE
	status_label.text = "Runtime destroyed; editable document preserved"
	_rebuild_host_inputs()
	_refresh_mode()
	_restore_edit_inspector()


func _on_play() -> void:
	if editor.play():
		status_label.text = "Automatic 10 Hz simulation running"
	else:
		_show_facade_error_if_any()
	_refresh_mode()


func _on_pause() -> void:
	editor.pause()
	status_label.text = "Runtime paused"
	_refresh_mode()


func _on_step() -> void:
	if editor.step_once():
		status_label.text = "Advanced exactly one CLife tick"
	else:
		_show_facade_error_if_any()


func _on_reset() -> void:
	if not editor.reset_runtime():
		_show_facade_error_if_any()
		return
	object_views.clear()
	object_views[editor.get_preview_object_id()] = $Cell
	runtime_object_selected = true
	status_label.text = "Runtime reconstructed from the Run snapshot"
	_show_runtime_inspector()
	_refresh_mode()


func _finish_edit(success: bool, message: String) -> void:
	if not success:
		_show_facade_error_if_any()
		return
	status_label.text = message
	_rebuild_world_tree()
	_restore_edit_inspector()


func _restore_edit_inspector() -> void:
	match selected_kind:
		"value":
			_show_value_inspector(selected_identity)
		"template":
			_show_template_inspector(selected_identity)
		"rule":
			_show_rule_inspector(selected_identity, false)
		_:
			_show_welcome_inspector()


func _refresh_mode() -> void:
	var running := editor.is_run_active()
	mode_label.text = "RUN" if running else "EDIT"
	run_button.disabled = running
	stop_button.disabled = not running
	play_button.disabled = not running or editor.is_playing()
	pause_button.disabled = not running or not editor.is_playing()
	step_button.disabled = not running
	reset_button.disabled = not running
	new_name.editable = not running
	add_value_button.disabled = running
	add_template_button.disabled = running
	add_rule_button.disabled = running
	world_tree.mouse_filter = Control.MOUSE_FILTER_IGNORE if running else Control.MOUSE_FILTER_STOP
	world_tree.modulate = Color(0.65, 0.65, 0.65, 1.0) if running else Color.WHITE
	tick_label.text = "Tick: %d" % editor.get_tick()


func _apply_runtime_to_views() -> void:
	if not editor.is_run_active():
		return
	for output in editor.get_host_outputs():
		if str(output.channel) != GEOMETRY_VOLUME_CHANNEL:
			continue
		var object_id := int(output.object_id)
		if not object_views.has(object_id):
			continue
		var view := object_views[object_id] as Node3D
		if is_instance_valid(view):
			var volume := maxf(float(output.amount), 0.0)
			var linear_scale := pow(volume, 1.0 / 3.0)
			view.scale = Vector3.ONE * linear_scale


func _show_facade_error_if_any() -> void:
	var message := editor.get_last_error()
	if not message.is_empty():
		status_label.text = "Error: " + message


func _value_name(key: int) -> String:
	var value := _find_by(editor.get_values(), "key", key)
	return "Unknown" if value.is_empty() else str(value.name)


func _find_by(items: Array, field: String, identity: int) -> Dictionary:
	for item in items:
		if int(item.get(field, -1)) == identity:
			return item
	return {}


func _value_option(selected_key: int = 0) -> OptionButton:
	var option := OptionButton.new()
	for value in editor.get_values():
		option.add_item("%s [#%d]" % [value.name, value.key])
		option.set_item_metadata(option.item_count - 1, int(value.key))
		if int(value.key) == selected_key:
			option.select(option.item_count - 1)
	return option


func _direction_option(selected_direction: int) -> OptionButton:
	var option := OptionButton.new()
	option.add_item("Input", INPUT_DIRECTION)
	option.set_item_metadata(0, INPUT_DIRECTION)
	option.add_item("Output", OUTPUT_DIRECTION)
	option.set_item_metadata(1, OUTPUT_DIRECTION)
	option.select(0 if selected_direction == INPUT_DIRECTION else 1)
	return option


func _capability_option(direction_id: int, selected_channel: String = "") -> OptionButton:
	var option := OptionButton.new()
	_populate_capability_option(option, direction_id, selected_channel)
	return option


func _populate_capability_option(option: OptionButton, direction_id: int, selected_channel: String) -> void:
	option.clear()
	for capability in editor.get_host_capabilities():
		if int(capability.direction_id) != direction_id:
			continue
		option.add_item("%s — %s" % [capability.display_name, capability.channel])
		option.set_item_metadata(option.item_count - 1, str(capability.channel))
		if str(capability.channel) == selected_channel:
			option.select(option.item_count - 1)


func _selected_capability_channel(option: OptionButton) -> String:
	if option.selected < 0:
		return ""
	return str(option.get_item_metadata(option.selected))


func _find_capability(channel: String, direction_id: int) -> Dictionary:
	for capability in editor.get_host_capabilities():
		if str(capability.channel) == channel and int(capability.direction_id) == direction_id:
			return capability
	return {}


func _selected_option_id(option: OptionButton) -> int:
	if option.selected < 0:
		return 0
	return int(option.get_item_metadata(option.selected))


func _amount_spin(value: float) -> SpinBox:
	var spin := SpinBox.new()
	spin.min_value = -1000000.0
	spin.max_value = 1000000.0
	spin.step = 0.01
	spin.allow_greater = true
	spin.allow_lesser = true
	spin.value = value
	return spin


func _positive_spin(value: float) -> SpinBox:
	var spin := _amount_spin(value)
	spin.min_value = 0.001
	spin.allow_lesser = false
	return spin


func _labeled_control(caption: String, control: Control) -> Control:
	var row := HBoxContainer.new()
	var label := Label.new()
	label.text = caption
	label.custom_minimum_size.x = 115.0
	row.add_child(label)
	control.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(control)
	return row


func _button(text: String, callback: Callable) -> Button:
	var button := Button.new()
	button.text = text
	button.pressed.connect(callback)
	return button


func _add_heading(parent: VBoxContainer, text: String) -> void:
	var label := Label.new()
	label.text = text
	label.add_theme_font_size_override("font_size", 18)
	parent.add_child(label)


func _add_wrapped_label(parent: VBoxContainer, text: String) -> void:
	var label := Label.new()
	label.text = text
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	parent.add_child(label)


func _add_separator(parent: VBoxContainer) -> void:
	parent.add_child(HSeparator.new())


func _clear_children(parent: Node) -> void:
	for child in parent.get_children():
		parent.remove_child(child)
		child.queue_free()
