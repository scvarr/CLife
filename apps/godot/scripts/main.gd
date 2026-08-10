extends Node3D

const INPUT_DIRECTION := 0
const OUTPUT_DIRECTION := 1
const GEOMETRY_VOLUME_CHANNEL := "geometry.volume"
const WORLD_SAVE_PATH := "user://current_world.clife.json"
const DEFAULT_LOCALE := "ru"
const SETTINGS_PATH := "user://settings.cfg"
const TRANSLATION_PATHS := [
	"res://translations/clife_editor.en.po",
	"res://translations/clife_editor.ru.po",
]
const FUNCTION_LIBRARY_SCENE := preload("res://scenes/function_library.tscn")

var editor: CLifeWorldEditor
var object_views: Dictionary = {}
var selected_kind := ""
var selected_identity := -1
var runtime_object_selected := false

var world_tree: Tree
var world_tree_menu: PopupMenu
var context_target_kind := ""
var context_calculation_id := -1
var context_port_id := -1
var inspector: VBoxContainer
var mode_label: Label
var tick_label: Label
var play_button: Button
var pause_button: Button
var step_button: Button
var reset_button: Button
var back_to_editor_button: Button
var save_world_button: Button
var new_name: LineEdit
var add_value_button: Button
var add_unit_button: Button
var add_template_button: Button
var add_calculation_button: Button
var add_rule_button: Button
var host_inputs_box: HBoxContainer
var status_label: Label
var runtime_value_labels: Dictionary = {}
var runtime_buffer_labels: Dictionary = {}
var runtime_end_labels: Dictionary = {}
var current_locale := DEFAULT_LOCALE
var status_key := "status.ready"
var status_arguments: Array = []
var status_error := ""
var workspace_host: Control
var function_library
var function_library_button: Button
var active_workspace := "world"
var function_library_selection := -1
var function_library_tabs: Dictionary = {}


func _ready() -> void:
	_load_translations()
	current_locale = _load_locale_preference()
	TranslationServer.set_locale(current_locale)
	editor = CLifeWorldEditor.new()
	_load_saved_world()
	_refresh_preview_visibility()
	_build_editor_ui()
	_show_world_workspace()
	_rebuild_host_inputs()
	_refresh_mode()
	_show_welcome_inspector()
	_refresh_status()


func _process(delta: float) -> void:
	editor.advance_time(delta)
	_apply_runtime_to_views()
	tick_label.text = tr("ui.tick_format") % editor.get_tick()
	if runtime_object_selected:
		_refresh_runtime_values()
	_show_facade_error_if_any()


func _load_translations() -> void:
	for path in TRANSLATION_PATHS:
		var translation := load(path) as Translation
		if translation != null:
			TranslationServer.add_translation(translation)


func _load_locale_preference() -> String:
	var config := ConfigFile.new()
	if config.load(SETTINGS_PATH) != OK:
		return DEFAULT_LOCALE
	var locale := str(config.get_value("ui", "locale", DEFAULT_LOCALE))
	return locale if locale == "ru" or locale == "en" else DEFAULT_LOCALE


func _save_locale_preference() -> void:
	var config := ConfigFile.new()
	config.set_value("ui", "locale", current_locale)
	var error := config.save(SETTINGS_PATH)
	if error != OK:
		_set_status("status.locale_save_failed", [error])


func _on_locale_selected(index: int, selector: OptionButton) -> void:
	if index < 0:
		return
	current_locale = str(selector.get_item_metadata(index))
	TranslationServer.set_locale(current_locale)
	_save_locale_preference()
	_rebuild_localized_ui()


func _load_saved_world() -> void:
	if not FileAccess.file_exists(WORLD_SAVE_PATH):
		return
	var file := FileAccess.open(WORLD_SAVE_PATH, FileAccess.READ)
	if file == null:
		_set_status_error("could not open saved world")
		return
	var json := JSON.new()
	if json.parse(file.get_as_text()) != OK:
		_set_status_error("could not parse saved world: %s" % json.get_error_message())
		return
	if not (json.data is Dictionary):
		_set_status_error("saved world root must be an object")
		return
	if not editor.import_world_snapshot(json.data):
		_set_status_error(editor.get_last_error())
		return
	_set_status("status.world_loaded")


func _on_save_world() -> void:
	if editor.is_run_active():
		return
	var file := FileAccess.open(WORLD_SAVE_PATH, FileAccess.WRITE)
	if file == null:
		_set_status("status.world_save_failed", [FileAccess.get_open_error()])
		return
	file.store_string(JSON.stringify(editor.export_world_snapshot(), "\t"))
	file.close()
	_set_status("status.world_saved")


func _rebuild_localized_ui() -> void:
	_clear_children($UI/Root)
	_build_editor_ui()
	if active_workspace == "functions":
		_show_function_library(function_library_selection)
	else:
		_show_world_workspace()
	_rebuild_host_inputs()
	_refresh_mode()
	if editor.is_run_active():
		if runtime_object_selected:
			_show_runtime_inspector()
		else:
			_show_run_inspector_help()
	elif active_workspace != "functions":
		_restore_edit_inspector()
	_refresh_status()


func _input(event: InputEvent) -> void:
	if not editor.is_run_active() or not event is InputEventMouseButton:
		return
	var mouse_event := event as InputEventMouseButton
	if mouse_event.button_index != MOUSE_BUTTON_LEFT or not mouse_event.pressed:
		return
	var camera := $Camera3D as Camera3D
	var ray_origin := camera.project_ray_origin(mouse_event.position)
	var ray_end := ray_origin + camera.project_ray_normal(mouse_event.position) * 1000.0
	var query := PhysicsRayQueryParameters3D.create(ray_origin, ray_end)
	query.collide_with_areas = true
	query.collide_with_bodies = false
	var hit := get_world_3d().direct_space_state.intersect_ray(query)
	if not hit.is_empty() and hit.get("collider") == $Cell/SelectionArea:
		runtime_object_selected = true
		_show_runtime_inspector()
		get_viewport().set_input_as_handled()


func _build_editor_ui() -> void:
	var root := $UI/Root as Control
	root.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	var margin := MarginContainer.new()
	margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	margin.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	margin.size_flags_vertical = Control.SIZE_EXPAND_FILL
	margin.add_theme_constant_override("margin_left", 12)
	margin.add_theme_constant_override("margin_right", 12)
	margin.add_theme_constant_override("margin_top", 10)
	margin.add_theme_constant_override("margin_bottom", 10)
	root.add_child(margin)

	var page := VBoxContainer.new()
	page.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	page.size_flags_vertical = Control.SIZE_EXPAND_FILL
	page.add_theme_constant_override("separation", 8)
	margin.add_child(page)
	page.add_child(_build_header())
	workspace_host = VBoxContainer.new()
	workspace_host.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	workspace_host.size_flags_vertical = Control.SIZE_EXPAND_FILL
	page.add_child(workspace_host)
	page.add_child(_build_status_area())


