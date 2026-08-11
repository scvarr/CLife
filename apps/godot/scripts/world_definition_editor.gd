extends Control

const WorldEditorSession = preload("res://scripts/world_editor_session.gd")
const ConstructionVolumePreview = preload("res://scenes/construction_volume_preview.tscn")
const WORLD_SAVE_PATH := "user://current_world.clife.json"
const GODOT_HOST_CONFIG_PATH := "user://current_world.godot.json"

var editor := CLifeWorldEditor.new()
var new_unit_row_active := false
var new_value_row_active := false
var new_conversion_row_active := false
var new_formula_active := false
var new_input_active := false
var new_output_active := false
var selected_calculation_id := 0
var selected_function_id := 0
var new_function_active := false
var new_parameter_active := false
var new_process_output_active := false
var new_function_material_active := false
var new_external_input_active := false
var external_inputs: Array[Dictionary] = []
var selected_template_id := 0
var preview_volume_characteristic_ids_by_template: Dictionary = {}
var new_object_active := false
var new_genome_function_active := false
var draft_genome_function_type_id := 0
var editing_genome_index := -1
var last_test_inputs: Array[Dictionary] = []
var last_runtime_values: Array[Dictionary] = []
var last_runtime_functions: Array[Dictionary] = []
var new_characteristic_active := false
var selected_construction_calculation_id := 0
var selected_formula_id_by_function: Dictionary = {}
var deletion_context: Dictionary = {}
var startup_status := ""
var context_menu := PopupMenu.new()
var delete_confirmation := ConfirmationDialog.new()
@onready var workspace: VBoxContainer = $Layout/Workspace/Margin/Content
@onready var status: Label = $Layout/Workspace/Margin/Content/Status

func _ready() -> void:
	$Layout/Sidebar/Back.text = tr("ux.back_to_menu")
	$Layout/Sidebar/Units.text = tr("ux.units")
	$Layout/Sidebar/WorldQuantities.text = tr("ux.world_quantities")
	$Layout/Sidebar/Conversions.text = tr("ux.conversions")
	$Layout/Sidebar/Formulas.text = tr("ux.formulas")
	$Layout/Sidebar/Functions.text = tr("ux.functions")
	$Layout/Sidebar/Characteristics.text = tr("ux.object_characteristics")
	$Layout/Sidebar/Construction.text = tr("ux.construction")
	$Layout/Sidebar/Objects.text = tr("ux.objects")
	$Layout/Sidebar/ExternalInputs.text = tr("ux.external_inputs")
	$Layout/Sidebar/Save.text = tr("ux.save_world")
	add_child(context_menu)
	context_menu.id_pressed.connect(_on_context_menu_pressed)
	add_child(delete_confirmation)
	delete_confirmation.confirmed.connect(_confirm_deletion)
	if WorldEditorSession.open_mode == WorldEditorSession.OpenMode.LOAD_CURRENT_WORLD:
		_load_current_world()
		_load_external_inputs()
	WorldEditorSession.open_mode = WorldEditorSession.OpenMode.NEW_WORLD
	_show_units()
	if not startup_status.is_empty(): status.text = startup_status

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

func _show_world_quantities() -> void:
	_discard_runtime_preview()
	_clear_workspace()
	_add_title(tr("ux.world_quantities"))
	var header := HBoxContainer.new()
	var name := Label.new(); name.text = tr("ux.name"); name.custom_minimum_size.x = 260
	var unit := Label.new(); unit.text = tr("ux.unit"); unit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(name); header.add_child(unit); workspace.add_child(header)
	for value in editor.get_values():
		_add_value_row(value)
	if new_value_row_active:
		_add_new_value_row()
	var add := Button.new(); add.text = "+ " + tr("ux.add_new"); add.disabled = new_value_row_active
	add.pressed.connect(func(): new_value_row_active = true; _show_world_quantities())
	workspace.add_child(add)

func _add_value_row(value: Dictionary) -> void:
	var row := HBoxContainer.new()
	var name := Label.new(); name.text = str(value.name); name.custom_minimum_size.x = 260
	var unit := Label.new(); unit.text = _value_unit_symbol(value); unit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(name); row.add_child(unit)
	row.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed:
			if (event as InputEventMouseButton).button_index == MOUSE_BUTTON_LEFT: _edit_value_row(row, value)
			elif (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT: _show_delete_menu({"kind": "value", "value": value}, row.get_global_position() + (event as InputEventMouseButton).position)
	)
	workspace.add_child(row)

func _edit_value_row(row: HBoxContainer, value: Dictionary) -> void:
	for child in row.get_children(): child.queue_free()
	var name := LineEdit.new(); name.text = str(value.name); name.custom_minimum_size.x = 260
	var unit := _unit_selector(_value_unit_id(value), true); unit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var complex_unit := _has_complex_value_unit(value)
	if complex_unit:
		unit.clear(); unit.add_item(tr("ux.complex_unit"), -1); unit.disabled = true
	var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		if not editor.rename_value(int(value.key), name.text): _show_error(); return
		var unit_id := _selected_unit_id(unit)
		var unit_saved := true
		if unit_id == 0: unit_saved = editor.clear_value_unit(int(value.key))
		elif unit_id > 0: unit_saved = editor.set_value_unit(int(value.key), unit_id)
		if not unit_saved: _show_error(); return
		_show_world_quantities()
	)
	cancel.pressed.connect(_show_world_quantities)
	row.add_child(name); row.add_child(unit); row.add_child(save); row.add_child(cancel)

