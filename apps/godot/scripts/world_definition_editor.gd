extends Control

var editor := CLifeWorldEditor.new()
var selected_unit := -1
@onready var list: ItemList = $Layout/Sidebar/Units
@onready var workspace: VBoxContainer = $Layout/Workspace/Margin/Content
@onready var status: Label = $Layout/Workspace/Margin/Content/Status

func _ready() -> void:
	$Layout/Sidebar/Back.text = tr("ux.back_to_menu")
	$Layout/Sidebar/UnitsLabel.text = tr("ux.units")
	$Layout/Sidebar/NewUnit.text = tr("ux.new_unit")
	_refresh()

func _refresh() -> void:
	list.clear()
	for unit in editor.get_units():
		list.add_item(str(unit.symbol))
		list.set_item_metadata(list.item_count - 1, int(unit.id))
	_show_list()

func _show_list() -> void:
	_clear_form()
	_add_title(tr("ux.units"))
	for unit in editor.get_units():
		var label := Label.new()
		label.text = "%s\n%s" % [unit.symbol, unit.description]
		workspace.add_child(label)

func _on_new_unit() -> void:
	selected_unit = -1
	_clear_form()
	_add_title(tr("ux.new_unit"))
	var symbol := LineEdit.new(); symbol.placeholder_text = tr("ux.symbol")
	var description := TextEdit.new(); description.custom_minimum_size = Vector2(0, 90); description.placeholder_text = tr("ux.comment")
	workspace.add_child(symbol); workspace.add_child(description)
	var create := Button.new(); create.text = tr("ux.create")
	create.pressed.connect(func():
		if editor.add_unit(symbol.text, description.text) == 0: status.text = editor.get_last_error(); return
		_refresh()
	)
	workspace.add_child(create)

func _on_unit_selected(index: int) -> void:
	selected_unit = int(list.get_item_metadata(index))
	var unit := _unit(selected_unit)
	_clear_form(); _add_title(tr("ux.unit"))
	var symbol := LineEdit.new(); symbol.text = str(unit.symbol)
	var description := TextEdit.new(); description.text = str(unit.description); description.custom_minimum_size = Vector2(0, 90)
	workspace.add_child(symbol); workspace.add_child(description)
	var save := Button.new(); save.text = tr("ux.save")
	save.pressed.connect(func():
		if not editor.update_unit(selected_unit, symbol.text, description.text): status.text = editor.get_last_error(); return
		_refresh()
	)
	var delete := Button.new(); delete.text = tr("ux.delete")
	delete.pressed.connect(func():
		if not editor.remove_unit(selected_unit): status.text = editor.get_last_error(); return
		_refresh()
	)
	workspace.add_child(save); workspace.add_child(delete)

func _unit(id: int) -> Dictionary:
	for unit in editor.get_units():
		if int(unit.id) == id: return unit
	return {}

func _clear_form() -> void:
	for child in workspace.get_children():
		if child != status: child.queue_free()
	status.text = ""

func _add_title(text: String) -> void:
	var title := Label.new(); title.text = text; title.add_theme_font_size_override("font_size", 26); workspace.add_child(title)

func _on_back() -> void:
	get_tree().change_scene_to_file("res://scenes/main_menu.tscn")
