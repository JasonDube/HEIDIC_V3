// EDEN ENGINE - AudioResource Class
// Unified audio loading: handles WAV (uncompressed) and OGG (compressed) audio files
// Uses SDL3 audio for cross-platform audio playback
//
// Supports two types:
// - Sound: Short audio clips (effects, UI sounds) - loaded into memory
// - Music: Long audio tracks (background music) - streamed from disk

#ifndef EDEN_AUDIO_RESOURCE_H
#define EDEN_AUDIO_RESOURCE_H

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <cstring>

// stb_vorbis for OGG decoding (header-only library)
// Define implementation only once - check if already defined
#ifndef STB_VORBIS_IMPLEMENTATION
#define STB_VORBIS_IMPLEMENTATION
#endif
// Ensure stdio and integer conversion support are enabled
// stb_vorbis_decode_filename requires both
// (don't define STB_VORBIS_NO_STDIO or STB_VORBIS_NO_INTEGER_CONVERSION)
#include "stb_vorbis.h"

// SDL3 audio - check if SDL3 is available
// Note: We check for SDL3 by trying to include it
// The build system should add -I path for SDL3 if available
#ifdef __has_include
    #if __has_include(<SDL3/SDL.h>)
        #include <SDL3/SDL.h>
        #include <SDL3/SDL_audio.h>
        #define SDL3_AUDIO_AVAILABLE
    #endif
#endif

// Stub types if SDL3 not available (for compilation without audio)
#ifndef SDL3_AUDIO_AVAILABLE
typedef void* SDL_AudioStream;
typedef void* SDL_AudioDeviceID;
struct SDL_AudioSpec {
    int freq;
    uint16_t format;
    uint8_t channels;
    uint8_t silence;
    uint16_t samples;
    uint32_t size;
};
#endif

/**
 * AudioResource - Unified audio loading and playback
 * 
 * Automatically detects format (WAV vs OGG) and loads appropriately.
 * - WAV: Loaded into memory (fast playback, good for short sounds)
 * - OGG: Streamed from disk (memory efficient, good for long music)
 * 
 * Supports hot-reload: audio files can be reloaded at runtime.
 */
class AudioResource {
public:
    enum class AudioType {
        Sound,  // Short clips (effects) - loaded into memory
        Music   // Long tracks (background) - streamed
    };

private:
    std::string m_path;
    AudioType m_type;
    bool m_loaded = false;
    
    // SDL3 audio data
    SDL_AudioStream* m_audioStream = nullptr;
    SDL_AudioDeviceID m_audioDevice = 0;
    
    // For Sound type (loaded into memory)
    std::vector<uint8_t> m_audioData;
    SDL_AudioSpec m_spec;
    
    // For Music type (streamed)
    // SDL3 handles streaming internally
    
    /**
     * Detect audio type from file extension
     * @param filepath Path to audio file
     * @return AudioType (Sound for WAV, Music for OGG)
     */
    AudioType detectAudioType(const std::string& filepath) {
        std::string ext = filepath.substr(filepath.find_last_of(".") + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == "wav") {
            return AudioType::Sound;
        } else if (ext == "ogg" || ext == "mp3") {
            return AudioType::Music;
        }
        
        // Default to Sound for unknown formats
        return AudioType::Sound;
    }
    
    /**
     * Load WAV file into memory
     * @param filepath Path to WAV file
     */
    void loadWAV(const std::string& filepath) {
#ifdef SDL3_AUDIO_AVAILABLE
        SDL_AudioSpec spec;
        uint8_t* audioBuffer = nullptr;
        uint32_t audioLength = 0;
        
        if (!SDL_LoadWAV(filepath.c_str(), &spec, &audioBuffer, &audioLength)) {
            throw std::runtime_error("Failed to load WAV file: " + filepath + " - " + SDL_GetError());
        }
        
        // Store audio data
        m_audioData.assign(audioBuffer, audioBuffer + audioLength);
        m_spec = spec;
        
        // Free SDL's buffer (we've copied the data)
        SDL_free(audioBuffer);
        
        m_loaded = true;
#else
        // Stub implementation when SDL3 is not available
        // Just mark as loaded with empty data (allows compilation without SDL3)
        m_loaded = true;
        m_spec = SDL_AudioSpec{};
#endif
    }
    
