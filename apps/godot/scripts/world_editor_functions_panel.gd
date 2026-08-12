extends WorldEditorPanel

var selected_function_id := 0
var new_function_active := false
var new_parameter_active := false
var new_process_output_active := false
var new_function_material_active := false
var selected_formula_id_by_function: Dictionary = {}

func show() -> void:
	_show_functions()

func _show_functions() -> void:
	_discard_runtime_preview()
	_clear_workspace()
	var split := HBoxContainer.new(); split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var list_panel := VBoxContainer.new(); list_panel.custom_minimum_size.x = 280
	var title := Label.new(); title.text = tr("ux.functions"); title.add_theme_font_size_override("font_size", 22); list_panel.add_child(title)
	var functions := editor.get_function_types()
	if selected_function_id != 0 and _find_function(selected_function_id).is_empty(): selected_function_id = 0
	if selected_function_id == 0 and not functions.is_empty(): selected_function_id = int(functions[0].id)
	for function_type in functions:
		_add_function_list_item(list_panel, function_type)
	if new_function_active: _add_new_function_row(list_panel)
	var add := Button.new(); add.text = "+ " + tr("ux.add_new"); add.disabled = new_function_active
	add.pressed.connect(func(): new_function_active = true; _show_functions())
	list_panel.add_child(add)
	var editor_scroll := ScrollContainer.new(); editor_scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL; editor_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var editor_panel := VBoxContainer.new(); editor_panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL; editor_scroll.add_child(editor_panel)
	if selected_function_id == 0:
		var hint := Label.new(); hint.text = tr("ux.create_first_function"); editor_panel.add_child(hint)
	else:
		_build_function_editor(editor_panel, _find_function(selected_function_id))
	split.add_child(list_panel); split.add_child(editor_scroll); workspace.add_child(split)

func _add_function_list_item(parent: VBoxContainer, function_type: Dictionary) -> void:
	var button := Button.new(); button.text = str(function_type.name); button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.button_pressed = int(function_type.id) == selected_function_id
	button.pressed.connect(func(): selected_function_id = int(function_type.id); _show_functions())
	button.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed and (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT:
			_show_delete_menu({"kind": "function", "function": function_type}, button.get_global_position() + (event as InputEventMouseButton).position)
	)
	parent.add_child(button)

func _add_new_function_row(parent: VBoxContainer) -> void:
	var name := LineEdit.new(); name.placeholder_text = tr("ux.function_name"); parent.add_child(name)
	var row := HBoxContainer.new(); var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		var id := editor.add_function_type(name.text)
		if id == 0: _show_error(); return
		selected_function_id = id; new_function_active = false; _show_functions()
	)
	cancel.pressed.connect(func(): new_function_active = false; _show_functions())
	row.add_child(save); row.add_child(cancel); parent.add_child(row)

func _build_function_editor(parent: VBoxContainer, function_type: Dictionary) -> void:
	_add_title_to(parent, tr("ux.function"))
	var name_row := HBoxContainer.new(); var name := LineEdit.new(); name.text = str(function_type.name); name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var rename := Button.new(); rename.text = tr("ux.save")
	rename.pressed.connect(func():
		if not editor.rename_function_type(int(function_type.id), name.text): _show_error(); return
		_show_functions()
	)
	name_row.add_child(name); name_row.add_child(rename); parent.add_child(_labeled_row(tr("ux.name"), name_row))
	_add_section(parent, tr("ux.genome_parameters"))
	for parameter in function_type.genome_parameters:
		_add_parameter_card(parent, function_type, parameter)
	if new_parameter_active: _add_new_parameter_row(parent, function_type)
	var add_parameter := Button.new(); add_parameter.text = "+ " + tr("ux.add_parameter"); add_parameter.disabled = new_parameter_active
	add_parameter.pressed.connect(func(): new_parameter_active = true; _show_functions())
	parent.add_child(add_parameter)
	_add_section(parent, tr("ux.semantic_genome_preview"))
	var preview := Label.new(); preview.text = tr("ux.semantic_record") % _semantic_genome_preview(function_type); parent.add_child(preview)
	_build_function_formula(parent, function_type)
	_build_function_characteristics(parent, function_type)
	_build_function_process(parent, function_type)
	_build_function_buffer(parent, function_type)
	_build_function_materials(parent, function_type)

