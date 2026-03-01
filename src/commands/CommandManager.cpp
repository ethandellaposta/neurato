#include "commands/CommandManager.hpp"
#include "model/Session.hpp"

namespace ampl {

void CommandManager::execute(CommandPtr cmd, Session& session)
{
    cmd->execute(session);
    undoStack_.push_back(std::move(cmd));
    redoStack_.clear();

    // Trim undo stack if too large
    while (static_cast<int>(undoStack_.size()) > kMaxUndoLevels)
        undoStack_.erase(undoStack_.begin());

    if (onStateChanged)
        onStateChanged();
}

bool CommandManager::undo(Session& session)
{
    if (undoStack_.empty())
        return false;

    auto cmd = std::move(undoStack_.back());
    undoStack_.pop_back();
    cmd->undo(session);
    redoStack_.push_back(std::move(cmd));

    if (onStateChanged)
        onStateChanged();

    return true;
}

bool CommandManager::redo(Session& session)
{
    if (redoStack_.empty())
        return false;

    auto cmd = std::move(redoStack_.back());
    redoStack_.pop_back();
    cmd->execute(session);
    undoStack_.push_back(std::move(cmd));

    if (onStateChanged)
        onStateChanged();

    return true;
}

juce::String CommandManager::getUndoDescription() const
{
    if (undoStack_.empty())
        return {};
    return undoStack_.back()->getDescription();
}

juce::String CommandManager::getRedoDescription() const
{
    if (redoStack_.empty())
        return {};
    return redoStack_.back()->getDescription();
}

void CommandManager::clear()
{
    undoStack_.clear();
    redoStack_.clear();

    if (onStateChanged)
        onStateChanged();
}

} // namespace ampl
