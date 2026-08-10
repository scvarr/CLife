extends Control

signal close_requested
signal world_changed
signal status_requested(message_key: String, arguments: Array)
signal error_requested(message: String)
signal function_selected(function_type_id: int)

var editor: CLifeWorldEditor
var selected_function_id := -1
var function_list: ItemList
var editor_host: VBoxContainer
var context_menu: PopupMenu
var context_kind := ""
var context_function_id := -1
var context_value_key := -1


func configure(world_editor: CLifeWorldEditor, function_type_id: int = -1) -> void:
	editor = world_editor
	selected_function_id = function_type_id
	_rebuild()


func selected_id() -> int:
	return selected_function_id


func _rebuild() -> void:
	for child in get_children():
		remove_child(child)
		child.queue_free()
	var page := VBoxContainer.new()
	page.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	page.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	page.size_flags_vertical = Control.SIZE_EXPAND_FILL
	page.add_theme_constant_override("separation", 8)
	add_child(page)
	var header := HBoxContainer.new()
	header.custom_minimum_size.y = 40.0
	header.add_child(_button(tr("ui.back_to_world"), func() -> void: close_requested.emit()))
	var title := Label.new()
	title.text = tr("ui.function_library")
	title.add_theme_font_size_override("font_size", 22)
	header.add_child(title)
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	header.add_child(spacer)
	page.add_child(header)
	var split := HSplitContainer.new()
	split.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	split.size_flags_vertical = Control.SIZE_EXPAND_FILL
	split.split_offset = 270
	page.add_child(split)
	split.add_child(_build_list_panel())
	split.add_child(_build_editor_panel())
	context_menu = PopupMenu.new()
	context_menu.id_pressed.connect(_on_context_menu_pressed)
	add_child(context_menu)
	_refresh_list()


func _build_list_panel() -> Control:
	var panel := PanelContainer.new()
	panel.custom_minimum_size.x = 250.0
	panel.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var column := VBoxContainer.new()
	panel.add_child(column)
	var heading := Label.new()
	heading.text = tr("ui.function_types")
	heading.add_theme_font_size_override("font_size", 18)
	column.add_child(heading)
	function_list = ItemList.new()
	function_list.size_flags_vertical = Control.SIZE_EXPAND_FILL
	function_list.item_selected.connect(func(index: int) -> void:
		selected_function_id = int(function_list.get_item_metadata(index))
		function_selected.emit(selected_function_id)
		_show_selected_function()
	)
	function_list.item_clicked.connect(func(index: int, position: Vector2, mouse_button: int) -> void:
		if mouse_button != MOUSE_BUTTON_RIGHT:
			return
		selected_function_id = int(function_list.get_item_metadata(index))
		function_list.select(index)
		function_selected.emit(selected_function_id)
		_show_context_menu("function", selected_function_id, -1,
			function_list.get_global_position() + position, tr("ui.delete_function_type"), false)
	)
	column.add_child(function_list)
	var name := LineEdit.new()
	name.placeholder_text = tr("ui.new_function_name")
	column.add_child(name)
	column.add_child(_button(tr("ui.add_function_type"), func() -> void:
		var id := editor.add_function_type(name.text)
		if id == 0:
			_emit_facade_error()
			return
		selected_function_id = id
		name.clear()
		status_requested.emit("status.function_type_added", [id])
		world_changed.emit()
		function_selected.emit(id)
		_refresh_list()
		_show_selected_function()
	))
	return panel


func _build_editor_panel() -> Control:
	var panel := PanelContainer.new()
	panel.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	panel.size_flags_vertical = Control.SIZE_EXPAND_FILL
	var scroll := ScrollContainer.new()
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	scroll.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.size_flags_vertical = Control.SIZE_EXPAND_FILL
	panel.add_child(scroll)
	editor_host = VBoxContainer.new()
	editor_host.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	editor_host.size_flags_vertical = Control.SIZE_EXPAND_FILL
	scroll.add_child(editor_host)
	return panel


