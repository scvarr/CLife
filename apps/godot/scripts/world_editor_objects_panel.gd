extends WorldEditorPanel

const ConstructionVolumePreview = preload("res://scenes/construction_volume_preview.tscn")

var selected_template_id := 0
var preview_volume_characteristic_ids_by_template: Dictionary = {}
var new_object_active := false
var new_genome_function_active := false
var draft_genome_function_type_id := 0
var editing_genome_index := -1
var last_test_inputs: Array[Dictionary] = []
var last_runtime_values: Array[Dictionary] = []
var last_runtime_functions: Array[Dictionary] = []
var last_end_buffer: Array[Dictionary] = []
var external_inputs: Array[Dictionary]
var autorun_timer: Timer
var autorun_active := false
var runtime_ticks_per_second := 10
var last_runtime_refresh_msec := 0

func configure(shell_instance: Control, editor_instance: CLifeWorldEditor, workspace_instance: VBoxContainer, status_instance: Label, host_config_instance: RefCounted) -> void:
	super.configure(shell_instance, editor_instance, workspace_instance, status_instance, host_config_instance)
	external_inputs = host_config.external_inputs
	if autorun_timer == null:
		autorun_timer = Timer.new()
		autorun_timer.one_shot = false
		shell.add_child(autorun_timer)
		autorun_timer.timeout.connect(_on_autorun_timer_timeout)

func deactivate() -> void:
	_discard_runtime_preview()

func show() -> void:
	_show_objects()

func _show_objects() -> void:
	_clear_workspace()
	var split := HBoxContainer.new(); split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var list_panel := VBoxContainer.new(); list_panel.custom_minimum_size.x = 280
	var title := Label.new(); title.text = tr("ux.objects"); title.add_theme_font_size_override("font_size", 22); list_panel.add_child(title)
	var templates := editor.get_templates()
	if selected_template_id != 0 and _find_template(selected_template_id).is_empty(): selected_template_id = 0
	if selected_template_id == 0 and not templates.is_empty(): selected_template_id = int(templates[0].id)
	for template in templates:
		_add_object_list_item(list_panel, template)
	if new_object_active: _add_new_object_row(list_panel)
	var add := Button.new(); add.text = "+ " + tr("ux.add_new"); add.disabled = new_object_active
	add.pressed.connect(func(): new_object_active = true; _show_objects())
	list_panel.add_child(add)
	var editor_scroll := ScrollContainer.new(); editor_scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL; editor_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var editor_panel := VBoxContainer.new(); editor_panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL; editor_scroll.add_child(editor_panel)
	if selected_template_id == 0:
		var hint := Label.new(); hint.text = tr("ux.create_first_object"); editor_panel.add_child(hint)
	else:
		_build_object_editor(editor_panel, _find_template(selected_template_id))
	split.add_child(list_panel); split.add_child(editor_scroll); workspace.add_child(split)

func _add_object_list_item(parent: VBoxContainer, template: Dictionary) -> void:
	var button := Button.new(); button.text = str(template.name); button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.button_pressed = int(template.id) == selected_template_id
	button.pressed.connect(func(): selected_template_id = int(template.id); _show_objects())
	button.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed and (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT:
			_show_delete_menu({"kind": "template", "template": template}, button.get_global_position() + (event as InputEventMouseButton).position)
	)
	parent.add_child(button)

func _add_new_object_row(parent: VBoxContainer) -> void:
	var name := LineEdit.new(); name.placeholder_text = tr("ux.object_name"); parent.add_child(name)
	var row := HBoxContainer.new(); var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		var id := editor.add_template(name.text)
		if id == 0: _show_error(); return
		selected_template_id = id; new_object_active = false; _show_objects()
	)
	cancel.pressed.connect(func(): new_object_active = false; _show_objects())
	row.add_child(save); row.add_child(cancel); parent.add_child(row)

func _build_object_editor(parent: VBoxContainer, template: Dictionary) -> void:
	_add_title_to(parent, tr("ux.object"))
	var name_row := HBoxContainer.new(); var name := LineEdit.new(); name.text = str(template.name); name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save_name := Button.new(); save_name.text = tr("ux.save")
	save_name.pressed.connect(func():
		if not editor.rename_template(int(template.id), name.text): _show_error(); return
		_show_objects()
	)
	name_row.add_child(name); name_row.add_child(save_name); parent.add_child(_labeled_row(tr("ux.name"), name_row))
	_build_initial_values(parent, template)
	_add_section(parent, tr("ux.genome"))
	for entry in editor.get_genome(int(template.id)):
		_add_genome_entry_card(parent, template, entry)
	if new_genome_function_active:
		_add_new_genome_function_row(parent, template)
	var add := Button.new(); add.text = "+ " + tr("ux.add_function"); add.disabled = new_genome_function_active
	add.pressed.connect(func(): new_genome_function_active = true; draft_genome_function_type_id = 0; _show_objects())
	parent.add_child(add)
	_add_object_construction_preview(parent, template)
	_add_stateful_runtime_preview(parent, template)

