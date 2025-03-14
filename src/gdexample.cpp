#include "gdexample.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/templates/vector.hpp>

using namespace godot;

void BoidComponent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_boid_layer", "layer"), &BoidComponent::set_boid_layer);
	ClassDB::bind_method(D_METHOD("get_boid_layer"), &BoidComponent::get_boid_layer);
	ClassDB::bind_method(D_METHOD("set_desired_distance", "distance"), &BoidComponent::set_desired_distance);
	ClassDB::bind_method(D_METHOD("get_desired_distance"), &BoidComponent::get_desired_distance);
	ClassDB::bind_method(D_METHOD("set_separation_weight", "weight"), &BoidComponent::set_separation_weight);
	ClassDB::bind_method(D_METHOD("get_separation_weight"), &BoidComponent::get_separation_weight);
	ClassDB::bind_method(D_METHOD("process_boid_behavior"), &BoidComponent::process_boid_behavior);
	ClassDB::bind_method(D_METHOD("get_calculated_velocity"), &BoidComponent::get_calculated_velocity);
	ClassDB::bind_method(D_METHOD("move_to_position", "position"), &BoidComponent::move_to_position);
	ClassDB::bind_method(D_METHOD("set_game_world", "game_world"), &BoidComponent::set_game_world);
	
	BIND_ENUM_CONSTANT(GROUND)
	BIND_ENUM_CONSTANT(FLYING)
	BIND_ENUM_CONSTANT(BOTH)
	BIND_ENUM_CONSTANT(NONE)

	ADD_PROPERTY(PropertyInfo(Variant::INT, "boid_layer", PROPERTY_HINT_ENUM, "Ground,Flying,Both,None"), "set_boid_layer", "get_boid_layer");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "desired_distance"), "set_desired_distance", "get_desired_distance");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "separation_weight"), "set_separation_weight", "get_separation_weight");
}

BoidComponent::BoidComponent() {
	// Initialize any variables here.
}

BoidComponent::~BoidComponent() {
	// Add your cleanup here.
}

void BoidComponent::_ready() {
        parent = Object::cast_to<Node2D>(get_parent());
        if (!parent) {
            UtilityFunctions::printerr("BoidComponent must be a child of a Node2D");
        }
    }

void BoidComponent::set_boid_layer(int layer) {
        boid_layer = static_cast<BOID_LAYER_SETTING>(layer);
    }
    
    int BoidComponent::get_boid_layer() const {
        return static_cast<int>(boid_layer);
    }
    
    void BoidComponent::set_desired_distance(double distance) {
        desired_distance = distance;
        desired_distance_squared = distance * distance;
    }
    
    double BoidComponent::get_desired_distance() const {
        return desired_distance;
    }
    
    void BoidComponent::set_separation_weight(double weight) {
        separation_weight = weight;
    }
    
    double BoidComponent::get_separation_weight() const {
        return separation_weight;
    }

	void BoidComponent::set_game_world(Node2D* node) {
        game_world = node;
    }

	// The main boid processing function (equivalent to _boid_process in GDScript)
    void BoidComponent::process_boid_behavior() {
        if (boid_layer == NONE || !parent) {
            return;
        }
        
        // Get position from parent
        Vector2 current_position = parent->get_global_position();
        
        // Get nearby enemies from game world
        TypedArray<Node2D> all_enemies;
        
		all_enemies = game_world->call("_get_enemies_in_area_and_of_layer", 
				current_position, desired_distance, static_cast<int>(boid_layer));
        // Check if we need to refresh the cache
		/*
        cache_timer += get_process_delta_time();
        if (cache_timer >= cache_refresh_time || cached_nearby_enemies.size() == 0) {
            // Call to your game world to get enemies
			all_enemies = game_world->call("_get_enemies_in_area_and_of_layer", 
				current_position, desired_distance, static_cast<int>(boid_layer));
			cached_nearby_enemies = all_enemies;
			cache_timer = 0.0;
        } else {
            all_enemies = cached_nearby_enemies;
        }
        */
        Vector2 separation_force(0, 0);
        
        // Calculate separation forces
        for (int i = 0; i < all_enemies.size(); i++) {
            Node2D* enemy = Object::cast_to<Node2D>(all_enemies[i]);
            if (enemy == parent) {
                continue;
            }
            
            Vector2 offset = current_position - enemy->get_global_position();
            double distance_squared = offset.length_squared();
            
            if (distance_squared < desired_distance_squared && distance_squared > 0) {
                separation_force += offset.normalized() * ((desired_distance - Math::sqrt(distance_squared)) / desired_distance);
            }
        }
        
        // Apply separation force to velocity
        velocity = separation_force * separation_weight;
    }
    
    Vector2 BoidComponent::get_calculated_velocity() const {
        return velocity;
    }

    void BoidComponent::move_to_position(const Vector2 p_position)
    {
		set_position(p_position);
    }