func _add_new_value_row() -> void:
	var row := HBoxContainer.new()
	var name := LineEdit.new(); name.placeholder_text = tr("ux.name"); name.custom_minimum_size.x = 260
	var unit := _unit_selector(0, true); unit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		var key := editor.add_value(name.text)
		if key == 0: _show_error(); return
		var unit_id := _selected_unit_id(unit)
		if unit_id != 0 and not editor.set_value_unit(key, unit_id): _show_error(); return
		new_value_row_active = false; _show_world_quantities()
	)
	cancel.pressed.connect(func(): new_value_row_active = false; _show_world_quantities())
	row.add_child(name); row.add_child(unit); row.add_child(save); row.add_child(cancel); workspace.add_child(row)

func _value_unit_id(value: Dictionary) -> int:
	var components: Array = value.get("unit_components", [])
	if components.size() == 1 and int((components[0] as Dictionary).get("exponent", 0)) == 1:
		return int((components[0] as Dictionary).get("id", 0))
	return 0

func _value_unit_symbol(value: Dictionary) -> String:
	if _has_complex_value_unit(value): return tr("ux.complex_unit")
	var unit_id := _value_unit_id(value)
	if unit_id == 0: return tr("ux.no_unit")
	return _unit_symbol(unit_id)

func _has_complex_value_unit(value: Dictionary) -> bool:
	var components: Array = value.get("unit_components", [])
	return not components.is_empty() and _value_unit_id(value) == 0

func _unit_selector(selected_id: int, allow_none: bool) -> OptionButton:
	var selector := OptionButton.new()
	if allow_none:
		selector.add_item(tr("ux.no_unit"), 0)
	for unit in editor.get_units():
		selector.add_item(str(unit.symbol), int(unit.id))
		if int(unit.id) == selected_id: selector.select(selector.item_count - 1)
	if selector.item_count == 0:
		selector.disabled = true
	return selector

func _selected_unit_id(selector: OptionButton) -> int:
	if selector.selected < 0:
		return 0
	return selector.get_item_id(selector.selected)

func _unit_symbol(unit_id: int) -> String:
	for unit in editor.get_units():
		if int(unit.id) == unit_id: return str(unit.symbol)
	return tr("ux.no_unit")

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

func _show_external_inputs() -> void:
	_discard_runtime_preview()
	_clear_workspace()
	_add_title(tr("ux.external_inputs"))
	var header := HBoxContainer.new()
	var channel := Label.new(); channel.text = tr("ux.godot_channel"); channel.custom_minimum_size.x = 220
	var value := Label.new(); value.text = tr("ux.world_quantity"); value.custom_minimum_size.x = 220
	var test_value := Label.new(); test_value.text = tr("ux.test_value"); test_value.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(channel); header.add_child(value); header.add_child(test_value); workspace.add_child(header)
	for index in external_inputs.size():
		_add_external_input_row(index, external_inputs[index])
	if new_external_input_active:
		_add_new_external_input_row()
	var add := Button.new(); add.text = "+ " + tr("ux.add"); add.disabled = new_external_input_active
	add.pressed.connect(func(): new_external_input_active = true; _show_external_inputs())
	workspace.add_child(add)

func _input_capability_selector(selected_channel: String) -> OptionButton:
	var selector := OptionButton.new()
	for capability in editor.get_host_capabilities():
		if int(capability.direction_id) != 0:
			continue
		selector.add_item("%s — %s" % [tr(str(capability.display_key)), str(capability.channel)])
		selector.set_item_metadata(selector.item_count - 1, str(capability.channel))
		if str(capability.channel) == selected_channel:
			selector.select(selector.item_count - 1)
	return selector

func _selected_capability_channel(selector: OptionButton) -> String:
	if selector.selected < 0:
		return ""
	return str(selector.get_item_metadata(selector.selected))

func _add_external_input_row(index: int, mapping: Dictionary) -> void:
	var row := HBoxContainer.new()
	var channel := Label.new(); channel.text = str(mapping.get("channel", "")); channel.custom_minimum_size.x = 220
	var value := Label.new(); value.text = _value_name(int(mapping.get("value_key", 0))); value.custom_minimum_size.x = 220
	var test_value := Label.new(); test_value.text = str(mapping.get("test_value", 1.0)); test_value.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(channel); row.add_child(value); row.add_child(test_value)
	row.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed:
			if (event as InputEventMouseButton).button_index == MOUSE_BUTTON_LEFT:
				_edit_external_input_row(row, index, mapping)
			elif (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT:
				_show_delete_menu({"kind": "external_input", "index": index, "mapping": mapping}, row.get_global_position() + (event as InputEventMouseButton).position)
	)
	workspace.add_child(row)