func _refresh_list() -> void:
	if function_list == null:
		return
	function_list.clear()
	var exists := false
	for function_type in editor.get_function_types():
		function_list.add_item("%s  [#%d]" % [function_type.name, function_type.id])
		function_list.set_item_metadata(function_list.item_count - 1, int(function_type.id))
		if int(function_type.id) == selected_function_id:
			function_list.select(function_list.item_count - 1)
			exists = true
	if not exists:
		selected_function_id = -1


func _show_selected_function() -> void:
	_clear(editor_host)
	var function_type := _function_type()
	if function_type.is_empty():
		var welcome := Label.new()
		welcome.text = tr("help.select_function")
		welcome.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		editor_host.add_child(welcome)
		return
	var name_row := HBoxContainer.new()
	var name := LineEdit.new()
	name.text = str(function_type.name)
	name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	name_row.add_child(name)
	name_row.add_child(_button(tr("ui.rename"), func() -> void:
		_finish(editor.rename_function_type(selected_function_id, name.text), "status.function_type_renamed")
	))
	editor_host.add_child(name_row)
	var stable := Label.new()
	stable.text = tr("ui.stable_function_type_id") % selected_function_id
	editor_host.add_child(stable)
	var tabs := TabContainer.new()
	tabs.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	tabs.size_flags_vertical = Control.SIZE_EXPAND_FILL
	editor_host.add_child(tabs)
	var construction := VBoxContainer.new()
	construction.name = tr("ui.construction")
	tabs.add_child(construction)
	_build_construction(construction, function_type)
	var process := VBoxContainer.new()
	process.name = tr("ui.process")
	tabs.add_child(process)
	_build_process(process, function_type)
	var materials := VBoxContainer.new()
	materials.name = tr("ui.materials")
	tabs.add_child(materials)
	_build_materials(materials, function_type)


func _build_construction(parent: VBoxContainer, function_type: Dictionary) -> void:
	_add_heading(parent, tr("ui.genome_parameters"))
	for parameter in function_type.genome_parameters:
		var card := _card("%s [#%d]" % [parameter.name, parameter.id])
		var box := card.get_child(0) as VBoxContainer
		var default_label := Label.new()
		default_label.text = tr("ui.default_value_format") % float(parameter.default_value)
		box.add_child(default_label)
		parent.add_child(card)
	_add_heading(parent, tr("ui.new_genome_parameter"))
	var new_name := LineEdit.new()
	var default_value := _amount_spin(1.0)
	parent.add_child(_labeled(tr("ui.name"), new_name))
	parent.add_child(_labeled(tr("ui.default_value"), default_value))
	parent.add_child(_button(tr("ui.add_genome_parameter"), func() -> void:
		_finish(editor.add_genome_parameter(selected_function_id, new_name.text, default_value.value) != 0,
			"status.genome_parameter_added")
	))
	_separator(parent)
	_add_heading(parent, tr("ui.function_calculations"))
	for binding in function_type.calculations:
		var calculation := _find(editor.get_calculations(), "id", int(binding.calculation_id))
		var card := _card(str(calculation.get("name", tr("ui.unknown"))))
		var box := card.get_child(0) as VBoxContainer
		for input_binding in binding.inputs:
			var input := _find(calculation.get("inputs", []), "id", int(input_binding.input_id))
			var label := Label.new()
			label.text = "%s <- %s" % [input.get("name", tr("ui.unknown")),
				_parameter_name(function_type, int(input_binding.genome_parameter_id))]
			box.add_child(label)
		var calculation_id := int(binding.calculation_id)
		card.gui_input.connect(func(event: InputEvent) -> void:
			if event is InputEventMouseButton:
				var mouse := event as InputEventMouseButton
				if mouse.button_index == MOUSE_BUTTON_RIGHT and mouse.pressed:
					_show_context_menu("binding", selected_function_id, calculation_id,
						card.get_global_position() + mouse.position, tr("ui.remove_calculation_binding"), false)
		)
		box.add_child(_button(tr("ui.remove_calculation_binding"), func() -> void:
			_finish(editor.remove_function_calculation_binding(selected_function_id, calculation_id),
				"status.calculation_binding_removed")
		))
		parent.add_child(card)
	var calculation_option := _calculation_option()
	var input_box := VBoxContainer.new()
	var input_options: Array = []
	if calculation_option.item_count > 0:
		_populate_binding_inputs(input_box, _selected_id(calculation_option), function_type, input_options)
	calculation_option.item_selected.connect(func(_index: int) -> void:
		_populate_binding_inputs(input_box, _selected_id(calculation_option), function_type, input_options)
	)
	parent.add_child(_labeled(tr("ui.calculation"), calculation_option))
	parent.add_child(input_box)
	var bind := _button(tr("ui.bind_calculation"), func() -> void:
		var bindings: Array = []
		for entry in input_options:
			bindings.append({"input_id": entry.input_id, "genome_parameter_id": _selected_id(entry.option)})
		_finish(editor.set_function_calculation_binding(selected_function_id, _selected_id(calculation_option), bindings),
			"status.calculation_binding_updated")
	)
	bind.disabled = calculation_option.item_count == 0
	parent.add_child(bind)


