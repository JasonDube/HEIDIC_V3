# Audio Resource Test

This project tests the new HEIDIC audio resource types (`Sound` and `Music`) for zero-boilerplate audio loading.

## What This Tests

✅ **Audio Resource Declaration** - Using `resource` keyword to declare audio resources  
✅ **Sound Resources** - WAV file loading (short sound effects)  
✅ **Music Resources** - OGG file loading (background music)  
✅ **Zero Boilerplate** - No manual C++ code needed  
✅ **Automatic Loading** - Audio resources load automatically when declared  

## Setup

1. **Add your audio files:**
   - Place your WAV file as `audio/jump.wav` (or rename your file)
   - Place your OGG file as `audio/bgm.ogg` (or rename your file)
   - Update the resource paths in `audio_test.hd` if your files have different names

2. **Build and Run:**
   - Open this project in ELECTROSCRIBE editor
   - Click the white "Run" button (▶)
   - The audio resources will be loaded automatically!

## HEIDIC Audio Resource Syntax

```heidic
// Declare a sound effect (WAV file)
resource JumpSound: Sound = "audio/jump.wav";

// Declare background music (OGG file)
resource BackgroundMusic: Music = "audio/bgm.ogg";
```

**This generates:**
```cpp
Resource<AudioResource> g_resource_jumpsound("audio/jump.wav");
Resource<AudioResource> g_resource_backgroundmusic("audio/bgm.ogg");
```

## Supported Audio Formats

- **WAV** - Uncompressed audio (good for short sound effects)
- **OGG** - Compressed audio (good for background music)
- **MP3** - Compressed audio (also supported)

## Features

- **Zero Boilerplate** - One line declares and loads the audio resource
- **RAII Cleanup** - Automatic resource management
- **Hot-Reload Ready** - Resources support CONTINUUM hot-reload
- **Type Safety** - Resource types are checked at compile time
- **Format Detection** - Automatically detects format from file extension

## Current Status

✅ **Loading** - Audio files are loaded into memory  
⚠️ **Playback** - Audio playback is not yet implemented (placeholder in AudioResource)  
✅ **Hot-Reload** - File watching and reload support is ready  

## Next Steps

In the future, we can add:
- Audio playback implementation (SDL3 audio API)
- Volume control
- Looping support
- Multiple simultaneous sounds
- 3D positional audio

## File Structure

```
audio_test/
├── audio_test.hd          # Main HEIDIC source file
├── README.md              # This file
└── audio/                 # Audio files directory
    ├── jump.wav          # Sound effect (WAV)
    └── bgm.ogg           # Background music (OGG)
```