func _build_function_characteristics(parent: VBoxContainer, function_type: Dictionary) -> void:
	_add_section(parent, tr("ux.function_characteristic_contributions"))
	for contribution in function_type.get("characteristic_contributions", []):
		var row := HBoxContainer.new()
		var characteristic := _characteristic_selector(int(contribution.get("characteristic_id", 0)), false); characteristic.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		var source := _source_selector(function_type, contribution.get("amount_source", {})); source.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		var save := Button.new(); save.text = tr("ux.save")
		save.pressed.connect(func():
			if not editor.set_function_characteristic_contribution(int(function_type.id), _selected_unit_id(characteristic), _selected_source(source)): _show_error(); return
			_show_functions()
		)
		var remove := Button.new(); remove.text = tr("ux.delete")
		remove.pressed.connect(func():
			if not editor.remove_function_characteristic_contribution(int(function_type.id), int(contribution.get("characteristic_id", 0))): _show_error(); return
			_show_functions()
		)
		row.add_child(characteristic); row.add_child(source); row.add_child(save); row.add_child(remove); parent.add_child(row)
	var add_row := HBoxContainer.new()
	var add_characteristic := _characteristic_selector(0, false); add_characteristic.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var add_source := _source_selector(function_type, {}); add_source.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var add := Button.new(); add.text = tr("ux.add")
	add.pressed.connect(func():
		if not editor.set_function_characteristic_contribution(int(function_type.id), _selected_unit_id(add_characteristic), _selected_source(add_source)): _show_error(); return
		_show_functions()
	)
	add_row.add_child(add_characteristic); add_row.add_child(add_source); add_row.add_child(add); parent.add_child(add_row)

func _build_function_buffer(parent: VBoxContainer, function_type: Dictionary) -> void:
	_add_section(parent, tr("ux.buffer_process"))
	var buffer_value = function_type.get("buffer")
	var buffer: Dictionary = buffer_value if buffer_value is Dictionary else {}
	var value := _value_selector(int(buffer.get("value_key", 0)))
	var capacity := _source_selector(function_type, buffer.get("capacity_source", {}))
	var throughput := _source_selector(function_type, buffer.get("throughput_source", {}))
	var leakage := _source_selector(function_type, buffer.get("leakage_source", {}))
	parent.add_child(_labeled_row(tr("ux.value"), value))
	parent.add_child(_labeled_row(tr("ux.capacity"), capacity))
	parent.add_child(_labeled_row(tr("ux.throughput"), throughput))
	parent.add_child(_labeled_row(tr("ux.leakage"), leakage))
	var save := Button.new()
	save.text = tr("ux.update_buffer_process") if not buffer.is_empty() else tr("ux.create_buffer_process")
	save.pressed.connect(func():
		if not editor.set_buffer_process(int(function_type.id), _selected_unit_id(value), _selected_source(capacity), _selected_source(throughput), _selected_source(leakage)): _show_error(); return
		_show_functions()
	)
	parent.add_child(save)
	if buffer.is_empty(): return
	var remove := Button.new(); remove.text = tr("ux.remove_buffer_process")
	remove.pressed.connect(func():
		if not editor.remove_buffer_process(int(function_type.id)): _show_error(); return
		_show_functions()
	)
	parent.add_child(remove)

func _build_function_materials(parent: VBoxContainer, function_type: Dictionary) -> void:
	_add_section(parent, tr("ux.construction_materials"))
	for contribution in function_type.get("material_contributions", []):
		_add_function_material_card(parent, function_type, contribution)
	if new_function_material_active:
		_add_new_function_material_row(parent, function_type)
	var add := Button.new(); add.text = "+ " + tr("ux.add"); add.disabled = new_function_material_active
	add.pressed.connect(func(): new_function_material_active = true; _show_functions())
	parent.add_child(add)

func _add_function_material_card(parent: VBoxContainer, function_type: Dictionary, contribution: Dictionary) -> void:
	var card := PanelContainer.new(); var row := HBoxContainer.new(); card.add_child(row)
	var material := _value_selector(int(contribution.get("value_key", 0))); material.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var amount := _source_selector(function_type, contribution.get("amount_source", {})); amount.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save := Button.new(); save.text = tr("ux.save")
	save.pressed.connect(func():
		if not editor.set_function_material_contribution(int(function_type.id), _selected_unit_id(material), _selected_source(amount)): _show_error(); return
		_show_functions()
	)
	row.add_child(material); row.add_child(amount); row.add_child(save)
	card.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed and (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT:
			_show_delete_menu({"kind": "function_material", "function_id": int(function_type.id), "contribution": contribution}, card.get_global_position() + (event as InputEventMouseButton).position)
	)
	parent.add_child(card)

