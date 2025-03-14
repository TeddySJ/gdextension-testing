#include "gdexample.hpp"
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void GDExample::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_amplitude"), &GDExample::get_amplitude);
	ClassDB::bind_method(D_METHOD("set_amplitude", "p_amplitude"), &GDExample::set_amplitude);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "amplitude"), "set_amplitude", "get_amplitude");

	ClassDB::bind_method(D_METHOD("get_velocity"), &GDExample::get_velocity);
	ClassDB::bind_method(D_METHOD("set_velocity", "p_velocity"), &GDExample::set_velocity);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "velocity"), "set_velocity", "get_velocity");

}

GDExample::GDExample() {
	// Initialize any variables here.
	time_passed = 0.0;
	amplitude = 10.0;
	velocity = Vector2(0, 0);
}

GDExample::~GDExample() {
	// Add your cleanup here.
}

void GDExample::_process(double delta) {
	time_passed += delta;

	Vector2 new_position = Vector2(
		amplitude + (amplitude * sin(time_passed * 2.0)),
		amplitude + (amplitude * cos(time_passed * 1.5))
	);

	new_position += time_passed * velocity;

	set_position(new_position);
}

void GDExample::set_amplitude(const double p_amplitude) {
	amplitude = p_amplitude;
}

double GDExample::get_amplitude() const {
	return amplitude;
}

void GDExample::set_velocity(const Vector2 p_velocity) {
	velocity = p_velocity;
}

Vector2 GDExample::get_velocity() const {
	return velocity;
}