#pragma once

#include <juce_core/juce_core.h>
#include <memory>
#include <string>
#include "../model/Session.hpp"

namespace ampl {

// Base class for all undoable commands.
// Each command captures enough state to undo/redo itself.
class Command
{
public:
    virtual ~Command() = default;

    // Execute the command (first time or redo)
    virtual void execute(Session& session) = 0;

    // Undo the command
    virtual void undo(Session& session) = 0;

    // Human-readable description for UI display
    virtual juce::String getDescription() const = 0;
};

using CommandPtr = std::unique_ptr<Command>;

} // namespace ampl