func _edit_external_input_row(row: HBoxContainer, index: int, mapping: Dictionary) -> void:
	for child in row.get_children(): child.queue_free()
	var channel := _input_capability_selector(str(mapping.get("channel", ""))); channel.custom_minimum_size.x = 220
	var value := _value_selector(int(mapping.get("value_key", 0))); value.custom_minimum_size.x = 220
	var test_value := SpinBox.new(); test_value.value = float(mapping.get("test_value", 1.0)); test_value.step = 0.1; test_value.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		var updated := {"channel": _selected_capability_channel(channel), "value_key": _selected_unit_id(value), "test_value": test_value.value}
		if not _validate_external_input(updated, index): return
		external_inputs[index] = updated; _show_external_inputs()
	)
	cancel.pressed.connect(_show_external_inputs)
	row.add_child(channel); row.add_child(value); row.add_child(test_value); row.add_child(save); row.add_child(cancel)

func _add_new_external_input_row() -> void:
	var row := HBoxContainer.new()
	var channel := _input_capability_selector(""); channel.custom_minimum_size.x = 220
	var value := _value_selector(0); value.custom_minimum_size.x = 220
	var test_value := SpinBox.new(); test_value.value = 1.0; test_value.step = 0.1; test_value.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		var mapping := {"channel": _selected_capability_channel(channel), "value_key": _selected_unit_id(value), "test_value": test_value.value}
		if not _validate_external_input(mapping): return
		external_inputs.append(mapping); new_external_input_active = false; _show_external_inputs()
	)
	cancel.pressed.connect(func(): new_external_input_active = false; _show_external_inputs())
	row.add_child(channel); row.add_child(value); row.add_child(test_value); row.add_child(save); row.add_child(cancel); workspace.add_child(row)

func _validate_external_input(mapping: Dictionary, ignored_index: int = -1) -> bool:
	var channel := str(mapping.get("channel", ""))
	var value_key := int(mapping.get("value_key", 0))
	if channel.is_empty() or value_key == 0 or not _is_input_capability(channel) or not _has_value_key(value_key):
		status.text = tr("status.external_input_invalid")
		return false
	for index in external_inputs.size():
		if index != ignored_index and str(external_inputs[index].get("channel", "")) == channel:
			status.text = tr("status.external_input_channel_duplicate") % channel
			return false
	return true

func _is_input_capability(channel: String) -> bool:
	for capability in editor.get_host_capabilities():
		if int(capability.direction_id) == 0 and str(capability.channel) == channel:
			return true
	return false

func _has_value_key(value_key: int) -> bool:
	for value in editor.get_values():
		if int(value.key) == value_key:
			return true
	return false

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
			if str(source.kind) == "material": item["value_key"] = int(source.value_key)
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
	for value in editor.get_values():
		var source := {"kind": "material", "value_key": int(value.key)}
		selector.add_item(tr("ux.material_source") % str(value.name)); selector.set_item_metadata(selector.item_count - 1, source)
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
			return {"kind": str(item.get("kind", "")), "value_key": int(item.get("value_key", 0))} if str(item.get("kind", "")) == "material" else {"kind": str(item.get("kind", "")), "characteristic_id": int(item.get("characteristic_id", 0))}
	return {}

func _construction_output_characteristic(outputs: Array, output_id: int) -> int:
	for output in outputs:
		if int((output as Dictionary).get("output_id", 0)) == output_id: return int((output as Dictionary).get("characteristic_id", 0))
	return 0

func _show_objects() -> void:
	_clear_workspace()
	var split := HBoxContainer.new(); split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var list_panel := VBoxContainer.new(); list_panel.custom_minimum_size.x = 280
	var title := Label.new(); title.text = tr("ux.objects"); title.add_theme_font_size_override("font_size", 22); list_panel.add_child(title)
	var templates := editor.get_templates()
	if selected_template_id != 0 and _find_template(selected_template_id).is_empty(): selected_template_id = 0
	if selected_template_id == 0 and not templates.is_empty(): selected_template_id = int(templates[0].id)
	for template in templates:
		_add_object_list_item(list_panel, template)
	if new_object_active: _add_new_object_row(list_panel)
	var add := Button.new(); add.text = "+ " + tr("ux.add_new"); add.disabled = new_object_active
	add.pressed.connect(func(): new_object_active = true; _show_objects())
	list_panel.add_child(add)
	var editor_scroll := ScrollContainer.new(); editor_scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL; editor_scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var editor_panel := VBoxContainer.new(); editor_panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL; editor_scroll.add_child(editor_panel)
	if selected_template_id == 0:
		var hint := Label.new(); hint.text = tr("ux.create_first_object"); editor_panel.add_child(hint)
	else:
		_build_object_editor(editor_panel, _find_template(selected_template_id))
	split.add_child(list_panel); split.add_child(editor_scroll); workspace.add_child(split)

