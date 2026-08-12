extends WorldEditorPanel

var new_material_row_active := false

func show() -> void:
	_show_materials()

func _show_materials() -> void:
	_discard_runtime_preview()
	_clear_workspace()
	_add_title(tr("ux.materials"))
	for material in editor.get_materials():
		_add_material_row(material)
	if new_material_row_active:
		_add_new_material_row()
	var add := Button.new(); add.text = "+ " + tr("ux.add_new"); add.disabled = new_material_row_active
	add.pressed.connect(func(): new_material_row_active = true; _show_materials())
	workspace.add_child(add)

func _add_material_row(material: Dictionary) -> void:
	var row := HBoxContainer.new()
	var name := Label.new(); name.text = str(material.name); name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(name)
	row.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed:
			if (event as InputEventMouseButton).button_index == MOUSE_BUTTON_LEFT: _edit_material_row(row, material)
			elif (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT: _show_delete_menu(material, row.get_global_position() + (event as InputEventMouseButton).position)
	)
	workspace.add_child(row)

func _edit_material_row(row: HBoxContainer, material: Dictionary) -> void:
	for child in row.get_children(): child.queue_free()
	var name := LineEdit.new(); name.text = str(material.name); name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save := Button.new(); save.text = tr("ux.save")
	var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		if not editor.rename_material(int(material.id), name.text): _show_error(); return
		_show_materials()
	)
	cancel.pressed.connect(_show_materials)
	row.add_child(name); row.add_child(save); row.add_child(cancel)

func _add_new_material_row() -> void:
	var row := HBoxContainer.new()
	var name := LineEdit.new(); name.placeholder_text = tr("ux.name"); name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save := Button.new(); save.text = tr("ux.save")
	var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		if editor.add_material(name.text) == 0: _show_error(); return
		new_material_row_active = false; _show_materials()
	)
	cancel.pressed.connect(func(): new_material_row_active = false; _show_materials())
	row.add_child(name); row.add_child(save); row.add_child(cancel); workspace.add_child(row)

func _show_delete_menu(material: Dictionary, position: Vector2) -> void:
	_request_delete(str(material.name), func():
		if not editor.remove_material(int(material.id)): _show_error(); return
		_show_materials(), position)
