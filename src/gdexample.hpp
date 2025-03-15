#ifndef GDEXAMPLE_H
#define GDEXAMPLE_H

#include <godot_cpp/classes/sprite2d.hpp>

using namespace godot;

class BoidComponent : public Node2D {
	GDCLASS(BoidComponent, Node2D)

public:
	enum BOID_LAYER_SETTING {
		GROUND = 0,
		FLYING = 1,
		BOTH = 2,
		NONE = 3
		};

protected:
    static void _bind_methods();

private:
	BOID_LAYER_SETTING boid_layer = GROUND;
    double desired_distance = 50.0;
    double desired_distance_squared = 2500.0;
    double separation_weight = 1.0;
    Vector2 velocity = Vector2(0, 0);
    Node2D* game_world = nullptr;

    // Reference to the parent enemy node
    Node2D* parent = nullptr;
    
    // Optional cache for performance
    TypedArray<Node2D> cached_nearby_enemies;
    double cache_timer = 0.0;
    double cache_refresh_time = 0.1; // Refresh nearby enemies every 0.1 seconds

public:
	void set_boid_layer(int layer);
    
    int get_boid_layer() const;
    
    void set_desired_distance(double distance);
    
    double get_desired_distance() const;
    
    void set_separation_weight(double weight);
    
    double get_separation_weight() const;

    void set_game_world(Node2D* world);

	void process_boid_behavior();

    void process_boid_behavior_experimental();

	Vector2 get_calculated_velocity() const;

    void set_calculated_velocity(const Vector2 p_velocity);

	void move_to_position(const Vector2 p_position);

public:
	BoidComponent();
	~BoidComponent();

	void _ready() override;
};

VARIANT_ENUM_CAST(BoidComponent::BOID_LAYER_SETTING);

#endif