func _build_header() -> Control:
	var header := HBoxContainer.new()
	header.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.custom_minimum_size.y = 42.0
	var title := Label.new()
	title.text = tr("ui.title")
	title.add_theme_font_size_override("font_size", 24)
	header.add_child(title)
	mode_label = Label.new()
	mode_label.add_theme_font_size_override("font_size", 18)
	mode_label.custom_minimum_size.x = 120.0
	header.add_child(mode_label)
	save_world_button = _button(tr("ui.save_world"), _on_save_world)
	header.add_child(save_world_button)
	function_library_button = _button(tr("ui.functions"), func() -> void:
		_show_function_library(function_library_selection)
	)
	header.add_child(function_library_button)
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(spacer)
	var locale_selector := OptionButton.new()
	locale_selector.add_item("Русский")
	locale_selector.set_item_metadata(0, "ru")
	locale_selector.add_item("English")
	locale_selector.set_item_metadata(1, "en")
	locale_selector.select(0 if current_locale == "ru" else 1)
	locale_selector.item_selected.connect(func(index: int) -> void:
		_on_locale_selected(index, locale_selector)
	)
	header.add_child(locale_selector)
	return header


func _build_workspace() -> Control:
	var split := HSplitContainer.new()
	split.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	split.split_offset = 240
	split.add_child(_build_world_panel())
	var detail_split := HSplitContainer.new()
	detail_split.split_offset = 470
	detail_split.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	detail_split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	split.add_child(detail_split)

	var viewport_slot := Control.new()
	viewport_slot.custom_minimum_size.x = 300.0
	viewport_slot.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	viewport_slot.size_flags_vertical = Control.SIZE_EXPAND_FILL
	viewport_slot.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var viewport_caption := Label.new()
	viewport_caption.text = tr("ui.viewport_help")
	viewport_caption.position = Vector2(16, 12)
	viewport_caption.mouse_filter = Control.MOUSE_FILTER_IGNORE
	viewport_slot.add_child(viewport_caption)
	detail_split.add_child(viewport_slot)
	detail_split.add_child(_build_inspector_panel())
	return split


func _show_world_workspace() -> void:
	active_workspace = "world"
	function_library = null
	_clear_children(workspace_host)
	workspace_host.add_child(_build_workspace())
	_rebuild_world_tree()
	_refresh_mode()
	if not editor.is_run_active():
		_restore_edit_inspector()


func _show_function_library(selected_id: int = -1) -> void:
	if editor.is_run_active():
		return
	active_workspace = "functions"
	selected_kind = ""
	selected_identity = -1
	if selected_id > 0:
		function_library_selection = selected_id
	_clear_children(workspace_host)
	function_library = FUNCTION_LIBRARY_SCENE.instantiate()
	workspace_host.add_child(function_library)
	function_library.close_requested.connect(_show_world_workspace)
	function_library.world_changed.connect(_on_function_library_world_changed)
	function_library.status_requested.connect(_on_function_library_status_requested)
	function_library.error_requested.connect(_set_status_error)
	function_library.function_selected.connect(func(id: int) -> void:
		function_library_selection = id
	)
	function_library.tab_selected.connect(func(function_id: int, tab: int) -> void:
		function_library_tabs[function_id] = tab
	)
	function_library.configure(editor, function_library_selection,
		int(function_library_tabs.get(function_library_selection, 0)))
	_refresh_mode()


func _on_function_library_world_changed() -> void:
	if active_workspace == "world":
		_rebuild_world_tree()


func _on_function_library_status_requested(key: String, arguments: Array) -> void:
	_set_status(key, arguments)


func _build_world_panel() -> Control:
	var panel := PanelContainer.new()
	panel.custom_minimum_size.x = 230.0
	panel.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 10)
	margin.add_theme_constant_override("margin_right", 10)
	margin.add_theme_constant_override("margin_top", 8)
	margin.add_theme_constant_override("margin_bottom", 8)
	panel.add_child(margin)
	var column := VBoxContainer.new()
	margin.add_child(column)
	var heading := Label.new()
	heading.text = tr("ui.world")
	heading.add_theme_font_size_override("font_size", 18)
	column.add_child(heading)
	world_tree = Tree.new()
	world_tree.hide_root = true
	world_tree.size_flags_vertical = Control.SIZE_EXPAND_FILL
	world_tree.item_selected.connect(_on_world_item_selected)
	world_tree.item_mouse_selected.connect(_on_world_item_mouse_selected)
	column.add_child(world_tree)
	world_tree_menu = PopupMenu.new()
	world_tree_menu.id_pressed.connect(_on_world_tree_menu_pressed)
	column.add_child(world_tree_menu)
	new_name = LineEdit.new()
	new_name.placeholder_text = tr("ui.new_name_placeholder")
	column.add_child(new_name)
	var buttons := HBoxContainer.new()
	add_value_button = _button(tr("ui.add_value"), _on_add_value)
	add_unit_button = _button(tr("ui.add_unit"), _on_add_unit)
	add_calculation_button = _button(tr("ui.add_calculation"), _on_add_calculation)
	add_template_button = _button(tr("ui.add_template"), _on_add_template)
	add_rule_button = _button(tr("ui.add_rule"), _on_add_rule)
	var add_characteristic_button := _button(tr("ui.add_object_characteristic"), _on_add_object_characteristic)
	buttons.add_child(add_value_button)
	buttons.add_child(add_unit_button)
	buttons.add_child(add_calculation_button)
	buttons.add_child(add_template_button)
	buttons.add_child(add_rule_button)
	buttons.add_child(add_characteristic_button)
	column.add_child(buttons)
	return panel


func _build_inspector_panel() -> Control:
	var panel := PanelContainer.new()
	panel.custom_minimum_size.x = 300.0
	panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	panel.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var margin := MarginContainer.new()
	margin.add_theme_constant_override("margin_left", 10)
	margin.add_theme_constant_override("margin_right", 10)
	margin.add_theme_constant_override("margin_top", 8)
	margin.add_theme_constant_override("margin_bottom", 8)
	panel.add_child(margin)
	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	margin.add_child(scroll)
	inspector = VBoxContainer.new()
	inspector.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	inspector.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.add_child(inspector)
	return panel


func _build_status_area() -> Control:
	var column := VBoxContainer.new()
	column.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	status_label = Label.new()
	status_label.text = tr("status.ready")
	status_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	column.add_child(status_label)
	var controls := HBoxContainer.new()
	controls.add_theme_constant_override("separation", 8)
	play_button = _button(tr("ui.play"), _on_play)
	pause_button = _button(tr("ui.pause"), _on_pause)
	step_button = _button(tr("ui.step"), _on_step)
	reset_button = _button(tr("ui.reset"), _on_reset)
	back_to_editor_button = _button(tr("ui.back_to_editor"), _on_back_to_editor)
	controls.add_child(play_button)
	controls.add_child(pause_button)
	controls.add_child(step_button)
	controls.add_child(reset_button)
	controls.add_child(back_to_editor_button)
	tick_label = Label.new()
	tick_label.custom_minimum_size.x = 90.0
	controls.add_child(tick_label)
	var separator := VSeparator.new()
	controls.add_child(separator)
	var inputs_caption := Label.new()
	inputs_caption.text = tr("ui.host_inputs")
	controls.add_child(inputs_caption)
	host_inputs_box = HBoxContainer.new()
	host_inputs_box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	controls.add_child(host_inputs_box)
	column.add_child(controls)
	return column


