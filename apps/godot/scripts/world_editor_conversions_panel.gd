extends WorldEditorPanel

var new_conversion_row_active := false

func show() -> void:
	_show_conversions()

func _show_conversions() -> void:
	_discard_runtime_preview()
	_clear_workspace()
	_add_title(tr("ux.conversions"))
	var header := HBoxContainer.new()
	var source := Label.new(); source.text = tr("ux.from"); source.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var arrow := Label.new(); arrow.text = "→"; arrow.custom_minimum_size.x = 50
	var target := Label.new(); target.text = tr("ux.to"); target.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(source); header.add_child(arrow); header.add_child(target); workspace.add_child(header)
	for conversion in editor.get_unit_conversions():
		_add_conversion_row(conversion)
	if new_conversion_row_active:
		_add_new_conversion_row()
	var add := Button.new(); add.text = "+ " + tr("ux.add_new"); add.disabled = new_conversion_row_active
	add.pressed.connect(func(): new_conversion_row_active = true; _show_conversions())
	workspace.add_child(add)

func _add_conversion_row(conversion: Dictionary) -> void:
	var row := HBoxContainer.new()
	var source := Label.new(); source.text = "%s %s" % [str(conversion.source_amount), _conversion_unit_symbol(conversion.get("source_components", []))]; source.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var arrow := Label.new(); arrow.text = "→"; arrow.custom_minimum_size.x = 50
	var target := Label.new(); target.text = "%s %s" % [str(conversion.target_amount), _conversion_unit_symbol(conversion.get("target_components", []))]; target.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(source); row.add_child(arrow); row.add_child(target)
	row.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed and (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT:
			_show_delete_menu({"kind": "conversion", "conversion": conversion}, row.get_global_position() + (event as InputEventMouseButton).position)
	)
	workspace.add_child(row)

func _conversion_unit_symbol(components: Array) -> String:
	if components.size() == 1 and int((components[0] as Dictionary).get("exponent", 0)) == 1:
		return _unit_symbol(int((components[0] as Dictionary).get("id", 0)))
	return tr("ux.complex_unit")

func _add_new_conversion_row() -> void:
	var row := HBoxContainer.new()
	var source_amount := SpinBox.new(); source_amount.min_value = 0.0; source_amount.step = 0.1; source_amount.value = 1.0; source_amount.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var source_unit := _unit_selector(0, false); source_unit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var arrow := Label.new(); arrow.text = "→"; arrow.custom_minimum_size.x = 30
	var target_amount := SpinBox.new(); target_amount.min_value = 0.0; target_amount.step = 0.1; target_amount.value = 1.0; target_amount.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var target_unit := _unit_selector(0, false); target_unit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		if editor.add_unit_conversion(_selected_unit_id(source_unit), source_amount.value, _selected_unit_id(target_unit), target_amount.value) == 0: _show_error(); return
		new_conversion_row_active = false; _show_conversions()
	)
	cancel.pressed.connect(func(): new_conversion_row_active = false; _show_conversions())
	row.add_child(source_amount); row.add_child(source_unit); row.add_child(arrow); row.add_child(target_amount); row.add_child(target_unit); row.add_child(save); row.add_child(cancel); workspace.add_child(row)

func _show_delete_menu(context: Dictionary, position: Vector2) -> void:
	var conversion: Dictionary = context.get("conversion", {})
	_request_delete(tr("ux.conversion"), func():
		if not editor.remove_unit_conversion(int(conversion.get("id", 0))): _show_error(); return
		_show_conversions(), position)