func _build_initial_values(parent: VBoxContainer, template: Dictionary) -> void:
	_add_section(parent, tr("ux.initial_values"))
	for initial in editor.get_initial_values(int(template.id)):
		var row := HBoxContainer.new()
		var value := _value_selector(int(initial.get("value_key", 0))); value.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		var amount := SpinBox.new(); amount.step = 0.1; amount.value = float(initial.get("amount", 0.0))
		var save := Button.new(); save.text = tr("ux.save")
		save.pressed.connect(func():
			if not editor.set_initial_value(int(template.id), _selected_unit_id(value), amount.value): _show_error(); return
			_show_objects()
		)
		var remove := Button.new(); remove.text = tr("ux.delete")
		remove.pressed.connect(func():
			if not editor.remove_initial_value(int(template.id), int(initial.get("value_key", 0))): _show_error(); return
			_show_objects()
		)
		row.add_child(value); row.add_child(amount); row.add_child(save); row.add_child(remove); parent.add_child(row)
	var add_row := HBoxContainer.new()
	var add_value := _value_selector(0); add_value.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var add_amount := SpinBox.new(); add_amount.step = 0.1
	var add := Button.new(); add.text = tr("ux.add")
	add.pressed.connect(func():
		if not editor.set_initial_value(int(template.id), _selected_unit_id(add_value), add_amount.value): _show_error(); return
		_show_objects()
	)
	add_row.add_child(add_value); add_row.add_child(add_amount); add_row.add_child(add); parent.add_child(add_row)

func _add_object_construction_preview(parent: VBoxContainer, template: Dictionary) -> void:
	_add_section(parent, tr("ux.object_construction_preview"))
	var materials := editor.get_material_contributions(int(template.id))
	if materials.is_empty() and not editor.get_last_error().is_empty():
		var unavailable := Label.new(); unavailable.text = editor.get_last_error(); parent.add_child(unavailable); return
	var materials_title := Label.new(); materials_title.text = tr("ux.materials"); parent.add_child(materials_title)
	for material in materials:
		var item := Label.new(); item.text = "%s = %s" % [_value_name(int(material.value_key)), str(material.amount)]; parent.add_child(item)
	var preview := editor.get_template_characteristic_preview(int(template.id))
	if preview.is_empty() and not editor.get_last_error().is_empty():
		var unavailable := Label.new(); unavailable.text = editor.get_last_error(); parent.add_child(unavailable); return
	var characteristics: Array = preview.get("characteristics", [])
	var characteristics_title := Label.new(); characteristics_title.text = tr("ux.object_characteristics"); parent.add_child(characteristics_title)
	for characteristic in characteristics:
		var item := Label.new(); item.text = "%s = %s" % [_characteristic_name(int((characteristic as Dictionary).get("characteristic_id", 0))), str((characteristic as Dictionary).get("amount", 0.0))]; parent.add_child(item)
	_add_construction_volume_preview(parent, int(template.id), characteristics)

func _add_construction_volume_preview(parent: VBoxContainer, template_id: int, characteristics: Array) -> void:
	_add_section(parent, tr("ux.construction_volume_preview"))
	if characteristics.is_empty():
		var empty := Label.new(); empty.text = tr("ux.volume_preview_no_characteristics"); parent.add_child(empty)
		return
	var selected_id := int(preview_volume_characteristic_ids_by_template.get(template_id, 0))
	var selected_characteristic: Dictionary = {}
	for characteristic in characteristics:
		if int((characteristic as Dictionary).get("characteristic_id", 0)) == selected_id:
			selected_characteristic = characteristic
			break
	if characteristics.size() == 1:
		selected_characteristic = characteristics[0]
		selected_id = int(selected_characteristic.get("characteristic_id", 0))
		preview_volume_characteristic_ids_by_template[template_id] = selected_id
	var selector := OptionButton.new(); selector.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	if characteristics.size() > 1:
		selector.add_item(tr("ux.select_characteristic"), 0)
	for characteristic in characteristics:
		var characteristic_id := int((characteristic as Dictionary).get("characteristic_id", 0))
		selector.add_item(_characteristic_name(characteristic_id), characteristic_id)
		if characteristic_id == selected_id:
			selector.select(selector.item_count - 1)
	selector.item_selected.connect(func(index: int):
		var characteristic_id := selector.get_item_id(index)
		if characteristic_id == 0: preview_volume_characteristic_ids_by_template.erase(template_id)
		else: preview_volume_characteristic_ids_by_template[template_id] = characteristic_id
		_show_objects()
	)
	parent.add_child(_labeled_row(tr("ux.volume_for_preview"), selector))
	var volume_preview = ConstructionVolumePreview.instantiate()
	parent.add_child(volume_preview)
	if selected_characteristic.is_empty():
		volume_preview.show_unavailable(tr("ux.volume_preview_select_characteristic"))
		return
	var volume := float(selected_characteristic.get("amount", 0.0))
	if not is_finite(volume) or volume <= 0.0:
		volume_preview.show_unavailable(tr("ux.volume_preview_invalid"))
		return
	var radii := editor.sample_template_shape(template_id, volume_preview.tessellation_directions())
	if radii.is_empty() and not editor.get_last_error().is_empty():
		volume_preview.show_unavailable(editor.get_last_error())
		return
	volume_preview.show_shape(volume, radii, tr("ux.volume_preview_value") % str(volume))