func _rebuild_world_tree() -> void:
	if world_tree == null:
		return
	world_tree.clear()
	var root := world_tree.create_item()
	var units_root := world_tree.create_item(root)
	units_root.set_text(0, tr("ui.units"))
	units_root.set_metadata(0, {"kind": "section"})
	for unit in editor.get_units():
		var item := world_tree.create_item(units_root)
		item.set_text(0, "%s  [#%d]" % [unit.symbol, unit.id])
		item.set_metadata(0, {"kind": "unit", "id": int(unit.id)})
	units_root.collapsed = false

	var conversions_root := world_tree.create_item(root)
	conversions_root.set_text(0, tr("ui.unit_conversions"))
	conversions_root.set_metadata(0, {"kind": "unit_conversions"})
	for conversion in editor.get_unit_conversions():
		var item := world_tree.create_item(conversions_root)
		item.set_text(0, _unit_conversion_text(conversion))
		item.set_metadata(0, {"kind": "unit_conversion", "id": int(conversion.id)})
	conversions_root.collapsed = false

	var values_root := world_tree.create_item(root)
	values_root.set_text(0, tr("ui.values"))
	values_root.set_metadata(0, {"kind": "section"})
	for value in editor.get_values():
		var item := world_tree.create_item(values_root)
		item.set_text(0, "%s%s  [#%d]" % [value.name, _value_unit_suffix(value), value.key])
		item.set_metadata(0, {"kind": "value", "id": int(value.key)})
	values_root.collapsed = false
	var characteristics_root := world_tree.create_item(root)
	characteristics_root.set_text(0, tr("ui.object_characteristics"))
	characteristics_root.set_metadata(0, {"kind": "section"})
	for characteristic in editor.get_object_characteristics():
		var characteristic_item := world_tree.create_item(characteristics_root)
		characteristic_item.set_text(0, str(characteristic.name))
		characteristic_item.set_metadata(0, {"kind": "object_characteristic", "id": int(characteristic.id)})
	characteristics_root.collapsed = false
	var construction_item := world_tree.create_item(root)
	construction_item.set_text(0, tr("ui.object_construction"))
	construction_item.set_metadata(0, {"kind": "object_construction"})

	var calculations_root := world_tree.create_item(root)
	calculations_root.set_text(0, tr("ui.calculations"))
	calculations_root.set_metadata(0, {"kind": "section"})
	for calculation in editor.get_calculations():
		var item := world_tree.create_item(calculations_root)
		item.set_text(0, "%s  [#%d]" % [calculation.name, calculation.id])
		item.set_metadata(0, {"kind": "calculation", "id": int(calculation.id)})
	calculations_root.collapsed = false

	var templates_root := world_tree.create_item(root)
	templates_root.set_text(0, tr("ui.templates"))
	templates_root.set_metadata(0, {"kind": "section"})
	for object_template in editor.get_templates():
		var item := world_tree.create_item(templates_root)
		item.set_text(0, "%s  [#%d]" % [object_template.name, object_template.id])
		item.set_metadata(0, {"kind": "template", "id": int(object_template.id)})
	templates_root.collapsed = false

	var function_types_root := world_tree.create_item(root)
	function_types_root.set_text(0, tr("ui.function_types"))
	function_types_root.set_metadata(0, {"kind": "section"})
	for function_type in editor.get_function_types():
		var item := world_tree.create_item(function_types_root)
		item.set_text(0, "%s  [#%d]" % [function_type.name, function_type.id])
		item.set_metadata(0, {"kind": "function_type", "id": int(function_type.id)})
	function_types_root.collapsed = false

	var rules_root := world_tree.create_item(root)
	rules_root.set_text(0, tr("ui.world_rules"))
	rules_root.set_metadata(0, {"kind": "section"})
	for rule in editor.get_world_rules():
		var item := world_tree.create_item(rules_root)
		item.set_text(0, "%s → %s → %s  × %.3f" % [
			_value_name(int(rule.source_key)),
			tr("ui.end_value_format") % _value_name(int(rule.end_buffer_key)),
			_value_name(int(rule.target_key)),
			float(rule.target_per_source),
		])
		item.set_metadata(0, {"kind": "rule", "id": int(rule.index)})
	rules_root.collapsed = false


func _on_world_item_selected() -> void:
	if editor.is_run_active():
		return
	var item := world_tree.get_selected()
	if item == null:
		return
	var metadata: Dictionary = item.get_metadata(0)
	selected_kind = str(metadata.get("kind", ""))
	selected_identity = int(metadata.get("id", -1))
	match selected_kind:
		"value":
			_show_value_inspector(selected_identity)
		"object_characteristic": _show_object_characteristic_inspector(selected_identity)
		"object_construction": _show_object_construction_inspector()
		"unit_conversions":
			_show_unit_conversion_creator()
		"unit_conversion":
			_show_unit_conversion_inspector(selected_identity)
		"template":
			if editor.select_template(selected_identity):
				_refresh_preview_visibility()
				_show_template_inspector(selected_identity)
			else:
				_show_facade_error_if_any()
		"function_type":
			_show_function_library(selected_identity)
		"calculation":
			_show_calculation_inspector(selected_identity)
		"rule":
			_show_rule_inspector(selected_identity, false)
		_:
			_show_welcome_inspector()


func _on_world_item_mouse_selected(position: Vector2, mouse_button_index: int) -> void:
	if editor.is_run_active() or mouse_button_index != MOUSE_BUTTON_RIGHT:
		return
	var item := world_tree.get_item_at_position(position)
	if item == null:
		return
	var metadata: Variant = item.get_metadata(0)
	if not (metadata is Dictionary):
		return
	var kind := str(metadata.get("kind", ""))
	if kind != "function_type" and kind != "calculation":
		return
	item.select(0)
	selected_kind = kind
	selected_identity = int(metadata.get("id", -1))
	_show_context_menu(kind, selected_identity, -1, world_tree.get_screen_position() + position)


func _on_world_tree_menu_pressed(id: int) -> void:
	if id != 1:
		return
	if context_target_kind == "function_type":
		_finish_deletion(editor.remove_function_type(selected_identity), "status.function_type_deleted")
	elif context_target_kind == "calculation":
		_finish_deletion(editor.remove_calculation(selected_identity), "status.calculation_deleted")
	elif context_target_kind == "calculation_input":
		_finish_calculation_port_deletion(
			editor.remove_calculation_input(context_calculation_id, context_port_id),
			"status.calculation_input_deleted")
	elif context_target_kind == "calculation_output":
		_finish_calculation_port_deletion(
			editor.remove_calculation_output(context_calculation_id, context_port_id),
			"status.calculation_output_deleted")


func _show_context_menu(kind: String, calculation_id: int, port_id: int, screen_position: Vector2) -> void:
	context_target_kind = kind
	context_calculation_id = calculation_id if kind.begins_with("calculation_") else -1
	context_port_id = port_id
	world_tree_menu.clear()
	world_tree_menu.add_item(tr("ui.delete"), 1)
	world_tree_menu.position = Vector2i(screen_position)
	world_tree_menu.popup()


func _show_welcome_inspector() -> void:
	_clear_children(inspector)
	_add_heading(inspector, tr("ui.inspector"))
	_add_wrapped_label(inspector, tr("help.select_definition_item"))


