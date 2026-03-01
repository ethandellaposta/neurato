#include "engine/io/ExternalIOManager.hpp"
#include "engine/io/ExternalIOManager.hpp"

namespace ampl
{

ExternalIOManager::ExternalIOManager()
{
    // Start the MIDI collector with a default sample rate;
    // will be updated when audio device starts.
    midiCollector_.reset(44100.0);
}

ExternalIOManager::~ExternalIOManager()
{
    disableAllMidiInputs();
}

// ─── MIDI Input ───────────────────────────────────────────────────

std::vector<MidiDeviceInfo> ExternalIOManager::getAvailableMidiInputs() const
{
    std::vector<MidiDeviceInfo> result;
    auto devices = juce::MidiInput::getAvailableDevices();
    for (const auto &d : devices)
    {
        MidiDeviceInfo info;
        info.name = d.name;
        info.identifier = d.identifier;
        info.isInput = true;
        info.isEnabled = isMidiInputEnabled(d.identifier);
        result.push_back(info);
    }
    return result;
}

bool ExternalIOManager::enableMidiInput(const juce::String &deviceIdentifier)
{
    std::lock_guard<std::mutex> lock(midiDeviceMutex_);

    // Check if already open
    for (const auto &open : openMidiDevices_)
    {
        if (open.identifier == deviceIdentifier)
            return true; // Already enabled
    }

    // Find the device info
    auto devices = juce::MidiInput::getAvailableDevices();
    juce::MidiDeviceInfo targetDevice;
    bool found = false;
    for (const auto &d : devices)
    {
        if (d.identifier == deviceIdentifier)
        {
            targetDevice = d;
            found = true;
            break;
        }
    }
    if (!found)
    {
        DBG("ExternalIOManager: MIDI device not found: " + deviceIdentifier);
        return false;
    }

    // Open the MIDI input
    auto midiInput = juce::MidiInput::openDevice(deviceIdentifier, this);
    if (!midiInput)
    {
        DBG("ExternalIOManager: Failed to open MIDI device: " + targetDevice.name);
        return false;
    }

    DBG("ExternalIOManager: Opened MIDI input: " + targetDevice.name);
    midiInput->start();

    OpenMidiDevice entry;
    entry.device = std::move(midiInput);
    entry.identifier = deviceIdentifier;
    openMidiDevices_.push_back(std::move(entry));

    notifyMidiDeviceListChanged();
    return true;
}

void ExternalIOManager::disableMidiInput(const juce::String &deviceIdentifier)
{
    std::lock_guard<std::mutex> lock(midiDeviceMutex_);

    for (auto it = openMidiDevices_.begin(); it != openMidiDevices_.end(); ++it)
    {
        if (it->identifier == deviceIdentifier)
        {
            if (it->device)
                it->device->stop();
            openMidiDevices_.erase(it);
            notifyMidiDeviceListChanged();
            return;
        }
    }
}

void ExternalIOManager::disableAllMidiInputs()
{
    std::lock_guard<std::mutex> lock(midiDeviceMutex_);

    for (auto &entry : openMidiDevices_)
    {
        if (entry.device)
            entry.device->stop();
    }
    openMidiDevices_.clear();
}

bool ExternalIOManager::isMidiInputEnabled(const juce::String &deviceIdentifier) const
{
    std::lock_guard<std::mutex> lock(midiDeviceMutex_);
    for (const auto &entry : openMidiDevices_)
    {
        if (entry.identifier == deviceIdentifier)
            return true;
    }
    return false;
}

std::vector<juce::String> ExternalIOManager::getEnabledMidiInputs() const
{
    std::lock_guard<std::mutex> lock(midiDeviceMutex_);
    std::vector<juce::String> result;
    for (const auto &entry : openMidiDevices_)
        result.push_back(entry.identifier);
    return result;
}

void ExternalIOManager::setSampleRate(double sampleRate)
{
    midiCollector_.reset(sampleRate);
}

void ExternalIOManager::addMidiMessageToQueue(const juce::MidiMessage &message)
{
    midiCollector_.addMessageToQueue(message);
}

void ExternalIOManager::getMidiMessagesForBlock(juce::MidiBuffer &buffer, int numSamples)
{
    midiCollector_.removeNextBlockOfMessages(buffer, numSamples);
}

void ExternalIOManager::routeMidiDeviceToTrack(const juce::String &deviceIdentifier,
                                               const juce::String &trackId)
{
    std::lock_guard<std::mutex> lock(midiDeviceMutex_);
    for (auto &entry : openMidiDevices_)
    {
        if (entry.identifier == deviceIdentifier)
        {
            entry.routedTrackId = trackId;
            return;
        }
    }
}

juce::String ExternalIOManager::getMidiDeviceTrackRouting(
    const juce::String &deviceIdentifier) const
{
    std::lock_guard<std::mutex> lock(midiDeviceMutex_);
    for (const auto &entry : openMidiDevices_)
    {
        if (entry.identifier == deviceIdentifier)
            return entry.routedTrackId;
    }
    return {};
}

// ─── Audio Input ──────────────────────────────────────────────────

int ExternalIOManager::getAvailableInputChannelCount(juce::AudioDeviceManager &dm)
{
    if (auto *device = dm.getCurrentAudioDevice())
    {
        auto activeInputs = device->getActiveInputChannels();
        return activeInputs.countNumberOfSetBits();
    }
    return 0;
}

bool ExternalIOManager::isAudioInputEnabled(juce::AudioDeviceManager &dm)
{
    return getAvailableInputChannelCount(dm) > 0;
}

// ─── Listener ─────────────────────────────────────────────────────

void ExternalIOManager::addListener(Listener *listener)
{
    std::lock_guard<std::mutex> lock(listenerMutex_);
    listeners_.push_back(listener);
}

void ExternalIOManager::removeListener(Listener *listener)
{
    std::lock_guard<std::mutex> lock(listenerMutex_);
    listeners_.erase(
        std::remove(listeners_.begin(), listeners_.end(), listener),
        listeners_.end());
}

// ─── MidiInputCallback ───────────────────────────────────────────

void ExternalIOManager::handleIncomingMidiMessage(juce::MidiInput *source,
                                                   const juce::MidiMessage &message)
{
    // Forward to the collector (thread-safe, lock-free for audio thread)
    midiCollector_.addMessageToQueue(message);

    // Notify listeners (for UI updates)
    juce::String deviceId;
    if (source)
        deviceId = source->getIdentifier();
    notifyMidiMessageReceived(message, deviceId);
}

void ExternalIOManager::notifyMidiDeviceListChanged()
{
    // Don't hold midiDeviceMutex_ here — listeners may re-enter
    std::vector<Listener *> listenersCopy;
    {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        listenersCopy = listeners_;
    }
    for (auto *l : listenersCopy)
        l->midiDeviceListChanged();
}

void ExternalIOManager::notifyMidiMessageReceived(const juce::MidiMessage &msg,
                                                   const juce::String &deviceId)
{
    std::vector<Listener *> listenersCopy;
    {
        std::lock_guard<std::mutex> lock(listenerMutex_);
        listenersCopy = listeners_;
    }
    for (auto *l : listenersCopy)
        l->midiMessageReceived(msg, deviceId);
}

} // namespace ampl
