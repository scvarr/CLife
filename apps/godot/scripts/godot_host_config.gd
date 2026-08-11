extends RefCounted
class_name GodotHostConfig

const SAVE_PATH := "user://current_world.godot.json"

var external_inputs: Array[Dictionary] = []

func save() -> Variant:
	var file := FileAccess.open(SAVE_PATH, FileAccess.WRITE)
	if file == null:
		return FileAccess.get_open_error()
	file.store_string(JSON.stringify({"external_inputs": external_inputs}, "\t"))
	file.close()
	return null

func load(editor) -> Variant:
	external_inputs.clear()
	if not FileAccess.file_exists(SAVE_PATH):
		return null
	var file := FileAccess.open(SAVE_PATH, FileAccess.READ)
	if file == null:
		return FileAccess.get_open_error()
	var json := JSON.new()
	if json.parse(file.get_as_text()) != OK or not (json.data is Dictionary):
		return json.get_error_message()
	var saved_inputs = (json.data as Dictionary).get("external_inputs", [])
	if not (saved_inputs is Array):
		return "invalid_host_config"
	for entry in saved_inputs:
		if not (entry is Dictionary) or not _is_valid_mapping(editor, entry as Dictionary):
			external_inputs.clear()
			return "invalid_host_config"
		var mapping: Dictionary = entry
		for existing in external_inputs:
			if str(existing.get("channel", "")) == str(mapping.get("channel", "")):
				external_inputs.clear()
				return "invalid_host_config"
		external_inputs.append({"channel": str(mapping.get("channel", "")), "value_key": int(mapping.get("value_key", 0)), "test_value": float(mapping.get("test_value", 1.0))})
	return null

func _is_valid_mapping(editor, mapping: Dictionary) -> bool:
	var channel := str(mapping.get("channel", ""))
	var value_key := int(mapping.get("value_key", 0))
	if channel.is_empty() or value_key == 0:
		return false
	var input_capability := false
	for capability in editor.get_host_capabilities():
		if int(capability.direction_id) == 0 and str(capability.channel) == channel:
			input_capability = true
			break
	if not input_capability:
		return false
	for value in editor.get_values():
		if int(value.key) == value_key:
			return true
	return false
