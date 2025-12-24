// EDEN ENGINE - VideoResource Class
// Unified video loading: handles MP4, AVI, MKV, WebM video files with audio
// Uses FFmpeg for decoding and SDL3 for rendering/audio playback
//
// Supports:
// - Video: Full video files with synchronized audio playback
// - Frame extraction: Get individual frames as textures
// - Seeking: Jump to specific positions in the video
// - Looping: Automatic loop playback

#ifndef EDEN_VIDEO_RESOURCE_H
#define EDEN_VIDEO_RESOURCE_H

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <cstring>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <chrono>

// FFmpeg headers (C API)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
}

// SDL3 for rendering and audio
#ifdef __has_include
    #if __has_include(<SDL3/SDL.h>)
        #include <SDL3/SDL.h>
        #include <SDL3/SDL_audio.h>
        #define SDL3_VIDEO_AVAILABLE
    #endif
#endif

// Stub types if SDL3 not available
#ifndef SDL3_VIDEO_AVAILABLE
typedef void* SDL_AudioStream;
typedef void* SDL_Texture;
typedef void* SDL_Renderer;
struct SDL_AudioSpec {
    int freq;
    uint16_t format;
    uint8_t channels;
};
#endif

/**
 * VideoResource - Unified video loading and playback with audio
 * 
 * Uses FFmpeg for decoding and SDL3 for rendering/audio output.
 * Supports hot-reload: video files can be reloaded at runtime.
 * 
 * Features:
 * - MP4, AVI, MKV, WebM support (any FFmpeg-supported format)
 * - Synchronized audio playback
 * - Frame-by-frame access for texture rendering
 * - Seeking to specific timestamps
 * - Loop playback support
 */
class VideoResource {
public:
    enum class PlayState {
        Stopped,
        Playing,
        Paused
    };

    struct VideoFrame {
        std::vector<uint8_t> data;  // RGBA pixel data
        int width = 0;
        int height = 0;
        double pts = 0.0;  // Presentation timestamp in seconds
    };

private:
    std::string m_path;
    bool m_loaded = false;
    
    // FFmpeg state
    AVFormatContext* m_formatCtx = nullptr;
    AVCodecContext* m_videoCodecCtx = nullptr;
    AVCodecContext* m_audioCodecCtx = nullptr;
    SwsContext* m_swsCtx = nullptr;
    SwrContext* m_swrCtx = nullptr;
    
    int m_videoStreamIdx = -1;
    int m_audioStreamIdx = -1;
    
    // Video properties
    int m_width = 0;
    int m_height = 0;
    double m_fps = 0.0;
    double m_duration = 0.0;  // Duration in seconds
    
    // Audio properties
    int m_sampleRate = 0;
    int m_audioChannels = 0;
    
    // Playback state
    std::atomic<PlayState> m_playState{PlayState::Stopped};
    std::atomic<bool> m_looping{false};
    std::atomic<double> m_currentTime{0.0};
    
    // Threading
    std::thread m_decodeThread;
    std::atomic<bool> m_stopThread{false};
    std::mutex m_frameMutex;
    std::mutex m_audioMutex;
    
    // Frame buffer (circular buffer for decoded frames)
    std::queue<VideoFrame> m_frameQueue;
    static constexpr size_t MAX_FRAME_QUEUE = 8;
    
    // Audio buffer
    std::queue<std::vector<uint8_t>> m_audioQueue;
    static constexpr size_t MAX_AUDIO_QUEUE = 32;
    
    // Current frame for rendering
    VideoFrame m_currentFrame;
    
    // SDL3 audio
    SDL_AudioStream* m_audioStream = nullptr;
    
    // Timing
    std::chrono::steady_clock::time_point m_playStartTime;
    double m_playStartPts = 0.0;
    