func _add_new_function_material_row(parent: VBoxContainer, function_type: Dictionary) -> void:
	var row := HBoxContainer.new(); var material := _value_selector(0); material.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var amount := _source_selector(function_type, {}); amount.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		if not editor.set_function_material_contribution(int(function_type.id), _selected_unit_id(material), _selected_source(amount)): _show_error(); return
		new_function_material_active = false; _show_functions()
	)
	cancel.pressed.connect(func(): new_function_material_active = false; _show_functions())
	row.add_child(material); row.add_child(amount); row.add_child(save); row.add_child(cancel); parent.add_child(row)

func _add_parameter_card(parent: VBoxContainer, function_type: Dictionary, parameter: Dictionary) -> void:
	var card := PanelContainer.new(); var row := HBoxContainer.new(); card.add_child(row)
	var name := LineEdit.new(); name.text = str(parameter.name); name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var value := SpinBox.new(); value.step = 0.1; value.value = float(parameter.default_value); value.custom_minimum_size.x = 160
	var save := Button.new(); save.text = tr("ux.save")
	save.pressed.connect(func():
		if not editor.update_genome_parameter(int(function_type.id), int(parameter.id), name.text, value.value): _show_error(); return
		_show_functions()
	)
	row.add_child(name); row.add_child(value); row.add_child(save)
	card.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed and (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT:
			_show_delete_menu({"kind": "parameter", "function_id": int(function_type.id), "parameter": parameter}, card.get_global_position() + (event as InputEventMouseButton).position)
	)
	parent.add_child(card)

func _add_new_parameter_row(parent: VBoxContainer, function_type: Dictionary) -> void:
	var row := HBoxContainer.new(); var name := LineEdit.new(); name.placeholder_text = tr("ux.parameter_name"); name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var value := SpinBox.new(); value.step = 0.1; value.value = 1.0; value.custom_minimum_size.x = 160
	var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		if editor.add_genome_parameter(int(function_type.id), name.text, value.value) == 0: _show_error(); return
		new_parameter_active = false; _show_functions()
	)
	cancel.pressed.connect(func(): new_parameter_active = false; _show_functions())
	row.add_child(name); row.add_child(value); row.add_child(save); row.add_child(cancel); parent.add_child(row)

func _semantic_genome_preview(function_type: Dictionary) -> String:
	var parts := ["%02X" % int(function_type.id)]
	for parameter in function_type.genome_parameters:
		parts.append(str(float(parameter.default_value)))
	return " | ".join(parts)

func _build_function_formula(parent: VBoxContainer, function_type: Dictionary) -> void:
	_add_section(parent, tr("ux.formula"))
	var current_id := int(selected_formula_id_by_function.get(int(function_type.id), int(function_type.calculations[0].calculation_id) if not function_type.calculations.is_empty() else 0))
	var selector := OptionButton.new(); selector.add_item(tr("ux.none"), 0)
	for calculation in editor.get_calculations():
		selector.add_item(str(calculation.name), int(calculation.id))
		if int(calculation.id) == current_id: selector.select(selector.item_count - 1)
	selector.item_selected.connect(func(index: int):
		selected_formula_id_by_function[int(function_type.id)] = selector.get_item_id(index)
		_show_functions()
	)
	parent.add_child(_labeled_row(tr("ux.formula"), selector))
	var selected_id := _selected_unit_id(selector)
	if selected_id == 0: return
	var calculation := _find_calculation(selected_id)
	var binding := _find_binding(function_type, selected_id)
	var bindings := []
	for input in calculation.inputs:
		var source := _parameter_selector(function_type, int(_binding_parameter_id(binding, int(input.id))))
		parent.add_child(_labeled_row(str(input.name), source))
		bindings.append({"input_id": int(input.id), "selector": source})
	var save := Button.new(); save.text = tr("ux.save_formula_binding")
	save.pressed.connect(func():
		var stored := []
		for entry in bindings:
			stored.append({"input_id": entry.input_id, "genome_parameter_id": _selected_unit_id(entry.selector)})
		if not editor.set_function_calculation_binding(int(function_type.id), selected_id, stored): _show_error(); return
		_show_functions()
	)
	parent.add_child(save)
	if not binding.is_empty():
		var remove := Button.new(); remove.text = tr("ux.remove_formula_binding")
		remove.pressed.connect(func():
			if not editor.remove_function_calculation_binding(int(function_type.id), selected_id): _show_error(); return
			_show_functions()
		)
		parent.add_child(remove)

