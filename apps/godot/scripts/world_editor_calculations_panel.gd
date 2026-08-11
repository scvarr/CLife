extends WorldEditorPanel

var selected_calculation_id := 0
var new_formula_active := false
var new_input_active := false
var new_output_active := false

func show() -> void:
	_show_formulas()

func _show_formulas() -> void:
	_discard_runtime_preview()
	_clear_workspace()
	var split := HBoxContainer.new(); split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var list_panel := VBoxContainer.new(); list_panel.custom_minimum_size.x = 280
	var list_title := Label.new(); list_title.text = tr("ux.formulas"); list_title.add_theme_font_size_override("font_size", 22)
	list_panel.add_child(list_title)
	var calculations := editor.get_calculations()
	if selected_calculation_id != 0 and _find_calculation(selected_calculation_id).is_empty(): selected_calculation_id = 0
	if selected_calculation_id == 0 and not calculations.is_empty(): selected_calculation_id = int(calculations[0].id)
	for calculation in calculations:
		_add_formula_list_item(list_panel, calculation)
	if new_formula_active:
		_add_new_formula_row(list_panel)
	var add := Button.new(); add.text = "+ " + tr("ux.add_new"); add.disabled = new_formula_active
	add.pressed.connect(_begin_new_formula)
	list_panel.add_child(add)
	var editor_panel := ScrollContainer.new(); editor_panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL; editor_panel.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var editor_content := VBoxContainer.new(); editor_content.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	editor_panel.add_child(editor_content)
	if selected_calculation_id == 0:
		var hint := Label.new(); hint.text = tr("ux.create_first_formula"); editor_content.add_child(hint)
	else:
		_build_formula_editor(editor_content, _find_calculation(selected_calculation_id))
	split.add_child(list_panel); split.add_child(editor_panel); workspace.add_child(split)

func _add_formula_list_item(parent: VBoxContainer, calculation: Dictionary) -> void:
	var button := Button.new()
	button.text = "%s\n%s" % [str(calculation.name), tr("ux.formula_ports") % [calculation.inputs.size(), calculation.outputs.size()]]
	button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.button_pressed = int(calculation.id) == selected_calculation_id
	button.pressed.connect(func(): selected_calculation_id = int(calculation.id); _show_formulas())
	button.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed and (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT:
			_show_delete_menu({"kind": "formula", "calculation": calculation}, button.get_global_position() + (event as InputEventMouseButton).position)
	)
	parent.add_child(button)

func _begin_new_formula() -> void:
	if new_formula_active: return
	new_formula_active = true; _show_formulas()

func _add_new_formula_row(parent: VBoxContainer) -> void:
	var name := LineEdit.new(); name.placeholder_text = tr("ux.formula_name"); parent.add_child(name)
	var row := HBoxContainer.new(); var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		var id := editor.add_calculation(name.text)
		if id == 0: _show_error(); return
		selected_calculation_id = id; new_formula_active = false; _show_formulas()
	)
	cancel.pressed.connect(func(): new_formula_active = false; _show_formulas())
	row.add_child(save); row.add_child(cancel); parent.add_child(row)

func _build_formula_editor(parent: VBoxContainer, calculation: Dictionary) -> void:
	var title := Label.new(); title.text = str(calculation.name); title.add_theme_font_size_override("font_size", 26); parent.add_child(title)
	_add_section(parent, tr("ux.inputs"))
	for input in calculation.inputs:
		_add_input_card(parent, calculation, input)
	if new_input_active:
		_add_new_input_row(parent, calculation)
	var add_input := Button.new(); add_input.text = "+ " + tr("ux.add_input"); add_input.disabled = new_input_active
	add_input.pressed.connect(func(): new_input_active = true; _show_formulas())
	parent.add_child(add_input)
	_add_section(parent, tr("ux.outputs"))
	var index := 1
	for output in calculation.outputs:
		_add_output_card(parent, calculation, output, index); index += 1
	if new_output_active:
		_add_new_output_row(parent, calculation)
	var add_output := Button.new(); add_output.text = "+ " + tr("ux.add_output"); add_output.disabled = new_output_active
	add_output.pressed.connect(func(): new_output_active = true; _show_formulas())
	parent.add_child(add_output)

func _add_input_card(parent: VBoxContainer, calculation: Dictionary, input: Dictionary) -> void:
	var card := PanelContainer.new(); var label := Label.new(); label.text = str(input.name); card.add_child(label)
	card.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed and (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT:
			_show_delete_menu({"kind": "input", "calculation_id": int(calculation.id), "port": input}, card.get_global_position() + (event as InputEventMouseButton).position)
	)
	parent.add_child(card)

func _add_new_input_row(parent: VBoxContainer, calculation: Dictionary) -> void:
	var row := HBoxContainer.new(); var name := LineEdit.new(); name.placeholder_text = tr("ux.input_name"); name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		if editor.add_calculation_input(int(calculation.id), name.text) == 0: _show_error(); return
		new_input_active = false; _show_formulas()
	)
	cancel.pressed.connect(func(): new_input_active = false; _show_formulas())
	row.add_child(name); row.add_child(save); row.add_child(cancel); parent.add_child(row)

func _add_output_card(parent: VBoxContainer, calculation: Dictionary, output: Dictionary, index: int) -> void:
	var card := PanelContainer.new(); var box := VBoxContainer.new(); card.add_child(box)
	var name := Label.new(); name.text = "%d. %s" % [index, str(output.name)]; box.add_child(name)
	var expression := LineEdit.new(); expression.text = str(output.expression_source); expression.placeholder_text = tr("ux.expression"); box.add_child(expression)
	var update := Button.new(); update.text = tr("ux.update_expression")
	update.pressed.connect(func():
		if not editor.set_calculation_output_expression(int(calculation.id), int(output.id), expression.text): _show_error(); return
		_show_formulas()
	)
	box.add_child(update)
	card.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed and (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT:
			_show_delete_menu({"kind": "output", "calculation_id": int(calculation.id), "port": output}, card.get_global_position() + (event as InputEventMouseButton).position)
	)
	parent.add_child(card)

func _add_new_output_row(parent: VBoxContainer, calculation: Dictionary) -> void:
	var box := VBoxContainer.new(); var name := LineEdit.new(); name.placeholder_text = tr("ux.output_name")
	var expression := LineEdit.new(); expression.placeholder_text = tr("ux.expression")
	var row := HBoxContainer.new(); var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		if editor.add_calculation_output(int(calculation.id), name.text, expression.text) == 0: _show_error(); return
		new_output_active = false; _show_formulas()
	)
	cancel.pressed.connect(func(): new_output_active = false; _show_formulas())
	box.add_child(name); box.add_child(expression); row.add_child(save); row.add_child(cancel); box.add_child(row); parent.add_child(box)

func _show_delete_menu(context: Dictionary, position: Vector2) -> void:
	var kind := str(context.get("kind", ""))
	if kind == "formula":
		var calculation: Dictionary = context.get("calculation", {})
		_request_delete(str(calculation.get("name", "")), func():
			if not editor.remove_calculation(int(calculation.get("id", 0))): _show_error(); return
			if selected_calculation_id == int(calculation.get("id", 0)): selected_calculation_id = 0
			_show_formulas(), position)
		return
	var port: Dictionary = context.get("port", {})
	var calculation_id := int(context.get("calculation_id", 0))
	_request_delete(str(port.get("name", "")), func():
		var removed := editor.remove_calculation_input(calculation_id, int(port.get("id", 0))) if kind == "input" else editor.remove_calculation_output(calculation_id, int(port.get("id", 0)))
		if not removed: _show_error(); return
		_show_formulas(), position)