    /**
     * Initialize FFmpeg decoder for both video and audio streams
     */
    bool initFFmpeg() {
        // Open input file
        if (avformat_open_input(&m_formatCtx, m_path.c_str(), nullptr, nullptr) < 0) {
            std::cerr << "[Video] Failed to open file: " << m_path << std::endl;
            return false;
        }
        
        // Find stream info
        if (avformat_find_stream_info(m_formatCtx, nullptr) < 0) {
            std::cerr << "[Video] Failed to find stream info" << std::endl;
            return false;
        }
        
        // Find video and audio streams
        for (unsigned int i = 0; i < m_formatCtx->nb_streams; i++) {
            AVStream* stream = m_formatCtx->streams[i];
            AVCodecParameters* codecPar = stream->codecpar;
            
            if (codecPar->codec_type == AVMEDIA_TYPE_VIDEO && m_videoStreamIdx < 0) {
                m_videoStreamIdx = i;
            } else if (codecPar->codec_type == AVMEDIA_TYPE_AUDIO && m_audioStreamIdx < 0) {
                m_audioStreamIdx = i;
            }
        }
        
        if (m_videoStreamIdx < 0) {
            std::cerr << "[Video] No video stream found" << std::endl;
            return false;
        }
        
        // Initialize video decoder
        if (!initVideoDecoder()) {
            return false;
        }
        
        // Initialize audio decoder (optional - video can play without audio)
        if (m_audioStreamIdx >= 0) {
            if (!initAudioDecoder()) {
                std::cerr << "[Video] Warning: Audio decoder init failed, playing without audio" << std::endl;
                m_audioStreamIdx = -1;
            }
        }
        
        // Get video properties
        AVStream* videoStream = m_formatCtx->streams[m_videoStreamIdx];
        m_width = m_videoCodecCtx->width;
        m_height = m_videoCodecCtx->height;
        m_fps = av_q2d(videoStream->avg_frame_rate);
        if (m_fps <= 0) {
            m_fps = av_q2d(videoStream->r_frame_rate);
        }
        if (m_fps <= 0) {
            m_fps = 30.0;  // Default fallback
        }
        m_duration = (double)m_formatCtx->duration / AV_TIME_BASE;
        
        // Initialize SWS context for pixel format conversion (to RGBA)
        m_swsCtx = sws_getContext(
            m_width, m_height, m_videoCodecCtx->pix_fmt,
            m_width, m_height, AV_PIX_FMT_RGBA,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        
        if (!m_swsCtx) {
            std::cerr << "[Video] Failed to create SWS context" << std::endl;
            return false;
        }
        
        return true;
    }
    
    bool initVideoDecoder() {
        AVStream* stream = m_formatCtx->streams[m_videoStreamIdx];
        const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
        
        if (!codec) {
            std::cerr << "[Video] Unsupported video codec" << std::endl;
            return false;
        }
        
        m_videoCodecCtx = avcodec_alloc_context3(codec);
        if (!m_videoCodecCtx) {
            std::cerr << "[Video] Failed to allocate video codec context" << std::endl;
            return false;
        }
        
        if (avcodec_parameters_to_context(m_videoCodecCtx, stream->codecpar) < 0) {
            std::cerr << "[Video] Failed to copy video codec params" << std::endl;
            return false;
        }
        
        if (avcodec_open2(m_videoCodecCtx, codec, nullptr) < 0) {
            std::cerr << "[Video] Failed to open video codec" << std::endl;
            return false;
        }
        
        return true;
    }
    
    bool initAudioDecoder() {
        AVStream* stream = m_formatCtx->streams[m_audioStreamIdx];
        const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
        
        if (!codec) {
            std::cerr << "[Video] Unsupported audio codec" << std::endl;
            return false;
        }
        
        m_audioCodecCtx = avcodec_alloc_context3(codec);
        if (!m_audioCodecCtx) {
            return false;
        }
        
        if (avcodec_parameters_to_context(m_audioCodecCtx, stream->codecpar) < 0) {
            return false;
        }
        
        if (avcodec_open2(m_audioCodecCtx, codec, nullptr) < 0) {
            return false;
        }
        
        // Get audio properties
        m_sampleRate = m_audioCodecCtx->sample_rate;
        m_audioChannels = m_audioCodecCtx->ch_layout.nb_channels;
        
        // Initialize resampler (convert to S16 stereo for SDL)
        m_swrCtx = swr_alloc();
        if (!m_swrCtx) {
            return false;
        }
        
        AVChannelLayout outLayout;
        av_channel_layout_default(&outLayout, 2);  // Stereo output
        
        av_opt_set_chlayout(m_swrCtx, "in_chlayout", &m_audioCodecCtx->ch_layout, 0);
        av_opt_set_chlayout(m_swrCtx, "out_chlayout", &outLayout, 0);
        av_opt_set_int(m_swrCtx, "in_sample_rate", m_sampleRate, 0);
        av_opt_set_int(m_swrCtx, "out_sample_rate", 44100, 0);  // Standard output rate
        av_opt_set_sample_fmt(m_swrCtx, "in_sample_fmt", m_audioCodecCtx->sample_fmt, 0);
        av_opt_set_sample_fmt(m_swrCtx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);
        
        if (swr_init(m_swrCtx) < 0) {
            swr_free(&m_swrCtx);
            m_swrCtx = nullptr;
            return false;
        }
        
        return true;
    }
    
    /**
     * Initialize SDL3 audio output
     */
    bool initSDLAudio() {
#ifdef SDL3_VIDEO_AVAILABLE
        if (m_audioStreamIdx < 0) {
            return true;  // No audio stream, nothing to init
        }
        
        if (SDL_Init(SDL_INIT_AUDIO) == 0) {
            std::cerr << "[Video] SDL_Init failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        SDL_AudioSpec spec;
        spec.freq = 44100;
        spec.channels = 2;
        
        #ifdef SDL_AUDIO_S16SYS
            spec.format = SDL_AUDIO_S16SYS;
        #elif defined(SDL_AUDIO_S16)
            spec.format = SDL_AUDIO_S16;
        #else
            typedef decltype(spec.format) AudioFormatType;
            spec.format = static_cast<AudioFormatType>(0x8010);
        #endif
        
        m_audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (!m_audioStream) {
            std::cerr << "[Video] Failed to open audio device: " << SDL_GetError() << std::endl;
            return false;
        }
        
        return true;
#else
        return true;  // Stub - no audio without SDL3
#endif
    }
    
    /**
     * Decode thread function - continuously decodes frames and audio
     */
    void decodeThreadFunc() {
        AVPacket* packet = av_packet_alloc();
        AVFrame* frame = av_frame_alloc();
        AVFrame* rgbaFrame = av_frame_alloc();
        
        // Allocate RGBA frame buffer
        int rgbaSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, m_width, m_height, 1);
        std::vector<uint8_t> rgbaBuffer(rgbaSize);
        av_image_fill_arrays(rgbaFrame->data, rgbaFrame->linesize, rgbaBuffer.data(),
                            AV_PIX_FMT_RGBA, m_width, m_height, 1);
        
        while (!m_stopThread.load()) {
            if (m_playState.load() != PlayState::Playing) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            
            // Check if queues are full
            {
                std::lock_guard<std::mutex> lock(m_frameMutex);
                if (m_frameQueue.size() >= MAX_FRAME_QUEUE) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    continue;
                }
            }
            
            // Read next packet
            int ret = av_read_frame(m_formatCtx, packet);
            if (ret < 0) {
                if (ret == AVERROR_EOF) {
                    if (m_looping.load()) {
                        // Seek to beginning and continue
                        av_seek_frame(m_formatCtx, -1, 0, AVSEEK_FLAG_BACKWARD);
                        avcodec_flush_buffers(m_videoCodecCtx);
                        if (m_audioCodecCtx) {
                            avcodec_flush_buffers(m_audioCodecCtx);
                        }
                        m_playStartTime = std::chrono::steady_clock::now();
                        m_playStartPts = 0.0;
                        continue;
                    } else {
                        m_playState.store(PlayState::Stopped);
                        break;
                    }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            
            // Decode video packet
            if (packet->stream_index == m_videoStreamIdx) {
                ret = avcodec_send_packet(m_videoCodecCtx, packet);
                if (ret >= 0) {
                    while (ret >= 0) {
                        ret = avcodec_receive_frame(m_videoCodecCtx, frame);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                            break;
                        }
                        if (ret < 0) {
                            break;
                        }
                        
                        // Convert to RGBA
                        sws_scale(m_swsCtx,
                                 frame->data, frame->linesize, 0, m_height,
                                 rgbaFrame->data, rgbaFrame->linesize);
                        
                        // Create frame
                        VideoFrame vf;
                        vf.width = m_width;
                        vf.height = m_height;
                        vf.data.assign(rgbaBuffer.begin(), rgbaBuffer.end());
                        
                        // Calculate PTS
                        AVStream* stream = m_formatCtx->streams[m_videoStreamIdx];
                        vf.pts = frame->pts * av_q2d(stream->time_base);
                        
                        // Add to queue
                        {
                            std::lock_guard<std::mutex> lock(m_frameMutex);
                            m_frameQueue.push(std::move(vf));
                        }
                    }
                }
            }
            // Decode audio packet
            else if (packet->stream_index == m_audioStreamIdx && m_audioCodecCtx && m_swrCtx) {
                ret = avcodec_send_packet(m_audioCodecCtx, packet);
                if (ret >= 0) {
                    while (ret >= 0) {
                        ret = avcodec_receive_frame(m_audioCodecCtx, frame);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                            break;
                        }
                        if (ret < 0) {
                            break;
                        }
                        
                        // Resample audio
                        int outSamples = swr_get_out_samples(m_swrCtx, frame->nb_samples);
                        std::vector<uint8_t> audioData(outSamples * 2 * 2);  // Stereo S16
                        uint8_t* outBuf = audioData.data();
                        
                        int converted = swr_convert(m_swrCtx, &outBuf, outSamples,
                                                   (const uint8_t**)frame->data, frame->nb_samples);
                        
                        if (converted > 0) {
                            audioData.resize(converted * 2 * 2);
                            
                            // Add to audio queue
                            {
                                std::lock_guard<std::mutex> lock(m_audioMutex);
                                if (m_audioQueue.size() < MAX_AUDIO_QUEUE) {
                                    m_audioQueue.push(std::move(audioData));
                                }
                            }
                            
                            // Feed audio to SDL
#ifdef SDL3_VIDEO_AVAILABLE
                            if (m_audioStream) {
                                std::lock_guard<std::mutex> lock(m_audioMutex);
                                if (!m_audioQueue.empty()) {
                                    auto& data = m_audioQueue.front();
                                    SDL_PutAudioStreamData(m_audioStream, data.data(), static_cast<int>(data.size()));
                                    m_audioQueue.pop();
                                }
                            }
#endif
                        }
                    }
                }
            }
            
            av_packet_unref(packet);
        }
        
        av_frame_free(&frame);
        av_frame_free(&rgbaFrame);
        av_packet_free(&packet);
    }

public:
    /**
     * Constructor - Loads video file
     * @param filepath Path to video file (MP4, AVI, MKV, WebM, etc.)
     */
    explicit VideoResource(const std::string& filepath) 
        : m_path(filepath) {
        
        if (!initFFmpeg()) {
            throw std::runtime_error("Failed to initialize video: " + filepath);
        }
        
        if (!initSDLAudio()) {
            std::cerr << "[Video] Warning: Audio init failed, playing without audio" << std::endl;
        }
        
        m_loaded = true;
        
        std::cout << "[Video] Loaded: " << filepath << std::endl;
        std::cout << "[Video] Resolution: " << m_width << "x" << m_height << std::endl;
        std::cout << "[Video] FPS: " << m_fps << std::endl;
        std::cout << "[Video] Duration: " << m_duration << "s" << std::endl;
        if (m_audioStreamIdx >= 0) {
            std::cout << "[Video] Audio: " << m_sampleRate << "Hz, " << m_audioChannels << " channels" << std::endl;
        }
    }
    