func _show_value_inspector(key: int) -> void:
	var value := _find_by(editor.get_values(), "key", key)
	if value.is_empty():
		_show_welcome_inspector()
		return
	_clear_children(inspector)
	_add_heading(inspector, tr("ui.value"))
	_add_wrapped_label(inspector, tr("ui.stable_value_key") % key)
	var name_edit := LineEdit.new()
	name_edit.text = str(value.name)
	inspector.add_child(name_edit)
	inspector.add_child(_button(tr("ui.rename"), func() -> void:
		_finish_edit(editor.rename_value(key, name_edit.text), "status.value_renamed")
	))
	var unit_components: Array = value.unit_components
	_add_wrapped_label(inspector, tr("ui.value_unit_format") % _unit_expression_text(unit_components))
	var unit_option := _unit_option(_atomic_unit_id(unit_components))
	inspector.add_child(_labeled_control(tr("ui.unit"), unit_option))
	var set_unit_button := _button(tr("ui.set_value_unit"), func() -> void:
		_finish_edit(editor.set_value_unit(key, _selected_option_id(unit_option)), "status.value_unit_set")
	)
	set_unit_button.disabled = unit_option.item_count == 0
	inspector.add_child(set_unit_button)
	var delete_button := _button(tr("ui.delete_value"), func() -> void:
		_finish_deletion(editor.remove_value(key), "status.value_deleted")
	)
	inspector.add_child(delete_button)


func _show_unit_conversion_creator() -> void:
	_clear_children(inspector)
	_add_heading(inspector, tr("ui.new_unit_conversion"))
	var source_unit := _unit_option()
	var source_amount := _positive_spin(1.0)
	var target_unit := _unit_option()
	var target_amount := _amount_spin(0.1)
	inspector.add_child(_labeled_control(tr("ui.source_unit"), source_unit))
	inspector.add_child(_labeled_control(tr("ui.source_amount"), source_amount))
	inspector.add_child(_labeled_control(tr("ui.target_unit"), target_unit))
	inspector.add_child(_labeled_control(tr("ui.target_amount"), target_amount))
	var add_button := _button(tr("ui.add_unit_conversion"), func() -> void:
		var conversion_id := editor.add_unit_conversion(
			_selected_option_id(source_unit), source_amount.value,
			_selected_option_id(target_unit), target_amount.value)
		if conversion_id == 0:
			_show_facade_error_if_any()
			return
		selected_kind = "unit_conversion"
		selected_identity = conversion_id
		_set_status("status.unit_conversion_added", [conversion_id])
		_rebuild_world_tree()
		_show_unit_conversion_inspector(conversion_id)
	)
	add_button.disabled = source_unit.item_count == 0 or target_unit.item_count == 0
	inspector.add_child(add_button)


func _show_unit_conversion_inspector(conversion_id: int) -> void:
	var conversion := _find_by(editor.get_unit_conversions(), "id", conversion_id)
	if conversion.is_empty():
		_show_welcome_inspector()
		return
	_clear_children(inspector)
	_add_heading(inspector, tr("ui.unit_conversion"))
	_add_wrapped_label(inspector, _unit_conversion_text(conversion))
	_add_separator(inspector)
	inspector.add_child(_button(tr("ui.new_unit_conversion"), func() -> void:
		selected_kind = "unit_conversions"
		selected_identity = -1
		_show_unit_conversion_creator()
	))


func _show_template_inspector(template_id: int) -> void:
	var object_template := _find_by(editor.get_templates(), "id", template_id)
	if object_template.is_empty():
		_show_welcome_inspector()
		return
	_clear_children(inspector)
	_add_heading(inspector, tr("ui.template"))
	_add_wrapped_label(inspector, tr("ui.stable_template_id") % template_id)
	var name_edit := LineEdit.new()
	name_edit.text = str(object_template.name)
	inspector.add_child(name_edit)
	inspector.add_child(_button(tr("ui.rename_template"), func() -> void:
		_finish_edit(editor.rename_template(template_id, name_edit.text), "status.template_renamed")
	))
	inspector.add_child(_button(tr("ui.delete_template"), func() -> void:
		_finish_deletion(editor.remove_template(template_id), "status.template_deleted")
	))
	_add_separator(inspector)
	_build_initial_values_editor(template_id)
	_add_separator(inspector)
	_build_material_contributions(template_id)
	_add_separator(inspector)
	_build_genome_editor(template_id)
	_add_separator(inspector)
	_build_bindings_editor(template_id)


func _show_calculation_inspector(calculation_id: int) -> void:
	var calculation := _find_by(editor.get_calculations(), "id", calculation_id)
	if calculation.is_empty():
		_show_welcome_inspector()
		return
	_clear_children(inspector)
	_add_heading(inspector, str(calculation.name))
	_add_wrapped_label(inspector, tr("ui.stable_calculation_id") % calculation_id)
	_add_separator(inspector)
	_add_heading(inspector, tr("ui.calculation_inputs"))
	for input in calculation.inputs:
		var input_id := int(input.id)
		var input_panel := _calculation_port_panel(tr("ui.calculation_input_title") % [input.name, int(input.id)])
		input_panel.gui_input.connect(func(event: InputEvent) -> void:
			if event is InputEventMouseButton:
				var mouse_event := event as InputEventMouseButton
				if mouse_event.button_index == MOUSE_BUTTON_RIGHT and mouse_event.pressed:
					_show_context_menu("calculation_input", calculation_id, input_id,
						input_panel.get_global_position() + mouse_event.position)
		)
		inspector.add_child(input_panel)
	_add_heading(inspector, tr("ui.new_calculation_input"))
	var input_name := LineEdit.new()
	input_name.placeholder_text = tr("ui.name")
	inspector.add_child(_labeled_control(tr("ui.name"), input_name))
	inspector.add_child(_button(tr("ui.add_calculation_input"), func() -> void:
		_finish_edit(editor.add_calculation_input(calculation_id, input_name.text) != 0,
			"status.calculation_input_added")
	))
	_add_separator(inspector)
	_add_heading(inspector, tr("ui.calculation_outputs"))
	for output_index in calculation.outputs.size():
		var output = calculation.outputs[output_index]
		var output_id := int(output.id)
		var output_panel := _calculation_port_panel(
			tr("ui.calculation_output_title") % [output_index + 1, output.name, int(output.id)])
		output_panel.gui_input.connect(func(event: InputEvent) -> void:
			if event is InputEventMouseButton:
				var mouse_event := event as InputEventMouseButton
				if mouse_event.button_index == MOUSE_BUTTON_RIGHT and mouse_event.pressed:
					_show_context_menu("calculation_output", calculation_id, output_id,
						output_panel.get_global_position() + mouse_event.position)
		)
		var output_box := output_panel.get_child(0) as VBoxContainer
		var expression_edit := LineEdit.new()
		expression_edit.text = str(output.expression_source)
		expression_edit.placeholder_text = tr("ui.expression")
		output_box.add_child(_labeled_control(tr("ui.expression"), expression_edit))
		output_box.add_child(_button(tr("ui.update_expression"), func() -> void:
			_finish_edit(editor.set_calculation_output_expression(calculation_id, output_id, expression_edit.text),
				"status.calculation_output_expression_updated")
		))
		inspector.add_child(output_panel)
	_add_heading(inspector, tr("ui.new_calculation_output"))
	var available := PackedStringArray()
	for input in calculation.inputs:
		available.append(str(input.name))
	for output in calculation.outputs:
		available.append(str(output.name))
	_add_wrapped_label(inspector, tr("ui.available_values_format") % ", ".join(available))
	var output_name := LineEdit.new()
	output_name.placeholder_text = tr("ui.name")
	var output_expression := LineEdit.new()
	output_expression.placeholder_text = tr("ui.expression")
	inspector.add_child(_labeled_control(tr("ui.name"), output_name))
	inspector.add_child(_labeled_control(tr("ui.expression"), output_expression))
	inspector.add_child(_button(tr("ui.add_calculation_output"), func() -> void:
		_finish_edit(editor.add_calculation_output(calculation_id, output_name.text, output_expression.text) != 0,
			"status.calculation_output_added")
	))
	_add_separator(inspector)
	_build_calculation_test_editor(calculation_id, calculation)