func _semantic_genome_entry(entry: Dictionary) -> String:
	var parts := ["%02X" % int(entry.get("function_type_id", 0))]
	for parameter in entry.get("genome_parameters", []):
		parts.append(str(float((parameter as Dictionary).get("amount", 0.0))))
	return " | ".join(parts)

func _add_genome_entry_card(parent: VBoxContainer, template: Dictionary, entry: Dictionary) -> void:
	var index := int(entry.get("index", -1))
	if editing_genome_index == index:
		_add_genome_entry_editor(parent, template, entry)
		return
	var card := PanelContainer.new(); var label := Label.new(); label.text = _semantic_genome_entry(entry); card.add_child(label)
	card.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed:
			if (event as InputEventMouseButton).button_index == MOUSE_BUTTON_LEFT:
				editing_genome_index = index; _show_objects()
			elif (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT:
				_show_delete_menu({"kind": "genome_entry", "template_id": int(template.id), "entry": entry}, card.get_global_position() + (event as InputEventMouseButton).position)
	)
	parent.add_child(card)

func _add_genome_entry_editor(parent: VBoxContainer, template: Dictionary, entry: Dictionary) -> void:
	var card := PanelContainer.new(); var box := VBoxContainer.new(); card.add_child(box)
	var function_name := Label.new(); function_name.text = str(entry.get("function_type_name", "")); box.add_child(_labeled_row(tr("ux.function"), function_name))
	var fields := []
	for parameter in entry.get("genome_parameters", []):
		var spin := SpinBox.new(); spin.step = 0.1; spin.value = float((parameter as Dictionary).get("amount", 0.0)); spin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		box.add_child(_labeled_row(str((parameter as Dictionary).get("name", "")), spin))
		fields.append({"parameter_id": int((parameter as Dictionary).get("parameter_id", 0)), "spin": spin})
	var row := HBoxContainer.new(); var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		for field in fields:
			if not editor.set_genome_parameter(int(template.id), int(entry.index), int(field.parameter_id), field.spin.value): _show_error(); return
		editing_genome_index = -1; _show_objects()
	)
	cancel.pressed.connect(func(): editing_genome_index = -1; _show_objects())
	row.add_child(save); row.add_child(cancel); box.add_child(row); parent.add_child(card)

func _add_new_genome_function_row(parent: VBoxContainer, template: Dictionary) -> void:
	var functions := editor.get_function_types()
	if functions.is_empty():
		var hint := Label.new(); hint.text = tr("ux.create_first_function"); parent.add_child(hint); return
	if draft_genome_function_type_id == 0: draft_genome_function_type_id = int(functions[0].id)
	var selector := OptionButton.new()
	for function_type in functions:
		selector.add_item(str(function_type.name), int(function_type.id))
		if int(function_type.id) == draft_genome_function_type_id: selector.select(selector.item_count - 1)
	selector.item_selected.connect(func(index: int): draft_genome_function_type_id = selector.get_item_id(index); _show_objects())
	parent.add_child(_labeled_row(tr("ux.function"), selector))
	var function_type := _find_function(draft_genome_function_type_id)
	var fields := []
	for parameter in function_type.get("genome_parameters", []):
		var spin := SpinBox.new(); spin.step = 0.1; spin.value = float((parameter as Dictionary).get("default_value", 0.0)); spin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		parent.add_child(_labeled_row(str((parameter as Dictionary).get("name", "")), spin))
		fields.append({"parameter_id": int((parameter as Dictionary).get("id", 0)), "default_value": float((parameter as Dictionary).get("default_value", 0.0)), "spin": spin})
	var row := HBoxContainer.new(); var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		if not editor.add_genome_function(int(template.id), draft_genome_function_type_id): _show_error(); return
		var genome := editor.get_genome(int(template.id)); var index := genome.size() - 1
		for field in fields:
			if field.spin.value != field.default_value and not editor.set_genome_parameter(int(template.id), index, int(field.parameter_id), field.spin.value):
				editor.remove_genome_function(int(template.id), index); _show_error(); return
		new_genome_function_active = false; draft_genome_function_type_id = 0; _show_objects()
	)
	cancel.pressed.connect(func(): new_genome_function_active = false; draft_genome_function_type_id = 0; _show_objects())
	row.add_child(save); row.add_child(cancel); parent.add_child(row)

