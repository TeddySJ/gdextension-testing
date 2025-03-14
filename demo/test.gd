extends Node2D
class_name Test

@onready var boid_component : BoidComponent = $BoidComponent

var passed_time : float = 0

func _process(delta: float) -> void:
	passed_time += delta
	boid_component.move_to_position(Vector2(passed_time * 50, 50))