func _calculation_port_panel(title: String) -> PanelContainer:
	var panel := PanelContainer.new()
	panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var box := VBoxContainer.new()
	box.mouse_filter = Control.MOUSE_FILTER_PASS
	var heading := _heading_label(title, 16)
	heading.mouse_filter = Control.MOUSE_FILTER_PASS
	box.add_child(heading)
	panel.add_child(box)
	return panel


func _build_calculation_test_editor(calculation_id: int, calculation: Dictionary) -> void:
	_add_heading(inspector, tr("ui.calculation_test"))
	var input_spins: Array[SpinBox] = []
	for input in calculation.inputs:
		var spin := _amount_spin(0.0)
		input_spins.append(spin)
		inspector.add_child(_labeled_control("%s [#%d]" % [input.name, int(input.id)], spin))
	var result_box := VBoxContainer.new()
	inspector.add_child(_button(tr("ui.calculate"), func() -> void:
		var inputs: Array = []
		for index in calculation.inputs.size():
			inputs.append({"port_id": int(calculation.inputs[index].id), "amount": input_spins[index].value})
		editor.clear_last_error()
		var results = editor.evaluate_calculation(calculation_id, inputs)
		if not editor.get_last_error().is_empty():
			_show_facade_error_if_any()
			return
		_clear_children(result_box)
		_add_wrapped_label(result_box, tr("ui.calculation_result"))
		for output_index in calculation.outputs.size():
			var output = calculation.outputs[output_index]
			if output_index < results.size():
				_add_wrapped_label(result_box, "%s = %.6f" % [output.name, float(results[output_index].amount)])
	))
	inspector.add_child(result_box)


func _build_material_contributions(template_id: int) -> void:
	_add_heading(inspector, tr("ui.material_cost"))
	for material in editor.get_material_contributions(template_id):
		var key := int(material.value_key)
		_add_wrapped_label(inspector, "%s [#%d]: %.6f" % [
			_value_name(key), key, float(material.amount),
		])


func _build_initial_values_editor(template_id: int) -> void:
	_add_heading(inspector, tr("ui.initial_values"))
	for initial in editor.get_initial_values(template_id):
		var key := int(initial.value_key)
		var row := HBoxContainer.new()
		var label := Label.new()
		label.text = "%s [#%d]" % [_value_name(key), key]
		label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		row.add_child(label)
		var amount := _amount_spin(float(initial.amount))
		row.add_child(amount)
		row.add_child(_button(tr("ui.set"), func() -> void:
			_finish_edit(editor.set_initial_value(template_id, key, amount.value), "status.initial_value_updated")
		))
		row.add_child(_button("×", func() -> void:
			_finish_edit(editor.remove_initial_value(template_id, key), "status.initial_value_removed")
		))
		inspector.add_child(row)
	var add_row := HBoxContainer.new()
	var value_option := _value_option()
	value_option.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var amount := _amount_spin(0.0)
	add_row.add_child(value_option)
	add_row.add_child(amount)
	add_row.add_child(_button(tr("ui.set_or_add"), func() -> void:
		_finish_edit(editor.set_initial_value(template_id, _selected_option_id(value_option), amount.value), "status.initial_value_set")
	))
	inspector.add_child(add_row)


func _build_genome_editor(template_id: int) -> void:
	_add_heading(inspector, tr("ui.genome"))
	for function in editor.get_genome(template_id):
		var index := int(function.index)
		_add_wrapped_label(inspector, tr("ui.function_instance_format") % [index, function.function_type_name])
		_add_wrapped_label(inspector, tr("ui.genome_parameters"))
		for parameter in function.genome_parameters:
			var amount := _amount_spin(float(parameter.amount))
			var row := _labeled_control("%s [#%d]" % [parameter.name, parameter.parameter_id], amount)
			var controls := row as HBoxContainer
			controls.add_child(_button(tr("ui.update"), func() -> void:
				_finish_edit(editor.set_genome_parameter(template_id, index, int(parameter.parameter_id), amount.value),
					"status.genome_parameter_updated")
			))
			inspector.add_child(row)
		_add_wrapped_label(inspector, tr("ui.calculation_outputs_read_only"))
		for output in function.calculation_outputs:
			_add_wrapped_label(inspector, "%s / %s [#%d]: %.6f" % [
				output.calculation_name, output.name, output.output_id, float(output.amount),
			])
		inspector.add_child(_button(tr("ui.remove"), func() -> void:
			_finish_edit(editor.remove_genome_function(template_id, index), "status.genome_function_removed")
		))
	var new_type := _function_type_option()
	_add_wrapped_label(inspector, tr("ui.new_function_instance"))
	inspector.add_child(_labeled_control(tr("ui.function_type"), new_type))
	inspector.add_child(_button(tr("ui.add_function"), func() -> void:
		_finish_edit(editor.add_genome_function(template_id, _selected_option_id(new_type)),
			"status.genome_function_added")
	))


func _build_bindings_editor(template_id: int) -> void:
	_add_heading(inspector, tr("ui.host_bindings"))
	for binding in editor.get_bindings(template_id):
		var index := int(binding.index)
		var binding_direction := int(binding.direction_id)
		var binding_channel := str(binding.channel)
		var capability := _find_capability(binding_channel, binding_direction)
		var value_option := _value_option(int(binding.value_key))
		if capability.is_empty():
			_add_wrapped_label(inspector, tr("ui.legacy_binding_format") % [
				index, binding_channel, _direction_text(binding_direction),
			])
			inspector.add_child(_labeled_control(tr("ui.value"), value_option))
			inspector.add_child(_button(tr("ui.remove_legacy_binding"), func() -> void:
				_finish_edit(editor.remove_host_binding(template_id, index), "status.legacy_binding_removed")
			))
			continue
		var direction := _direction_option(binding_direction)
		var channel := _capability_option(binding_direction, binding_channel)
		direction.item_selected.connect(func(_selected_index: int) -> void:
			_populate_capability_option(channel, _selected_option_id(direction), "")
		)
		_add_wrapped_label(inspector, tr("ui.binding_format") % index)
		inspector.add_child(_labeled_control(tr("ui.direction"), direction))
		inspector.add_child(_labeled_control(tr("ui.capability"), channel))
		inspector.add_child(_labeled_control(tr("ui.value"), value_option))
		var actions := HBoxContainer.new()
		actions.add_child(_button(tr("ui.update"), func() -> void:
			_finish_edit(editor.change_host_binding(template_id, index, _selected_capability_channel(channel),
				_selected_option_id(direction), {"kind": "value", "value_key": _selected_option_id(value_option)}), "status.binding_updated")
		))
		actions.add_child(_button(tr("ui.remove"), func() -> void:
			_finish_edit(editor.remove_host_binding(template_id, index), "status.binding_removed")
		))
		inspector.add_child(actions)
	var new_direction := _direction_option(INPUT_DIRECTION)
	var new_channel := _capability_option(INPUT_DIRECTION)
	new_direction.item_selected.connect(func(_selected_index: int) -> void:
		_populate_capability_option(new_channel, _selected_option_id(new_direction), "")
	)
	var new_value := _value_option()
	_add_wrapped_label(inspector, tr("ui.new_binding"))
	inspector.add_child(_labeled_control(tr("ui.direction"), new_direction))
	inspector.add_child(_labeled_control(tr("ui.capability"), new_channel))
	inspector.add_child(_labeled_control(tr("ui.value"), new_value))
	inspector.add_child(_button(tr("ui.add_binding"), func() -> void:
		_finish_edit(editor.add_host_binding(template_id, _selected_capability_channel(new_channel),
			_selected_option_id(new_direction),
			{"kind": "value", "value_key": _selected_option_id(new_value)}), "status.binding_added")
	))


