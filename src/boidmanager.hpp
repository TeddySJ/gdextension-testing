#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/mutex.hpp>
#include <gdexample.hpp>

#include <random>
#include <vector>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <functional>
#include <memory>

using namespace godot;

// Forward declaration
class BoidComponent;

// Custom ThreadPool implementation
class ThreadPool {
public:
    ThreadPool(size_t num_threads) : stop(false) {
        // Start worker threads
        for (size_t i = 0; i < num_threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex);
                        condition.wait(lock, [this] { 
                            return stop || !tasks.empty(); 
                        });
                        
                        if (stop && tasks.empty()) {
                            return;
                        }
                        
                        task = std::move(tasks.front());
                        tasks.pop();
                    }
                    
                    task();
                }
            });
        }
    }
    
    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        
        condition.notify_all();
        
        for (std::thread& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
    
    template<class F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.emplace(std::forward<F>(f));
        }
        condition.notify_one();
    }
    
    void wait_all() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        wait_condition.wait(lock, [this] { 
            return tasks.empty() && active_tasks == 0; 
        });
    }
    
    void increment_active() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        active_tasks++;
    }
    
    void decrement_active() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        active_tasks--;
        if (tasks.empty() && active_tasks == 0) {
            wait_condition.notify_all();
        }
    }
    
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::condition_variable wait_condition;
    std::atomic<bool> stop;
    size_t active_tasks = 0;
};

class BoidManager : public Node {
    GDCLASS(BoidManager, Node);

private:
    // Boid registry grouped by layer
    std::unordered_map<int, std::vector<BoidComponent*>> boid_layers;
    
    // Batch size for processing
    int batch_size = 128;
    
    // Number of threads to use (0 = auto)
    int thread_count = 0;
    
    // Batching settings
    int update_frequency = 3; // Similar to your boid_process_splits
    int current_batch = 0;
    int spatial_update_frequency = 2;

    // Thread pool
    std::unique_ptr<ThreadPool> thread_pool;
    
    // Thread sync
    std::mutex boid_mutex;

    std::vector<float> testing_vector;
    
    // Spatial partitioning cache
    struct SpatialCache {
        Vector2 position;
        double radius;
        TypedArray<Node> nearby_boids;
        double last_update_time;
    };
    
    std::unordered_map<BoidComponent*, SpatialCache> spatial_cache;
    double cache_lifetime = 0.1; // 100ms cache
    
    // Safe for multiple threads to call
    TypedArray<Node> get_enemies_safely(Object* game_world, const Vector2& position, double radius, int layer) {
        // This is safe because we're just calling a Godot method which is thread-safe as long as
        // we don't modify the game_world object while calling it
        return game_world->call("_get_enemies_in_area_and_of_layer", position, radius, layer);
    }

protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("register_boid", "boid", "layer"), &BoidManager::register_boid);
        ClassDB::bind_method(D_METHOD("unregister_boid", "boid"), &BoidManager::unregister_boid);
        ClassDB::bind_method(D_METHOD("process_boids"), &BoidManager::process_boids);
        ClassDB::bind_method(D_METHOD("set_batch_size", "size"), &BoidManager::set_batch_size);
        ClassDB::bind_method(D_METHOD("get_batch_size"), &BoidManager::get_batch_size);
        ClassDB::bind_method(D_METHOD("set_thread_count", "count"), &BoidManager::set_thread_count);
        ClassDB::bind_method(D_METHOD("get_thread_count"), &BoidManager::get_thread_count);
        ClassDB::bind_method(D_METHOD("set_update_frequency", "frequency"), &BoidManager::set_update_frequency);
        ClassDB::bind_method(D_METHOD("get_update_frequency"), &BoidManager::get_update_frequency);
        ClassDB::bind_method(D_METHOD("set_cache_lifetime", "seconds"), &BoidManager::set_cache_lifetime);
        ClassDB::bind_method(D_METHOD("get_cache_lifetime"), &BoidManager::get_cache_lifetime);
        
        ClassDB::bind_method(D_METHOD("prepare_meaningless_work", "amount"), &BoidManager::prepare_meaningless_work);
        ClassDB::bind_method(D_METHOD("do_meaningless_work"), &BoidManager::do_meaningless_work);
        ClassDB::bind_method(D_METHOD("do_meaningless_work_with_threads"), &BoidManager::do_meaningless_work_with_threads);

        ADD_PROPERTY(PropertyInfo(Variant::INT, "batch_size"), "set_batch_size", "get_batch_size");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "thread_count"), "set_thread_count", "get_thread_count");
        ADD_PROPERTY(PropertyInfo(Variant::INT, "update_frequency"), "set_update_frequency", "get_update_frequency");
        ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cache_lifetime"), "set_cache_lifetime", "get_cache_lifetime");
    }

