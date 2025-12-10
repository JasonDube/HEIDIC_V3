// Hot-Reload Runtime Module
// Handles file watching, DLL compilation, and reloading

use std::path::{Path, PathBuf};
use std::fs;
use std::time::SystemTime;
use anyhow::{Result, Context};
use notify::{Watcher, RecursiveMode, Event, EventKind};

pub struct HotReloadManager {
    source_file: PathBuf,
    dll_path: PathBuf,
    last_modified: SystemTime,
    watcher: Option<notify::RecommendedWatcher>,
}

impl HotReloadManager {
    pub fn new(source_file: &str) -> Result<Self> {
        let source_path = PathBuf::from(source_file);
        let dll_path = source_path.with_extension("dll");
        
        // Get initial modification time
        let metadata = fs::metadata(&source_path)
            .with_context(|| format!("Failed to read source file: {}", source_file))?;
        let last_modified = metadata.modified()
            .with_context(|| "Failed to get file modification time")?;
        
        Ok(Self {
            source_file: source_path,
            dll_path,
            last_modified,
            watcher: None,
        })
    }
    
    pub fn start_watching(&mut self) -> Result<()> {
        // Create file watcher
        let (tx, rx) = std::sync::mpsc::channel();
        let mut watcher = notify::recommended_watcher(tx)
            .context("Failed to create file watcher")?;
        
        watcher.watch(&self.source_file, RecursiveMode::NonRecursive)
            .with_context(|| format!("Failed to watch file: {}", self.source_file.display()))?;
        
        self.watcher = Some(watcher);
        
        // Spawn a thread to handle file change events
        let source_file = self.source_file.clone();
        std::thread::spawn(move || {
            loop {
                match rx.recv() {
                    Ok(Ok(Event { kind: EventKind::Modify(_), paths, .. })) => {
                        for path in paths {
                            if path == source_file {
                                println!("[Hot-Reload] Source file changed: {}", path.display());
                                // In a full implementation, we'd trigger recompilation here
                            }
                        }
                    }
                    Ok(Ok(_)) => {}
                    Ok(Err(e)) => {
                        eprintln!("[Hot-Reload] Watcher error: {}", e);
                        break;
                    }
                    Err(e) => {
                        eprintln!("[Hot-Reload] Channel error: {}", e);
                        break;
                    }
                }
            }
        });
        
        Ok(())
    }
    
    pub fn check_for_changes(&mut self) -> Result<bool> {
        let metadata = fs::metadata(&self.source_file)
            .with_context(|| format!("Failed to read source file: {}", self.source_file.display()))?;
        let modified = metadata.modified()
            .with_context(|| "Failed to get file modification time")?;
        
        if modified > self.last_modified {
            self.last_modified = modified;
            Ok(true)
        } else {
            Ok(false)
        }
    }
    
    pub fn get_source_file(&self) -> &Path {
        &self.source_file
    }
    
    pub fn get_dll_path(&self) -> &Path {
        &self.dll_path
    }
}

// Windows-specific DLL loading
#[cfg(windows)]
pub mod dll_loader {
    use std::ffi::OsStr;
    use std::os::windows::ffi::OsStrExt;
    use windows_sys::Win32::Foundation::HMODULE;
    use windows_sys::Win32::System::LibraryLoader::{LoadLibraryW, FreeLibrary, GetProcAddress};
    use anyhow::Result;
    
    pub struct DllHandle {
        handle: HMODULE,
    }
    
    impl DllHandle {
        pub fn load(path: &std::path::Path) -> Result<Self> {
            let wide_path: Vec<u16> = OsStr::new(path)
                .encode_wide()
                .chain(Some(0))
                .collect();
            
            unsafe {
                let handle = LoadLibraryW(wide_path.as_ptr());
                if handle == 0 {
                    return Err(anyhow::anyhow!("Failed to load DLL: {}", path.display()));
                }
                
                Ok(Self { handle })
            }
        }
        
        pub fn get_function<T>(&self, name: &str) -> Result<*const T> {
            let name_bytes = name.as_bytes();
            let mut name_cstr = Vec::with_capacity(name_bytes.len() + 1);
            name_cstr.extend_from_slice(name_bytes);
            name_cstr.push(0);
            
            unsafe {
                let func_ptr = GetProcAddress(self.handle, name_cstr.as_ptr());
                if func_ptr.is_none() {
                    return Err(anyhow::anyhow!("Function not found: {}", name));
                }
                
                // This is unsafe - caller must ensure T matches the actual function signature
                Ok(std::mem::transmute(func_ptr))
            }
        }
    }
    
    impl Drop for DllHandle {
        fn drop(&mut self) {
            unsafe {
                FreeLibrary(self.handle);
            }
        }
    }
}

#[cfg(not(windows))]
pub mod dll_loader {
    use anyhow::Result;
    
    pub struct DllHandle;
    
    impl DllHandle {
        pub fn load(_path: &std::path::Path) -> Result<Self> {
            Err(anyhow::anyhow!("DLL loading not implemented for this platform"))
        }
        
        pub fn get_function<T>(&self, _name: &str) -> Result<*const T> {
            Err(anyhow::anyhow!("DLL loading not implemented for this platform"))
        }
    }
}