func _add_stateful_runtime_preview(parent: VBoxContainer, template: Dictionary) -> void:
	_add_section(parent, tr("ux.runtime_preview"))
	for mapping in external_inputs:
		var input := Label.new(); input.text = "%s -> %s = %s" % [str(mapping.get("channel", "")), _value_name(int(mapping.get("value_key", 0))), str(mapping.get("test_value", 0.0))]; parent.add_child(input)
	var active := editor.is_run_active()
	var tick := Label.new(); tick.text = tr("ux.runtime_tick") % editor.get_tick(); parent.add_child(tick)
	var controls := HBoxContainer.new()
	var start := Button.new(); start.text = tr("ux.runtime_start"); start.disabled = active
	start.pressed.connect(func(): _start_runtime_preview(int(template.id)))
	var run := Button.new(); run.text = tr("ux.runtime_run"); run.disabled = not active or autorun_active
	run.pressed.connect(_run_runtime_preview)
	var pause := Button.new(); pause.text = tr("ux.runtime_pause"); pause.disabled = not autorun_active
	pause.pressed.connect(_pause_runtime_preview)
	var step := Button.new(); step.text = tr("ux.runtime_step"); step.disabled = not active or autorun_active
	step.pressed.connect(_step_runtime_preview)
	var reset := Button.new(); reset.text = tr("ux.runtime_reset"); reset.disabled = not active
	reset.pressed.connect(_reset_runtime_preview)
	var stop := Button.new(); stop.text = tr("ux.runtime_stop"); stop.disabled = not active
	stop.pressed.connect(_stop_runtime_preview)
	var speed := SpinBox.new(); speed.min_value = 1.0; speed.max_value = 100.0; speed.step = 1.0; speed.value = runtime_ticks_per_second
	speed.value_changed.connect(func(value: float): _set_runtime_ticks_per_second(value))
	controls.add_child(start); controls.add_child(run); controls.add_child(pause); controls.add_child(step); controls.add_child(reset); controls.add_child(stop); parent.add_child(controls)
	parent.add_child(_labeled_row(tr("ux.runtime_speed"), speed))
	if not active:
		return
	var inputs_title := Label.new(); inputs_title.text = tr("ux.sent_inputs"); parent.add_child(inputs_title)
	for input in last_test_inputs:
		var input_label := Label.new(); input_label.text = "%s -> %s = %s" % [str(input.channel), _value_name(int(input.value_key)), str(input.amount)]; parent.add_child(input_label)
	var values_title := Label.new(); values_title.text = tr("ux.runtime_values"); parent.add_child(values_title)
	for value in last_runtime_values:
		var value_label := Label.new(); value_label.text = "%s = %s" % [str(value.name), str(value.amount)]; parent.add_child(value_label)
	var end_title := Label.new(); end_title.text = tr("ux.runtime_end_buffer"); parent.add_child(end_title)
	if last_end_buffer.is_empty():
		var end_empty := Label.new(); end_empty.text = tr("ux.runtime_no_end_buffer"); parent.add_child(end_empty)
	else:
		for value in last_end_buffer:
			var end_value_label := Label.new(); end_value_label.text = "%s = %s" % [str(value.name), str(value.amount)]; parent.add_child(end_value_label)
	var buffers_title := Label.new(); buffers_title.text = tr("ux.runtime_buffers"); parent.add_child(buffers_title)
	var has_buffers := false
	for function in last_runtime_functions:
		var buffer: Dictionary = (function as Dictionary).get("buffer", {})
		if buffer.is_empty(): continue
		has_buffers = true
		var function_title := Label.new(); function_title.text = str((function as Dictionary).get("function_type_name", "")); parent.add_child(function_title)
		for detail in [["ux.buffer_stored", "stored_amount"], ["ux.buffer_received", "received_last_tick"], ["ux.buffer_supplied", "supplied_last_tick"]]:
			var item := Label.new(); item.text = "%s = %s" % [tr(str(detail[0])), str(buffer.get(str(detail[1]), 0.0))]; parent.add_child(item)
	if not has_buffers:
		var empty := Label.new(); empty.text = tr("ux.runtime_no_buffers"); parent.add_child(empty)

