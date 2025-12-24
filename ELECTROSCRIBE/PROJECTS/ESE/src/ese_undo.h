// ESE (Echo Synapse Editor) - Undo System
// Manages undo/redo stack for mesh editing operations

#ifndef ESE_UNDO_H
#define ESE_UNDO_H

#include "ese_types.h"
#include <vector>

struct MeshVertex;
class MeshResource;

namespace ese {

class UndoManager {
public:
    UndoManager(size_t maxLevels = 20);
    ~UndoManager() = default;
    
    // Save current mesh state to undo stack
    void saveState(const std::vector<MeshVertex>& vertices,
                   const std::vector<uint32_t>& indices);
    
    // Save state directly from MeshResource
    void saveState(MeshResource* mesh);
    
    // Undo to previous state
    // Returns true if undo was successful
    bool undo(std::vector<MeshVertex>& vertices,
              std::vector<uint32_t>& indices);
    
    // Undo directly to MeshResource
    bool undo(MeshResource* mesh);
    
    // Redo to next state (if available)
    bool redo(std::vector<MeshVertex>& vertices,
              std::vector<uint32_t>& indices);
    
    // Check if undo is available
    bool canUndo() const { return !m_undoStack.empty(); }
    
    // Check if redo is available
    bool canRedo() const { return !m_redoStack.empty(); }
    
    // Clear all undo/redo history
    void clear();
    
    // Get undo stack size
    size_t getUndoCount() const { return m_undoStack.size(); }
    
    // Get redo stack size
    size_t getRedoCount() const { return m_redoStack.size(); }
    
    // Set max undo levels
    void setMaxLevels(size_t maxLevels);
    
private:
    std::vector<MeshState> m_undoStack;
    std::vector<MeshState> m_redoStack;
    size_t m_maxLevels;
    
    void trimStack(std::vector<MeshState>& stack);
};

} // namespace ese

#endif // ESE_UNDO_H

