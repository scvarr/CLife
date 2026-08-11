extends Control

const WorldEditorSession = preload("res://scripts/world_editor_session.gd")
const GodotHostConfig = preload("res://scripts/godot_host_config.gd")
const UnitsPanel = preload("res://scripts/world_editor_units_panel.gd")
const ValuesPanel = preload("res://scripts/world_editor_values_panel.gd")
const ConversionsPanel = preload("res://scripts/world_editor_conversions_panel.gd")
const CalculationsPanel = preload("res://scripts/world_editor_calculations_panel.gd")
const FunctionsPanel = preload("res://scripts/world_editor_functions_panel.gd")
const CharacteristicsPanel = preload("res://scripts/world_editor_characteristics_panel.gd")
const ConstructionPanel = preload("res://scripts/world_editor_construction_panel.gd")
const ObjectsPanel = preload("res://scripts/world_editor_objects_panel.gd")
const WorldRulesPanel = preload("res://scripts/world_editor_world_rules_panel.gd")
const ExternalInputsPanel = preload("res://scripts/world_editor_external_inputs_panel.gd")
const WORLD_SAVE_PATH := "user://current_world.clife.json"

var editor := CLifeWorldEditor.new()
var host_config := GodotHostConfig.new()
var current_panel: WorldEditorPanel
var pending_delete: Callable
var pending_delete_name := ""
var pending_delete_kind := "generic"
var startup_status := ""
var context_menu := PopupMenu.new()
var delete_confirmation := ConfirmationDialog.new()
var units_panel := UnitsPanel.new()
var values_panel := ValuesPanel.new()
var conversions_panel := ConversionsPanel.new()
var calculations_panel := CalculationsPanel.new()
var functions_panel := FunctionsPanel.new()
var characteristics_panel := CharacteristicsPanel.new()
var construction_panel := ConstructionPanel.new()
var objects_panel := ObjectsPanel.new()
var world_rules_panel := WorldRulesPanel.new()
var external_inputs_panel := ExternalInputsPanel.new()
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
	$Layout/Sidebar/WorldRules.text = tr("ux.world_rules")
	$Layout/Sidebar/ExternalInputs.text = tr("ux.external_inputs")
	$Layout/Sidebar/Save.text = tr("ux.save_world")
	for panel in _all_panels(): panel.configure(self, editor, workspace, status, host_config)
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

func _all_panels() -> Array[WorldEditorPanel]:
	return [units_panel, values_panel, conversions_panel, calculations_panel, functions_panel, characteristics_panel, construction_panel, objects_panel, world_rules_panel, external_inputs_panel]

func _activate_panel(panel: WorldEditorPanel) -> void:
	if current_panel != panel:
		if current_panel != null: current_panel.deactivate()
		current_panel = panel
		current_panel.activate()
	current_panel.show()

func _show_units() -> void: _activate_panel(units_panel)
func _show_world_quantities() -> void: _activate_panel(values_panel)
func _show_conversions() -> void: _activate_panel(conversions_panel)
func _show_formulas() -> void: _activate_panel(calculations_panel)
func _show_functions() -> void: _activate_panel(functions_panel)
func _show_characteristics() -> void: _activate_panel(characteristics_panel)
func _show_construction() -> void: _activate_panel(construction_panel)
func _show_objects() -> void: _activate_panel(objects_panel)
func _show_world_rules() -> void: _activate_panel(world_rules_panel)
func _show_external_inputs() -> void: _activate_panel(external_inputs_panel)

func _request_delete(name: String, operation: Callable, position: Vector2, confirmation_kind := "generic") -> void:
	pending_delete_name = name
	pending_delete = operation
	pending_delete_kind = confirmation_kind
	context_menu.clear()
	context_menu.add_item(tr("ux.delete"), 1)
	context_menu.position = Vector2i(position)
	context_menu.popup()

func _on_context_menu_pressed(id: int) -> void:
	if id != 1 or pending_delete.is_null(): return
	delete_confirmation.dialog_text = (tr("ux.delete_unit_confirmation") if pending_delete_kind == "unit" else tr("ux.delete_value_confirmation") if pending_delete_kind == "value" else tr("ux.delete_confirmation")) % pending_delete_name
	delete_confirmation.ok_button_text = tr("ux.delete")
	delete_confirmation.cancel_button_text = tr("ux.cancel")
	delete_confirmation.popup_centered()

func _confirm_deletion() -> void:
	if pending_delete.is_null(): return
	var operation := pending_delete
	pending_delete = Callable()
	pending_delete_name = ""
	pending_delete_kind = "generic"
	operation.call()

func _discard_runtime_preview() -> void:
	objects_panel.deactivate()

func _value_unit_id(value: Dictionary) -> int:
	var unit: Dictionary = value.get("unit", {})
	return int(unit.get("id", 0))

func _value_unit_symbol(value: Dictionary) -> String:
	var unit_id := _value_unit_id(value)
	return _unit_symbol(unit_id) if unit_id != 0 else tr("ux.no_unit")

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
	return selector

func _selected_unit_id(selector: OptionButton) -> int:
	if selector.selected < 0: return 0
	return selector.get_item_id(selector.selected)

func _unit_symbol(unit_id: int) -> String:
	for unit in editor.get_units():
		if int(unit.id) == unit_id: return str(unit.symbol)
	return tr("ux.no_unit")

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
		selector.add_item(tr("ux.source_genome") % str(parameter.name))
		selector.set_item_metadata(selector.item_count - 1, source)
		if _source_matches(selected_source, source): selector.select(selector.item_count - 1)
	for binding in function_type.calculations:
		var calculation := _find_calculation(int(binding.calculation_id))
		for output in calculation.get("outputs", []):
			var source := {"kind": "calculation", "calculation_id": int(binding.calculation_id), "calculation_output_id": int(output.id)}
			selector.add_item(tr("ux.source_formula") % [str(calculation.name), str(output.name)])
			selector.set_item_metadata(selector.item_count - 1, source)
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

func _conversion_unit_symbol(components: Array) -> String:
	if components.size() == 1 and int((components[0] as Dictionary).get("exponent", 0)) == 1:
		return _unit_symbol(int((components[0] as Dictionary).get("id", 0)))
	return tr("ux.complex_unit")

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
	var host_error = host_config.save()
	if host_error != null:
		status.text = tr("status.host_config_save_failed") % host_error
		return
	status.text = tr("status.world_saved")

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
	var host_error = host_config.load(editor)
	if host_error != null:
		startup_status = tr("status.host_config_load_failed") % (tr("status.invalid_host_config") if host_error == "invalid_host_config" else str(host_error))

func _clear_workspace() -> void:
	for child in workspace.get_children():
		if child != status: child.queue_free()
	status.text = ""

func _labeled_row(label_text: String, control: Control) -> HBoxContainer:
	var row := HBoxContainer.new()
	var label := Label.new(); label.text = label_text; label.custom_minimum_size.x = 220
	row.add_child(label); row.add_child(control)
	return row

func _add_title_to(parent: VBoxContainer, text: String) -> void:
	var title := Label.new(); title.text = text; title.add_theme_font_size_override("font_size", 26); parent.add_child(title)

func _add_section(parent: VBoxContainer, title_text: String) -> void:
	var title := Label.new(); title.text = title_text; title.add_theme_font_size_override("font_size", 18); parent.add_child(title)

func _add_title(text: String) -> void:
	_add_title_to(workspace, text)

func _on_back() -> void:
	if current_panel != null: current_panel.deactivate()
	get_tree().change_scene_to_file("res://scenes/main_menu.tscn")