func _build_function_process(parent: VBoxContainer, function_type: Dictionary) -> void:
	_add_section(parent, tr("ux.process"))
	var process_value = function_type.get("process")
	var process: Dictionary = process_value if process_value is Dictionary else {}
	var input := _value_selector(int(process.get("input_key", 0)))
	var throughput := _source_selector(function_type, process.get("throughput_source", {}))
	var conversion := _conversion_selector(int(process.get("conversion_id", 0)))
	parent.add_child(_labeled_row(tr("ux.input"), input)); parent.add_child(_labeled_row(tr("ux.throughput"), throughput)); parent.add_child(_labeled_row(tr("ux.conversion"), conversion))
	if process.is_empty():
		var output_value := _value_selector(0); var allocation := _source_selector(function_type, {})
		parent.add_child(_labeled_row(tr("ux.output"), output_value)); parent.add_child(_labeled_row(tr("ux.allocation"), allocation))
		var create := Button.new(); create.text = tr("ux.create_process")
		create.pressed.connect(func():
			var outputs := [{"output_key": _selected_unit_id(output_value), "allocation_source": _selected_source(allocation)}]
			if not editor.set_function_process_full(int(function_type.id), _selected_unit_id(input), _selected_source(throughput), _selected_unit_id(conversion), outputs): _show_error(); return
			_show_functions()
		)
		parent.add_child(create); return
	var update := Button.new(); update.text = tr("ux.update_process")
	update.pressed.connect(func():
		if not editor.change_function_process_settings(int(function_type.id), _selected_unit_id(input), _selected_source(throughput), _selected_unit_id(conversion)): _show_error(); return
		_show_functions()
	)
	parent.add_child(update)
	var remove := Button.new(); remove.text = tr("ux.remove_process")
	remove.pressed.connect(func():
		if not editor.remove_function_process(int(function_type.id)): _show_error(); return
		_show_functions()
	)
	parent.add_child(remove)
	_add_section(parent, tr("ux.outputs"))
	for output in process.outputs:
		_add_process_output_card(parent, function_type, output)
	if new_process_output_active: _add_new_process_output_row(parent, function_type)
	var add := Button.new(); add.text = "+ " + tr("ux.add_output"); add.disabled = new_process_output_active
	add.pressed.connect(func(): new_process_output_active = true; _show_functions())
	parent.add_child(add)

func _add_process_output_card(parent: VBoxContainer, function_type: Dictionary, output: Dictionary) -> void:
	var card := PanelContainer.new(); var box := VBoxContainer.new(); card.add_child(box)
	var value := _value_selector(int(output.output_key)); var source := _source_selector(function_type, output.get("allocation_source", {}))
	box.add_child(_labeled_row(tr("ux.output"), value)); box.add_child(_labeled_row(tr("ux.allocation"), source))
	var update := Button.new(); update.text = tr("ux.save")
	update.pressed.connect(func():
		if not editor.change_function_process_output(int(function_type.id), int(output.output_key), _selected_unit_id(value), _selected_source(source)): _show_error(); return
		_show_functions()
	)
	box.add_child(update)
	card.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed and (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT:
			_show_delete_menu({"kind": "process_output", "function_id": int(function_type.id), "output": output}, card.get_global_position() + (event as InputEventMouseButton).position)
	)
	parent.add_child(card)

func _add_new_process_output_row(parent: VBoxContainer, function_type: Dictionary) -> void:
	var row := HBoxContainer.new(); var value := _value_selector(0); value.size_flags_horizontal = Control.SIZE_EXPAND_FILL; var source := _source_selector(function_type, {}); source.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		if not editor.add_function_process_output(int(function_type.id), _selected_unit_id(value), _selected_source(source)): _show_error(); return
		new_process_output_active = false; _show_functions()
	)
	cancel.pressed.connect(func(): new_process_output_active = false; _show_functions())
	row.add_child(value); row.add_child(source); row.add_child(save); row.add_child(cancel); parent.add_child(row)

func _value_selector(selected_id: int) -> OptionButton:
	var selector := OptionButton.new()
	for value in editor.get_values():
		selector.add_item(str(value.name), int(value.key))
		if int(value.key) == selected_id: selector.select(selector.item_count - 1)
	return selector

func _conversion_selector(selected_id: int) -> OptionButton:
	var selector := OptionButton.new()
	for conversion in editor.get_unit_conversions():
		selector.add_item("%s %s → %s %s" % [str(conversion.source_amount), _conversion_unit_symbol(conversion.source_components), str(conversion.target_amount), _conversion_unit_symbol(conversion.target_components)], int(conversion.id))
		if int(conversion.id) == selected_id: selector.select(selector.item_count - 1)
	return selector