func _show_rule_inspector(index: int, is_new: bool) -> void:
	var rules := editor.get_world_rules()
	var rule: Dictionary = {} if is_new else _find_by(rules, "index", index)
	if not is_new and rule.is_empty():
		_show_welcome_inspector()
		return
	_clear_children(inspector)
	_add_heading(inspector, tr("ui.new_world_rule") if is_new else tr("ui.world_rule_format") % index)
	var source := _value_option(0 if is_new else int(rule.source_key))
	var end_buffer := _value_option(0 if is_new else int(rule.end_buffer_key))
	var target := _value_option(0 if is_new else int(rule.target_key))
	if is_new and target.item_count > 1:
		target.select(1)
	var factor := _amount_spin(1.0 if is_new else float(rule.target_per_source))
	inspector.add_child(_labeled_control(tr("ui.source"), source))
	inspector.add_child(_labeled_control(tr("ui.end_buffer"), end_buffer))
	inspector.add_child(_labeled_control(tr("ui.target"), target))
	inspector.add_child(_labeled_control(tr("ui.target_per_source"), factor))
	if is_new:
		inspector.add_child(_button(tr("ui.add_rule_plain"), func() -> void:
			_finish_edit(editor.add_world_rule(_selected_option_id(source), _selected_option_id(end_buffer),
				_selected_option_id(target), factor.value),
				"status.world_rule_added")
		))
	else:
		inspector.add_child(_button(tr("ui.update_rule"), func() -> void:
			_finish_edit(editor.change_world_rule(index, _selected_option_id(source),
				_selected_option_id(end_buffer), _selected_option_id(target), factor.value),
				"status.world_rule_updated")
		))
		inspector.add_child(_button(tr("ui.remove_rule"), func() -> void:
			_finish_edit(editor.remove_world_rule(index), "status.world_rule_removed")
		))


func _show_runtime_inspector() -> void:
	_clear_children(inspector)
	runtime_value_labels.clear()
	runtime_buffer_labels.clear()
	runtime_end_labels.clear()
	_add_heading(inspector, tr("ui.runtime_object_format") % editor.get_preview_object_id())
	_add_wrapped_label(inspector, tr("help.runtime_values"))
	for value in editor.get_runtime_values():
		var key := int(value.key)
		var label := Label.new()
		label.text = "%s [#%d]: %.6f" % [value.name, key, float(value.amount)]
		inspector.add_child(label)
		runtime_value_labels[key] = label
	_add_heading(inspector, tr("ui.runtime_functions"))
	for function in editor.get_runtime_functions():
		var function_index := int(function.function_index)
		_add_wrapped_label(inspector, tr("ui.runtime_function_format") % [
			function_index, function.function_type_name,
		])
		_add_wrapped_label(inspector, tr("ui.genome_parameters"))
		for parameter in function.genome_parameters:
			_add_wrapped_label(inspector, "%s: %.6f" % [parameter.name, float(parameter.amount)])
		_add_wrapped_label(inspector, tr("ui.calculation_outputs_read_only"))
		for output in function.calculation_outputs:
			_add_wrapped_label(inspector, "%s / %s: %.6f" % [
				output.calculation_name, output.name, float(output.amount)])
		if function.has("buffer"):
			var buffer: Dictionary = function.buffer
			_add_wrapped_label(inspector, tr("ui.buffer_capacity_format") % float(buffer.capacity))
			_add_wrapped_label(inspector, tr("ui.buffer_throughput_format") % float(buffer.throughput))
			_add_wrapped_label(inspector, tr("ui.buffer_leakage_format") % float(buffer.leakage))
			for field in ["stored_amount", "received_last_tick", "supplied_last_tick"]:
				var live_label := Label.new()
				live_label.text = _buffer_state_text(field, float(buffer[field]))
				inspector.add_child(live_label)
				runtime_buffer_labels["%d:%s" % [function_index, field]] = live_label
	_add_heading(inspector, tr("ui.end_buffer"))
	for end_value in editor.get_last_end_buffer():
		var key := int(end_value.value_key)
		var label := Label.new()
		label.text = "%s [#%d]: %.6f" % [end_value.name, key, float(end_value.amount)]
		inspector.add_child(label)
		runtime_end_labels[key] = label


func _show_run_inspector_help() -> void:
	_clear_children(inspector)
	runtime_value_labels.clear()
	runtime_buffer_labels.clear()
	runtime_end_labels.clear()
	_add_heading(inspector, tr("ui.run_inspector"))
	_add_wrapped_label(inspector, tr("help.click_runtime_object"))


func _refresh_runtime_values() -> void:
	for value in editor.get_runtime_values():
		var key := int(value.key)
		if runtime_value_labels.has(key):
			var label := runtime_value_labels[key] as Label
			label.text = "%s [#%d]: %.6f" % [value.name, key, float(value.amount)]
	for function in editor.get_runtime_functions():
		if not function.has("buffer"):
			continue
		var buffer: Dictionary = function.buffer
		var function_index := int(function.function_index)
		for field in ["stored_amount", "received_last_tick", "supplied_last_tick"]:
			var label_key := "%d:%s" % [function_index, field]
			if runtime_buffer_labels.has(label_key):
				var label := runtime_buffer_labels[label_key] as Label
				label.text = _buffer_state_text(field, float(buffer[field]))
	for end_value in editor.get_last_end_buffer():
		var key := int(end_value.value_key)
		if runtime_end_labels.has(key):
			var label := runtime_end_labels[key] as Label
			label.text = "%s [#%d]: %.6f" % [end_value.name, key, float(end_value.amount)]


func _buffer_state_text(field: String, amount: float) -> String:
	match field:
		"stored_amount":
			return tr("ui.buffer_stored_format") % amount
		"received_last_tick":
			return tr("ui.buffer_received_format") % amount
		_:
			return tr("ui.buffer_supplied_format") % amount


func _rebuild_host_inputs() -> void:
	_clear_children(host_inputs_box)
	for input in editor.get_host_inputs():
		var channel := str(input.channel)
		var label := Label.new()
		label.text = channel
		host_inputs_box.add_child(label)
		var amount := _amount_spin(float(input.amount))
		amount.custom_minimum_size.x = 100.0
		amount.value_changed.connect(func(value: float) -> void:
			if not editor.set_host_input(channel, value):
				_show_facade_error_if_any()
		)
		host_inputs_box.add_child(amount)


