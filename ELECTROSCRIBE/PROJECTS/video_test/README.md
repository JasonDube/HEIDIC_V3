# Video Resource Test

This project tests the new HEIDIC `Video` resource type for MP4 video playback with audio.

## What This Tests

✅ **Video Resource Declaration** - Using `resource` keyword to declare video resources  
✅ **MP4 Video Loading** - FFmpeg-based video decoding  
✅ **Audio Sync** - Synchronized audio playback via SDL3  
✅ **Zero Boilerplate** - No manual C++ code needed  
✅ **Automatic Loading** - Video resources load automatically when declared  

## Setup

### Prerequisites

1. **FFmpeg** - Required for video decoding
   - Download from https://ffmpeg.org/download.html
   - Set `FFMPEG_PATH` environment variable to the FFmpeg installation directory
   - Or install to `C:\ffmpeg` (default search location)

2. **SDL3** - Required for audio playback
   - Already set up if you have audio working in HEIDIC

### Add Your Video File

Place your MP4 video file in the `video/` folder:
```
video_test/
├── video_test.hd          # Main HEIDIC source file
├── README.md              # This file
└── video/                 # Video files directory
    └── intro.mp4          # Your MP4 video file
```

### Build and Run

1. Open this project in ELECTROSCRIBE editor
2. Click the white "Run" button (▶)
3. The video resource will be loaded and played automatically!

## HEIDIC Video Resource Syntax

```heidic
// Declare a video resource
resource IntroVideo: Video = "video/intro.mp4";
```

**This generates:**
```cpp
Resource<VideoResource> g_resource_introvideo("video/intro.mp4");
```

## Available Video Functions

For a video resource named `MyVideo`, these functions are automatically generated:

| Function | Description |
|----------|-------------|
| `play_video_myvideo(loop: i32)` | Start playback (loop=1 for looping) |
| `pause_video_myvideo()` | Pause playback |
| `stop_video_myvideo()` | Stop playback and reset |
| `seek_video_myvideo(seconds: f64)` | Seek to position |
| `update_video_myvideo()` | Update (call every frame), returns 1 if new frame |
| `get_video_frame_myvideo()` | Get RGBA frame data pointer |
| `get_video_width_myvideo()` | Get video width in pixels |
| `get_video_height_myvideo()` | Get video height in pixels |
| `get_video_duration_myvideo()` | Get video duration in seconds |
| `get_video_current_time_myvideo()` | Get current playback position |
| `is_video_playing_myvideo()` | Returns 1 if playing |

## Supported Video Formats

- **MP4** - MPEG-4 Part 14 (recommended)
- **AVI** - Audio Video Interleave
- **MKV** - Matroska Video
- **WebM** - Web Media
- Any other format supported by FFmpeg

## Features

- **Zero Boilerplate** - One line declares and loads the video resource
- **RAII Cleanup** - Automatic resource management
- **Hot-Reload Ready** - Resources support CONTINUUM hot-reload
- **Type Safety** - Resource types are checked at compile time
- **Audio Sync** - Audio is automatically synchronized with video
- **Seeking** - Jump to any position in the video
- **Looping** - Optional loop playback support

## Integration Example

To display video frames in your game:

```heidic
resource CutsceneVideo: Video = "video/cutscene.mp4";

fn render_cutscene(): void {
    // Start video
    play_video_cutsceneVideo(0);
    
    // In your render loop:
    if update_video_cutsceneVideo() == 1 {
        // New frame available!
        let frame_data = get_video_frame_cutsceneVideo();
        let width = get_video_width_cutsceneVideo();
        let height = get_video_height_cutsceneVideo();
        
        // Upload frame_data to a texture and render it
        // frame_data is RGBA format, size = width * height * 4 bytes
    }
}
```

## File Structure

```
video_test/
├── video_test.hd          # Main HEIDIC source file
├── video_test.cpp         # Generated C++ (auto-generated)
├── README.md              # This file
└── video/                 # Video files directory
    └── intro.mp4          # Video file (MP4 format)
```