    // Delete copy constructor and assignment
    VideoResource(const VideoResource&) = delete;
    VideoResource& operator=(const VideoResource&) = delete;
    
    // Move constructor
    VideoResource(VideoResource&& other) noexcept
        : m_path(std::move(other.m_path)),
          m_loaded(other.m_loaded),
          m_formatCtx(other.m_formatCtx),
          m_videoCodecCtx(other.m_videoCodecCtx),
          m_audioCodecCtx(other.m_audioCodecCtx),
          m_swsCtx(other.m_swsCtx),
          m_swrCtx(other.m_swrCtx),
          m_videoStreamIdx(other.m_videoStreamIdx),
          m_audioStreamIdx(other.m_audioStreamIdx),
          m_width(other.m_width),
          m_height(other.m_height),
          m_fps(other.m_fps),
          m_duration(other.m_duration),
          m_sampleRate(other.m_sampleRate),
          m_audioChannels(other.m_audioChannels),
          m_audioStream(other.m_audioStream) {
        
        other.m_formatCtx = nullptr;
        other.m_videoCodecCtx = nullptr;
        other.m_audioCodecCtx = nullptr;
        other.m_swsCtx = nullptr;
        other.m_swrCtx = nullptr;
        other.m_audioStream = nullptr;
        other.m_loaded = false;
    }
    