func _build_process(parent: VBoxContainer, function_type: Dictionary) -> void:
	var process_value: Variant = function_type.get("process")
	var process: Dictionary = process_value if process_value is Dictionary else {}
	var process_card := _card(tr("ui.process"))
	var box := process_card.get_child(0) as VBoxContainer
	var input_option := _value_option(int(process.get("input_key", 0)))
	var throughput := _source_option(function_type, process.get("throughput_source", {}))
	var conversion := _conversion_option(int(process.get("conversion_id", 0)))
	box.add_child(_labeled(tr("ui.input"), input_option))
	box.add_child(_labeled(tr("ui.throughput"), throughput))
	box.add_child(_labeled(tr("ui.unit_conversion"), conversion))
	var outputs: Array = process.get("outputs", [])
	if outputs.is_empty():
		var first_output := _value_option()
		var first_allocation := _source_option(function_type)
		box.add_child(_labeled(tr("ui.first_output"), first_output))
		box.add_child(_labeled(tr("ui.allocation"), first_allocation))
		var create := _button(tr("ui.set_process"), func() -> void:
			_finish(editor.set_function_process(selected_function_id, _selected_id(input_option), _selected_source(throughput),
				_selected_id(conversion), _selected_id(first_output), _selected_source(first_allocation)), "status.process_set")
		)
		create.disabled = input_option.item_count == 0 or throughput.item_count == 0 or conversion.item_count == 0 or first_output.item_count == 0 or first_allocation.item_count == 0
		box.add_child(create)
	else:
		box.add_child(_button(tr("ui.update_process"), func() -> void:
			_finish(editor.change_function_process_settings(selected_function_id, _selected_id(input_option),
				_selected_source(throughput), _selected_id(conversion)), "status.process_updated")
		))
		process_card.gui_input.connect(func(event: InputEvent) -> void:
			if event is InputEventMouseButton:
				var mouse := event as InputEventMouseButton
				if mouse.button_index == MOUSE_BUTTON_RIGHT and mouse.pressed:
					_show_context_menu("process", selected_function_id, -1,
						process_card.get_global_position() + mouse.position, tr("ui.remove_process"), false)
		)
		box.add_child(_button(tr("ui.remove_process"), func() -> void:
			_finish(editor.remove_function_process(selected_function_id), "status.process_removed")
		))
	parent.add_child(process_card)
	if not outputs.is_empty():
		_add_heading(parent, tr("ui.process_outputs"))
		for output in outputs:
			var output_key := int(output.output_key)
			var output_card := _card(_value_name(output_key))
			var output_box := output_card.get_child(0) as VBoxContainer
			var value_option := _value_option(output_key)
			var allocation := _source_option(function_type, output.allocation_source)
			output_box.add_child(_labeled(tr("ui.output"), value_option))
			output_box.add_child(_labeled(tr("ui.allocation"), allocation))
			output_box.add_child(_button(tr("ui.update"), func() -> void:
				_finish(editor.change_function_process_output(selected_function_id, output_key, _selected_id(value_option),
					_selected_source(allocation)), "status.process_output_updated")
			))
			output_card.gui_input.connect(func(event: InputEvent) -> void:
				if event is InputEventMouseButton:
					var mouse := event as InputEventMouseButton
					if mouse.button_index == MOUSE_BUTTON_RIGHT and mouse.pressed:
						_show_context_menu("process_output", selected_function_id, output_key,
							output_card.get_global_position() + mouse.position, tr("ui.remove"), outputs.size() == 1)
			)
			parent.add_child(output_card)
		var new_output := _value_option()
		var new_allocation := _source_option(function_type)
		parent.add_child(_labeled(tr("ui.output"), new_output))
		parent.add_child(_labeled(tr("ui.allocation"), new_allocation))
		parent.add_child(_button(tr("ui.add_process_output"), func() -> void:
			_finish(editor.add_function_process_output(selected_function_id, _selected_id(new_output), _selected_source(new_allocation)),
				"status.process_output_added")
		))
	_separator(parent)
	_build_buffer(parent, function_type)


