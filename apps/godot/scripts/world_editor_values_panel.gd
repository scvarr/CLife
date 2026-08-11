extends WorldEditorPanel

var new_value_row_active := false

func show() -> void:
	_show_world_quantities()

func _show_world_quantities() -> void:
	_discard_runtime_preview()
	_clear_workspace()
	_add_title(tr("ux.world_quantities"))
	var header := HBoxContainer.new()
	var name := Label.new(); name.text = tr("ux.name"); name.custom_minimum_size.x = 260
	var unit := Label.new(); unit.text = tr("ux.unit"); unit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(name); header.add_child(unit); workspace.add_child(header)
	for value in editor.get_values():
		_add_value_row(value)
	if new_value_row_active:
		_add_new_value_row()
	var add := Button.new(); add.text = "+ " + tr("ux.add_new"); add.disabled = new_value_row_active
	add.pressed.connect(func(): new_value_row_active = true; _show_world_quantities())
	workspace.add_child(add)

func _add_value_row(value: Dictionary) -> void:
	var row := HBoxContainer.new()
	var name := Label.new(); name.text = str(value.name); name.custom_minimum_size.x = 260
	var unit := Label.new(); unit.text = _value_unit_symbol(value); unit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(name); row.add_child(unit)
	row.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed:
			if (event as InputEventMouseButton).button_index == MOUSE_BUTTON_LEFT: _edit_value_row(row, value)
			elif (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT: _show_delete_menu({"kind": "value", "value": value}, row.get_global_position() + (event as InputEventMouseButton).position)
	)
	workspace.add_child(row)

func _edit_value_row(row: HBoxContainer, value: Dictionary) -> void:
	for child in row.get_children(): child.queue_free()
	var name := LineEdit.new(); name.text = str(value.name); name.custom_minimum_size.x = 260
	var unit := _unit_selector(_value_unit_id(value), true); unit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var complex_unit := _has_complex_value_unit(value)
	if complex_unit:
		unit.clear(); unit.add_item(tr("ux.complex_unit"), -1); unit.disabled = true
	var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		if not editor.rename_value(int(value.key), name.text): _show_error(); return
		var unit_id := _selected_unit_id(unit)
		var unit_saved := true
		if unit_id == 0: unit_saved = editor.clear_value_unit(int(value.key))
		elif unit_id > 0: unit_saved = editor.set_value_unit(int(value.key), unit_id)
		if not unit_saved: _show_error(); return
		_show_world_quantities()
	)
	cancel.pressed.connect(_show_world_quantities)
	row.add_child(name); row.add_child(unit); row.add_child(save); row.add_child(cancel)

func _add_new_value_row() -> void:
	var row := HBoxContainer.new()
	var name := LineEdit.new(); name.placeholder_text = tr("ux.name"); name.custom_minimum_size.x = 260
	var unit := _unit_selector(0, true); unit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		var key := editor.add_value(name.text)
		if key == 0: _show_error(); return
		var unit_id := _selected_unit_id(unit)
		if unit_id != 0 and not editor.set_value_unit(key, unit_id): _show_error(); return
		new_value_row_active = false; _show_world_quantities()
	)
	cancel.pressed.connect(func(): new_value_row_active = false; _show_world_quantities())
	row.add_child(name); row.add_child(unit); row.add_child(save); row.add_child(cancel); workspace.add_child(row)

func _value_unit_id(value: Dictionary) -> int:
	var components: Array = value.get("unit_components", [])
	if components.size() == 1 and int((components[0] as Dictionary).get("exponent", 0)) == 1:
		return int((components[0] as Dictionary).get("id", 0))
	return 0

func _value_unit_symbol(value: Dictionary) -> String:
	if _has_complex_value_unit(value): return tr("ux.complex_unit")
	var unit_id := _value_unit_id(value)
	if unit_id == 0: return tr("ux.no_unit")
	return _unit_symbol(unit_id)

func _has_complex_value_unit(value: Dictionary) -> bool:
	var components: Array = value.get("unit_components", [])
	return not components.is_empty() and _value_unit_id(value) == 0

func _unit_selector(selected_id: int, allow_none: bool) -> OptionButton:
	var selector := OptionButton.new()
	if allow_none:
		selector.add_item(tr("ux.no_unit"), 0)
	for unit in editor.get_units():
		selector.add_item(str(unit.symbol), int(unit.id))
		if int(unit.id) == selected_id: selector.select(selector.item_count - 1)
	if selector.item_count == 0:
		selector.disabled = true
	return selector

func _selected_unit_id(selector: OptionButton) -> int:
	if selector.selected < 0:
		return 0
	return selector.get_item_id(selector.selected)

func _unit_symbol(unit_id: int) -> String:
	for unit in editor.get_units():
		if int(unit.id) == unit_id: return str(unit.symbol)
	return tr("ux.no_unit")

func _show_delete_menu(context: Dictionary, position: Vector2) -> void:
	var value: Dictionary = context.get("value", {})
	_request_delete(str(value.get("name", "")), func():
		if not editor.remove_value(int(value.get("key", 0))): _show_error(); return
		_show_world_quantities(), position, "value")
