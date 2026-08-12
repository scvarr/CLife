extends WorldEditorPanel

var new_rule_calculation_id := 0
var new_rule_source_id := 0
var calculation_id_by_rule_index := {}
var source_id_by_rule_index := {}

func show() -> void:
	_show_world_rules()

func _show_world_rules() -> void:
	_discard_runtime_preview()
	_clear_workspace()
	_add_title(tr("ux.world_rules"))
	for rule in editor.get_calculation_world_rules():
		_add_calculation_world_rule_card(rule)
	_add_section(workspace, tr("ux.add_rule"))
	_add_calculation_world_rule_editor(workspace, {}, -1, true)

func _add_calculation_world_rule_card(rule: Dictionary) -> void:
	var card := PanelContainer.new()
	var box := VBoxContainer.new()
	card.add_child(box)
	_add_calculation_world_rule_editor(box, rule, int(rule.get("index", -1)), false)
	workspace.add_child(card)

func _add_calculation_world_rule_editor(parent: VBoxContainer, rule: Dictionary, rule_index: int, is_new: bool) -> void:
	var source := _value_selector(_selected_source_id(rule, rule_index, is_new))
	source.item_selected.connect(func(index: int):
		var source_id := source.get_item_id(index)
		if is_new: new_rule_source_id = source_id
		else: source_id_by_rule_index[rule_index] = source_id
		_show_world_rules()
	)
	parent.add_child(_labeled_row(tr("ux.source"), source))
	var selected_calculation := _selected_calculation_id(rule, rule_index, is_new)
	var calculation_selector := _calculation_selector(selected_calculation)
	calculation_selector.item_selected.connect(func(index: int):
		var calculation_id := calculation_selector.get_item_id(index)
		if is_new: new_rule_calculation_id = calculation_id
		else: calculation_id_by_rule_index[rule_index] = calculation_id
		_show_world_rules()
	)
	parent.add_child(_labeled_row(tr("ux.calculation"), calculation_selector))
	var calculation := _find_calculation(_selected_unit_id(calculation_selector))
	if calculation.is_empty(): return
	_add_section(parent, tr("ux.inputs"))
	var input_selectors := []
	for input in calculation.get("inputs", []):
		var binding := _find_input_binding(rule.get("inputs", []), int(input.id))
		var selector := _world_rule_input_selector(_selected_unit_id(source), binding)
		parent.add_child(_labeled_row(str(input.name), selector))
		input_selectors.append({"input": int(input.id), "selector": selector})
	_add_section(parent, tr("ux.outputs"))
	var output_selectors := []
	for output in calculation.get("outputs", []):
		var binding := _find_output_binding(rule.get("outputs", []), int(output.id))
		var selector := _value_selector(int(binding.get("target", 0)))
		parent.add_child(_labeled_row(str(output.name), selector))
		output_selectors.append({"output": int(output.id), "selector": selector})
	var actions := HBoxContainer.new()
	var save := Button.new(); save.text = tr("ux.add") if is_new else tr("ux.update_rule")
	save.pressed.connect(func():
		var inputs := _collect_input_bindings(input_selectors)
		var outputs := _collect_output_bindings(output_selectors)
		var saved := editor.add_calculation_world_rule(_selected_unit_id(source), _selected_unit_id(calculation_selector), inputs, outputs) if is_new else editor.change_calculation_world_rule(rule_index, _selected_unit_id(source), _selected_unit_id(calculation_selector), inputs, outputs)
		if not saved: _show_error(); return
		if is_new:
			new_rule_calculation_id = 0
			new_rule_source_id = 0
		_show_world_rules()
	)
	actions.add_child(save)
	if not is_new:
		var remove := Button.new(); remove.text = tr("ux.remove_rule")
		remove.pressed.connect(func():
			if not editor.remove_calculation_world_rule(rule_index): _show_error(); return
			calculation_id_by_rule_index.erase(rule_index)
			source_id_by_rule_index.erase(rule_index)
			_show_world_rules()
		)
		actions.add_child(remove)
	parent.add_child(actions)