func _build_buffer(parent: VBoxContainer, function_type: Dictionary) -> void:
	_add_heading(parent, tr("ui.buffer_process"))
	var buffer_value: Variant = function_type.get("buffer")
	var buffer: Dictionary = buffer_value if buffer_value is Dictionary else {}
	var card := _card(tr("ui.buffer_process"))
	var box := card.get_child(0) as VBoxContainer
	var value := _value_option(int(buffer.get("value_key", 0)))
	var capacity := _source_option(function_type, buffer.get("capacity_source", {}))
	var throughput := _source_option(function_type, buffer.get("throughput_source", {}))
	var leakage := _source_option(function_type, buffer.get("leakage_source", {}))
	box.add_child(_labeled(tr("ui.value"), value))
	box.add_child(_labeled(tr("ui.capacity"), capacity))
	box.add_child(_labeled(tr("ui.throughput"), throughput))
	box.add_child(_labeled(tr("ui.leakage"), leakage))
	box.add_child(_button(tr("ui.set_buffer_process"), func() -> void:
		_finish(editor.set_buffer_process(selected_function_id, _selected_id(value), _selected_source(capacity),
			_selected_source(throughput), _selected_source(leakage)), "status.buffer_process_set")
	))
	if not buffer.is_empty():
		card.gui_input.connect(func(event: InputEvent) -> void:
			if event is InputEventMouseButton:
				var mouse := event as InputEventMouseButton
				if mouse.button_index == MOUSE_BUTTON_RIGHT and mouse.pressed:
					_show_context_menu("buffer", selected_function_id, -1,
						card.get_global_position() + mouse.position, tr("ui.remove_buffer_process"), false)
		)
		box.add_child(_button(tr("ui.remove_buffer_process"), func() -> void:
			_finish(editor.remove_buffer_process(selected_function_id), "status.buffer_process_removed")
		))
	parent.add_child(card)


func _build_materials(parent: VBoxContainer, function_type: Dictionary) -> void:
	_add_heading(parent, tr("ui.material_cost"))
	for contribution in function_type.material_contributions:
		var value_key := int(contribution.value_key)
		var card := _card(_value_name(value_key))
		var box := card.get_child(0) as VBoxContainer
		var source := _source_option(function_type, contribution.amount_source)
		box.add_child(_labeled(tr("ui.amount_source"), source))
		box.add_child(_button(tr("ui.update"), func() -> void:
			_finish(editor.set_function_material_contribution(selected_function_id, value_key, _selected_source(source)),
				"status.function_material_contribution_updated")
		))
		card.gui_input.connect(func(event: InputEvent) -> void:
			if event is InputEventMouseButton:
				var mouse := event as InputEventMouseButton
				if mouse.button_index == MOUSE_BUTTON_RIGHT and mouse.pressed:
					_show_context_menu("material", selected_function_id, value_key,
						card.get_global_position() + mouse.position, tr("ui.remove"), false)
		)
		parent.add_child(card)
	var value := _value_option()
	var source := _source_option(function_type)
	parent.add_child(_labeled(tr("ui.value"), value))
	parent.add_child(_labeled(tr("ui.amount_source"), source))
	parent.add_child(_button(tr("ui.set_or_add"), func() -> void:
		_finish(editor.set_function_material_contribution(selected_function_id, _selected_id(value), _selected_source(source)),
			"status.function_material_contribution_updated")
	))