func _on_add_value() -> void:
	var key := editor.add_value(new_name.text)
	if key == 0:
		_show_facade_error_if_any()
		return
	new_name.clear()
	selected_kind = "value"
	selected_identity = key
	_set_status("status.value_added", [key])
	_rebuild_world_tree()
	_show_value_inspector(key)


func _on_add_unit() -> void:
	var unit_id := editor.add_unit(new_name.text)
	if unit_id == 0:
		_show_facade_error_if_any()
		return
	new_name.clear()
	selected_kind = "unit"
	selected_identity = unit_id
	_set_status("status.unit_added", [unit_id])
	_rebuild_world_tree()
	_show_welcome_inspector()

func _on_add_object_characteristic() -> void:
	var id := editor.add_object_characteristic(new_name.text)
	if id == 0:
		_show_facade_error_if_any()
		return
	new_name.clear()
	selected_kind = "object_characteristic"
	selected_identity = id
	_rebuild_world_tree()
	_show_object_characteristic_inspector(id)

func _show_object_characteristic_inspector(id: int) -> void:
	var characteristic := _find_by(editor.get_object_characteristics(), "id", id)
	if characteristic.is_empty():
		_show_welcome_inspector()
		return
	_clear_children(inspector)
	_add_heading(inspector, str(characteristic.name))
	var name := LineEdit.new()
	name.text = str(characteristic.name)
	inspector.add_child(name)
	inspector.add_child(_button(tr("ui.rename"), func() -> void:
		_finish_edit(editor.rename_object_characteristic(id, name.text), "status.object_characteristic_updated")
	))
	inspector.add_child(_button(tr("ui.delete"), func() -> void:
		_finish_deletion(editor.remove_object_characteristic(id), "status.object_characteristic_removed")
	))

func _show_object_construction_inspector() -> void:
	_clear_children(inspector)
	_add_heading(inspector, tr("ui.object_construction"))
	_add_wrapped_label(inspector, tr("help.object_construction"))


func _on_add_calculation() -> void:
	var calculation_id := editor.add_calculation(new_name.text)
	if calculation_id == 0:
		_show_facade_error_if_any()
		return
	new_name.clear()
	selected_kind = "calculation"
	selected_identity = calculation_id
	_set_status("status.calculation_added", [calculation_id])
	_rebuild_world_tree()
	_show_calculation_inspector(calculation_id)


func _on_add_template() -> void:
	var template_id := editor.add_template(new_name.text)
	if template_id == 0:
		_show_facade_error_if_any()
		return
	new_name.clear()
	selected_kind = "template"
	selected_identity = template_id
	_set_status("status.template_added", [template_id])
	_refresh_preview_visibility()
	_rebuild_world_tree()
	_show_template_inspector(template_id)


func _on_add_rule() -> void:
	if editor.get_values().size() < 2:
		_set_status("status.need_two_values")
		return
	_show_rule_inspector(-1, true)


func _on_back_to_editor() -> void:
	editor.stop()
	object_views.clear()
	runtime_object_selected = false
	$Cell.scale = Vector3.ONE
	_set_status("status.runtime_stopped")
	_refresh_preview_visibility()
	_rebuild_host_inputs()
	_refresh_mode()
	_restore_edit_inspector()


func _on_play() -> void:
	if active_workspace == "functions":
		_show_world_workspace()
	if not editor.is_run_active():
		if not editor.run():
			_show_facade_error_if_any()
			return
		_select_preview_runtime_object()
		_set_status("status.run_started")
		_rebuild_host_inputs()
		_refresh_mode()
		return
	if editor.play():
		_set_status("status.runtime_playing")
	else:
		_show_facade_error_if_any()
	_refresh_mode()


func _on_pause() -> void:
	editor.pause()
	_set_status("status.runtime_paused")
	_refresh_mode()


func _on_step() -> void:
	if active_workspace == "functions":
		_show_world_workspace()
	if not editor.is_run_active():
		if not editor.run():
			_show_facade_error_if_any()
			return
		editor.pause()
		_rebuild_host_inputs()
		_select_preview_runtime_object()
	if editor.step_once():
		_set_status("status.tick_advanced")
	else:
		_show_facade_error_if_any()
	_refresh_runtime_display()


func _on_reset() -> void:
	if not editor.reset_runtime():
		_show_facade_error_if_any()
		return
	editor.pause()
	_select_preview_runtime_object()
	_set_status("status.runtime_reset")
	_refresh_runtime_display()


func _select_preview_runtime_object() -> void:
	object_views.clear()
	$Cell.scale = Vector3.ONE
	_refresh_preview_visibility()
	object_views[editor.get_preview_object_id()] = $Cell
	runtime_object_selected = true
	_show_runtime_inspector()


func _refresh_runtime_display() -> void:
	_apply_runtime_to_views()
	tick_label.text = tr("ui.tick_format") % editor.get_tick()
	if runtime_object_selected:
		_refresh_runtime_values()
	_refresh_mode()


func _finish_edit(success: bool, message_key: String, arguments: Array = []) -> void:
	if not success:
		_show_facade_error_if_any()
		return
	_set_status(message_key, arguments)
	_refresh_preview_visibility()
	_rebuild_world_tree()
	_restore_edit_inspector()


func _finish_deletion(success: bool, message_key: String) -> void:
	if not success:
		_show_facade_error_if_any()
		return
	selected_kind = ""
	selected_identity = -1
	_set_status(message_key)
	_refresh_preview_visibility()
	_rebuild_world_tree()
	_show_welcome_inspector()


func _finish_calculation_port_deletion(success: bool, message_key: String) -> void:
	if not success:
		_show_facade_error_if_any()
		return
	_set_status(message_key)
	_rebuild_world_tree()
	_show_calculation_inspector(context_calculation_id)


func _restore_edit_inspector() -> void:
	match selected_kind:
		"value":
			_show_value_inspector(selected_identity)
		"unit_conversions":
			_show_unit_conversion_creator()
		"unit_conversion":
			_show_unit_conversion_inspector(selected_identity)
		"template":
			_show_template_inspector(selected_identity)
		"function_type":
			_show_function_library(selected_identity)
		"calculation":
			_show_calculation_inspector(selected_identity)
		"rule":
			_show_rule_inspector(selected_identity, false)
		_:
			_show_welcome_inspector()


func _refresh_mode() -> void:
	var running := editor.is_run_active()
	mode_label.text = tr("ui.mode_run") if running else tr("ui.mode_edit")
	play_button.disabled = running and editor.is_playing()
	pause_button.disabled = not running or not editor.is_playing()
	step_button.disabled = running and editor.is_playing()
	reset_button.disabled = not running
	back_to_editor_button.disabled = not running
	save_world_button.disabled = running
	function_library_button.disabled = running
	if active_workspace == "world" and is_instance_valid(world_tree):
		new_name.editable = not running
		add_value_button.disabled = running
		add_unit_button.disabled = running
		add_calculation_button.disabled = running
		add_template_button.disabled = running
		add_rule_button.disabled = running
		world_tree.mouse_filter = Control.MOUSE_FILTER_IGNORE if running else Control.MOUSE_FILTER_STOP
		world_tree.modulate = Color(0.65, 0.65, 0.65, 1.0) if running else Color.WHITE
	tick_label.text = tr("ui.tick_format") % editor.get_tick()