func _selected_calculation_id(rule: Dictionary, rule_index: int, is_new: bool) -> int:
	if is_new:
		if new_rule_calculation_id == 0 and not editor.get_calculations().is_empty(): new_rule_calculation_id = int(editor.get_calculations()[0].id)
		return new_rule_calculation_id
	var current := int(rule.get("calculation", 0))
	if not calculation_id_by_rule_index.has(rule_index): calculation_id_by_rule_index[rule_index] = current
	return int(calculation_id_by_rule_index[rule_index])

func _selected_source_id(rule: Dictionary, rule_index: int, is_new: bool) -> int:
	if is_new:
		if new_rule_source_id == 0 and not editor.get_values().is_empty(): new_rule_source_id = int(editor.get_values()[0].key)
		return new_rule_source_id
	var current := int(rule.get("source", 0))
	if not source_id_by_rule_index.has(rule_index): source_id_by_rule_index[rule_index] = current
	return int(source_id_by_rule_index[rule_index])

func _calculation_selector(selected_id: int) -> OptionButton:
	var selector := OptionButton.new()
	for calculation in editor.get_calculations():
		selector.add_item(str(calculation.name), int(calculation.id))
		if int(calculation.id) == selected_id: selector.select(selector.item_count - 1)
	return selector

func _world_rule_input_selector(source_id: int, selected_binding: Dictionary) -> OptionButton:
	var selector := OptionButton.new()
	var source_binding := {"kind": "source_residual", "value": source_id}
	selector.add_item(tr("ux.source_residual")); selector.set_item_metadata(0, source_binding)
	if _binding_matches(selected_binding, source_binding): selector.select(0)
	for value in editor.get_values():
		var binding := {"kind": "runtime_value", "value": int(value.key)}
		selector.add_item(tr("ux.runtime_value_source") % str(value.name)); selector.set_item_metadata(selector.item_count - 1, binding)
		if _binding_matches(selected_binding, binding): selector.select(selector.item_count - 1)
	for characteristic in editor.get_object_characteristics():
		var binding := {"kind": "object_characteristic", "characteristic": int(characteristic.id)}
		selector.add_item(tr("ux.object_characteristic_source") % str(characteristic.name)); selector.set_item_metadata(selector.item_count - 1, binding)
		if _binding_matches(selected_binding, binding): selector.select(selector.item_count - 1)
	if selector.selected < 0: selector.select(0)
	return selector

func _collect_input_bindings(selectors: Array) -> Array:
	var bindings := []
	for entry in selectors:
		var selector: OptionButton = entry.selector
		var source: Dictionary = selector.get_item_metadata(selector.selected) if selector.selected >= 0 else {}
		var binding := {"input": int(entry.input), "kind": str(source.get("kind", ""))}
		if binding.kind == "object_characteristic": binding["characteristic"] = int(source.get("characteristic", 0))
		else: binding["value"] = int(source.get("value", 0))
		bindings.append(binding)
	return bindings

func _collect_output_bindings(selectors: Array) -> Array:
	var bindings := []
	for entry in selectors:
		bindings.append({"output": int(entry.output), "target": _selected_unit_id(entry.selector)})
	return bindings

func _find_input_binding(bindings: Array, input_id: int) -> Dictionary:
	for binding in bindings:
		if int((binding as Dictionary).get("input", 0)) == input_id: return binding
	return {}

func _find_output_binding(bindings: Array, output_id: int) -> Dictionary:
	for binding in bindings:
		if int((binding as Dictionary).get("output", 0)) == output_id: return binding
	return {}

func _binding_matches(left: Dictionary, right: Dictionary) -> bool:
	if str(left.get("kind", "")) != str(right.get("kind", "")): return false
	return int(left.get("characteristic", 0)) == int(right.get("characteristic", 0)) and int(left.get("value", 0)) == int(right.get("value", 0))
