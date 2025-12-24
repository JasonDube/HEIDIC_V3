// EDEN ENGINE - Native File Dialog Utilities
// Extracted from eden_vulkan_helpers.cpp for modularity
// Platform-specific file open/save dialogs

#ifndef EDEN_FILE_DIALOG_H
#define EDEN_FILE_DIALOG_H

#include <string>

#ifdef __cplusplus
extern "C" {
#endif

// Open a file dialog with custom filter
// filterList: Filter string in Windows format (e.g., "OBJ Files\0*.obj\0All Files\0*.*\0")
// defaultPath: Initial directory path (or NULL for current directory)
// Returns: Selected file path, or empty string if cancelled
const char* nfd_open_file_dialog(const char* filterList, const char* defaultPath);

// Save file dialog with custom filter
// filterList: Filter string in Windows format
// defaultPath: Initial directory path (or NULL for current directory)
// defaultName: Default filename (or NULL)
// Returns: Selected file path, or empty string if cancelled
const char* nfd_save_file_dialog(const char* filterList, const char* defaultPath, const char* defaultName);

// Convenience function for 3D model files (OBJ, GLB, GLTF)
const char* nfd_open_obj_dialog();

// Convenience function for GLB/GLTF files only
const char* nfd_open_glb_dialog();

// Convenience function for texture files (PNG, DDS, JPG, BMP)
const char* nfd_open_texture_dialog();

// Convenience function for HDM/HDMA files
const char* nfd_open_hdm_dialog();

// Convenience function for saving HDM files
const char* nfd_save_hdm_dialog(const char* defaultName);

// Browse for folder dialog
// Returns: Selected folder path, or empty string if cancelled
const char* nfd_browse_folder_dialog(const char* defaultPath);

#ifdef __cplusplus
}
#endif

// C++ convenience wrappers
#ifdef __cplusplus
namespace eden {
namespace dialog {

// Open file dialog with std::string filter
std::string openFile(const std::string& filter = "", const std::string& defaultPath = "");

// Save file dialog with std::string filter
std::string saveFile(const std::string& filter = "", const std::string& defaultPath = "", 
                     const std::string& defaultName = "");

// Open OBJ file dialog
std::string openOBJ();

// Open texture file dialog
std::string openTexture();

// Open HDM file dialog
std::string openHDM();

// Save HDM file dialog
std::string saveHDM(const std::string& defaultName = "model.hdma");

// Browse for folder
std::string browseFolder(const std::string& defaultPath = "");

} // namespace dialog
} // namespace eden
#endif

#endif // EDEN_FILE_DIALOG_H

