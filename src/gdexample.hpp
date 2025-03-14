#ifndef GDEXAMPLE_H
#define GDEXAMPLE_H

#include <godot_cpp/classes/sprite2d.hpp>

namespace godot {

// Define Boid Layers (matching your GDScript enum)
enum BoidLayer {
    GROUND = 0,
    FLYING = 1,
    BOTH = 2,
    NONE = 3
};

class BoidComponent : public Sprite2D {
	GDCLASS(BoidComponent, Sprite2D)

protected:
    static void _bind_methods();

private:
	double time_passed;
	double amplitude;

	BoidLayer boid_layer = GROUND;
    double desired_distance = 50.0;
    double desired_distance_squared = 2500.0;
    double separation_weight = 1.0;
    Vector2 velocity = Vector2(0, 0);
    
    // Reference to the parent enemy node
    Node2D* parent = nullptr;
    
    // Optional cache for performance
    TypedArray<Node2D> cached_nearby_enemies;
    double cache_timer = 0.0;
    double cache_refresh_time = 0.1; // Refresh nearby enemies every 0.1 seconds

public:
	void set_amplitude(const double p_amplitude);
	double get_amplitude() const;

	void set_velocity(const Vector2 p_velocity);
	Vector2 get_velocity() const;

	void set_boid_layer(int layer);
    
    int get_boid_layer() const;
    
    void set_desired_distance(double distance);
    
    double get_desired_distance() const;
    
    void set_separation_weight(double weight);
    
    double get_separation_weight() const;

	void process_boid_behavior();

	Vector2 get_calculated_velocity() const;

public:
	BoidComponent();
	~BoidComponent();

	void _process(double delta) override;
};

}

#endif