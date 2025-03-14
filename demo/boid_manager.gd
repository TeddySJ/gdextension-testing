extends BoidManager
class_name BoidManagerS

func _process(delta: float) -> void:
	
	if Input.is_action_just_pressed("Debug1"):
		prepare_meaningless_work(1000)
		do_meaningless_work()
	
	if Input.is_action_just_pressed("Debug2"):
		prepare_meaningless_work(10000)
		do_meaningless_work()
	
	if Input.is_action_just_pressed("Debug3"):
		prepare_meaningless_work(50000000)
		do_meaningless_work()
		print("jora")

	if Input.is_action_just_pressed("Debug4"):
		prepare_meaningless_work(50000000)
		do_meaningless_work_with_threads()
		print("jametrawdar")