func _add_object_list_item(parent: VBoxContainer, template: Dictionary) -> void:
	var button := Button.new(); button.text = str(template.name); button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.button_pressed = int(template.id) == selected_template_id
	button.pressed.connect(func(): selected_template_id = int(template.id); _show_objects())
	button.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed and (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT:
			_show_delete_menu({"kind": "template", "template": template}, button.get_global_position() + (event as InputEventMouseButton).position)
	)
	parent.add_child(button)

func _add_new_object_row(parent: VBoxContainer) -> void:
	var name := LineEdit.new(); name.placeholder_text = tr("ux.object_name"); parent.add_child(name)
	var row := HBoxContainer.new(); var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		var id := editor.add_template(name.text)
		if id == 0: _show_error(); return
		selected_template_id = id; new_object_active = false; _show_objects()
	)
	cancel.pressed.connect(func(): new_object_active = false; _show_objects())
	row.add_child(save); row.add_child(cancel); parent.add_child(row)

func _build_object_editor(parent: VBoxContainer, template: Dictionary) -> void:
	_add_title_to(parent, tr("ux.object"))
	var name_row := HBoxContainer.new(); var name := LineEdit.new(); name.text = str(template.name); name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var save_name := Button.new(); save_name.text = tr("ux.save")
	save_name.pressed.connect(func():
		if not editor.rename_template(int(template.id), name.text): _show_error(); return
		_show_objects()
	)
	name_row.add_child(name); name_row.add_child(save_name); parent.add_child(_labeled_row(tr("ux.name"), name_row))
	_add_section(parent, tr("ux.genome"))
	for entry in editor.get_genome(int(template.id)):
		_add_genome_entry_card(parent, template, entry)
	if new_genome_function_active:
		_add_new_genome_function_row(parent, template)
	var add := Button.new(); add.text = "+ " + tr("ux.add_function"); add.disabled = new_genome_function_active
	add.pressed.connect(func(): new_genome_function_active = true; draft_genome_function_type_id = 0; _show_objects())
	parent.add_child(add)
	_add_object_construction_preview(parent, template)
	_add_stateful_runtime_preview(parent, template)

func _add_object_construction_preview(parent: VBoxContainer, template: Dictionary) -> void:
	_add_section(parent, tr("ux.object_construction_preview"))
	var materials := editor.get_material_contributions(int(template.id))
	if materials.is_empty() and not editor.get_last_error().is_empty():
		var unavailable := Label.new(); unavailable.text = editor.get_last_error(); parent.add_child(unavailable); return
	var materials_title := Label.new(); materials_title.text = tr("ux.materials"); parent.add_child(materials_title)
	for material in materials:
		var item := Label.new(); item.text = "%s = %s" % [_value_name(int(material.value_key)), str(material.amount)]; parent.add_child(item)
	var preview := editor.get_template_characteristic_preview(int(template.id))
	if preview.is_empty() and not editor.get_last_error().is_empty():
		var unavailable := Label.new(); unavailable.text = editor.get_last_error(); parent.add_child(unavailable); return
	var characteristics: Array = preview.get("characteristics", [])
	var characteristics_title := Label.new(); characteristics_title.text = tr("ux.object_characteristics"); parent.add_child(characteristics_title)
	for characteristic in characteristics:
		var item := Label.new(); item.text = "%s = %s" % [_characteristic_name(int((characteristic as Dictionary).get("characteristic_id", 0))), str((characteristic as Dictionary).get("amount", 0.0))]; parent.add_child(item)
	_add_construction_volume_preview(parent, int(template.id), characteristics)

func _add_construction_volume_preview(parent: VBoxContainer, template_id: int, characteristics: Array) -> void:
	_add_section(parent, tr("ux.construction_volume_preview"))
	if characteristics.is_empty():
		var empty := Label.new(); empty.text = tr("ux.volume_preview_no_characteristics"); parent.add_child(empty)
		return
	var selected_id := int(preview_volume_characteristic_ids_by_template.get(template_id, 0))
	var selected_characteristic: Dictionary = {}
	for characteristic in characteristics:
		if int((characteristic as Dictionary).get("characteristic_id", 0)) == selected_id:
			selected_characteristic = characteristic
			break
	if characteristics.size() == 1:
		selected_characteristic = characteristics[0]
		selected_id = int(selected_characteristic.get("characteristic_id", 0))
		preview_volume_characteristic_ids_by_template[template_id] = selected_id
	var selector := OptionButton.new(); selector.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	if characteristics.size() > 1:
		selector.add_item(tr("ux.select_characteristic"), 0)
	for characteristic in characteristics:
		var characteristic_id := int((characteristic as Dictionary).get("characteristic_id", 0))
		selector.add_item(_characteristic_name(characteristic_id), characteristic_id)
		if characteristic_id == selected_id:
			selector.select(selector.item_count - 1)
	selector.item_selected.connect(func(index: int):
		var characteristic_id := selector.get_item_id(index)
		if characteristic_id == 0: preview_volume_characteristic_ids_by_template.erase(template_id)
		else: preview_volume_characteristic_ids_by_template[template_id] = characteristic_id
		_show_objects()
	)
	parent.add_child(_labeled_row(tr("ux.volume_for_preview"), selector))
	var volume_preview = ConstructionVolumePreview.instantiate()
	parent.add_child(volume_preview)
	if selected_characteristic.is_empty():
		volume_preview.show_unavailable(tr("ux.volume_preview_select_characteristic"))
		return
	var volume := float(selected_characteristic.get("amount", 0.0))
	if not is_finite(volume) or volume <= 0.0:
		volume_preview.show_unavailable(tr("ux.volume_preview_invalid"))
		return
	var radii := editor.sample_template_shape(template_id, volume_preview.tessellation_directions())
	if radii.is_empty() and not editor.get_last_error().is_empty():
		volume_preview.show_unavailable(editor.get_last_error())
		return
	volume_preview.show_shape(volume, radii, tr("ux.volume_preview_value") % str(volume))

