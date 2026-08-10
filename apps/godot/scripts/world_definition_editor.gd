extends Control

var editor := CLifeWorldEditor.new()
var new_unit_row_active := false
var new_formula_active := false
var new_input_active := false
var new_output_active := false
var selected_calculation_id := 0
var deletion_context: Dictionary = {}
var context_menu := PopupMenu.new()
var delete_confirmation := ConfirmationDialog.new()
@onready var workspace: VBoxContainer = $Layout/Workspace/Margin/Content
@onready var status: Label = $Layout/Workspace/Margin/Content/Status

func _ready() -> void:
	$Layout/Sidebar/Back.text = tr("ux.back_to_menu")
	$Layout/Sidebar/Units.text = tr("ux.units")
	$Layout/Sidebar/Conversions.text = tr("ux.conversions")
	$Layout/Sidebar/Formulas.text = tr("ux.formulas")
	add_child(context_menu)
	context_menu.id_pressed.connect(_on_context_menu_pressed)
	add_child(delete_confirmation)
	delete_confirmation.confirmed.connect(_confirm_deletion)
	_show_units()

func _show_units() -> void:
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

func _show_conversions() -> void:
	_clear_workspace()
	_add_title(tr("ux.conversions"))
	var empty := Label.new(); empty.text = tr("ux.no_conversions"); workspace.add_child(empty)

func _show_formulas() -> void:
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

func _add_section(parent: VBoxContainer, title_text: String) -> void:
	var title := Label.new(); title.text = title_text; title.add_theme_font_size_override("font_size", 18); parent.add_child(title)

func _show_delete_menu(context: Dictionary, position: Vector2) -> void:
	deletion_context = context; context_menu.clear(); context_menu.add_item(tr("ux.delete"), 1); context_menu.position = Vector2i(position); context_menu.popup()

func _on_context_menu_pressed(id: int) -> void:
	if id != 1 or deletion_context.is_empty(): return
	var name := ""
	match str(deletion_context.get("kind", "")):
		"unit": name = str((deletion_context.get("unit", {}) as Dictionary).get("symbol", ""))
		"formula": name = str((deletion_context.get("calculation", {}) as Dictionary).get("name", ""))
		_: name = str((deletion_context.get("port", {}) as Dictionary).get("name", ""))
	delete_confirmation.dialog_text = (tr("ux.delete_unit_confirmation") if str(deletion_context.get("kind", "")) == "unit" else tr("ux.delete_confirmation")) % name
	delete_confirmation.ok_button_text = tr("ux.delete"); delete_confirmation.cancel_button_text = tr("ux.cancel"); delete_confirmation.popup_centered()

func _confirm_deletion() -> void:
	if deletion_context.is_empty(): return
	var context := deletion_context; deletion_context = {}; var removed := false
	match str(context.get("kind", "")):
		"unit": removed = editor.remove_unit(int((context.get("unit", {}) as Dictionary).get("id", 0)))
		"formula":
			removed = editor.remove_calculation(int((context.get("calculation", {}) as Dictionary).get("id", 0)))
			if removed and selected_calculation_id == int((context.get("calculation", {}) as Dictionary).get("id", 0)): selected_calculation_id = 0
		"input": removed = editor.remove_calculation_input(int(context.get("calculation_id", 0)), int((context.get("port", {}) as Dictionary).get("id", 0)))
		"output": removed = editor.remove_calculation_output(int(context.get("calculation_id", 0)), int((context.get("port", {}) as Dictionary).get("id", 0)))
	if not removed: _show_error(); return
	if str(context.get("kind", "")) == "unit": _show_units()
	else: _show_formulas()

func _find_calculation(id: int) -> Dictionary:
	for calculation in editor.get_calculations():
		if int(calculation.id) == id: return calculation
	return {}

func _show_error() -> void:
	status.text = editor.get_last_error()

func _clear_workspace() -> void:
	for child in workspace.get_children():
		if child != status: child.queue_free()
	status.text = ""

func _add_title(text: String) -> void:
	var title := Label.new(); title.text = text; title.add_theme_font_size_override("font_size", 26); workspace.add_child(title)

func _on_back() -> void:
	get_tree().change_scene_to_file("res://scenes/main_menu.tscn")