    /**
     * Destructor - Cleanup all resources
     */
    ~VideoResource() {
        stop();
        cleanup();
    }
    
    /**
     * Cleanup all resources
     */
    void cleanup() {
        // Stop decode thread
        m_stopThread.store(true);
        if (m_decodeThread.joinable()) {
            m_decodeThread.join();
        }
        
#ifdef SDL3_VIDEO_AVAILABLE
        if (m_audioStream) {
            SDL_PauseAudioStreamDevice(m_audioStream);
            SDL_DestroyAudioStream(m_audioStream);
            m_audioStream = nullptr;
        }
#endif
        
        if (m_swrCtx) {
            swr_free(&m_swrCtx);
            m_swrCtx = nullptr;
        }
        
        if (m_swsCtx) {
            sws_freeContext(m_swsCtx);
            m_swsCtx = nullptr;
        }
        
        if (m_audioCodecCtx) {
            avcodec_free_context(&m_audioCodecCtx);
            m_audioCodecCtx = nullptr;
        }
        
        if (m_videoCodecCtx) {
            avcodec_free_context(&m_videoCodecCtx);
            m_videoCodecCtx = nullptr;
        }
        
        if (m_formatCtx) {
            avformat_close_input(&m_formatCtx);
            m_formatCtx = nullptr;
        }
        
        // Clear queues
        {
            std::lock_guard<std::mutex> lock(m_frameMutex);
            while (!m_frameQueue.empty()) m_frameQueue.pop();
        }
        {
            std::lock_guard<std::mutex> lock(m_audioMutex);
            while (!m_audioQueue.empty()) m_audioQueue.pop();
        }
        
        m_loaded = false;
    }
    
