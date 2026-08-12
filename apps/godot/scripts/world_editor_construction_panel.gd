extends WorldEditorPanel

var selected_construction_calculation_id := 0

func show() -> void:
	_show_construction()

func _show_construction() -> void:
	_discard_runtime_preview()
	_clear_workspace()
	_add_title(tr("ux.construction"))
	var existing := editor.get_object_construction()
	var calculations := editor.get_calculations()
	if selected_construction_calculation_id == 0 and existing is Dictionary and not existing.is_empty(): selected_construction_calculation_id = int(existing.get("calculation_id", 0))
	if selected_construction_calculation_id == 0 and not calculations.is_empty(): selected_construction_calculation_id = int(calculations[0].id)
	var selector := OptionButton.new(); selector.add_item(tr("ux.none"), 0)
	for calculation in calculations:
		selector.add_item(str(calculation.name), int(calculation.id))
		if int(calculation.id) == selected_construction_calculation_id: selector.select(selector.item_count - 1)
	selector.item_selected.connect(func(index: int): selected_construction_calculation_id = selector.get_item_id(index); _show_construction())
	workspace.add_child(_labeled_row(tr("ux.formula"), selector))
	if selected_construction_calculation_id == 0:
		return
	var calculation := _find_calculation(selected_construction_calculation_id)
	var existing_inputs: Array = existing.get("inputs", []) if existing is Dictionary else []
	var existing_outputs: Array = existing.get("outputs", []) if existing is Dictionary else []
	_add_section(workspace, tr("ux.inputs"))
	var input_fields := []
	for input in calculation.get("inputs", []):
		var source := _construction_source_selector(_find_construction_input(existing_inputs, int(input.id)))
		workspace.add_child(_labeled_row(str(input.name), source)); input_fields.append({"input_id": int(input.id), "selector": source})
	_add_section(workspace, tr("ux.outputs"))
	var output_fields := []
	for output in calculation.get("outputs", []):
		var target := _characteristic_selector(_construction_output_characteristic(existing_outputs, int(output.id)), true)
		workspace.add_child(_labeled_row(str(output.name), target)); output_fields.append({"output_id": int(output.id), "selector": target})
	var save := Button.new(); save.text = tr("ux.save")
	save.pressed.connect(func():
		var inputs := []; var outputs := []
		for field in input_fields:
			var source = field.selector.get_item_metadata(field.selector.selected) if field.selector.selected >= 0 else {}
			if not (source is Dictionary) or source.is_empty(): status.text = tr("status.construction_input_required"); return
			var item := {"input_id": field.input_id, "kind": source.kind}
			if str(source.kind) == "material": item["material_id"] = int(source.material_id)
			else: item["characteristic_id"] = int(source.characteristic_id)
			inputs.append(item)
		for field in output_fields:
			var characteristic_id := _selected_unit_id(field.selector)
			if characteristic_id != 0: outputs.append({"output_id": field.output_id, "characteristic_id": characteristic_id})
		if not editor.set_object_construction(selected_construction_calculation_id, inputs, outputs): _show_error(); return
		_show_construction()
	)
	workspace.add_child(save)
	if not existing.is_empty():
		var remove := Button.new(); remove.text = tr("ux.delete_construction")
		remove.pressed.connect(func(): _show_delete_menu({"kind": "construction"}, remove.get_global_position()))
		workspace.add_child(remove)

func _construction_source_selector(selected: Dictionary) -> OptionButton:
	var selector := OptionButton.new(); selector.add_item(tr("ux.none")); selector.set_item_metadata(0, {})
	for material in editor.get_materials():
		var source := {"kind": "material", "material_id": int(material.id)}
		selector.add_item(tr("ux.material_source") % str(material.name)); selector.set_item_metadata(selector.item_count - 1, source)
		if selected == source: selector.select(selector.item_count - 1)
	for characteristic in editor.get_object_characteristics():
		for kind in ["base", "function_sum"]:
			var source := {"kind": kind, "characteristic_id": int(characteristic.id)}
			selector.add_item((tr("ux.base_source") if kind == "base" else tr("ux.function_sum_source")) % str(characteristic.name)); selector.set_item_metadata(selector.item_count - 1, source)
			if selected == source: selector.select(selector.item_count - 1)
	return selector

func _characteristic_selector(selected_id: int, allow_none: bool) -> OptionButton:
	var selector := OptionButton.new()
	if allow_none: selector.add_item(tr("ux.none"), 0)
	for characteristic in editor.get_object_characteristics():
		selector.add_item(str(characteristic.name), int(characteristic.id))
		if int(characteristic.id) == selected_id: selector.select(selector.item_count - 1)
	return selector

func _find_construction_input(inputs: Array, input_id: int) -> Dictionary:
	for input in inputs:
		if int((input as Dictionary).get("input_id", 0)) == input_id:
			var item: Dictionary = input
			return {"kind": str(item.get("kind", "")), "material_id": int(item.get("material_id", 0))} if str(item.get("kind", "")) == "material" else {"kind": str(item.get("kind", "")), "characteristic_id": int(item.get("characteristic_id", 0))}
	return {}

func _construction_output_characteristic(outputs: Array, output_id: int) -> int:
	for output in outputs:
		if int((output as Dictionary).get("output_id", 0)) == output_id: return int((output as Dictionary).get("characteristic_id", 0))
	return 0

func _show_delete_menu(_context: Dictionary, position: Vector2) -> void:
	_request_delete(tr("ux.construction"), func():
		if not editor.remove_object_construction(): _show_error(); return
		_show_construction(), position)