func _parameter_selector(function_type: Dictionary, selected_id: int) -> OptionButton:
	var selector := OptionButton.new()
	for parameter in function_type.genome_parameters:
		selector.add_item(str(parameter.name), int(parameter.id))
		if int(parameter.id) == selected_id: selector.select(selector.item_count - 1)
	return selector

func _source_selector(function_type: Dictionary, selected_source: Dictionary) -> OptionButton:
	var selector := OptionButton.new(); selector.add_item(tr("ux.none"), 0); selector.set_item_metadata(0, {})
	for parameter in function_type.genome_parameters:
		var source := {"kind": "genome", "genome_parameter_id": int(parameter.id)}
		selector.add_item(tr("ux.source_genome") % str(parameter.name)); selector.set_item_metadata(selector.item_count - 1, source)
		if _source_matches(selected_source, source): selector.select(selector.item_count - 1)
	for binding in function_type.calculations:
		var calculation := _find_calculation(int(binding.calculation_id))
		for output in calculation.get("outputs", []):
			var source := {"kind": "calculation", "calculation_id": int(binding.calculation_id), "calculation_output_id": int(output.id)}
			selector.add_item(tr("ux.source_formula") % [str(calculation.name), str(output.name)]); selector.set_item_metadata(selector.item_count - 1, source)
			if _source_matches(selected_source, source): selector.select(selector.item_count - 1)
	return selector

func _selected_source(selector: OptionButton) -> Dictionary:
	if selector.selected < 0: return {}
	var value = selector.get_item_metadata(selector.selected)
	return value if value is Dictionary else {}

func _source_matches(left: Dictionary, right: Dictionary) -> bool:
	if str(left.get("kind", "")) != str(right.get("kind", "")): return false
	if str(right.get("kind", "")) == "genome":
		return int(left.get("genome_parameter_id", 0)) == int(right.get("genome_parameter_id", 0))
	return int(left.get("calculation_id", 0)) == int(right.get("calculation_id", 0)) and int(left.get("calculation_output_id", 0)) == int(right.get("calculation_output_id", 0))

func _find_function(id: int) -> Dictionary:
	for function_type in editor.get_function_types():
		if int(function_type.id) == id: return function_type
	return {}

func _find_binding(function_type: Dictionary, calculation_id: int) -> Dictionary:
	for binding in function_type.calculations:
		if int(binding.calculation_id) == calculation_id: return binding
	return {}

func _binding_parameter_id(binding: Dictionary, input_id: int) -> int:
	for entry in binding.get("inputs", []):
		if int(entry.input_id) == input_id: return int(entry.genome_parameter_id)
	return 0

func _labeled_row(label_text: String, control: Control) -> HBoxContainer:
	var row := HBoxContainer.new(); var label := Label.new(); label.text = label_text; label.custom_minimum_size.x = 220
	row.add_child(label); row.add_child(control); return row

func _add_title_to(parent: VBoxContainer, text: String) -> void:
	var title := Label.new(); title.text = text; title.add_theme_font_size_override("font_size", 26); parent.add_child(title)

func _show_delete_menu(context: Dictionary, position: Vector2) -> void:
	var kind := str(context.get("kind", ""))
	if kind == "function":
		var function_type: Dictionary = context.get("function", {})
		_request_delete(str(function_type.get("name", "")), func():
			if not editor.remove_function_type(int(function_type.get("id", 0))): _show_error(); return
			if selected_function_id == int(function_type.get("id", 0)): selected_function_id = 0
			_show_functions(), position)
		return
	if kind == "parameter":
		var parameter: Dictionary = context.get("parameter", {})
		_request_delete(str(parameter.get("name", "")), func():
			if not editor.remove_genome_parameter(int(context.get("function_id", 0)), int(parameter.get("id", 0))): _show_error(); return
			_show_functions(), position)
		return
	if kind == "process_output":
		var output: Dictionary = context.get("output", {})
		_request_delete(_value_name(int(output.get("output_key", 0))), func():
			if not editor.remove_function_process_output(int(context.get("function_id", 0)), int(output.get("output_key", 0))): _show_error(); return
			_show_functions(), position)
		return
	var contribution: Dictionary = context.get("contribution", {})
	_request_delete(_value_name(int(contribution.get("value_key", 0))), func():
		if not editor.remove_function_material_contribution(int(context.get("function_id", 0)), int(contribution.get("value_key", 0))): _show_error(); return
		_show_functions(), position)