    /**
     * Start video playback
     * @param loop Whether to loop the video (default: false)
     * @return true if playback started successfully
     */
    bool play(bool loop = false) {
        if (!m_loaded) {
            return false;
        }
        
        if (m_playState.load() == PlayState::Playing) {
            return true;  // Already playing
        }
        
        m_looping.store(loop);
        
        // If paused, resume
        if (m_playState.load() == PlayState::Paused) {
            m_playStartTime = std::chrono::steady_clock::now();
            m_playState.store(PlayState::Playing);
            
#ifdef SDL3_VIDEO_AVAILABLE
            if (m_audioStream) {
                SDL_ResumeAudioStreamDevice(m_audioStream);
            }
#endif
            return true;
        }
        
        // Start from beginning
        m_stopThread.store(false);
        m_playState.store(PlayState::Playing);
        m_playStartTime = std::chrono::steady_clock::now();
        m_playStartPts = 0.0;
        m_currentTime.store(0.0);
        
#ifdef SDL3_VIDEO_AVAILABLE
        if (m_audioStream) {
            SDL_ResumeAudioStreamDevice(m_audioStream);
        }
#endif
        
        // Start decode thread
        if (!m_decodeThread.joinable()) {
            m_decodeThread = std::thread(&VideoResource::decodeThreadFunc, this);
        }
        
        return true;
    }
    
    /**
     * Pause video playback
     */
    void pause() {
        if (m_playState.load() == PlayState::Playing) {
            m_playState.store(PlayState::Paused);
            
#ifdef SDL3_VIDEO_AVAILABLE
            if (m_audioStream) {
                SDL_PauseAudioStreamDevice(m_audioStream);
            }
#endif
        }
    }
    
