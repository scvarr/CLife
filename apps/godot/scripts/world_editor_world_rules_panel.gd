extends WorldEditorPanel


func show() -> void:
	_show_world_rules()

func _show_world_rules() -> void:
	_discard_runtime_preview()
	_clear_workspace()
	_add_title(tr("ux.world_rules"))
	for rule in editor.get_world_rules():
		_add_world_rule_card(rule)
	_add_section(workspace, tr("ux.add_rule"))
	var add_row := HBoxContainer.new()
	var source := _value_selector(0); source.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var end_buffer := _value_selector(0); end_buffer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var target := _value_selector(0); target.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var factor := SpinBox.new(); factor.step = 0.1; factor.value = 1.0
	var add := Button.new(); add.text = tr("ux.add")
	add.pressed.connect(func():
		if not editor.add_world_rule(_selected_unit_id(source), _selected_unit_id(end_buffer), _selected_unit_id(target), factor.value): _show_error(); return
		_show_world_rules()
	)
	add_row.add_child(source); add_row.add_child(end_buffer); add_row.add_child(target); add_row.add_child(factor); add_row.add_child(add); workspace.add_child(add_row)

func _add_world_rule_card(rule: Dictionary) -> void:
	var card := PanelContainer.new(); var box := VBoxContainer.new(); card.add_child(box)
	var source := _value_selector(int(rule.get("source_key", 0)))
	var end_buffer := _value_selector(int(rule.get("end_buffer_key", 0)))
	var target := _value_selector(int(rule.get("target_key", 0)))
	var factor := SpinBox.new(); factor.step = 0.1; factor.value = float(rule.get("target_per_source", 1.0))
	box.add_child(_labeled_row(tr("ux.source"), source))
	box.add_child(_labeled_row(tr("ux.end_buffer"), end_buffer))
	box.add_child(_labeled_row(tr("ux.target"), target))
	box.add_child(_labeled_row(tr("ux.target_per_source"), factor))
	var actions := HBoxContainer.new()
	var update := Button.new(); update.text = tr("ux.update_rule")
	update.pressed.connect(func():
		if not editor.change_world_rule(int(rule.get("index", -1)), _selected_unit_id(source), _selected_unit_id(end_buffer), _selected_unit_id(target), factor.value): _show_error(); return
		_show_world_rules()
	)
	var remove := Button.new(); remove.text = tr("ux.remove_rule")
	remove.pressed.connect(func():
		if not editor.remove_world_rule(int(rule.get("index", -1))): _show_error(); return
		_show_world_rules()
	)
	actions.add_child(update); actions.add_child(remove); box.add_child(actions); workspace.add_child(card)
