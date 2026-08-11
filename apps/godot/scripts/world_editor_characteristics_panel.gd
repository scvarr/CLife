extends WorldEditorPanel

var new_characteristic_active := false

func show() -> void:
	_show_characteristics()

func _show_characteristics() -> void:
	_discard_runtime_preview()
	_clear_workspace()
	_add_title(tr("ux.object_characteristics"))
	for characteristic in editor.get_object_characteristics():
		_add_characteristic_row(characteristic)
	if new_characteristic_active:
		_add_new_characteristic_row()
	var add := Button.new(); add.text = "+ " + tr("ux.add_new"); add.disabled = new_characteristic_active
	add.pressed.connect(func(): new_characteristic_active = true; _show_characteristics())
	workspace.add_child(add)

func _add_characteristic_row(characteristic: Dictionary) -> void:
	var row := HBoxContainer.new(); var name := Label.new(); name.text = str(characteristic.name); name.size_flags_horizontal = Control.SIZE_EXPAND_FILL; row.add_child(name)
	row.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed:
			if (event as InputEventMouseButton).button_index == MOUSE_BUTTON_LEFT: _edit_characteristic_row(row, characteristic)
			elif (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT: _show_delete_menu({"kind": "characteristic", "characteristic": characteristic}, row.get_global_position() + (event as InputEventMouseButton).position)
	)
	workspace.add_child(row)

func _edit_characteristic_row(row: HBoxContainer, characteristic: Dictionary) -> void:
	for child in row.get_children(): child.queue_free()
	var name := LineEdit.new(); name.text = str(characteristic.name); name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		if not editor.rename_object_characteristic(int(characteristic.id), name.text): _show_error(); return
		_show_characteristics()
	)
	cancel.pressed.connect(_show_characteristics)
	row.add_child(name); row.add_child(save); row.add_child(cancel)

func _add_new_characteristic_row() -> void:
	var row := HBoxContainer.new(); var name := LineEdit.new(); name.placeholder_text = tr("ux.characteristic_name"); name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		if editor.add_object_characteristic(name.text) == 0: _show_error(); return
		new_characteristic_active = false; _show_characteristics()
	)
	cancel.pressed.connect(func(): new_characteristic_active = false; _show_characteristics())
	row.add_child(name); row.add_child(save); row.add_child(cancel); workspace.add_child(row)

func _show_delete_menu(context: Dictionary, position: Vector2) -> void:
	var characteristic: Dictionary = context.get("characteristic", {})
	_request_delete(str(characteristic.get("name", "")), func():
		if not editor.remove_object_characteristic(int(characteristic.get("id", 0))): _show_error(); return
		_show_characteristics(), position)
