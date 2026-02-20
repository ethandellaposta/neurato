#pragma once

#include "commands/Command.h"
#include <vector>
#include <functional>

namespace neurato {

class Session;

// Manages the undo/redo stack. All session mutations go through here.
class CommandManager
{
public:
    CommandManager() = default;

    // Execute a command and push it onto the undo stack.
    // Clears the redo stack.
    void execute(CommandPtr cmd, Session& session);

    // Undo the most recent command
    bool undo(Session& session);

    // Redo the most recently undone command
    bool redo(Session& session);

    bool canUndo() const { return !undoStack_.empty(); }
    bool canRedo() const { return !redoStack_.empty(); }

    juce::String getUndoDescription() const;
    juce::String getRedoDescription() const;

    void clear();

    // Callback when undo/redo state changes (for UI updates)
    std::function<void()> onStateChanged;

    int getUndoStackSize() const { return static_cast<int>(undoStack_.size()); }
    int getRedoStackSize() const { return static_cast<int>(redoStack_.size()); }

private:
    std::vector<CommandPtr> undoStack_;
    std::vector<CommandPtr> redoStack_;

    static constexpr int kMaxUndoLevels = 200;
};

} // namespace neurato