    /**
     * Stop video playback and reset to beginning
     */
    void stop() {
        m_playState.store(PlayState::Stopped);
        
        // Stop decode thread
        m_stopThread.store(true);
        if (m_decodeThread.joinable()) {
            m_decodeThread.join();
        }
        
#ifdef SDL3_VIDEO_AVAILABLE
        if (m_audioStream) {
            SDL_PauseAudioStreamDevice(m_audioStream);
            SDL_ClearAudioStream(m_audioStream);
        }
#endif
        
        // Clear queues
        {
            std::lock_guard<std::mutex> lock(m_frameMutex);
            while (!m_frameQueue.empty()) m_frameQueue.pop();
        }
        
        // Reset to beginning
        if (m_formatCtx) {
            av_seek_frame(m_formatCtx, -1, 0, AVSEEK_FLAG_BACKWARD);
            avcodec_flush_buffers(m_videoCodecCtx);
            if (m_audioCodecCtx) {
                avcodec_flush_buffers(m_audioCodecCtx);
            }
        }
        
        m_currentTime.store(0.0);
        m_stopThread.store(false);
    }
    
    /**
     * Seek to a specific position
     * @param seconds Position in seconds
     */
    void seek(double seconds) {
        if (!m_loaded || !m_formatCtx) {
            return;
        }
        
        seconds = std::max(0.0, std::min(seconds, m_duration));
        
        int64_t timestamp = static_cast<int64_t>(seconds * AV_TIME_BASE);
        av_seek_frame(m_formatCtx, -1, timestamp, AVSEEK_FLAG_BACKWARD);
        
        avcodec_flush_buffers(m_videoCodecCtx);
        if (m_audioCodecCtx) {
            avcodec_flush_buffers(m_audioCodecCtx);
        }
        
        // Clear queues
        {
            std::lock_guard<std::mutex> lock(m_frameMutex);
            while (!m_frameQueue.empty()) m_frameQueue.pop();
        }
        
        m_playStartPts = seconds;
        m_playStartTime = std::chrono::steady_clock::now();
        m_currentTime.store(seconds);
    }
    
    /**
     * Update video playback - call this every frame
     * Updates current time and gets next frame if needed
     * @return true if a new frame is available
     */
    bool update() {
        if (m_playState.load() != PlayState::Playing) {
            return false;
        }
        
        // Calculate current playback time
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - m_playStartTime).count();
        double currentTime = m_playStartPts + elapsed;
        m_currentTime.store(currentTime);
        
        // Get next frame that should be displayed
        std::lock_guard<std::mutex> lock(m_frameMutex);
        
        while (!m_frameQueue.empty()) {
            VideoFrame& front = m_frameQueue.front();
            
            // If frame is ready to display (or we need to catch up)
            if (front.pts <= currentTime) {
                m_currentFrame = std::move(front);
                m_frameQueue.pop();
                
                // Keep getting frames until we find one that's in the future
                if (m_frameQueue.empty() || m_frameQueue.front().pts > currentTime) {
                    return true;
                }
            } else {
                break;  // Frame is in the future, wait
            }
        }
        
        return false;
    }
    
    /**
     * Get current frame pixel data (RGBA format)
     * @return Pointer to RGBA pixel data, or nullptr if no frame
     */
    const uint8_t* getFrameData() const {
        if (m_currentFrame.data.empty()) {
            return nullptr;
        }
        return m_currentFrame.data.data();
    }
    
    /**
     * Get current frame as a copy
     * @return VideoFrame struct with pixel data
     */
    VideoFrame getCurrentFrame() const {
        return m_currentFrame;
    }
    
    // Getters
    bool isLoaded() const { return m_loaded; }
    bool isPlaying() const { return m_playState.load() == PlayState::Playing; }
    bool isPaused() const { return m_playState.load() == PlayState::Paused; }
    bool isStopped() const { return m_playState.load() == PlayState::Stopped; }
    bool hasAudio() const { return m_audioStreamIdx >= 0; }
    
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    double getFPS() const { return m_fps; }
    double getDuration() const { return m_duration; }
    double getCurrentTime() const { return m_currentTime.load(); }
    
    const std::string& getPath() const { return m_path; }
    
    /**
     * Reload video file (for hot-reload)
     */
    void reload() {
        bool wasPlaying = isPlaying();
        double position = getCurrentTime();
        
        stop();
        cleanup();
        
        if (initFFmpeg() && initSDLAudio()) {
            m_loaded = true;
            
            if (wasPlaying) {
                play(m_looping.load());
                seek(position);
            }
        }
    }
};

#endif // EDEN_VIDEO_RESOURCE_H

