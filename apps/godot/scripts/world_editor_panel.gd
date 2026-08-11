extends RefCounted
class_name WorldEditorPanel

var shell
var editor
var workspace: VBoxContainer
var status: Label
var host_config

func configure(shell_instance: Control, editor_instance: Object, workspace_instance: VBoxContainer, status_instance: Label, host_config_instance: RefCounted) -> void:
	shell = shell_instance
	editor = editor_instance
	workspace = workspace_instance
	status = status_instance
	host_config = host_config_instance

func activate() -> void:
	pass

func deactivate() -> void:
	pass

func show() -> void:
	pass

func _clear_workspace() -> void:
	shell._clear_workspace()

func _add_title(text: String) -> void:
	shell._add_title(text)

func _add_title_to(parent: VBoxContainer, text: String) -> void:
	shell._add_title_to(parent, text)

func _add_section(parent: VBoxContainer, title_text: String) -> void:
	shell._add_section(parent, title_text)

func _labeled_row(label_text: String, control: Control) -> HBoxContainer:
	return shell._labeled_row(label_text, control)

func _show_error() -> void:
	shell._show_error()

func _request_delete(name: String, operation: Callable, position: Vector2, confirmation_kind := "generic") -> void:
	shell._request_delete(name, operation, position, confirmation_kind)

func _discard_runtime_preview() -> void:
	shell._discard_runtime_preview()

func _unit_selector(selected_id: int, allow_none: bool) -> OptionButton:
	return shell._unit_selector(selected_id, allow_none)

func _selected_unit_id(selector: OptionButton) -> int:
	return shell._selected_unit_id(selector)

func _unit_symbol(unit_id: int) -> String:
	return shell._unit_symbol(unit_id)

func _value_unit_id(value: Dictionary) -> int:
	return shell._value_unit_id(value)

func _value_unit_symbol(value: Dictionary) -> String:
	return shell._value_unit_symbol(value)

func _has_complex_value_unit(value: Dictionary) -> bool:
	return shell._has_complex_value_unit(value)

func _value_selector(selected_id: int) -> OptionButton:
	return shell._value_selector(selected_id)

func _conversion_selector(selected_id: int) -> OptionButton:
	return shell._conversion_selector(selected_id)

func _parameter_selector(function_type: Dictionary, selected_id: int) -> OptionButton:
	return shell._parameter_selector(function_type, selected_id)

func _source_selector(function_type: Dictionary, selected_source: Dictionary) -> OptionButton:
	return shell._source_selector(function_type, selected_source)

func _selected_source(selector: OptionButton) -> Dictionary:
	return shell._selected_source(selector)

func _source_matches(left: Dictionary, right: Dictionary) -> bool:
	return shell._source_matches(left, right)

func _find_calculation(id: int) -> Dictionary:
	return shell._find_calculation(id)

func _value_name(key: int) -> String:
	return shell._value_name(key)

func _characteristic_name(id: int) -> String:
	return shell._characteristic_name(id)
