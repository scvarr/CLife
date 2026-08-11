extends WorldEditorPanel

var new_unit_row_active := false

func show() -> void:
	_show_units()

func _show_units() -> void:
	_discard_runtime_preview()
	_clear_workspace()
	_add_title(tr("ux.units"))
	var header := HBoxContainer.new()
	var symbol := Label.new(); symbol.text = tr("ux.symbol"); symbol.custom_minimum_size.x = 180
	var description := Label.new(); description.text = tr("ux.comment"); description.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(symbol); header.add_child(description); workspace.add_child(header)
	for unit in editor.get_units():
		_add_unit_row(unit)
	var add := Button.new(); add.text = "+ " + tr("ux.add_new"); add.disabled = new_unit_row_active
	add.pressed.connect(_add_new_unit_row)
	workspace.add_child(add)

func _add_unit_row(unit: Dictionary) -> void:
	var row := HBoxContainer.new()
	var symbol := Label.new(); symbol.text = str(unit.symbol); symbol.custom_minimum_size.x = 180
	var description := Label.new(); description.text = str(unit.description); description.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(symbol); row.add_child(description)
	row.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed:
			if (event as InputEventMouseButton).button_index == MOUSE_BUTTON_LEFT: _edit_unit_row(row, unit)
			elif (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT: _show_delete_menu({"kind": "unit", "unit": unit}, row.get_global_position() + (event as InputEventMouseButton).position)
	)
	workspace.add_child(row)

func _edit_unit_row(row: HBoxContainer, unit: Dictionary) -> void:
	for child in row.get_children(): child.queue_free()
	var symbol := LineEdit.new(); symbol.text = str(unit.symbol); symbol.custom_minimum_size.x = 180
	var description := LineEdit.new(); description.text = str(unit.description); description.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save := Button.new(); save.text = tr("ux.save")
	var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		if not editor.update_unit(int(unit.id), symbol.text, description.text): _show_error(); return
		_show_units()
	)
	cancel.pressed.connect(_show_units)
	row.add_child(symbol); row.add_child(description); row.add_child(save); row.add_child(cancel)

func _add_new_unit_row() -> void:
	if new_unit_row_active: return
	new_unit_row_active = true; _show_units()
	var row := HBoxContainer.new()
	var symbol := LineEdit.new(); symbol.placeholder_text = tr("ux.symbol"); symbol.custom_minimum_size.x = 180
	var description := LineEdit.new(); description.placeholder_text = tr("ux.comment"); description.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save := Button.new(); save.text = tr("ux.save")
	var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		if editor.add_unit(symbol.text, description.text) == 0: _show_error(); return
		new_unit_row_active = false; _show_units()
	)
	cancel.pressed.connect(func(): new_unit_row_active = false; _show_units())
	workspace.add_child(row); row.add_child(symbol); row.add_child(description); row.add_child(save); row.add_child(cancel)

func _show_delete_menu(context: Dictionary, position: Vector2) -> void:
	var unit: Dictionary = context.get("unit", {})
	_request_delete(str(unit.get("symbol", "")), func():
		if not editor.remove_unit(int(unit.get("id", 0))): _show_error(); return
		_show_units(), position, "unit")