func _start_runtime_preview(template_id: int) -> void:
	last_test_inputs.clear(); last_runtime_values.clear(); last_runtime_functions.clear(); last_end_buffer.clear()
	if not editor.select_template(template_id): _show_error(); return
	if not editor.run(): _show_error(); return
	_refresh_runtime_preview(); _show_objects()

func _step_runtime_preview() -> void:
	if not editor.is_run_active(): return
	if not _advance_runtime_preview(): return
	_show_objects()

func _advance_runtime_preview() -> bool:
	last_test_inputs.clear()
	for mapping in external_inputs:
		if not editor.set_preview_input(int(mapping.get("value_key", 0)), float(mapping.get("test_value", 0.0))):
			status.text = editor.get_last_error(); return false
		last_test_inputs.append({"channel": str(mapping.get("channel", "")), "value_key": int(mapping.get("value_key", 0)), "amount": float(mapping.get("test_value", 0.0))})
	if not editor.step_once():
		status.text = editor.get_last_error(); return false
	return true

func _run_runtime_preview() -> void:
	if not editor.is_run_active() or autorun_active: return
	autorun_active = true
	last_runtime_refresh_msec = 0
	_update_autorun_timer()
	_show_objects()

func _pause_runtime_preview() -> void:
	if not autorun_active: return
	_stop_autorun_timer()
	_refresh_runtime_preview()
	_show_objects()

func _set_runtime_ticks_per_second(value: float) -> void:
	runtime_ticks_per_second = clampi(int(round(value)), 1, 100)
	if autorun_active:
		_update_autorun_timer()

func _update_autorun_timer() -> void:
	if autorun_timer == null: return
	autorun_timer.stop()
	autorun_timer.wait_time = 1.0 / float(runtime_ticks_per_second)
	autorun_timer.start()

func _stop_autorun_timer() -> void:
	autorun_active = false
	if autorun_timer != null:
		autorun_timer.stop()

func _on_autorun_timer_timeout() -> void:
	if not autorun_active or not editor.is_run_active():
		_stop_autorun_timer()
		return
	if not _advance_runtime_preview():
		_stop_autorun_timer()
		_refresh_runtime_preview()
		_show_objects()
		return
	var now_msec := Time.get_ticks_msec()
	if now_msec - last_runtime_refresh_msec >= 100:
		last_runtime_refresh_msec = now_msec
		_refresh_runtime_preview()
		_show_objects()

func _reset_runtime_preview() -> void:
	_stop_autorun_timer()
	if not editor.reset_runtime(): _show_error(); return
	last_test_inputs.clear(); _refresh_runtime_preview(); _show_objects()

func _stop_runtime_preview() -> void:
	_stop_autorun_timer()
	_discard_runtime_preview()
	if selected_template_id != 0: _show_objects()

func _discard_runtime_preview() -> void:
	_stop_autorun_timer()
	if editor.is_run_active(): editor.stop()
	last_test_inputs.clear(); last_runtime_values.clear(); last_runtime_functions.clear(); last_end_buffer.clear()

func _refresh_runtime_preview() -> void:
	last_runtime_values.assign(editor.get_runtime_values())
	last_runtime_functions.assign(editor.get_runtime_functions())
	last_end_buffer.assign(editor.get_last_end_buffer())
	var read_error := editor.get_last_error()
	if not read_error.is_empty(): status.text = read_error

func _find_template(id: int) -> Dictionary:
	for template in editor.get_templates():
		if int(template.id) == id: return template
	return {}

func _show_delete_menu(context: Dictionary, position: Vector2) -> void:
	var kind := str(context.get("kind", ""))
	if kind == "template":
		var template: Dictionary = context.get("template", {})
		_request_delete(str(template.get("name", "")), func():
			if not editor.remove_template(int(template.get("id", 0))): _show_error(); return
			if selected_template_id == int(template.get("id", 0)): selected_template_id = 0
			_show_objects(), position)
		return
	var entry: Dictionary = context.get("entry", {})
	_request_delete(_semantic_genome_entry(entry), func():
		if not editor.remove_genome_function(int(context.get("template_id", 0)), int(entry.get("index", -1))): _show_error(); return
		_show_objects(), position)