func _refresh_preview_visibility() -> void:
	$Cell.visible = editor.is_run_active()


func _apply_runtime_to_views() -> void:
	if not editor.is_run_active():
		return
	for output in editor.get_host_outputs():
		if str(output.channel) != GEOMETRY_VOLUME_CHANNEL:
			continue
		var object_id := int(output.object_id)
		if not object_views.has(object_id):
			continue
		var view := object_views[object_id] as Node3D
		if is_instance_valid(view):
			var volume := maxf(float(output.amount), 0.0)
			var linear_scale := pow(volume, 1.0 / 3.0)
			view.scale = Vector3.ONE * linear_scale


func _show_facade_error_if_any() -> void:
	var message := editor.get_last_error()
	if not message.is_empty():
		_set_status_error(message)


func _set_status(message_key: String, arguments: Array = []) -> void:
	status_key = message_key
	status_arguments = arguments.duplicate()
	status_error = ""
	_refresh_status()


func _set_status_error(message: String) -> void:
	status_error = message
	_refresh_status()


func _refresh_status() -> void:
	if status_label == null:
		return
	if not status_error.is_empty():
		status_label.text = tr("status.error_format") % status_error
		return
	var message := tr(status_key)
	status_label.text = message % status_arguments if not status_arguments.is_empty() else message


func _value_name(key: int) -> String:
	var value := _find_by(editor.get_values(), "key", key)
	return tr("ui.unknown") if value.is_empty() else str(value.name)


func _value_unit_suffix(value: Dictionary) -> String:
	var text := _unit_expression_text(value.get("unit_components", []))
	return "" if text == tr("ui.unit_not_set") else " [%s]" % text


func _atomic_unit_id(components: Array) -> int:
	if components.size() != 1:
		return 0
	var component: Dictionary = components[0]
	return int(component.get("id", 0)) if int(component.get("exponent", 0)) == 1 else 0


func _unit_expression_text(components: Array) -> String:
	if components.is_empty():
		return tr("ui.unit_not_set")
	var terms := PackedStringArray()
	for component in components:
		var unit := _find_by(editor.get_units(), "id", int(component.get("id", 0)))
		var symbol := tr("ui.unknown") if unit.is_empty() else str(unit.symbol)
		var exponent := int(component.get("exponent", 0))
		terms.append(symbol if exponent == 1 else "%s^%d" % [symbol, exponent])
	return " * ".join(terms)


func _unit_conversion_text(conversion: Dictionary) -> String:
	return "%s %s → %s %s  [#%d]" % [
		str(conversion.get("source_amount", 0.0)),
		_unit_expression_text(conversion.get("source_components", [])),
		str(conversion.get("target_amount", 0.0)),
		_unit_expression_text(conversion.get("target_components", [])),
		int(conversion.get("id", 0)),
	]


func _find_by(items: Array, field: String, identity: int) -> Dictionary:
	for item in items:
		if int(item.get(field, -1)) == identity:
			return item
	return {}


func _value_option(selected_key: int = 0) -> OptionButton:
	var option := OptionButton.new()
	for value in editor.get_values():
		option.add_item("%s [#%d]" % [value.name, value.key])
		option.set_item_metadata(option.item_count - 1, int(value.key))
		if int(value.key) == selected_key:
			option.select(option.item_count - 1)
	return option


func _unit_option(selected_id: int = 0) -> OptionButton:
	var option := OptionButton.new()
	for unit in editor.get_units():
		option.add_item("%s [#%d]" % [unit.symbol, unit.id])
		option.set_item_metadata(option.item_count - 1, int(unit.id))
		if int(unit.id) == selected_id:
			option.select(option.item_count - 1)
	return option


func _function_type_option(selected_id: int = 0) -> OptionButton:
	var option := OptionButton.new()
	for function_type in editor.get_function_types():
		option.add_item("%s [#%d]" % [function_type.name, function_type.id])
		option.set_item_metadata(option.item_count - 1, int(function_type.id))
		if int(function_type.id) == selected_id:
			option.select(option.item_count - 1)
	return option


func _direction_option(selected_direction: int) -> OptionButton:
	var option := OptionButton.new()
	option.add_item(tr("ui.input"), INPUT_DIRECTION)
	option.set_item_metadata(0, INPUT_DIRECTION)
	option.add_item(tr("ui.output"), OUTPUT_DIRECTION)
	option.set_item_metadata(1, OUTPUT_DIRECTION)
	option.select(0 if selected_direction == INPUT_DIRECTION else 1)
	return option


func _direction_text(direction_id: int) -> String:
	return tr("ui.input") if direction_id == INPUT_DIRECTION else tr("ui.output")


func _capability_option(direction_id: int, selected_channel: String = "") -> OptionButton:
	var option := OptionButton.new()
	_populate_capability_option(option, direction_id, selected_channel)
	return option


func _populate_capability_option(option: OptionButton, direction_id: int, selected_channel: String) -> void:
	option.clear()
	for capability in editor.get_host_capabilities():
		if int(capability.direction_id) != direction_id:
			continue
		option.add_item("%s — %s" % [tr(str(capability.display_key)), capability.channel])
		option.set_item_metadata(option.item_count - 1, str(capability.channel))
		if str(capability.channel) == selected_channel:
			option.select(option.item_count - 1)


func _selected_capability_channel(option: OptionButton) -> String:
	if option.selected < 0:
		return ""
	return str(option.get_item_metadata(option.selected))


func _find_capability(channel: String, direction_id: int) -> Dictionary:
	for capability in editor.get_host_capabilities():
		if str(capability.channel) == channel and int(capability.direction_id) == direction_id:
			return capability
	return {}


func _selected_option_id(option: OptionButton) -> int:
	if option.selected < 0:
		return 0
	return int(option.get_item_metadata(option.selected))


func _amount_spin(value: float) -> SpinBox:
	var spin := SpinBox.new()
	spin.min_value = -1000000.0
	spin.max_value = 1000000.0
	spin.step = 0.01
	spin.allow_greater = true
	spin.allow_lesser = true
	spin.value = value
	return spin


func _positive_spin(value: float) -> SpinBox:
	var spin := _amount_spin(value)
	spin.min_value = 0.001
	spin.allow_lesser = false
	return spin


func _labeled_control(caption: String, control: Control) -> Control:
	var row := HBoxContainer.new()
	var label := Label.new()
	label.text = caption
	label.custom_minimum_size.x = 115.0
	row.add_child(label)
	control.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(control)
	return row


func _button(text: String, callback: Callable) -> Button:
	var button := Button.new()
	button.text = text
	button.pressed.connect(callback)
	return button


func _add_heading(parent: VBoxContainer, text: String) -> void:
	parent.add_child(_heading_label(text, 18))


func _heading_label(text: String, size: int) -> Label:
	var label := Label.new()
	label.text = text
	label.add_theme_font_size_override("font_size", size)
	return label


func _add_wrapped_label(parent: VBoxContainer, text: String) -> void:
	var label := Label.new()
	label.text = text
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	parent.add_child(label)


func _add_separator(parent: VBoxContainer) -> void:
	parent.add_child(HSeparator.new())


func _clear_children(parent: Node) -> void:
	for child in parent.get_children():
		parent.remove_child(child)
		child.queue_free()