public:
    BoidManager() {
        // Initialize with default values
        // Determine thread count automatically based on CPU cores
        int cpu_threads = std::thread::hardware_concurrency();
        thread_count = cpu_threads > 0 ? cpu_threads : 4; // Default to 4 if can't detect
        
        // Initialize thread pool
        thread_pool = std::make_unique<ThreadPool>(thread_count);
    }
    
    ~BoidManager() {
        // Thread pool destructor will handle thread cleanup
        thread_pool.reset();
        
        // Cleanup collections
        for (auto& layer : boid_layers) {
            layer.second.clear();
        }
        boid_layers.clear();
        spatial_cache.clear();
    }

    void prepare_meaningless_work(int operations){
        testing_vector.resize(operations);
        for (int i = 0; i < operations; i++)
        {
            testing_vector[i] = static_cast <float> (std::rand()) / (static_cast <float> (RAND_MAX/100.0));
        }
    }

    inline float do_work(float f){
        return f * f + std::sin(f);
    }

    void do_meaningless_work(){
        for (size_t i = 0; i < testing_vector.size(); i++)
        {
            volatile float result = do_work(testing_vector[i]);
        }
    }

    void meaningless_work_in_thread(const std::vector<float>& vector, size_t start_i, size_t end_i){
        for (size_t i = start_i; i < end_i; i++)
        {
            volatile float result = do_work(vector[i]);
        }
    }
    
    void do_meaningless_work_with_threads(){
        // Determine start and end indices for this batch
        size_t total_operations = testing_vector.size();
        size_t batch_count = thread_count;
        size_t per_batch = total_operations / batch_count;
        size_t start_idx = 0;
        size_t end_idx = per_batch;
        
        for (size_t i = 0; i < batch_count; i++) {
            if(i + 1 == batch_count)
                end_idx = total_operations;

            int this_start = start_idx;
            int this_end = end_idx;

            thread_pool->increment_active();
            thread_pool->enqueue([=]() {
                meaningless_work_in_thread(testing_vector, this_start, this_end);
                thread_pool->decrement_active();
            });

            start_idx += per_batch;
            end_idx += per_batch;
            
        }
        
        // Wait for all tasks to complete
        thread_pool->wait_all();
    }



    // Register a boid with the manager
    void register_boid(BoidComponent* boid, int layer) {
        if (!boid) return;
        
        std::lock_guard<std::mutex> lock(boid_mutex);
        
        // Unregister first to avoid duplicates
        for (auto& layer_pair : boid_layers) {
            auto& boids = layer_pair.second;
            boids.erase(std::remove(boids.begin(), boids.end(), boid), boids.end());
        }
        
        // Add to the appropriate layer
        boid_layers[layer].push_back(boid);
    }
    
    // Unregister a boid from the manager
    void unregister_boid(BoidComponent* boid) {
        if (!boid) return;
        
        std::lock_guard<std::mutex> lock(boid_mutex);
        
        // Remove from spatial cache
        if (spatial_cache.find(boid) != spatial_cache.end()) {
            spatial_cache.erase(boid);
        }
        
        // Remove from all layers
        for (auto& layer : boid_layers) {
            auto& boids = layer.second;
            boids.erase(std::remove(boids.begin(), boids.end(), boid), boids.end());
        }
    }
    
    // Process boids in batches using C++ threads
    void process_boids() {
        double current_time = get_process_delta_time();
        
        // Process one batch per frame based on update frequency
        current_batch = (current_batch + 1) % update_frequency;
        
        // Get game world reference
        Object* game_world = nullptr;
        if (Engine::get_singleton()->has_singleton("Globals")) {
            Object* globals = Engine::get_singleton()->get_singleton("Globals");
            if (globals) {
                game_world = globals->call("get", "game_world");
            }
        }
        
        if (!game_world) {
            UtilityFunctions::printerr("BoidManager: Failed to get game_world reference");
            return;
        }
        
        // Make a thread-safe copy of the data we need
        std::vector<std::pair<int, std::vector<BoidComponent*>>> layers_to_process;
        {
            std::lock_guard<std::mutex> lock(boid_mutex);
            for (const auto& layer_pair : boid_layers) {
                if (!layer_pair.second.empty()) {
                    layers_to_process.push_back(layer_pair);
                }
            }
        }
        
        // Process each layer separately
        for (const auto& layer_pair : layers_to_process) {
            int layer = layer_pair.first;
            const auto& boids = layer_pair.second;
            
            if (boids.empty()) continue;
            
            // Determine start and end indices for this batch
            size_t total_boids = boids.size();
            size_t boids_per_batch = (total_boids + update_frequency - 1) / update_frequency;
            size_t start_idx = current_batch * boids_per_batch;
            size_t end_idx = std::min(start_idx + boids_per_batch, total_boids);
            
            if (start_idx >= total_boids) continue;
            
            // Determine how many sub-batches to create for threading
            size_t boids_to_process = end_idx - start_idx;
            size_t batch_count = std::min((size_t)thread_count, (boids_to_process + batch_size - 1) / batch_size);
            
            if (batch_count <= 1) {
                // Process in the main thread if only one batch
                process_boid_batch(boids, start_idx, end_idx, layer, game_world, current_time);
            } else {
                // Process in multiple threads
                size_t sub_batch_size = (boids_to_process + batch_count - 1) / batch_count;
                
                for (size_t i = 0; i < batch_count; i++) {
                    size_t sub_start = start_idx + (i * sub_batch_size);
                    size_t sub_end = std::min(sub_start + sub_batch_size, end_idx);
                    
                    if (sub_start < sub_end) {
                        thread_pool->increment_active();
                        thread_pool->enqueue([this, boids, sub_start, sub_end, layer, game_world, current_time]() {
                            process_boid_batch(boids, sub_start, sub_end, layer, game_world, current_time);
                            thread_pool->decrement_active();
                        });
                    }
                }
                
                // Wait for all tasks to complete
                thread_pool->wait_all();
            }
        }
    }
    
    // Process a specific batch of boids
    void process_boid_batch(const std::vector<BoidComponent*>& boids, 
                           size_t start_idx, size_t end_idx, 
                           int layer, Object* game_world, double current_time) {
        if (!game_world) return;
        
        // Pre-fetch positions for cache efficiency
        std::vector<Vector2> positions;
        positions.reserve(end_idx - start_idx);
        
        for (size_t i = start_idx; i < end_idx; i++) {
            BoidComponent* boid = boids[i];
            positions.push_back(boid->get_global_position());
        }
        
        // Process each boid in the batch
        for (size_t i = start_idx; i < end_idx; i++) {
            BoidComponent* boid = boids[i];
            if (boid->get_boid_layer() == 3) continue;
            
            Vector2 position = positions[i - start_idx];
            double desired_distance = boid->get_desired_distance();
            double desired_distance_squared = desired_distance * desired_distance;
            double separation_weight = boid->get_separation_weight();
            
            // Check if we need to update the spatial cache
            bool update_cache = true;
            TypedArray<Node> nearby_enemies;
            
            {
                std::lock_guard<std::mutex> lock(boid_mutex);
                auto cache_it = spatial_cache.find(boid);
                if (cache_it != spatial_cache.end()) {
                    SpatialCache& cache = cache_it->second;
                    
                    // Only update if position changed significantly or cache is old
                    double position_diff = (cache.position - position).length_squared();
                    double time_diff = current_time - cache.last_update_time;
                    
                    if (position_diff < 100.0 && time_diff < cache_lifetime) {
                        update_cache = false;
                        nearby_enemies = cache.nearby_boids;
                    }
                }
            }
            
            // Update spatial cache if needed
            if (update_cache) {
                // This call to game_world is thread-safe in our design
                nearby_enemies = get_enemies_safely(game_world, position, desired_distance, layer);
                
                // Update cache
                SpatialCache cache;
                cache.position = position;
                cache.radius = desired_distance;
                cache.nearby_boids = nearby_enemies;
                cache.last_update_time = current_time;
                
                std::lock_guard<std::mutex> lock(boid_mutex);
                spatial_cache[boid] = cache;
            }
            
            // Calculate separation force
            Vector2 separation_force(0, 0);
            
            for (int j = 0; j < nearby_enemies.size(); j++) {
                Node* enemy_node = Object::cast_to<Node>(nearby_enemies[j]);
                if (!enemy_node || enemy_node == boid->get_parent()) {
                    continue;
                }
                
                Vector2 enemy_pos = Object::cast_to<Node2D>(enemy_node)->get_global_position();
                Vector2 offset = position - enemy_pos;
                double distance_squared = offset.length_squared();
                
                if (distance_squared < desired_distance_squared && distance_squared > 0) {
                    double distance = Math::sqrt(distance_squared);
                    separation_force += offset.normalized() * ((desired_distance - distance) / desired_distance);
                }
            }
            
            // Apply separation force
            boid->set_calculated_velocity(separation_force * separation_weight);
        }
    }
    
    // Property setters and getters
    void set_batch_size(int size) {
        batch_size = Math::max(1, size);
    }
    
    int get_batch_size() const {
        return batch_size;
    }
    
    void set_thread_count(int count) {
        if (count == thread_count) return;
        
        thread_count = Math::max(1, count);
        
        // Recreate thread pool with new count
        thread_pool.reset();
        thread_pool = std::make_unique<ThreadPool>(thread_count);
    }
    
    int get_thread_count() const {
        return thread_count;
    }
    
    void set_update_frequency(int frequency) {
        update_frequency = Math::max(1, frequency);
        current_batch = 0;
    }
    
    int get_update_frequency() const {
        return update_frequency;
    }
    
    void set_cache_lifetime(double seconds) {
        cache_lifetime = Math::max(0.0, seconds);
    }
    
    double get_cache_lifetime() const {
        return cache_lifetime;
    }
};

// Register the class
void register_boid_manager() {
    ClassDB::register_class<BoidManager>();
}