func _semantic_genome_entry(entry: Dictionary) -> String:
	var parts := ["%02X" % int(entry.get("function_type_id", 0))]
	for parameter in entry.get("genome_parameters", []):
		parts.append(str(float((parameter as Dictionary).get("amount", 0.0))))
	return " | ".join(parts)

func _add_genome_entry_card(parent: VBoxContainer, template: Dictionary, entry: Dictionary) -> void:
	var index := int(entry.get("index", -1))
	if editing_genome_index == index:
		_add_genome_entry_editor(parent, template, entry)
		return
	var card := PanelContainer.new(); var label := Label.new(); label.text = _semantic_genome_entry(entry); card.add_child(label)
	card.gui_input.connect(func(event: InputEvent) -> void:
		if event is InputEventMouseButton and (event as InputEventMouseButton).pressed:
			if (event as InputEventMouseButton).button_index == MOUSE_BUTTON_LEFT:
				editing_genome_index = index; _show_objects()
			elif (event as InputEventMouseButton).button_index == MOUSE_BUTTON_RIGHT:
				_show_delete_menu({"kind": "genome_entry", "template_id": int(template.id), "entry": entry}, card.get_global_position() + (event as InputEventMouseButton).position)
	)
	parent.add_child(card)

func _add_genome_entry_editor(parent: VBoxContainer, template: Dictionary, entry: Dictionary) -> void:
	var card := PanelContainer.new(); var box := VBoxContainer.new(); card.add_child(box)
	var function_name := Label.new(); function_name.text = str(entry.get("function_type_name", "")); box.add_child(_labeled_row(tr("ux.function"), function_name))
	var fields := []
	for parameter in entry.get("genome_parameters", []):
		var spin := SpinBox.new(); spin.step = 0.1; spin.value = float((parameter as Dictionary).get("amount", 0.0)); spin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		box.add_child(_labeled_row(str((parameter as Dictionary).get("name", "")), spin))
		fields.append({"parameter_id": int((parameter as Dictionary).get("parameter_id", 0)), "spin": spin})
	var row := HBoxContainer.new(); var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		for field in fields:
			if not editor.set_genome_parameter(int(template.id), int(entry.index), int(field.parameter_id), field.spin.value): _show_error(); return
		editing_genome_index = -1; _show_objects()
	)
	cancel.pressed.connect(func(): editing_genome_index = -1; _show_objects())
	row.add_child(save); row.add_child(cancel); box.add_child(row); parent.add_child(card)

func _add_new_genome_function_row(parent: VBoxContainer, template: Dictionary) -> void:
	var functions := editor.get_function_types()
	if functions.is_empty():
		var hint := Label.new(); hint.text = tr("ux.create_first_function"); parent.add_child(hint); return
	if draft_genome_function_type_id == 0: draft_genome_function_type_id = int(functions[0].id)
	var selector := OptionButton.new()
	for function_type in functions:
		selector.add_item(str(function_type.name), int(function_type.id))
		if int(function_type.id) == draft_genome_function_type_id: selector.select(selector.item_count - 1)
	selector.item_selected.connect(func(index: int): draft_genome_function_type_id = selector.get_item_id(index); _show_objects())
	parent.add_child(_labeled_row(tr("ux.function"), selector))
	var function_type := _find_function(draft_genome_function_type_id)
	var fields := []
	for parameter in function_type.get("genome_parameters", []):
		var spin := SpinBox.new(); spin.step = 0.1; spin.value = float((parameter as Dictionary).get("default_value", 0.0)); spin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		parent.add_child(_labeled_row(str((parameter as Dictionary).get("name", "")), spin))
		fields.append({"parameter_id": int((parameter as Dictionary).get("id", 0)), "default_value": float((parameter as Dictionary).get("default_value", 0.0)), "spin": spin})
	var row := HBoxContainer.new(); var save := Button.new(); save.text = tr("ux.save"); var cancel := Button.new(); cancel.text = tr("ux.cancel")
	save.pressed.connect(func():
		if not editor.add_genome_function(int(template.id), draft_genome_function_type_id): _show_error(); return
		var genome := editor.get_genome(int(template.id)); var index := genome.size() - 1
		for field in fields:
			if field.spin.value != field.default_value and not editor.set_genome_parameter(int(template.id), index, int(field.parameter_id), field.spin.value):
				editor.remove_genome_function(int(template.id), index); _show_error(); return
		new_genome_function_active = false; draft_genome_function_type_id = 0; _show_objects()
	)
	cancel.pressed.connect(func(): new_genome_function_active = false; draft_genome_function_type_id = 0; _show_objects())
	row.add_child(save); row.add_child(cancel); parent.add_child(row)

