// EDEN ENGINE - Native File Dialog Implementation
// Extracted from eden_vulkan_helpers.cpp for modularity

#include "file_dialog.h"
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>

// Static buffer for return values (to avoid memory allocation issues with C interface)
static std::string g_dialogResult;

extern "C" const char* nfd_open_file_dialog(const char* filterList, const char* defaultPath) {
    g_dialogResult.clear();
    
    OPENFILENAMEA ofn;
    char szFile[MAX_PATH] = {0};
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filterList ? filterList : "All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = defaultPath ? defaultPath : NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    
    if (GetOpenFileNameA(&ofn) == TRUE) {
        g_dialogResult = szFile;
        return g_dialogResult.c_str();
    }
    
    return "";
}

extern "C" const char* nfd_save_file_dialog(const char* filterList, const char* defaultPath, const char* defaultName) {
    g_dialogResult.clear();
    
    OPENFILENAMEA ofn;
    char szFile[MAX_PATH] = {0};
    
    // Copy default name if provided
    if (defaultName && defaultName[0]) {
        strncpy(szFile, defaultName, sizeof(szFile) - 1);
    }
    
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filterList ? filterList : "All Files\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = defaultPath ? defaultPath : NULL;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    
    if (GetSaveFileNameA(&ofn) == TRUE) {
        g_dialogResult = szFile;
        return g_dialogResult.c_str();
    }
    
    return "";
}

extern "C" const char* nfd_open_obj_dialog() {
    // Now supports OBJ and GLB/GLTF
    return nfd_open_file_dialog(
        "3D Models\0*.obj;*.glb;*.gltf\0"
        "OBJ Files\0*.obj\0"
        "GLB Files\0*.glb\0"
        "GLTF Files\0*.gltf\0"
        "All Files\0*.*\0", 
        NULL);
}

extern "C" const char* nfd_open_glb_dialog() {
    return nfd_open_file_dialog(
        "GLTF Binary\0*.glb\0"
        "GLTF Files\0*.gltf\0"
        "All Files\0*.*\0", 
        NULL);
}

extern "C" const char* nfd_open_texture_dialog() {
    return nfd_open_file_dialog(
        "Image Files\0*.png;*.jpg;*.jpeg;*.dds;*.bmp\0"
        "PNG Files\0*.png\0"
        "DDS Files\0*.dds\0"
        "JPEG Files\0*.jpg;*.jpeg\0"
        "All Files\0*.*\0", 
        NULL);
}

extern "C" const char* nfd_open_hdm_dialog() {
    return nfd_open_file_dialog(
        "HDM Files\0*.hdm;*.hdma\0"
        "HDM Binary\0*.hdm\0"
        "HDM ASCII\0*.hdma\0"
        "All Files\0*.*\0", 
        NULL);
}

extern "C" const char* nfd_save_hdm_dialog(const char* defaultName) {
    return nfd_save_file_dialog(
        "HDM ASCII\0*.hdma\0"
        "HDM Binary\0*.hdm\0"
        "All Files\0*.*\0", 
        NULL, 
        defaultName ? defaultName : "model.hdma");
}

extern "C" const char* nfd_browse_folder_dialog(const char* defaultPath) {
    g_dialogResult.clear();
    
    BROWSEINFOA bi;
    char szFolder[MAX_PATH] = {0};
    
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = NULL;
    bi.pidlRoot = NULL;
    bi.pszDisplayName = szFolder;
    bi.lpszTitle = "Select Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = NULL;
    bi.lParam = 0;
    
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl != NULL) {
        if (SHGetPathFromIDListA(pidl, szFolder)) {
            g_dialogResult = szFolder;
        }
        CoTaskMemFree(pidl);
    }
    
    return g_dialogResult.empty() ? "" : g_dialogResult.c_str();
}

#else
// Linux/Mac stub implementations
// TODO: Implement with GTK/Cocoa

extern "C" const char* nfd_open_file_dialog(const char* filterList, const char* defaultPath) {
    (void)filterList;
    (void)defaultPath;
    return "";
}

extern "C" const char* nfd_save_file_dialog(const char* filterList, const char* defaultPath, const char* defaultName) {
    (void)filterList;
    (void)defaultPath;
    (void)defaultName;
    return "";
}

extern "C" const char* nfd_open_obj_dialog() {
    return "";
}

extern "C" const char* nfd_open_glb_dialog() {
    return "";
}

extern "C" const char* nfd_open_texture_dialog() {
    return "";
}

extern "C" const char* nfd_open_hdm_dialog() {
    return "";
}

extern "C" const char* nfd_save_hdm_dialog(const char* defaultName) {
    (void)defaultName;
    return "";
}

extern "C" const char* nfd_browse_folder_dialog(const char* defaultPath) {
    (void)defaultPath;
    return "";
}

#endif

// C++ convenience wrappers
namespace eden {
namespace dialog {

std::string openFile(const std::string& filter, const std::string& defaultPath) {
    const char* result = nfd_open_file_dialog(
        filter.empty() ? nullptr : filter.c_str(),
        defaultPath.empty() ? nullptr : defaultPath.c_str());
    return result ? result : "";
}

std::string saveFile(const std::string& filter, const std::string& defaultPath, 
                     const std::string& defaultName) {
    const char* result = nfd_save_file_dialog(
        filter.empty() ? nullptr : filter.c_str(),
        defaultPath.empty() ? nullptr : defaultPath.c_str(),
        defaultName.empty() ? nullptr : defaultName.c_str());
    return result ? result : "";
}

std::string openOBJ() {
    const char* result = nfd_open_obj_dialog();
    return result ? result : "";
}

std::string openTexture() {
    const char* result = nfd_open_texture_dialog();
    return result ? result : "";
}

std::string openHDM() {
    const char* result = nfd_open_hdm_dialog();
    return result ? result : "";
}

std::string saveHDM(const std::string& defaultName) {
    const char* result = nfd_save_hdm_dialog(defaultName.c_str());
    return result ? result : "";
}

std::string browseFolder(const std::string& defaultPath) {
    const char* result = nfd_browse_folder_dialog(
        defaultPath.empty() ? nullptr : defaultPath.c_str());
    return result ? result : "";
}

} // namespace dialog
} // namespace eden

