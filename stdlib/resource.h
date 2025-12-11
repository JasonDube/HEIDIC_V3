// EDEN ENGINE - Resource<T> Template Wrapper
// Generic resource wrapper with hot-reload, file watching, and RAII lifecycle management
// Works with any resource type (TextureResource, MeshResource, etc.)

#ifndef EDEN_RESOURCE_H
#define EDEN_RESOURCE_H

#include <memory>
#include <string>
#include <ctime>
#include <stdexcept>

#ifdef _WIN32
#include <sys/stat.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

/**
 * Resource<T> - Generic resource wrapper with hot-reload support
 * 
 * Provides:
 * - RAII lifecycle management
 * - File modification time tracking
 * - Hot-reload capability (check and reload on file change)
 * - Convenient accessors (get(), operator*, operator->)
 * 
 * Usage:
 *   Resource<TextureResource> texture("textures/brick.dds");
 *   auto* tex = texture.get();
 *   texture.reload(); // Check for file changes and reload if needed
 */
template<typename T>
class Resource {
private:
    std::unique_ptr<T> m_data;
    std::string m_path;
    std::time_t m_lastModified;
    bool m_loaded;
    
    /**
     * Get file modification time
     * @return Modification time, or 0 if file doesn't exist
     */
    std::time_t getFileModificationTime(const std::string& filepath) {
#ifdef _WIN32
        struct _stat fileStat;
        if (_stat(filepath.c_str(), &fileStat) == 0) {
            return fileStat.st_mtime;
        }
#else
        struct stat fileStat;
        if (stat(filepath.c_str(), &fileStat) == 0) {
            return fileStat.st_mtime;
        }
#endif
        return 0;
    }
    
    /**
     * Load the resource from file
     * @throws std::runtime_error if loading fails
     */
    void loadResource() {
        try {
            m_data = std::make_unique<T>(m_path);
            m_lastModified = getFileModificationTime(m_path);
            m_loaded = true;
        } catch (const std::exception& e) {
            m_loaded = false;
            throw std::runtime_error("Failed to load resource '" + m_path + "': " + e.what());
        }
    }

public:
    /**
     * Constructor - Stores path but does NOT load resource yet (lazy loading)
     * Resource will be loaded on first access (get(), operator*, operator->)
     * This allows resources to be declared as globals before Vulkan is initialized
     * @param filepath Path to resource file
     */
    explicit Resource(const std::string& filepath) 
        : m_path(filepath), m_lastModified(0), m_loaded(false) {
        // Don't load yet - lazy load on first access
        // This allows resources to be declared before Vulkan is initialized
    }
    
    // Delete copy constructor and assignment (resources are unique)
    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;
    
    // Move constructor
    Resource(Resource&& other) noexcept
        : m_data(std::move(other.m_data)),
          m_path(std::move(other.m_path)),
          m_lastModified(other.m_lastModified),
          m_loaded(other.m_loaded) {
        other.m_loaded = false;
        other.m_lastModified = 0;
    }
    
    // Move assignment
    Resource& operator=(Resource&& other) noexcept {
        if (this != &other) {
            m_data = std::move(other.m_data);
            m_path = std::move(other.m_path);
            m_lastModified = other.m_lastModified;
            m_loaded = other.m_loaded;
            other.m_loaded = false;
            other.m_lastModified = 0;
        }
        return *this;
    }
    
    // Destructor - RAII cleanup (unique_ptr handles destruction)
    ~Resource() = default;
    
    /**
     * Get raw pointer to resource (lazy loads if not already loaded)
     * @return Pointer to resource, or nullptr if loading failed
     */
    T* get() {
        if (!m_loaded && !m_path.empty()) {
            try {
                loadResource();
            } catch (const std::exception& e) {
                // Loading failed - return nullptr
                return nullptr;
            }
        }
        return m_data.get();
    }
    
    /**
     * Get const raw pointer to resource
     * @return Const pointer to resource, or nullptr if not loaded
     */
    const T* get() const {
        return m_data.get();
    }
    
    /**
     * Dereference operator - get reference to resource (lazy loads if needed)
     * @return Reference to resource
     * @throws std::runtime_error if resource is not loaded or loading fails
     */
    T& operator*() {
        if (!m_loaded && !m_path.empty()) {
            loadResource();
        }
        if (!m_loaded || !m_data) {
            throw std::runtime_error("Resource '" + m_path + "' is not loaded");
        }
        return *m_data;
    }
    
    /**
     * Const dereference operator
     */
    const T& operator*() const {
        if (!m_loaded || !m_data) {
            throw std::runtime_error("Resource '" + m_path + "' is not loaded");
        }
        return *m_data;
    }
    
    /**
     * Member access operator (lazy loads if needed)
     * @return Pointer to resource
     * @throws std::runtime_error if resource is not loaded or loading fails
     */
    T* operator->() {
        if (!m_loaded && !m_path.empty()) {
            loadResource();
        }
        if (!m_loaded || !m_data) {
            throw std::runtime_error("Resource '" + m_path + "' is not loaded");
        }
        return m_data.get();
    }
    
    /**
     * Const member access operator
     */
    const T* operator->() const {
        if (!m_loaded || !m_data) {
            throw std::runtime_error("Resource '" + m_path + "' is not loaded");
        }
        return m_data.get();
    }
    
    /**
     * Check if resource is loaded
     * @return true if loaded, false otherwise
     */
    bool isLoaded() const {
        return m_loaded && m_data != nullptr;
    }
    
    /**
     * Get resource file path
     * @return File path
     */
    const std::string& getPath() const {
        return m_path;
    }
    
    /**
     * Get last modification time
     * @return Last modification time
     */
    std::time_t getLastModified() const {
        return m_lastModified;
    }
    
    /**
     * Check if file has been modified and reload if needed
     * This is the hot-reload method for CONTINUUM integration
     * @return true if resource was reloaded, false otherwise
     */
    bool reload() {
        if (m_path.empty()) {
            return false;
        }
        
        std::time_t currentModified = getFileModificationTime(m_path);
        
        // Check if file has been modified
        if (currentModified > m_lastModified && currentModified > 0) {
            try {
                // Destroy old resource
                m_data.reset();
                m_loaded = false;
                
                // Load new resource
                loadResource();
                
                return true; // Reloaded successfully
            } catch (const std::exception& e) {
                // Reload failed - log error but don't throw
                // (allows game to continue running even if reload fails)
                m_loaded = false;
                return false;
            }
        }
        
        return false; // No reload needed
    }
    
    /**
     * Force reload resource (even if file hasn't changed)
     * @return true if reloaded successfully, false otherwise
     */
    bool forceReload() {
        if (m_path.empty()) {
            return false;
        }
        
        try {
            // Destroy old resource
            m_data.reset();
            m_loaded = false;
            
            // Load new resource
            loadResource();
            
            return true;
        } catch (const std::exception& e) {
            m_loaded = false;
            return false;
        }
    }
    
    /**
     * Reset resource (unload)
     */
    void reset() {
        m_data.reset();
        m_loaded = false;
        m_lastModified = 0;
    }
};

#endif // EDEN_RESOURCE_H