func _add_stateful_runtime_preview(parent: VBoxContainer, template: Dictionary) -> void:
	_add_section(parent, tr("ux.runtime_preview"))
	for mapping in external_inputs:
		var input := Label.new(); input.text = "%s -> %s = %s" % [str(mapping.get("channel", "")), _value_name(int(mapping.get("value_key", 0))), str(mapping.get("test_value", 0.0))]; parent.add_child(input)
	var active := editor.is_run_active()
	var tick := Label.new(); tick.text = tr("ux.runtime_tick") % editor.get_tick(); parent.add_child(tick)
	var controls := HBoxContainer.new()
	var start := Button.new(); start.text = tr("ux.runtime_start"); start.disabled = active
	start.pressed.connect(func(): _start_runtime_preview(int(template.id)))
	var step := Button.new(); step.text = tr("ux.runtime_step"); step.disabled = not active
	step.pressed.connect(_step_runtime_preview)
	var reset := Button.new(); reset.text = tr("ux.runtime_reset"); reset.disabled = not active
	reset.pressed.connect(_reset_runtime_preview)
	var stop := Button.new(); stop.text = tr("ux.runtime_stop"); stop.disabled = not active
	stop.pressed.connect(_stop_runtime_preview)
	controls.add_child(start); controls.add_child(step); controls.add_child(reset); controls.add_child(stop); parent.add_child(controls)
	if not active:
		return
	var inputs_title := Label.new(); inputs_title.text = tr("ux.sent_inputs"); parent.add_child(inputs_title)
	for input in last_test_inputs:
		var input_label := Label.new(); input_label.text = "%s -> %s = %s" % [str(input.channel), _value_name(int(input.value_key)), str(input.amount)]; parent.add_child(input_label)
	var values_title := Label.new(); values_title.text = tr("ux.runtime_values"); parent.add_child(values_title)
	for value in last_runtime_values:
		var value_label := Label.new(); value_label.text = "%s = %s" % [str(value.name), str(value.amount)]; parent.add_child(value_label)
	var buffers_title := Label.new(); buffers_title.text = tr("ux.runtime_buffers"); parent.add_child(buffers_title)
	var has_buffers := false
	for function in last_runtime_functions:
		var buffer: Dictionary = (function as Dictionary).get("buffer", {})
		if buffer.is_empty(): continue
		has_buffers = true
		var function_title := Label.new(); function_title.text = str((function as Dictionary).get("function_type_name", "")); parent.add_child(function_title)
		for detail in [["ux.buffer_stored", "stored_amount"], ["ux.buffer_received", "received_last_tick"], ["ux.buffer_supplied", "supplied_last_tick"]]:
			var item := Label.new(); item.text = "%s = %s" % [tr(str(detail[0])), str(buffer.get(str(detail[1]), 0.0))]; parent.add_child(item)
	if not has_buffers:
		var empty := Label.new(); empty.text = tr("ux.runtime_no_buffers"); parent.add_child(empty)

func _start_runtime_preview(template_id: int) -> void:
	last_test_inputs.clear(); last_runtime_values.clear(); last_runtime_functions.clear()
	if not editor.select_template(template_id): _show_error(); return
	if not editor.run(): _show_error(); return
	_refresh_runtime_preview(); _show_objects()

func _step_runtime_preview() -> void:
	if not editor.is_run_active(): return
	last_test_inputs.clear()
	for mapping in external_inputs:
		if not editor.set_preview_input(int(mapping.get("value_key", 0)), float(mapping.get("test_value", 0.0))):
			status.text = editor.get_last_error(); return
		last_test_inputs.append({"channel": str(mapping.get("channel", "")), "value_key": int(mapping.get("value_key", 0)), "amount": float(mapping.get("test_value", 0.0))})
	if not editor.step_once():
		status.text = editor.get_last_error(); return
	_refresh_runtime_preview(); _show_objects()

func _reset_runtime_preview() -> void:
	if not editor.reset_runtime(): _show_error(); return
	last_test_inputs.clear(); _refresh_runtime_preview(); _show_objects()

func _stop_runtime_preview() -> void:
	_discard_runtime_preview()
	if selected_template_id != 0: _show_objects()

func _discard_runtime_preview() -> void:
	if editor.is_run_active(): editor.stop()
	last_test_inputs.clear(); last_runtime_values.clear(); last_runtime_functions.clear()