    /**
     * Load OGG file using stb_vorbis decoder
     * @param filepath Path to OGG file
     */
    void loadOGG(const std::string& filepath) {
#ifdef SDL3_AUDIO_AVAILABLE
        // Use stb_vorbis to decode OGG file
        // Read file into memory first, then decode (more reliable than decode_filename)
        std::ifstream file(filepath, std::ios::binary | std::ios::ate);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open OGG file: " + filepath);
        }
        
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        std::vector<unsigned char> buffer(size);
        if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
            throw std::runtime_error("Failed to read OGG file: " + filepath);
        }
        file.close();
        
        // Decode OGG from memory
        int channels = 0;
        int sample_rate = 0;
        short* output = nullptr;
        
        int samples = stb_vorbis_decode_memory(buffer.data(), static_cast<int>(size), &channels, &sample_rate, &output);
        
        if (samples < 0 || !output) {
            throw std::runtime_error("Failed to decode OGG file: " + filepath);
        }
        
        // Convert 16-bit shorts to bytes for SDL3
        // stb_vorbis returns interleaved samples: [L, R, L, R, ...] for stereo
        int total_samples = samples * channels;
        m_audioData.resize(total_samples * sizeof(int16_t));
        std::memcpy(m_audioData.data(), output, total_samples * sizeof(int16_t));
        
        // Free stb_vorbis's buffer
        free(output);
        
        // Set up SDL audio spec
        // SDL3's AudioSpec only has: freq, format, channels (no silence, samples, size)
        m_spec.freq = sample_rate;
        m_spec.channels = static_cast<uint8_t>(channels);
        
        // Set format - SDL3 uses SDL_AudioFormat (typically uint16_t)
        // Try to use SDL3 format constants, fallback to numeric value
        #ifdef SDL_AUDIO_S16SYS
            m_spec.format = SDL_AUDIO_S16SYS;
        #elif defined(SDL_AUDIO_S16)
            m_spec.format = SDL_AUDIO_S16;
        #else
            // Fallback: cast to SDL_AudioFormat type (0x8010 = AUDIO_S16SYS)
            typedef decltype(m_spec.format) AudioFormatType;
            m_spec.format = static_cast<AudioFormatType>(0x8010);
        #endif
        
        m_loaded = true;
#else
        // Stub implementation when SDL3 is not available
        m_loaded = true;
        m_spec = SDL_AudioSpec{};
#endif
    }

