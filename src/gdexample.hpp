#ifndef GDEXAMPLE_H
#define GDEXAMPLE_H

#include <godot_cpp/classes/sprite2d.hpp>

namespace godot {

class GDExample : public Sprite2D {
	GDCLASS(GDExample, Sprite2D)

protected:
    static void _bind_methods();

private:
	double time_passed;
	double amplitude;
	Vector2 velocity;

public:
	void set_amplitude(const double p_amplitude);
	double get_amplitude() const;

	void set_velocity(const Vector2 p_velocity);
	Vector2 get_velocity() const;

public:
	GDExample();
	~GDExample();

	void _process(double delta) override;
};

}

#endif