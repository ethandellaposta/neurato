#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace ampl {

// Persists a list of recently opened project files using JUCE's PropertiesFile.
// Stored in the user's application data directory.
class RecentProjects
{
public:
    RecentProjects();

    // Add a file to the top of the recent list (moves it if already present)
    void addFile(const juce::File& file);

    // Remove a file from the list
    void removeFile(const juce::File& file);

    // Get the list of recent files (most recent first)
    std::vector<juce::File> getFiles() const;

    // Clear all recent files
    void clear();

    static constexpr int kMaxRecentFiles = 10;

private:
    void load();
    void save();

    std::vector<juce::File> files_;
    juce::File storageFile_;
};

} // namespace ampl
