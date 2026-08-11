extends WorldEditorPanel

var new_external_input_active := false
var external_inputs: Array[Dictionary]

func configure(shell_instance: Control, editor_instance: CLifeWorldEditor, workspace_instance: VBoxContainer, status_instance: Label, host_config_instance: RefCounted) -> void:
	super.configure(shell_instance, editor_instance, workspace_instance, status_instance, host_config_instance)
	external_inputs = host_config.external_inputs

func show() -> void:
	_show_external_inputs()

func _show_external_inputs() -> void:
	_discard_runtime_preview()
	_clear_workspace()
	_add_title(tr("ux.external_inputs"))
	var header := HBoxContainer.new()
	var channel := Label.new(); channel.text = tr("ux.godot_channel"); channel.custom_minimum_size.x = 220
	var value := Label.new(); value.text = tr("ux.world_quantity"); value.custom_minimum_size.x = 220
	var test_value := Label.new(); test_value.text = tr("ux.test_value"); test_value.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(channel); header.add_child(value); header.add_child(test_value); workspace.add_child(header)
	for index in external_inputs.size():
		_add_external_input_row(index, external_inputs[index])
	if new_external_input_active:
		_add_new_external_input_row()
	var add := Button.new(); add.text = "+ " + tr("ux.add"); add.disabled = new_external_input_active
	add.pressed.connect(func(): new_external_input_active = true; _show_external_inputs())
	workspace.add_child(add)

func _input_capability_selector(selected_channel: String) -> OptionButton:
	var selector := OptionButton.new()
	for capability in editor.get_host_capabilities():
		if int(capability.direction_id) != 0:
			continue
		selector.add_item("%s — %s" % [tr(str(capability.display_key)), str(capability.channel)])
		selector.set_item_metadata(selector.item_count - 1, str(capability.channel))
		if str(capability.channel) == selected_channel:
			selector.select(selector.item_count - 1)
	return selector

func _selected_capability_channel(selector: OptionButton) -> String:
	if selector.selected < 0:
		return ""
	return str(selector.get_item_metadata(selector.selected))

func _add_external_input_row(index: int, mapping: Dictionary) -> void:
	var row := HBoxContainer.new()
	var channel := Label.new(); channel.text = str(mapping.get("channel", "")); channel.custom_minimum_size.x = 220
	var value := Label.new(); value.text = _value_name(int(mapping.get("value_key", 0))); value.custom_minimum_size.x = 220
	var test_value := Label.new(); test_value.text = str(mapping.get("test_value", 1.0)); test_value.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(channel); row.add_child(value); row.add_child(test_value)
	row.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed:
			if (event as InputEventMouseButton).button_index == MOUSE_BUTTON_LEFT:
				_edit_external_input_row(row, index, mapping)
			elif (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT:
				_show_delete_menu({"kind": "external_input", "index": index, "mapping": mapping}, row.get_global_position() + (event as InputEventMouseButton).position)
	)
	workspace.add_child(row)

func _edit_external_input_row(row: HBoxContainer, index: int, mapping: Dictionary) -> void:
	for child in row.get_children(): child.queue_free()
	var channel := _input_capability_selector(str(mapping.get("channel", ""))); channel.custom_minimum_size.x = 220
	var value := _value_selector(int(mapping.get("value_key", 0))); value.custom_minimum_size.x = 220
	var test_value := SpinBox.new(); test_value.value = float(mapping.get("test_value", 1.0)); test_value.step = 0.1; test_value.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		var updated := {"channel": _selected_capability_channel(channel), "value_key": _selected_unit_id(value), "test_value": test_value.value}
		if not _validate_external_input(updated, index): return
		external_inputs[index] = updated; _show_external_inputs()
	)
	cancel.pressed.connect(_show_external_inputs)
	row.add_child(channel); row.add_child(value); row.add_child(test_value); row.add_child(save); row.add_child(cancel)

func _add_new_external_input_row() -> void:
	var row := HBoxContainer.new()
	var channel := _input_capability_selector(""); channel.custom_minimum_size.x = 220
	var value := _value_selector(0); value.custom_minimum_size.x = 220
	var test_value := SpinBox.new(); test_value.value = 1.0; test_value.step = 0.1; test_value.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		var mapping := {"channel": _selected_capability_channel(channel), "value_key": _selected_unit_id(value), "test_value": test_value.value}
		if not _validate_external_input(mapping): return
		external_inputs.append(mapping); new_external_input_active = false; _show_external_inputs()
	)
	cancel.pressed.connect(func(): new_external_input_active = false; _show_external_inputs())
	row.add_child(channel); row.add_child(value); row.add_child(test_value); row.add_child(save); row.add_child(cancel); workspace.add_child(row)

func _validate_external_input(mapping: Dictionary, ignored_index: int = -1) -> bool:
	var channel := str(mapping.get("channel", ""))
	var value_key := int(mapping.get("value_key", 0))
	if channel.is_empty() or value_key == 0 or not _is_input_capability(channel) or not _has_value_key(value_key):
		status.text = tr("status.external_input_invalid")
		return false
	for index in external_inputs.size():
		if index != ignored_index and str(external_inputs[index].get("channel", "")) == channel:
			status.text = tr("status.external_input_channel_duplicate") % channel
			return false
	return true

func _is_input_capability(channel: String) -> bool:
	for capability in editor.get_host_capabilities():
		if int(capability.direction_id) == 0 and str(capability.channel) == channel:
			return true
	return false

func _has_value_key(value_key: int) -> bool:
	for value in editor.get_values():
		if int(value.key) == value_key:
			return true
	return false

func _show_delete_menu(context: Dictionary, position: Vector2) -> void:
	var index := int(context.get("index", -1))
	var mapping: Dictionary = context.get("mapping", {})
	_request_delete(str(mapping.get("channel", "")), func():
		if index < 0 or index >= external_inputs.size(): _show_error(); return
		external_inputs.remove_at(index)
		_show_external_inputs(), position)
