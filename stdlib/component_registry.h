// Component Registry and Reflection System
// Provides runtime component metadata, registration, and reflection

#ifndef COMPONENT_REGISTRY_H
#define COMPONENT_REGISTRY_H

#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <cstddef> // for offsetof

// Component ID type
using ComponentId = uint32_t;

// Template function to get component ID
// Each component type gets a unique ID based on its type hash
template<typename T>
ComponentId component_id() {
    // Use type hash to generate unique ID
    // This ensures each component type gets a consistent ID
    static ComponentId id = 0;
    if (id == 0) {
        // Generate ID from type name hash (simplified - in production use better hashing)
        const char* type_name = typeid(T).name();
        ComponentId hash = 0;
        for (const char* p = type_name; *p; ++p) {
            hash = hash * 31 + *p;
        }
        id = hash ? hash : 1; // Ensure non-zero
    }
    return id;
}

// Component Metadata Template
template<typename T>
struct ComponentMetadata {
    static constexpr const char* name() { return "Unknown"; }
    static constexpr ComponentId id() { return component_id<T>(); }
    static constexpr size_t size() { return sizeof(T); }
    static constexpr size_t alignment() { return alignof(T); }
    static constexpr bool is_soa() { return false; }
};

// Component Fields Reflection Template
template<typename T>
struct ComponentFields {
    static constexpr size_t field_count = 0;
    struct FieldInfo {
        const char* name;
        const char* type_name;
        size_t offset;
        size_t size;
    };
    static constexpr FieldInfo fields[] = {};
};

// Component Registry
class ComponentRegistry {
public:
    // Register a component type
    template<typename T>
    static void register_component() {
        ComponentId id = ComponentMetadata<T>::id();
        const char* name = ComponentMetadata<T>::name();
        
        // Store metadata in registry
        get_instance().component_names[id] = name;
        get_instance().component_sizes[id] = ComponentMetadata<T>::size();
        get_instance().component_alignments[id] = ComponentMetadata<T>::alignment();
        get_instance().component_soa_flags[id] = ComponentMetadata<T>::is_soa();
    }
    
    // Convenience function: register<T>() is shorter than register_component<T>()
    template<typename T>
    static void register_() {
        register_component<T>();
    }
    
    // Get component name by ID
    static const char* get_name(ComponentId id) {
        auto it = get_instance().component_names.find(id);
        if (it != get_instance().component_names.end()) {
            return it->second;
        }
        return "Unknown";
    }
    
    // Get component size by ID
    static size_t get_size(ComponentId id) {
        auto it = get_instance().component_sizes.find(id);
        if (it != get_instance().component_sizes.end()) {
            return it->second;
        }
        return 0;
    }
    
    // Get component alignment by ID
    static size_t get_alignment(ComponentId id) {
        auto it = get_instance().component_alignments.find(id);
        if (it != get_instance().component_alignments.end()) {
            return it->second;
        }
        return 0;
    }
    
    // Check if component is SOA
    static bool is_soa(ComponentId id) {
        auto it = get_instance().component_soa_flags.find(id);
        if (it != get_instance().component_soa_flags.end()) {
            return it->second;
        }
        return false;
    }
    
    // Get field count for a component
    template<typename T>
    static size_t get_field_count() {
        return ComponentFields<T>::field_count;
    }
    
    // Get field info for a component
    template<typename T>
    static const typename ComponentFields<T>::FieldInfo* get_fields() {
        return ComponentFields<T>::get_fields();
    }
    
private:
    static ComponentRegistry& get_instance() {
        static ComponentRegistry instance;
        return instance;
    }
    
    std::unordered_map<ComponentId, const char*> component_names;
    std::unordered_map<ComponentId, size_t> component_sizes;
    std::unordered_map<ComponentId, size_t> component_alignments;
    std::unordered_map<ComponentId, bool> component_soa_flags;
};

// Helper macros for easier registration
#define REGISTER_COMPONENT(T) ComponentRegistry::register_component<T>()

#endif // COMPONENT_REGISTRY_H