func _refresh_runtime_preview() -> void:
	last_runtime_values.assign(editor.get_runtime_values())
	last_runtime_functions.assign(editor.get_runtime_functions())
	var read_error := editor.get_last_error()
	if not read_error.is_empty(): status.text = read_error

func _find_template(id: int) -> Dictionary:
	for template in editor.get_templates():
		if int(template.id) == id: return template
	return {}

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
	_build_function_process(parent, function_type)
	_build_function_buffer(parent, function_type)
	_build_function_materials(parent, function_type)

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

func _add_section(parent: VBoxContainer, title_text: String) -> void:
	var title := Label.new(); title.text = title_text; title.add_theme_font_size_override("font_size", 18); parent.add_child(title)

func _show_delete_menu(context: Dictionary, position: Vector2) -> void:
	deletion_context = context; context_menu.clear(); context_menu.add_item(tr("ux.delete"), 1); context_menu.position = Vector2i(position); context_menu.popup()

func _on_context_menu_pressed(id: int) -> void:
	if id != 1 or deletion_context.is_empty(): return
	var name := ""
	match str(deletion_context.get("kind", "")):
		"unit": name = str((deletion_context.get("unit", {}) as Dictionary).get("symbol", ""))
		"value": name = str((deletion_context.get("value", {}) as Dictionary).get("name", ""))
		"conversion": name = tr("ux.conversion")
		"function": name = str((deletion_context.get("function", {}) as Dictionary).get("name", ""))
		"characteristic": name = str((deletion_context.get("characteristic", {}) as Dictionary).get("name", ""))
		"construction": name = tr("ux.construction")
		"template": name = str((deletion_context.get("template", {}) as Dictionary).get("name", ""))
		"parameter": name = str((deletion_context.get("parameter", {}) as Dictionary).get("name", ""))
		"process_output": name = _value_name(int((deletion_context.get("output", {}) as Dictionary).get("output_key", 0)))
		"function_material": name = _value_name(int((deletion_context.get("contribution", {}) as Dictionary).get("value_key", 0)))
		"formula": name = str((deletion_context.get("calculation", {}) as Dictionary).get("name", ""))
		"external_input": name = str((deletion_context.get("mapping", {}) as Dictionary).get("channel", ""))
		"genome_entry": name = _semantic_genome_entry(deletion_context.get("entry", {}) as Dictionary)
		_: name = str((deletion_context.get("port", {}) as Dictionary).get("name", ""))
	var kind := str(deletion_context.get("kind", ""))
	delete_confirmation.dialog_text = (tr("ux.delete_unit_confirmation") if kind == "unit" else tr("ux.delete_value_confirmation") if kind == "value" else tr("ux.delete_confirmation")) % name
	delete_confirmation.ok_button_text = tr("ux.delete"); delete_confirmation.cancel_button_text = tr("ux.cancel"); delete_confirmation.popup_centered()

func _confirm_deletion() -> void:
	if deletion_context.is_empty(): return
	var context := deletion_context; deletion_context = {}; var removed := false
	match str(context.get("kind", "")):
		"unit": removed = editor.remove_unit(int((context.get("unit", {}) as Dictionary).get("id", 0)))
		"value": removed = editor.remove_value(int((context.get("value", {}) as Dictionary).get("key", 0)))
		"conversion": removed = editor.remove_unit_conversion(int((context.get("conversion", {}) as Dictionary).get("id", 0)))
		"function":
			removed = editor.remove_function_type(int((context.get("function", {}) as Dictionary).get("id", 0)))
			if removed and selected_function_id == int((context.get("function", {}) as Dictionary).get("id", 0)): selected_function_id = 0
		"characteristic": removed = editor.remove_object_characteristic(int((context.get("characteristic", {}) as Dictionary).get("id", 0)))
		"construction": removed = editor.remove_object_construction()
		"template":
			removed = editor.remove_template(int((context.get("template", {}) as Dictionary).get("id", 0)))
			if removed and selected_template_id == int((context.get("template", {}) as Dictionary).get("id", 0)): selected_template_id = 0
		"parameter": removed = editor.remove_genome_parameter(int(context.get("function_id", 0)), int((context.get("parameter", {}) as Dictionary).get("id", 0)))
		"process_output": removed = editor.remove_function_process_output(int(context.get("function_id", 0)), int((context.get("output", {}) as Dictionary).get("output_key", 0)))
		"function_material": removed = editor.remove_function_material_contribution(int(context.get("function_id", 0)), int((context.get("contribution", {}) as Dictionary).get("value_key", 0)))
		"formula":
			removed = editor.remove_calculation(int((context.get("calculation", {}) as Dictionary).get("id", 0)))
			if removed and selected_calculation_id == int((context.get("calculation", {}) as Dictionary).get("id", 0)): selected_calculation_id = 0
		"input": removed = editor.remove_calculation_input(int(context.get("calculation_id", 0)), int((context.get("port", {}) as Dictionary).get("id", 0)))
		"output": removed = editor.remove_calculation_output(int(context.get("calculation_id", 0)), int((context.get("port", {}) as Dictionary).get("id", 0)))
		"external_input":
			var index := int(context.get("index", -1))
			if index >= 0 and index < external_inputs.size():
				external_inputs.remove_at(index); removed = true
		"genome_entry": removed = editor.remove_genome_function(int(context.get("template_id", 0)), int((context.get("entry", {}) as Dictionary).get("index", -1)))
	if not removed: _show_error(); return
	if str(context.get("kind", "")) == "unit": _show_units()
	elif str(context.get("kind", "")) == "value": _show_world_quantities()
	elif str(context.get("kind", "")) == "conversion": _show_conversions()
	elif str(context.get("kind", "")) in ["function", "parameter", "process_output"]: _show_functions()
	elif str(context.get("kind", "")) == "function_material": _show_functions()
	elif str(context.get("kind", "")) == "characteristic": _show_characteristics()
	elif str(context.get("kind", "")) == "construction": _show_construction()
	elif str(context.get("kind", "")) in ["template", "genome_entry"]: _show_objects()
	elif str(context.get("kind", "")) == "external_input": _show_external_inputs()
	else: _show_formulas()

