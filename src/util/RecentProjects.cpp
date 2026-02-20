#include "util/RecentProjects.h"

namespace neurato {

RecentProjects::RecentProjects()
{
    auto appDataDir = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory).getChildFile("Neurato");
    appDataDir.createDirectory();
    storageFile_ = appDataDir.getChildFile("recent_projects.json");
    load();
}

void RecentProjects::addFile(const juce::File& file)
{
    // Remove if already present
    files_.erase(std::remove_if(files_.begin(), files_.end(),
        [&file](const juce::File& f) { return f == file; }), files_.end());

    // Insert at front
    files_.insert(files_.begin(), file);

    // Trim to max
    if (static_cast<int>(files_.size()) > kMaxRecentFiles)
        files_.resize(static_cast<size_t>(kMaxRecentFiles));

    save();
}

void RecentProjects::removeFile(const juce::File& file)
{
    files_.erase(std::remove_if(files_.begin(), files_.end(),
        [&file](const juce::File& f) { return f == file; }), files_.end());
    save();
}

std::vector<juce::File> RecentProjects::getFiles() const
{
    return files_;
}

void RecentProjects::clear()
{
    files_.clear();
    save();
}

void RecentProjects::load()
{
    files_.clear();

    if (!storageFile_.existsAsFile())
        return;

    auto jsonString = storageFile_.loadFileAsString();
    auto json = juce::JSON::parse(jsonString);

    if (!json.isArray())
        return;

    for (int i = 0; i < json.size() && i < kMaxRecentFiles; ++i)
    {
        juce::String path = json[i].toString();
        if (path.isNotEmpty())
        {
            juce::File f(path);
            if (f.existsAsFile())
                files_.push_back(f);
        }
    }
}

void RecentProjects::save()
{
    juce::Array<juce::var> arr;
    for (const auto& f : files_)
        arr.add(f.getFullPathName());

    auto jsonString = juce::JSON::toString(juce::var(arr), true);
    storageFile_.replaceWithText(jsonString);
}

} // namespace neurato