public:
    /**
     * Constructor - Loads audio file
     * @param filepath Path to audio file (WAV or OGG)
     */
    explicit AudioResource(const std::string& filepath) 
        : m_path(filepath) {
        m_type = detectAudioType(filepath);
        
        // Load based on type
        std::string ext = filepath.substr(filepath.find_last_of(".") + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == "wav") {
            loadWAV(filepath);
        } else if (ext == "ogg" || ext == "mp3") {
            loadOGG(filepath);
        } else {
            throw std::runtime_error("Unsupported audio format: " + ext);
        }
    }
    
    // Delete copy constructor and assignment
    AudioResource(const AudioResource&) = delete;
    AudioResource& operator=(const AudioResource&) = delete;
    
    // Move constructor
    AudioResource(AudioResource&& other) noexcept
        : m_path(std::move(other.m_path)),
          m_type(other.m_type),
          m_loaded(other.m_loaded),
          m_audioStream(other.m_audioStream),
          m_audioDevice(other.m_audioDevice),
          m_audioData(std::move(other.m_audioData)),
          m_spec(other.m_spec) {
        other.m_audioStream = nullptr;
        other.m_audioDevice = 0;
        other.m_loaded = false;
    }
    
    // Move assignment
    AudioResource& operator=(AudioResource&& other) noexcept {
        if (this != &other) {
            cleanup();
            
            m_path = std::move(other.m_path);
            m_type = other.m_type;
            m_loaded = other.m_loaded;
            m_audioStream = other.m_audioStream;
            m_audioDevice = other.m_audioDevice;
            m_audioData = std::move(other.m_audioData);
            m_spec = other.m_spec;
            
            other.m_audioStream = nullptr;
            other.m_audioDevice = 0;
            other.m_loaded = false;
        }
        return *this;
    }
    
    /**
     * Destructor - Cleanup audio resources
     */
    ~AudioResource() {
        cleanup();
    }
    
    /**
     * Cleanup audio resources
     */
    void cleanup() {
#ifdef SDL3_AUDIO_AVAILABLE
        if (m_audioStream) {
            SDL_DestroyAudioStream(m_audioStream);
            m_audioStream = nullptr;
        }
        if (m_audioDevice) {
            SDL_CloseAudioDevice(m_audioDevice);
            m_audioDevice = 0;
        }
#endif
        m_audioData.clear();
        m_loaded = false;
    }
    
    /**
     * Play the audio
     * @param loop Whether to loop the audio (default: false)
     * @return true if playback started successfully
     */
    bool play(bool loop = false) {
#ifdef SDL3_AUDIO_AVAILABLE
        if (!m_loaded || m_audioData.empty()) {
            std::cerr << "[Audio] Not loaded or empty data" << std::endl;
            return false;
        }
        
        // Stop any existing playback
        stop();
        
        // Ensure SDL3 is initialized (SDL3 requires SDL_Init() before subsystems)
        // SDL3 returns 0 on failure, non-zero on success (opposite of SDL2!)
        // Note: SDL_Init can be called multiple times safely
        if (SDL_Init(SDL_INIT_AUDIO) == 0) {
            // Failed to initialize - SDL_GetError() will have details
            std::cerr << "[Audio] SDL_Init failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // SDL3 audio API: Use SDL_OpenAudioDeviceStream which combines device and stream creation
        // This is the recommended way in SDL3
        SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &m_spec, nullptr, nullptr);
        if (!stream) {
            // Stream creation failed
            std::cerr << "[Audio] SDL_OpenAudioDeviceStream failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // Queue the audio data
        int result = SDL_PutAudioStreamData(stream, m_audioData.data(), static_cast<int>(m_audioData.size()));
        if (result < 0) {
            std::cerr << "[Audio] SDL_PutAudioStreamData failed: " << SDL_GetError() << std::endl;
            SDL_DestroyAudioStream(stream);
            return false;
        }
        
        // Start playback - SDL3 uses SDL_ResumeAudioStreamDevice which takes the stream
        SDL_ResumeAudioStreamDevice(stream);
        
        // Store references for cleanup
        m_audioStream = stream;
        m_audioDevice = 0;  // Device is managed by the stream
        
        return true;
#else
        // Stub: return false when SDL3 is not available
        // This means SDL3 headers weren't found at compile time
        std::cerr << "[Audio] SDL3_AUDIO_AVAILABLE not defined - SDL3 headers not found" << std::endl;
        return false;
#endif
    }
    
    /**
     * Stop playback
     */
    void stop() {
#ifdef SDL3_AUDIO_AVAILABLE
        if (m_audioStream) {
            // Pause the stream device
            SDL_PauseAudioStreamDevice(m_audioStream);
            // Destroy the stream (this also closes the associated device)
            SDL_DestroyAudioStream(m_audioStream);
            m_audioStream = nullptr;
            m_audioDevice = 0;
        }
        // Also close device if we have it separately
        if (m_audioDevice) {
            SDL_CloseAudioDevice(m_audioDevice);
            m_audioDevice = 0;
        }
#endif
    }
    
    /**
     * Check if audio is loaded
     * @return true if loaded
     */
    bool isLoaded() const {
        return m_loaded;
    }
    
    /**
     * Get audio file path
     * @return File path
     */
    const std::string& getPath() const {
        return m_path;
    }
    
    /**
     * Get audio type
     * @return AudioType (Sound or Music)
     */
    AudioType getType() const {
        return m_type;
    }
    
    /**
     * Get audio data (for direct access)
     * @return Reference to audio data vector
     */
    const std::vector<uint8_t>& getAudioData() const {
        return m_audioData;
    }
    
    /**
     * Get audio spec (format, sample rate, etc.)
     * @return SDL_AudioSpec
     */
    const SDL_AudioSpec& getSpec() const {
        return m_spec;
    }
    
    /**
     * Reload audio file (for hot-reload)
     */
    void reload() {
        cleanup();
        
        std::string ext = m_path.substr(m_path.find_last_of(".") + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        
        if (ext == "wav") {
            loadWAV(m_path);
        } else if (ext == "ogg" || ext == "mp3") {
            loadOGG(m_path);
        }
    }
};

#endif // EDEN_AUDIO_RESOURCE_H

