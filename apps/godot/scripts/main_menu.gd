extends Control

const TRANSLATION_PATHS := [
	"res://translations/clife_editor.en.po",
	"res://translations/clife_editor.ru.po",
]

@onready var message: Label = $Center/Panel/Margin/Content/Message

func _ready() -> void:
	for path in TRANSLATION_PATHS:
		var translation := load(path) as Translation
		if translation != null:
			TranslationServer.add_translation(translation)
	TranslationServer.set_locale("ru")
	$Center/Panel/Margin/Content/NewWorld.text = tr("menu.new_world")
	$Center/Panel/Margin/Content/LoadWorld.text = tr("menu.load_world")
	$Center/Panel/Margin/Content/Exit.text = tr("menu.exit")
	message.text = ""

func _on_pending_action() -> void:
	message.text = tr("menu.coming_next")

func _on_exit_pressed() -> void:
	get_tree().quit()