func _show_context_menu(kind: String, function_id: int, value_key: int, position: Vector2, caption: String, disabled: bool) -> void:
	context_kind = kind
	context_function_id = function_id
	context_value_key = value_key
	context_menu.clear()
	context_menu.add_item(caption, 1)
	context_menu.set_item_disabled(0, disabled)
	context_menu.position = Vector2i(position)
	context_menu.popup()


func _on_context_menu_pressed(id: int) -> void:
	if id != 1:
		return
	match context_kind:
		"function": _finish(editor.remove_function_type(context_function_id), "status.function_type_deleted")
		"binding": _finish(editor.remove_function_calculation_binding(context_function_id, context_value_key), "status.calculation_binding_removed")
		"process": _finish(editor.remove_function_process(context_function_id), "status.process_removed")
		"process_output": _finish(editor.remove_function_process_output(context_function_id, context_value_key), "status.process_output_removed")
		"material": _finish(editor.remove_function_material_contribution(context_function_id, context_value_key), "status.function_material_contribution_removed")
		"buffer": _finish(editor.remove_buffer_process(context_function_id), "status.buffer_process_removed")


func _finish(success: bool, status_key: String) -> void:
	if not success:
		_emit_facade_error()
		return
	status_requested.emit(status_key, [])
	world_changed.emit()
	_refresh_list()
	_show_selected_function()


func _emit_facade_error() -> void:
	var error := editor.get_last_error()
	if not error.is_empty():
		error_requested.emit(error)


func _function_type() -> Dictionary:
	return _find(editor.get_function_types(), "id", selected_function_id)


func _populate_binding_inputs(parent: VBoxContainer, calculation_id: int, function_type: Dictionary, entries: Array) -> void:
	_clear(parent)
	entries.clear()
	var calculation := _find(editor.get_calculations(), "id", calculation_id)
	var existing := _find(function_type.calculations, "calculation_id", calculation_id)
	for input in calculation.get("inputs", []):
		var selected := 0
		for binding in existing.get("inputs", []):
			if int(binding.input_id) == int(input.id):
				selected = int(binding.genome_parameter_id)
		var option := _genome_parameter_option(function_type, selected)
		parent.add_child(_labeled(str(input.name), option))
		entries.append({"input_id": int(input.id), "option": option})


func _card(title: String) -> PanelContainer:
	var card := PanelContainer.new()
	card.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	var box := VBoxContainer.new()
	box.mouse_filter = Control.MOUSE_FILTER_PASS
	var heading := Label.new()
	heading.text = title
	heading.add_theme_font_size_override("font_size", 16)
	heading.mouse_filter = Control.MOUSE_FILTER_PASS
	box.add_child(heading)
	card.add_child(box)
	return card


func _add_heading(parent: VBoxContainer, text: String) -> void:
	var label := Label.new()
	label.text = text
	label.add_theme_font_size_override("font_size", 18)
	parent.add_child(label)


func _separator(parent: VBoxContainer) -> void:
	parent.add_child(HSeparator.new())


func _button(caption: String, callback: Callable) -> Button:
	var button := Button.new()
	button.text = caption
	button.pressed.connect(callback)
	return button


func _labeled(caption: String, control: Control) -> HBoxContainer:
	var row := HBoxContainer.new()
	var label := Label.new()
	label.text = caption
	label.custom_minimum_size.x = 150.0
	row.add_child(label)
	control.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(control)
	return row


func _amount_spin(value: float) -> SpinBox:
	var spin := SpinBox.new()
	spin.min_value = -1000000.0
	spin.max_value = 1000000.0
	spin.step = 0.01
	spin.allow_greater = true
	spin.allow_lesser = true
	spin.value = value
	return spin


func _clear(node: Node) -> void:
	for child in node.get_children():
		node.remove_child(child)
		child.queue_free()


func _find(items: Array, field: String, identity: int) -> Dictionary:
	for item in items:
		if int(item.get(field, -1)) == identity:
			return item
	return {}