func _find_calculation(id: int) -> Dictionary:
	for calculation in editor.get_calculations():
		if int(calculation.id) == id: return calculation
	return {}

func _show_error() -> void:
	status.text = editor.get_last_error()

func _value_name(key: int) -> String:
	for value in editor.get_values():
		if int(value.key) == key: return str(value.name)
	return tr("ux.unknown")

func _characteristic_name(id: int) -> String:
	for characteristic in editor.get_object_characteristics():
		if int(characteristic.id) == id: return str(characteristic.name)
	return tr("ux.unknown")

func _save_current_world() -> void:
	var file := FileAccess.open(WORLD_SAVE_PATH, FileAccess.WRITE)
	if file == null:
		status.text = tr("status.world_save_failed") % FileAccess.get_open_error()
		return
	file.store_string(JSON.stringify(editor.export_world_snapshot(), "\t"))
	file.close()
	if not _save_external_inputs():
		return
	status.text = tr("status.world_saved")

func _save_external_inputs() -> bool:
	var file := FileAccess.open(GODOT_HOST_CONFIG_PATH, FileAccess.WRITE)
	if file == null:
		status.text = tr("status.host_config_save_failed") % FileAccess.get_open_error()
		return false
	file.store_string(JSON.stringify({"external_inputs": external_inputs}, "\t"))
	file.close()
	return true

func _load_current_world() -> void:
	_discard_runtime_preview()
	if not FileAccess.file_exists(WORLD_SAVE_PATH):
		startup_status = tr("menu.saved_world_not_found")
		return
	var file := FileAccess.open(WORLD_SAVE_PATH, FileAccess.READ)
	if file == null:
		startup_status = tr("status.world_load_failed") % FileAccess.get_open_error()
		return
	var json := JSON.new()
	if json.parse(file.get_as_text()) != OK or not (json.data is Dictionary):
		startup_status = tr("status.world_load_failed") % json.get_error_message()
		return
	if not editor.import_world_snapshot(json.data):
		startup_status = editor.get_last_error()
		return
	startup_status = tr("status.world_loaded")

func _load_external_inputs() -> void:
	external_inputs.clear()
	if not FileAccess.file_exists(GODOT_HOST_CONFIG_PATH):
		return
	var file := FileAccess.open(GODOT_HOST_CONFIG_PATH, FileAccess.READ)
	if file == null:
		startup_status = tr("status.host_config_load_failed") % FileAccess.get_open_error()
		return
	var json := JSON.new()
	if json.parse(file.get_as_text()) != OK or not (json.data is Dictionary):
		startup_status = tr("status.host_config_load_failed") % json.get_error_message()
		return
	var saved_inputs = (json.data as Dictionary).get("external_inputs", [])
	if not (saved_inputs is Array):
		startup_status = tr("status.host_config_load_failed") % tr("status.invalid_host_config")
		return
	for entry in saved_inputs:
		if not (entry is Dictionary):
			startup_status = tr("status.host_config_load_failed") % tr("status.invalid_host_config")
			external_inputs.clear()
			return
		var mapping: Dictionary = entry
		if not _validate_external_input(mapping):
			startup_status = tr("status.host_config_load_failed") % tr("status.invalid_host_config")
			external_inputs.clear()
			return
		external_inputs.append({"channel": str(mapping.get("channel", "")), "value_key": int(mapping.get("value_key", 0)), "test_value": float(mapping.get("test_value", 1.0))})

func _clear_workspace() -> void:
	for child in workspace.get_children():
		if child != status: child.queue_free()
	status.text = ""

func _add_title(text: String) -> void:
	var title := Label.new(); title.text = text; title.add_theme_font_size_override("font_size", 26); workspace.add_child(title)

func _on_back() -> void:
	_discard_runtime_preview()
	get_tree().change_scene_to_file("res://scenes/main_menu.tscn")