func _value_option(selected: int = 0) -> OptionButton:
	var option := OptionButton.new()
	for value in editor.get_values():
		option.add_item("%s [#%d]" % [value.name, value.key])
		option.set_item_metadata(option.item_count - 1, int(value.key))
		if int(value.key) == selected:
			option.select(option.item_count - 1)
	return option


func _conversion_option(selected: int = 0) -> OptionButton:
	var option := OptionButton.new()
	for conversion in editor.get_unit_conversions():
		option.add_item(_conversion_text(conversion))
		option.set_item_metadata(option.item_count - 1, int(conversion.id))
		if int(conversion.id) == selected:
			option.select(option.item_count - 1)
	return option


func _conversion_text(conversion: Dictionary) -> String:
	return "%s %s → %s %s" % [conversion.source_amount, _unit_text(conversion.source_components),
		conversion.target_amount, _unit_text(conversion.target_components)]


func _unit_text(components: Array) -> String:
	var parts := PackedStringArray()
	for component in components:
		var unit := _find(editor.get_units(), "id", int(component.id))
		var symbol := str(unit.get("symbol", tr("ui.unknown")))
		var exponent := int(component.exponent)
		parts.append(symbol if exponent == 1 else "%s^%d" % [symbol, exponent])
	return " * ".join(parts)


func _genome_parameter_option(function_type: Dictionary, selected: int = 0) -> OptionButton:
	var option := OptionButton.new()
	for parameter in function_type.genome_parameters:
		option.add_item("%s [#%d]" % [parameter.name, parameter.id])
		option.set_item_metadata(option.item_count - 1, int(parameter.id))
		if int(parameter.id) == selected:
			option.select(option.item_count - 1)
	return option


func _calculation_option() -> OptionButton:
	var option := OptionButton.new()
	for calculation in editor.get_calculations():
		option.add_item("%s [#%d]" % [calculation.name, calculation.id])
		option.set_item_metadata(option.item_count - 1, int(calculation.id))
	return option


func _source_option(function_type: Dictionary, selected_value: Variant = null) -> OptionButton:
	var option := OptionButton.new()
	var selected: Dictionary = selected_value if selected_value is Dictionary else {}
	for parameter in function_type.genome_parameters:
		var source := {"kind": "genome", "genome_parameter_id": int(parameter.id), "calculation_id": 0, "calculation_output_id": 0}
		option.add_item(str(parameter.name))
		option.set_item_metadata(option.item_count - 1, source)
		if _same_source(source, selected): option.select(option.item_count - 1)
	for binding in function_type.calculations:
		var calculation := _find(editor.get_calculations(), "id", int(binding.calculation_id))
		for output in calculation.get("outputs", []):
			var source := {"kind": "calculation", "genome_parameter_id": 0,
				"calculation_id": int(calculation.id), "calculation_output_id": int(output.id)}
			option.add_item("%s / %s" % [calculation.name, output.name])
			option.set_item_metadata(option.item_count - 1, source)
			if _same_source(source, selected): option.select(option.item_count - 1)
	return option


func _same_source(left: Dictionary, right: Dictionary) -> bool:
	return str(left.get("kind", "")) == str(right.get("kind", "")) and \
		int(left.get("genome_parameter_id", 0)) == int(right.get("genome_parameter_id", 0)) and \
		int(left.get("calculation_id", 0)) == int(right.get("calculation_id", 0)) and \
		int(left.get("calculation_output_id", 0)) == int(right.get("calculation_output_id", 0))


func _selected_source(option: OptionButton) -> Dictionary:
	if option.selected < 0: return {}
	var value: Variant = option.get_item_metadata(option.selected)
	return value if value is Dictionary else {}


func _selected_id(option: OptionButton) -> int:
	if option.selected < 0: return 0
	return int(option.get_item_metadata(option.selected))


func _parameter_name(function_type: Dictionary, parameter_id: int) -> String:
	for parameter in function_type.genome_parameters:
		if int(parameter.id) == parameter_id: return str(parameter.name)
	return tr("ui.unknown")


func _value_name(key: int) -> String:
	var value := _find(editor.get_values(), "key", key)
	return str(value.get("name", tr("ui.unknown")))
