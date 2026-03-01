#include "LogicAudioEngine.hpp" #include "engine/plugins/host/PluginHost.hpp" #include
<algorithm>
    #include
    <cmath>
        namespace ampl { // LogicAudioEngine implementation LogicAudioEngine::LogicAudioEngine() {
        audioGraph_ = std::make_unique<AudioGraph
            >(); environment_ = std::make_unique<LogicEnvironment
                >(); } LogicAudioEngine::~LogicAudioEngine() { shutdown(); } bool
                LogicAudioEngine::initialize(double sampleRate, int bufferSize) { sampleRate_ =
                sampleRate; bufferSize_ = bufferSize; // Initialize audio graph if
                (!audioGraph_->prepareToPlay(sampleRate, bufferSize)) { return false; } // Create
                master bus auto masterBus = environment_->createBus("Stereo Out"); // Setup
                processing buffers masterBuffer_.setSize(2, bufferSize, false, false, true); //
                Create default audio nodes for master output auto masterOutput =
                std::make_shared<TrackOutputNode
                    >("master_output"); audioGraph_->addNode(masterOutput); processing_.store(true);
                    return true; } void LogicAudioEngine::shutdown() { processing_.store(false); if
                    (audioGraph_) { audioGraph_->releaseResources(); } channels_.clear();
                    trackBuffers_.clear(); busBuffers_.clear(); sendBuffers_.clear(); } std::string
                    LogicAudioEngine::createTrack(const std::string& name, LogicTrackType type) {
                    std::string trackId = "track_" + std::to_string(nextTrackId_++); // Create
                    channel strip LogicMixerChannel::ChannelStrip strip; strip.id = trackId;
                    strip.name = name; strip.type = type; strip.volume = 0.0f; strip.pan = 0.0f;
                    strip.mute = false; strip.solo = false; strip.recordArm = false; auto channel =
                    std::make_shared<LogicMixerChannel
                        >(strip); channels_[trackId] = channel; // Create audio nodes for this track
                        createAudioNodes(trackId, type); // Setup track buffer
                        trackBuffers_[trackId].setSize(2, bufferSize_, false, false, true); return
                        trackId; } void LogicAudioEngine::removeTrack(const std::string& trackId) {
                        // Remove from solo/mute tracking if (soloedTrack_ == trackId) {
                        soloedTrack_.clear(); } mutedTracks_.erase(trackId); // Remove channel auto
                        channelIt = channels_.find(trackId); if (channelIt != channels_.end()) {
                        channels_.erase(channelIt); } // Remove buffer trackBuffers_.erase(trackId);
                        // Update audio graph updateAudioGraph(); } void
                        LogicAudioEngine::renameTrack(const std::string& trackId, const std::string&
                        newName) { auto channelIt = channels_.find(trackId); if (channelIt !=
                        channels_.end()) { auto strip = channelIt->second->getChannelStrip();
                        strip.name = newName; channelIt->second = std::make_shared<LogicMixerChannel
                            >(strip); } } std::shared_ptr<LogicMixerChannel>
                                LogicAudioEngine::getChannel(const std::string& trackId) { auto it =
                                channels_.find(trackId); return (it != channels_.end()) ? it->second
                                : nullptr; } void LogicAudioEngine::updateChannel(const std::string&
                                trackId, const LogicMixerChannel::ChannelStrip& strip) { auto
                                channelIt = channels_.find(trackId); if (channelIt !=
                                channels_.end()) { channelIt->second =
                                std::make_shared<LogicMixerChannel
                                    >(strip); updateAudioNodeConnections(trackId); } } void
                                    LogicAudioEngine::processAudio(float** outputBuffers, int
                                    numChannels, int numSamples, SampleCount position) { if
                                    (!processing_.load()) return; // Clear all buffers for (auto&
                                    [trackId, buffer] : trackBuffers_) { buffer.clear(); } for
                                    (auto& [busId, buffer] : busBuffers_) { buffer.clear(); } for
                                    (auto& [sendId, buffer] : sendBuffers_) { buffer.clear(); }
                                    masterBuffer_.clear(); // Process each track through its channel
                                    strip for (auto& [trackId, channel] : channels_) { auto&
                                    trackBuffer = trackBuffers_[trackId]; // Get input audio (would
                                    come from clips, input, etc.) // For now, we'll generate silence
                                    // Process through channel strip
                                    channel->processAudio(trackBuffer, numSamples); // Apply sends
                                    channel->applySends(trackBuffer, sendBuffers_); // Handle
                                    solo/mute logic bool shouldProcess = true; if
                                    (!soloedTrack_.empty() && trackId != soloedTrack_) {
                                    shouldProcess = false; // Another track is soloed } if
                                    (mutedTracks_.count(trackId) > 0) { shouldProcess = false; //
                                    This track is muted } if (!shouldProcess) { trackBuffer.clear();
                                    } } // Process environment (buses, VCAs, routing)
                                    environment_->processEnvironment(trackBuffers_, busBuffers_,
                                    numSamples); // Sum all tracks to master for (auto& [trackId,
                                    buffer] : trackBuffers_) { masterBuffer_.addFrom(0, 0, buffer,
                                    0, 0, numSamples); if (buffer.getNumChannels() > 1) {
                                    masterBuffer_.addFrom(1, 0, buffer, 1, 0, numSamples); } } //
                                    Add buses to master for (auto& [busId, buffer] : busBuffers_) {
                                    masterBuffer_.addFrom(0, 0, buffer, 0, 0, numSamples); if
                                    (buffer.getNumChannels() > 1) { masterBuffer_.addFrom(1, 0,
                                    buffer, 1, 0, numSamples); } } // Copy to output for (int ch =
                                    0; ch < std::min(numChannels, masterBuffer_.getNumChannels());
                                    ++ch) { if (outputBuffers[ch]) {
                                    juce::FloatVectorOperations::copy(outputBuffers[ch],
                                    masterBuffer_.getReadPointer(ch), numSamples); } } } void
                                    LogicAudioEngine::setTrackVolume(const std::string& trackId,
                                    float dB) { auto channel = getChannel(trackId); if (channel) {
                                    channel->setVolume(dB); } } void
                                    LogicAudioEngine::setTrackPan(const std::string& trackId, float
                                    pan) { auto channel = getChannel(trackId); if (channel) {
                                    channel->setPan(pan); } } void
                                    LogicAudioEngine::setTrackMute(const std::string& trackId, bool
                                    muted) { auto channel = getChannel(trackId); if (channel) {
                                    channel->setMute(muted); if (muted) {
                                    mutedTracks_.insert(trackId); } else {
                                    mutedTracks_.erase(trackId); } updateMuteStates(); } } void
                                    LogicAudioEngine::setTrackSolo(const std::string& trackId, bool
                                    soloed) { auto channel = getChannel(trackId); if (channel) {
                                    channel->setSolo(soloed); handleSoloLogic(trackId, soloed);
                                    updateMuteStates(); } } void
                                    LogicAudioEngine::setSendLevel(const std::string& trackId, int
                                    sendIndex, float level) { auto channel = getChannel(trackId); if
                                    (channel) { channel->setSendLevel(sendIndex, level); } } void
                                    LogicAudioEngine::insertPlugin(const std::string& trackId, int
                                    slot, const std::string& pluginId) { auto channel =
                                    getChannel(trackId); if (channel) { channel->insertPlugin(slot,
                                    pluginId); updateAudioNodeConnections(trackId); } } void
                                    LogicAudioEngine::bypassPlugin(const std::string& trackId, int
                                    slot, bool bypassed) { auto channel = getChannel(trackId); if
                                    (channel) { channel->bypassPlugin(slot, bypassed); } } void
                                    LogicAudioEngine::removePlugin(const std::string& trackId, int
                                    slot) { auto channel = getChannel(trackId); if (channel) {
                                    channel->removePlugin(slot);
                                    updateAudioNodeConnections(trackId); } } void
                                    LogicAudioEngine::addAutomationLane(const std::string& trackId,
                                    const std::string& parameter, std::shared_ptr<AutomationLane>
                                        lane) { auto channel = getChannel(trackId); if (channel) {
                                        channel->addAutomationLane(parameter, lane); } } void
                                        LogicAudioEngine::removeAutomationLane(const std::string&
                                        trackId, const std::string& parameter) { auto channel =
                                        getChannel(trackId); if (channel) {
                                        channel->removeAutomationLane(parameter); } }
                                        LogicAudioEngine::EngineState
                                        LogicAudioEngine::getEngineState() const { EngineState
                                        state; // Save channel strips for (auto& [trackId, channel]
                                        : channels_) { state.channelStrips[trackId] =
                                        channel->getChannelStrip(); } // Save environment
                                        state.buses = environment_->getBuses(); state.vcas =
                                        environment_->getVCAs(); state.trackOutputs =
                                        environment_->getTrackOutputs(); return state; } void
                                        LogicAudioEngine::setEngineState(const EngineState& state) {
                                        // Clear current state channels_.clear();
                                        trackBuffers_.clear(); // Restore channel strips for (auto&
                                        [trackId, strip] : state.channelStrips) { auto channel =
                                        std::make_shared<LogicMixerChannel
                                            >(strip); channels_[trackId] = channel;
                                            trackBuffers_[trackId].setSize(2, bufferSize_, false,
                                            false, true); } // Update audio graph
                                            updateAudioGraph(); } void
                                            LogicAudioEngine::updateAudioGraph() { // Rebuild audio
                                            graph based on current channel configuration
                                            audioGraph_->clear(); // Add master output auto
                                            masterOutput = std::make_shared<TrackOutputNode
                                                >("master_output");
                                                audioGraph_->addNode(masterOutput); // Add nodes for
                                                each track for (auto& [trackId, channel] :
                                                channels_) { updateAudioNodeConnections(trackId); }
                                                } void LogicAudioEngine::createAudioNodes(const
                                                std::string& trackId, LogicTrackType type) { //
                                                Create input node based on track type auto inputNode
                                                = createInputNode(type);
                                                audioGraph_->addNode(inputNode); // Create gain node
                                                auto gainNode = createGainNode(trackId);
                                                audioGraph_->addNode(gainNode); // Create plugin
                                                nodes for (int i = 0; i <
                                                LogicMixerChannel::kPluginSlots; ++i) { auto
                                                pluginNode = createPluginNode(trackId, i);
                                                audioGraph_->addNode(pluginNode); } // Create send
                                                nodes for (int i = 0; i < 8; ++i) { auto sendNode =
                                                createSendNode(trackId, i);
                                                audioGraph_->addNode(sendNode); } // Create output
                                                node auto outputNode = createOutputNode();
                                                audioGraph_->addNode(outputNode); // Connect nodes
                                                updateAudioNodeConnections(trackId); } void
                                                LogicAudioEngine::updateAudioNodeConnections(const
                                                std::string& trackId) { auto channel =
                                                getChannel(trackId); if (!channel) return; const
                                                auto& strip = channel->getChannelStrip(); // Update
                                                node parameters based on channel strip state // This
                                                would involve finding the nodes and updating their
                                                parameters // Update gain node auto gainNode =
                                                audioGraph_->getNode(trackId + "_gain"); if
                                                (gainNode) { gainNode->setParameterValue("gain",
                                                strip.volume); } // Update plugin nodes for (int i =
                                                0; i < LogicMixerChannel::kPluginSlots; ++i) { if
                                                (!strip.pluginChain[i].empty()) { auto pluginNode =
                                                audioGraph_->getNode(trackId + "_plugin_" +
                                                std::to_string(i)); if (pluginNode) { //
                                                Enable/disable based on bypass state
                                                pluginNode->setParameterValue("enabled",
                                                strip.pluginBypass[i] ? 0.0f : 1.0f); } } } } void
                                                LogicAudioEngine::handleSoloLogic(const std::string&
                                                trackId, bool soloed) { if (soloed) { if
                                                (soloedTrack_ != trackId) { // Unsolo previous track
                                                if (!soloedTrack_.empty()) { auto prevChannel =
                                                getChannel(soloedTrack_); if (prevChannel) {
                                                prevChannel->setSolo(false); } } soloedTrack_ =
                                                trackId; } } else { if (soloedTrack_ == trackId) {
                                                soloedTrack_.clear(); } } } void
                                                LogicAudioEngine::updateMuteStates() { for (auto&
                                                [trackId, channel] : channels_) { bool shouldMute =
                                                false; // If any track is soloed, mute all
                                                non-soloed tracks if (!soloedTrack_.empty() &&
                                                trackId != soloedTrack_) { shouldMute = true; } else
                                                { // Use original mute state shouldMute =
                                                mutedTracks_.count(trackId) > 0; }
                                                channel->setMute(shouldMute); } }
                                                std::shared_ptr<AudioNode>
                                                    LogicAudioEngine::createInputNode(LogicTrackType
                                                    type) { switch (type) { case
                                                    LogicTrackType::Audio: return
                                                    std::make_shared<TrackInputNode
                                                        >("audio_input"); case
                                                        LogicTrackType::Instrument: return
                                                        std::make_shared<TrackInputNode
                                                            >("instrument_input"); case
                                                            LogicTrackType::DrumMachine: return
                                                            std::make_shared<TrackInputNode
                                                                >("drum_input"); default: return
                                                                std::make_shared<TrackInputNode
                                                                    >("input"); } }
                                                                    std::shared_ptr<AudioNode>
                                                                        LogicAudioEngine::createOutputNode()
                                                                        { return
                                                                        std::make_shared<TrackOutputNode
                                                                            >("output"); }
                                                                            std::shared_ptr<AudioNode>
                                                                                LogicAudioEngine::createGainNode(const
                                                                                std::string&
                                                                                trackId) { auto
                                                                                gainNode =
                                                                                std::make_shared<GainNode
                                                                                    >(trackId +
                                                                                    "_gain");
                                                                                    gainNode->setParameterValue("gain",
                                                                                    0.0f); // Unity
                                                                                    gain return
                                                                                    gainNode; }
                                                                                    std::shared_ptr<AudioNode>
                                                                                        LogicAudioEngine::createPluginNode(const
                                                                                        std::string&
                                                                                        trackId, int
                                                                                        slot) { //
                                                                                        This would
                                                                                        create a
                                                                                        plugin node
                                                                                        that wraps
                                                                                        the actual
                                                                                        plugin //
                                                                                        For now,
                                                                                        return a
                                                                                        gain node as
                                                                                        placeholder
                                                                                        auto
                                                                                        pluginNode =
                                                                                        std::make_shared<GainNode
                                                                                            >(trackId
                                                                                            +
                                                                                            "_plugin_"
                                                                                            +
                                                                                            std::to_string(slot));
                                                                                            pluginNode->setParameterValue("gain",
                                                                                            1.0f);
                                                                                            // Unity
                                                                                            gain
                                                                                            return
                                                                                            pluginNode;
                                                                                            }
                                                                                            std::shared_ptr<AudioNode>
                                                                                                LogicAudioEngine::createSendNode(const
                                                                                                std::string&
                                                                                                trackId,
                                                                                                int
                                                                                                sendIndex)
                                                                                                {
                                                                                                auto
                                                                                                sendNode
                                                                                                =
                                                                                                std::make_shared<GainNode
                                                                                                    >(trackId
                                                                                                    +
                                                                                                    "_send_"
                                                                                                    +
                                                                                                    std::to_string(sendIndex));
                                                                                                    sendNode->setParameterValue("gain",
                                                                                                    -60.0f);
                                                                                                    //
                                                                                                    Minimum
                                                                                                    gain
                                                                                                    (muted)
                                                                                                    return
                                                                                                    sendNode;
                                                                                                    }
                                                                                                    std::shared_ptr<AudioNode>
                                                                                                        LogicAudioEngine::createBusNode(const
                                                                                                        std::string&
                                                                                                        busId)
                                                                                                        {
                                                                                                        auto
                                                                                                        busNode
                                                                                                        =
                                                                                                        std::make_shared<GainNode
                                                                                                            >(busId
                                                                                                            +
                                                                                                            "_bus");
                                                                                                            busNode->setParameterValue("gain",
                                                                                                            0.0f);
                                                                                                            //
                                                                                                            Unity
                                                                                                            gain
                                                                                                            return
                                                                                                            busNode;
                                                                                                            }
                                                                                                            std::shared_ptr<AudioNode>
                                                                                                                LogicAudioEngine::createVCANode(const
                                                                                                                std::string&
                                                                                                                vcaId)
                                                                                                                {
                                                                                                                auto
                                                                                                                vcaNode
                                                                                                                =
                                                                                                                std::make_shared<GainNode
                                                                                                                    >(vcaId
                                                                                                                    +
                                                                                                                    "_vca");
                                                                                                                    vcaNode->setParameterValue("gain",
                                                                                                                    0.0f);
                                                                                                                    //
                                                                                                                    Unity
                                                                                                                    gain
                                                                                                                    return
                                                                                                                    vcaNode;
                                                                                                                    }
                                                                                                                    //
                                                                                                                    LogicSessionManager
                                                                                                                    implementation
                                                                                                                    LogicSessionManager::LogicSessionManager()
                                                                                                                    {
                                                                                                                    audioEngine_
                                                                                                                    =
                                                                                                                    std::make_shared<LogicAudioEngine
                                                                                                                        >();
                                                                                                                        environment_
                                                                                                                        =
                                                                                                                        std::make_shared<LogicEnvironment
                                                                                                                            >();
                                                                                                                            smartControls_
                                                                                                                            =
                                                                                                                            std::make_shared<LogicSmartControls
                                                                                                                                >();
                                                                                                                                trackAlternatives_
                                                                                                                                =
                                                                                                                                std::make_shared<LogicTrackAlternatives
                                                                                                                                    >();
                                                                                                                                    flexTime_
                                                                                                                                    =
                                                                                                                                    std::make_shared<LogicFlexTime
                                                                                                                                        >();
                                                                                                                                        stepSequencer_
                                                                                                                                        =
                                                                                                                                        std::make_shared<LogicStepSequencer
                                                                                                                                            >();
                                                                                                                                            scoreEditor_
                                                                                                                                            =
                                                                                                                                            std::make_shared<LogicScoreEditor
                                                                                                                                                >();
                                                                                                                                                }
                                                                                                                                                void
                                                                                                                                                LogicSessionManager::createNewSession()
                                                                                                                                                {
                                                                                                                                                currentSessionPath_.clear();
                                                                                                                                                hasUnsavedChanges_
                                                                                                                                                =
                                                                                                                                                false;
                                                                                                                                                //
                                                                                                                                                Clear
                                                                                                                                                audio
                                                                                                                                                engine
                                                                                                                                                audioEngine_->shutdown();
                                                                                                                                                audioEngine_->initialize(44100.0,
                                                                                                                                                512);
                                                                                                                                                //
                                                                                                                                                Clear
                                                                                                                                                environment
                                                                                                                                                environment_
                                                                                                                                                =
                                                                                                                                                std::make_shared<LogicEnvironment
                                                                                                                                                    >();
                                                                                                                                                    //
                                                                                                                                                    Clear
                                                                                                                                                    other
                                                                                                                                                    components
                                                                                                                                                    smartControls_
                                                                                                                                                    =
                                                                                                                                                    std::make_shared<LogicSmartControls
                                                                                                                                                        >();
                                                                                                                                                        trackAlternatives_
                                                                                                                                                        =
                                                                                                                                                        std::make_shared<LogicTrackAlternatives
                                                                                                                                                            >();
                                                                                                                                                            flexTime_
                                                                                                                                                            =
                                                                                                                                                            std::make_shared<LogicFlexTime
                                                                                                                                                                >();
                                                                                                                                                                stepSequencer_
                                                                                                                                                                =
                                                                                                                                                                std::make_shared<LogicStepSequencer
                                                                                                                                                                    >();
                                                                                                                                                                    scoreEditor_
                                                                                                                                                                    =
                                                                                                                                                                    std::make_shared<LogicScoreEditor
                                                                                                                                                                        >();
                                                                                                                                                                        updateAudioEngine();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::loadSession(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        filePath)
                                                                                                                                                                        {
                                                                                                                                                                        //
                                                                                                                                                                        Load
                                                                                                                                                                        session
                                                                                                                                                                        from
                                                                                                                                                                        file
                                                                                                                                                                        //
                                                                                                                                                                        This
                                                                                                                                                                        would
                                                                                                                                                                        deserialize
                                                                                                                                                                        the
                                                                                                                                                                        session
                                                                                                                                                                        state
                                                                                                                                                                        currentSessionPath_
                                                                                                                                                                        =
                                                                                                                                                                        filePath;
                                                                                                                                                                        hasUnsavedChanges_
                                                                                                                                                                        =
                                                                                                                                                                        false;
                                                                                                                                                                        updateAudioEngine();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::saveSession(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        filePath)
                                                                                                                                                                        {
                                                                                                                                                                        //
                                                                                                                                                                        Save
                                                                                                                                                                        session
                                                                                                                                                                        to
                                                                                                                                                                        file
                                                                                                                                                                        //
                                                                                                                                                                        This
                                                                                                                                                                        would
                                                                                                                                                                        serialize
                                                                                                                                                                        the
                                                                                                                                                                        session
                                                                                                                                                                        state
                                                                                                                                                                        currentSessionPath_
                                                                                                                                                                        =
                                                                                                                                                                        filePath;
                                                                                                                                                                        hasUnsavedChanges_
                                                                                                                                                                        =
                                                                                                                                                                        false;
                                                                                                                                                                        }
                                                                                                                                                                        std::string
                                                                                                                                                                        LogicSessionManager::addTrack(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        name,
                                                                                                                                                                        LogicTrackType
                                                                                                                                                                        type)
                                                                                                                                                                        {
                                                                                                                                                                        std::string
                                                                                                                                                                        trackId
                                                                                                                                                                        =
                                                                                                                                                                        audioEngine_->createTrack(name,
                                                                                                                                                                        type);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        updateAudioEngine();
                                                                                                                                                                        return
                                                                                                                                                                        trackId;
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::deleteTrack(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        trackId)
                                                                                                                                                                        {
                                                                                                                                                                        audioEngine_->removeTrack(trackId);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        updateAudioEngine();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::duplicateTrack(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        trackId)
                                                                                                                                                                        {
                                                                                                                                                                        auto
                                                                                                                                                                        channel
                                                                                                                                                                        =
                                                                                                                                                                        audioEngine_->getChannel(trackId);
                                                                                                                                                                        if
                                                                                                                                                                        (channel)
                                                                                                                                                                        {
                                                                                                                                                                        const
                                                                                                                                                                        auto&
                                                                                                                                                                        strip
                                                                                                                                                                        =
                                                                                                                                                                        channel->getChannelStrip();
                                                                                                                                                                        std::string
                                                                                                                                                                        newTrackId
                                                                                                                                                                        =
                                                                                                                                                                        addTrack(strip.name
                                                                                                                                                                        +
                                                                                                                                                                        "
                                                                                                                                                                        Copy",
                                                                                                                                                                        strip.type);
                                                                                                                                                                        //
                                                                                                                                                                        Copy
                                                                                                                                                                        channel
                                                                                                                                                                        settings
                                                                                                                                                                        auto
                                                                                                                                                                        newChannel
                                                                                                                                                                        =
                                                                                                                                                                        audioEngine_->getChannel(newTrackId);
                                                                                                                                                                        if
                                                                                                                                                                        (newChannel)
                                                                                                                                                                        {
                                                                                                                                                                        newChannel->updateStrip(strip);
                                                                                                                                                                        }
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        updateAudioEngine();
                                                                                                                                                                        }
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::moveTrack(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        trackId,
                                                                                                                                                                        int
                                                                                                                                                                        newPosition)
                                                                                                                                                                        {
                                                                                                                                                                        //
                                                                                                                                                                        Reorder
                                                                                                                                                                        tracks
                                                                                                                                                                        in
                                                                                                                                                                        the
                                                                                                                                                                        mixer
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::setTrackVolume(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        trackId,
                                                                                                                                                                        float
                                                                                                                                                                        dB)
                                                                                                                                                                        {
                                                                                                                                                                        audioEngine_->setTrackVolume(trackId,
                                                                                                                                                                        dB);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::setTrackPan(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        trackId,
                                                                                                                                                                        float
                                                                                                                                                                        pan)
                                                                                                                                                                        {
                                                                                                                                                                        audioEngine_->setTrackPan(trackId,
                                                                                                                                                                        pan);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::setTrackMute(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        trackId,
                                                                                                                                                                        bool
                                                                                                                                                                        muted)
                                                                                                                                                                        {
                                                                                                                                                                        audioEngine_->setTrackMute(trackId,
                                                                                                                                                                        muted);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::setTrackSolo(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        trackId,
                                                                                                                                                                        bool
                                                                                                                                                                        soloed)
                                                                                                                                                                        {
                                                                                                                                                                        audioEngine_->setTrackSolo(trackId,
                                                                                                                                                                        soloed);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::insertPlugin(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        trackId,
                                                                                                                                                                        int
                                                                                                                                                                        slot,
                                                                                                                                                                        const
                                                                                                                                                                        std::string&
                                                                                                                                                                        pluginId)
                                                                                                                                                                        {
                                                                                                                                                                        audioEngine_->insertPlugin(trackId,
                                                                                                                                                                        slot,
                                                                                                                                                                        pluginId);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::bypassPlugin(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        trackId,
                                                                                                                                                                        int
                                                                                                                                                                        slot,
                                                                                                                                                                        bool
                                                                                                                                                                        bypassed)
                                                                                                                                                                        {
                                                                                                                                                                        audioEngine_->bypassPlugin(trackId,
                                                                                                                                                                        slot,
                                                                                                                                                                        bypassed);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::removePlugin(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        trackId,
                                                                                                                                                                        int
                                                                                                                                                                        slot)
                                                                                                                                                                        {
                                                                                                                                                                        audioEngine_->removePlugin(trackId,
                                                                                                                                                                        slot);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::setSendLevel(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        trackId,
                                                                                                                                                                        int
                                                                                                                                                                        sendIndex,
                                                                                                                                                                        float
                                                                                                                                                                        level)
                                                                                                                                                                        {
                                                                                                                                                                        audioEngine_->setSendLevel(trackId,
                                                                                                                                                                        sendIndex,
                                                                                                                                                                        level);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::setSendTarget(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        trackId,
                                                                                                                                                                        int
                                                                                                                                                                        sendIndex,
                                                                                                                                                                        const
                                                                                                                                                                        std::string&
                                                                                                                                                                        target)
                                                                                                                                                                        {
                                                                                                                                                                        auto
                                                                                                                                                                        channel
                                                                                                                                                                        =
                                                                                                                                                                        audioEngine_->getChannel(trackId);
                                                                                                                                                                        if
                                                                                                                                                                        (channel)
                                                                                                                                                                        {
                                                                                                                                                                        channel->setSendTarget(sendIndex,
                                                                                                                                                                        target);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        }
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::setSendPreFader(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        trackId,
                                                                                                                                                                        int
                                                                                                                                                                        sendIndex,
                                                                                                                                                                        bool
                                                                                                                                                                        preFader)
                                                                                                                                                                        {
                                                                                                                                                                        auto
                                                                                                                                                                        channel
                                                                                                                                                                        =
                                                                                                                                                                        audioEngine_->getChannel(trackId);
                                                                                                                                                                        if
                                                                                                                                                                        (channel)
                                                                                                                                                                        {
                                                                                                                                                                        channel->setSendPreFader(sendIndex,
                                                                                                                                                                        preFader);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        }
                                                                                                                                                                        }
                                                                                                                                                                        std::string
                                                                                                                                                                        LogicSessionManager::createVCA(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        name)
                                                                                                                                                                        {
                                                                                                                                                                        std::string
                                                                                                                                                                        vcaId
                                                                                                                                                                        =
                                                                                                                                                                        environment_->createVCA(name);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        updateAudioEngine();
                                                                                                                                                                        return
                                                                                                                                                                        vcaId;
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::deleteVCA(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        vcaId)
                                                                                                                                                                        {
                                                                                                                                                                        environment_->removeVCA(vcaId);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        updateAudioEngine();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::assignTrackToVCA(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        trackId,
                                                                                                                                                                        const
                                                                                                                                                                        std::string&
                                                                                                                                                                        vcaId)
                                                                                                                                                                        {
                                                                                                                                                                        environment_->assignTrackToVCA(trackId,
                                                                                                                                                                        vcaId);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        updateAudioEngine();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::unassignTrackFromVCA(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        trackId)
                                                                                                                                                                        {
                                                                                                                                                                        environment_->removeTrackFromVCA(trackId,
                                                                                                                                                                        "");
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        updateAudioEngine();
                                                                                                                                                                        }
                                                                                                                                                                        std::string
                                                                                                                                                                        LogicSessionManager::createBus(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        name)
                                                                                                                                                                        {
                                                                                                                                                                        std::string
                                                                                                                                                                        busId
                                                                                                                                                                        =
                                                                                                                                                                        environment_->createBus(name);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        updateAudioEngine();
                                                                                                                                                                        return
                                                                                                                                                                        busId;
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::deleteBus(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        busId)
                                                                                                                                                                        {
                                                                                                                                                                        environment_->removeBus(busId);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        updateAudioEngine();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::assignTrackToBus(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        trackId,
                                                                                                                                                                        const
                                                                                                                                                                        std::string&
                                                                                                                                                                        busId)
                                                                                                                                                                        {
                                                                                                                                                                        environment_->assignTrackToBus(trackId,
                                                                                                                                                                        busId);
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        updateAudioEngine();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::setBusVolume(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        busId,
                                                                                                                                                                        float
                                                                                                                                                                        dB)
                                                                                                                                                                        {
                                                                                                                                                                        //
                                                                                                                                                                        Update
                                                                                                                                                                        bus
                                                                                                                                                                        volume
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::setBusPan(const
                                                                                                                                                                        std::string&
                                                                                                                                                                        busId,
                                                                                                                                                                        float
                                                                                                                                                                        pan)
                                                                                                                                                                        {
                                                                                                                                                                        //
                                                                                                                                                                        Update
                                                                                                                                                                        bus
                                                                                                                                                                        pan
                                                                                                                                                                        markSessionModified();
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::markSessionModified()
                                                                                                                                                                        {
                                                                                                                                                                        hasUnsavedChanges_
                                                                                                                                                                        =
                                                                                                                                                                        true;
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicSessionManager::updateAudioEngine()
                                                                                                                                                                        {
                                                                                                                                                                        //
                                                                                                                                                                        Update
                                                                                                                                                                        audio
                                                                                                                                                                        engine
                                                                                                                                                                        with
                                                                                                                                                                        current
                                                                                                                                                                        session
                                                                                                                                                                        state
                                                                                                                                                                        if
                                                                                                                                                                        (audioEngine_)
                                                                                                                                                                        {
                                                                                                                                                                        auto
                                                                                                                                                                        state
                                                                                                                                                                        =
                                                                                                                                                                        audioEngine_->getEngineState();
                                                                                                                                                                        //
                                                                                                                                                                        Update
                                                                                                                                                                        state
                                                                                                                                                                        with
                                                                                                                                                                        current
                                                                                                                                                                        session
                                                                                                                                                                        data
                                                                                                                                                                        audioEngine_->setEngineState(state);
                                                                                                                                                                        }
                                                                                                                                                                        }
                                                                                                                                                                        //
                                                                                                                                                                        LogicTransport
                                                                                                                                                                        implementation
                                                                                                                                                                        LogicTransport::LogicTransport()
                                                                                                                                                                        {
                                                                                                                                                                        samplePosition_.store(0.0);
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicTransport::play()
                                                                                                                                                                        {
                                                                                                                                                                        if
                                                                                                                                                                        (state_.playMode
                                                                                                                                                                        ==
                                                                                                                                                                        PlayMode::Stop)
                                                                                                                                                                        {
                                                                                                                                                                        state_.playMode
                                                                                                                                                                        =
                                                                                                                                                                        PlayMode::Play;
                                                                                                                                                                        }
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicTransport::stop()
                                                                                                                                                                        {
                                                                                                                                                                        state_.playMode
                                                                                                                                                                        =
                                                                                                                                                                        PlayMode::Stop;
                                                                                                                                                                        samplePosition_.store(0.0);
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicTransport::record()
                                                                                                                                                                        {
                                                                                                                                                                        state_.playMode
                                                                                                                                                                        =
                                                                                                                                                                        PlayMode::Record;
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicTransport::toggleLoop()
                                                                                                                                                                        {
                                                                                                                                                                        state_.isLooping
                                                                                                                                                                        =
                                                                                                                                                                        !state_.isLooping;
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicTransport::setLoopRange(double
                                                                                                                                                                        start,
                                                                                                                                                                        double
                                                                                                                                                                        end)
                                                                                                                                                                        {
                                                                                                                                                                        state_.loopStart
                                                                                                                                                                        =
                                                                                                                                                                        start;
                                                                                                                                                                        state_.loopEnd
                                                                                                                                                                        =
                                                                                                                                                                        end;
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicTransport::setPosition(double
                                                                                                                                                                        position)
                                                                                                                                                                        {
                                                                                                                                                                        samplePosition_.store(position);
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicTransport::setCycleMode(bool
                                                                                                                                                                        enabled)
                                                                                                                                                                        {
                                                                                                                                                                        state_.cycleMode
                                                                                                                                                                        =
                                                                                                                                                                        enabled;
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicTransport::setReplaceMode(bool
                                                                                                                                                                        enabled)
                                                                                                                                                                        {
                                                                                                                                                                        state_.replaceMode
                                                                                                                                                                        =
                                                                                                                                                                        enabled;
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicTransport::setAutoPunch(bool
                                                                                                                                                                        enabled)
                                                                                                                                                                        {
                                                                                                                                                                        state_.autoPunch
                                                                                                                                                                        =
                                                                                                                                                                        enabled;
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicTransport::setCountIn(bool
                                                                                                                                                                        enabled)
                                                                                                                                                                        {
                                                                                                                                                                        state_.countIn
                                                                                                                                                                        =
                                                                                                                                                                        enabled;
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicTransport::setPreRollBars(int
                                                                                                                                                                        bars)
                                                                                                                                                                        {
                                                                                                                                                                        state_.preRollBars
                                                                                                                                                                        =
                                                                                                                                                                        bars;
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicTransport::setPostRollBars(int
                                                                                                                                                                        bars)
                                                                                                                                                                        {
                                                                                                                                                                        state_.postRollBars
                                                                                                                                                                        =
                                                                                                                                                                        bars;
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicTransport::setState(const
                                                                                                                                                                        TransportState&
                                                                                                                                                                        state)
                                                                                                                                                                        {
                                                                                                                                                                        state_
                                                                                                                                                                        =
                                                                                                                                                                        state;
                                                                                                                                                                        samplePosition_.store(state.currentPosition);
                                                                                                                                                                        }
                                                                                                                                                                        bool
                                                                                                                                                                        LogicTransport::isInLoop()
                                                                                                                                                                        const
                                                                                                                                                                        {
                                                                                                                                                                        return
                                                                                                                                                                        state_.isLooping
                                                                                                                                                                        &&
                                                                                                                                                                        samplePosition_.load()
                                                                                                                                                                        >=
                                                                                                                                                                        state_.loopStart
                                                                                                                                                                        &&
                                                                                                                                                                        samplePosition_.load()
                                                                                                                                                                        <
                                                                                                                                                                        state_.loopEnd;
                                                                                                                                                                        }
                                                                                                                                                                        bool
                                                                                                                                                                        LogicTransport::isInPunchRange()
                                                                                                                                                                        const
                                                                                                                                                                        {
                                                                                                                                                                        return
                                                                                                                                                                        state_.isPunching
                                                                                                                                                                        &&
                                                                                                                                                                        samplePosition_.load()
                                                                                                                                                                        >=
                                                                                                                                                                        state_.punchIn
                                                                                                                                                                        &&
                                                                                                                                                                        samplePosition_.load()
                                                                                                                                                                        <
                                                                                                                                                                        state_.punchOut;
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicTransport::setTempo(double
                                                                                                                                                                        bpm)
                                                                                                                                                                        {
                                                                                                                                                                        tempo_
                                                                                                                                                                        =
                                                                                                                                                                        bpm;
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicTransport::setTimeSignature(int
                                                                                                                                                                        numerator,
                                                                                                                                                                        int
                                                                                                                                                                        denominator)
                                                                                                                                                                        {
                                                                                                                                                                        timeSignatureNumerator_
                                                                                                                                                                        =
                                                                                                                                                                        numerator;
                                                                                                                                                                        timeSignatureDenominator_
                                                                                                                                                                        =
                                                                                                                                                                        denominator;
                                                                                                                                                                        }
                                                                                                                                                                        void
                                                                                                                                                                        LogicTransport::updatePosition()
                                                                                                                                                                        {
                                                                                                                                                                        //
                                                                                                                                                                        Update
                                                                                                                                                                        position
                                                                                                                                                                        based
                                                                                                                                                                        on
                                                                                                                                                                        transport
                                                                                                                                                                        state
                                                                                                                                                                        if
                                                                                                                                                                        (state_.playMode
                                                                                                                                                                        !=
                                                                                                                                                                        PlayMode::Stop)
                                                                                                                                                                        {
                                                                                                                                                                        double
                                                                                                                                                                        currentPos
                                                                                                                                                                        =
                                                                                                                                                                        samplePosition_.load();
                                                                                                                                                                        //
                                                                                                                                                                        Advance
                                                                                                                                                                        position
                                                                                                                                                                        currentPos
                                                                                                                                                                        +=
                                                                                                                                                                        bufferSize_;
                                                                                                                                                                        //
                                                                                                                                                                        Handle
                                                                                                                                                                        looping
                                                                                                                                                                        if
                                                                                                                                                                        (shouldLoop())
                                                                                                                                                                        {
                                                                                                                                                                        if
                                                                                                                                                                        (currentPos
                                                                                                                                                                        >=
                                                                                                                                                                        state_.loopEnd)
                                                                                                                                                                        {
                                                                                                                                                                        currentPos
                                                                                                                                                                        =
                                                                                                                                                                        state_.loopStart;
                                                                                                                                                                        }
                                                                                                                                                                        }
                                                                                                                                                                        samplePosition_.store(currentPos);
                                                                                                                                                                        state_.currentPosition
                                                                                                                                                                        =
                                                                                                                                                                        currentPos;
                                                                                                                                                                        }
                                                                                                                                                                        }
                                                                                                                                                                        bool
                                                                                                                                                                        LogicTransport::shouldLoop()
                                                                                                                                                                        const
                                                                                                                                                                        {
                                                                                                                                                                        return
                                                                                                                                                                        state_.isLooping
                                                                                                                                                                        &&
                                                                                                                                                                        state_.loopEnd
                                                                                                                                                                        >
                                                                                                                                                                        state_.loopStart;
                                                                                                                                                                        }
                                                                                                                                                                        bool
                                                                                                                                                                        LogicTransport::shouldPunch()
                                                                                                                                                                        const
                                                                                                                                                                        {
                                                                                                                                                                        return
                                                                                                                                                                        state_.isPunching
                                                                                                                                                                        &&
                                                                                                                                                                        state_.punchOut
                                                                                                                                                                        >
                                                                                                                                                                        state_.punchIn;
                                                                                                                                                                        }
                                                                                                                                                                        double
                                                                                                                                                                        LogicTransport::samplesToBeats(double
                                                                                                                                                                        samples)
                                                                                                                                                                        const
                                                                                                                                                                        {
                                                                                                                                                                        double
                                                                                                                                                                        samplesPerBeat
                                                                                                                                                                        =
                                                                                                                                                                        (sampleRate_
                                                                                                                                                                        *
                                                                                                                                                                        60.0)
                                                                                                                                                                        /
                                                                                                                                                                        tempo_;
                                                                                                                                                                        return
                                                                                                                                                                        samples
                                                                                                                                                                        /
                                                                                                                                                                        samplesPerBeat;
                                                                                                                                                                        }
                                                                                                                                                                        double
                                                                                                                                                                        LogicTransport::beatsToSamples(double
                                                                                                                                                                        beats)
                                                                                                                                                                        const
                                                                                                                                                                        {
                                                                                                                                                                        double
                                                                                                                                                                        samplesPerBeat
                                                                                                                                                                        =
                                                                                                                                                                        (sampleRate_
                                                                                                                                                                        *
                                                                                                                                                                        60.0)
                                                                                                                                                                        /
                                                                                                                                                                        tempo_;
                                                                                                                                                                        return
                                                                                                                                                                        beats
                                                                                                                                                                        *
                                                                                                                                                                        samplesPerBeat;
                                                                                                                                                                        }
                                                                                                                                                                        //
                                                                                                                                                                        LogicController
                                                                                                                                                                        implementation
                                                                                                                                                                        LogicController::LogicController()
                                                                                                                                                                        {
                                                                                                                                                                        sessionManager_
                                                                                                                                                                        =
                                                                                                                                                                        std::make_shared<LogicSessionManager
                                                                                                                                                                            >();
                                                                                                                                                                            transport_
                                                                                                                                                                            =
                                                                                                                                                                            std::make_shared<LogicTransport
                                                                                                                                                                                >();
                                                                                                                                                                                audioEngine_
                                                                                                                                                                                =
                                                                                                                                                                                std::make_shared<LogicAudioEngine
                                                                                                                                                                                    >();
                                                                                                                                                                                    }
                                                                                                                                                                                    bool
                                                                                                                                                                                    LogicController::initialize(double
                                                                                                                                                                                    sampleRate,
                                                                                                                                                                                    int
                                                                                                                                                                                    bufferSize)
                                                                                                                                                                                    {
                                                                                                                                                                                    sampleRate_
                                                                                                                                                                                    =
                                                                                                                                                                                    sampleRate;
                                                                                                                                                                                    bufferSize_
                                                                                                                                                                                    =
                                                                                                                                                                                    bufferSize;
                                                                                                                                                                                    //
                                                                                                                                                                                    Initialize
                                                                                                                                                                                    audio
                                                                                                                                                                                    engine
                                                                                                                                                                                    if
                                                                                                                                                                                    (!audioEngine_->initialize(sampleRate,
                                                                                                                                                                                    bufferSize))
                                                                                                                                                                                    {
                                                                                                                                                                                    return
                                                                                                                                                                                    false;
                                                                                                                                                                                    }
                                                                                                                                                                                    //
                                                                                                                                                                                    Connect
                                                                                                                                                                                    components
                                                                                                                                                                                    connectComponents();
                                                                                                                                                                                    //
                                                                                                                                                                                    Setup
                                                                                                                                                                                    default
                                                                                                                                                                                    session
                                                                                                                                                                                    setupDefaultSession();
                                                                                                                                                                                    initialized_
                                                                                                                                                                                    =
                                                                                                                                                                                    true;
                                                                                                                                                                                    return
                                                                                                                                                                                    true;
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::shutdown()
                                                                                                                                                                                    {
                                                                                                                                                                                    if
                                                                                                                                                                                    (audioEngine_)
                                                                                                                                                                                    {
                                                                                                                                                                                    audioEngine_->shutdown();
                                                                                                                                                                                    }
                                                                                                                                                                                    initialized_
                                                                                                                                                                                    =
                                                                                                                                                                                    false;
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::createNewProject()
                                                                                                                                                                                    {
                                                                                                                                                                                    sessionManager_->createNewSession();
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::loadProject(const
                                                                                                                                                                                    std::string&
                                                                                                                                                                                    filePath)
                                                                                                                                                                                    {
                                                                                                                                                                                    sessionManager_->loadSession(filePath);
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::saveProject(const
                                                                                                                                                                                    std::string&
                                                                                                                                                                                    filePath)
                                                                                                                                                                                    {
                                                                                                                                                                                    sessionManager_->saveSession(filePath);
                                                                                                                                                                                    }
                                                                                                                                                                                    std::string
                                                                                                                                                                                    LogicController::addAudioTrack(const
                                                                                                                                                                                    std::string&
                                                                                                                                                                                    name)
                                                                                                                                                                                    {
                                                                                                                                                                                    return
                                                                                                                                                                                    sessionManager_->addTrack(name,
                                                                                                                                                                                    LogicTrackType::Audio);
                                                                                                                                                                                    }
                                                                                                                                                                                    std::string
                                                                                                                                                                                    LogicController::addInstrumentTrack(const
                                                                                                                                                                                    std::string&
                                                                                                                                                                                    name)
                                                                                                                                                                                    {
                                                                                                                                                                                    return
                                                                                                                                                                                    sessionManager_->addTrack(name,
                                                                                                                                                                                    LogicTrackType::Instrument);
                                                                                                                                                                                    }
                                                                                                                                                                                    std::string
                                                                                                                                                                                    LogicController::addDrumTrack(const
                                                                                                                                                                                    std::string&
                                                                                                                                                                                    name)
                                                                                                                                                                                    {
                                                                                                                                                                                    return
                                                                                                                                                                                    sessionManager_->addTrack(name,
                                                                                                                                                                                    LogicTrackType::DrumMachine);
                                                                                                                                                                                    }
                                                                                                                                                                                    std::string
                                                                                                                                                                                    LogicController::addBus(const
                                                                                                                                                                                    std::string&
                                                                                                                                                                                    name)
                                                                                                                                                                                    {
                                                                                                                                                                                    return
                                                                                                                                                                                    sessionManager_->createBus(name);
                                                                                                                                                                                    }
                                                                                                                                                                                    std::string
                                                                                                                                                                                    LogicController::addVCA(const
                                                                                                                                                                                    std::string&
                                                                                                                                                                                    name)
                                                                                                                                                                                    {
                                                                                                                                                                                    return
                                                                                                                                                                                    sessionManager_->createVCA(name);
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::setTrackVolume(const
                                                                                                                                                                                    std::string&
                                                                                                                                                                                    trackId,
                                                                                                                                                                                    float
                                                                                                                                                                                    dB)
                                                                                                                                                                                    {
                                                                                                                                                                                    sessionManager_->setTrackVolume(trackId,
                                                                                                                                                                                    dB);
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::setTrackPan(const
                                                                                                                                                                                    std::string&
                                                                                                                                                                                    trackId,
                                                                                                                                                                                    float
                                                                                                                                                                                    pan)
                                                                                                                                                                                    {
                                                                                                                                                                                    sessionManager_->setTrackPan(trackId,
                                                                                                                                                                                    pan);
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::setTrackMute(const
                                                                                                                                                                                    std::string&
                                                                                                                                                                                    trackId,
                                                                                                                                                                                    bool
                                                                                                                                                                                    muted)
                                                                                                                                                                                    {
                                                                                                                                                                                    sessionManager_->setTrackMute(trackId,
                                                                                                                                                                                    muted);
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::setTrackSolo(const
                                                                                                                                                                                    std::string&
                                                                                                                                                                                    trackId,
                                                                                                                                                                                    bool
                                                                                                                                                                                    soloed)
                                                                                                                                                                                    {
                                                                                                                                                                                    sessionManager_->setTrackSolo(trackId,
                                                                                                                                                                                    soloed);
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::loadPlugin(const
                                                                                                                                                                                    std::string&
                                                                                                                                                                                    trackId,
                                                                                                                                                                                    const
                                                                                                                                                                                    std::string&
                                                                                                                                                                                    pluginPath)
                                                                                                                                                                                    {
                                                                                                                                                                                    //
                                                                                                                                                                                    Find
                                                                                                                                                                                    first
                                                                                                                                                                                    empty
                                                                                                                                                                                    plugin
                                                                                                                                                                                    slot
                                                                                                                                                                                    auto
                                                                                                                                                                                    channel
                                                                                                                                                                                    =
                                                                                                                                                                                    audioEngine_->getChannel(trackId);
                                                                                                                                                                                    if
                                                                                                                                                                                    (channel)
                                                                                                                                                                                    {
                                                                                                                                                                                    const
                                                                                                                                                                                    auto&
                                                                                                                                                                                    strip
                                                                                                                                                                                    =
                                                                                                                                                                                    channel->getChannelStrip();
                                                                                                                                                                                    for
                                                                                                                                                                                    (int
                                                                                                                                                                                    i
                                                                                                                                                                                    =
                                                                                                                                                                                    0;
                                                                                                                                                                                    i
                                                                                                                                                                                    <
                                                                                                                                                                                    LogicMixerChannel::kPluginSlots;
                                                                                                                                                                                    ++i)
                                                                                                                                                                                    {
                                                                                                                                                                                    if
                                                                                                                                                                                    (strip.pluginChain[i].empty())
                                                                                                                                                                                    {
                                                                                                                                                                                    sessionManager_->insertPlugin(trackId,
                                                                                                                                                                                    i,
                                                                                                                                                                                    pluginPath);
                                                                                                                                                                                    break;
                                                                                                                                                                                    }
                                                                                                                                                                                    }
                                                                                                                                                                                    }
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::bypassPlugin(const
                                                                                                                                                                                    std::string&
                                                                                                                                                                                    trackId,
                                                                                                                                                                                    int
                                                                                                                                                                                    slot,
                                                                                                                                                                                    bool
                                                                                                                                                                                    bypassed)
                                                                                                                                                                                    {
                                                                                                                                                                                    sessionManager_->bypassPlugin(trackId,
                                                                                                                                                                                    slot,
                                                                                                                                                                                    bypassed);
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::removePlugin(const
                                                                                                                                                                                    std::string&
                                                                                                                                                                                    trackId,
                                                                                                                                                                                    int
                                                                                                                                                                                    slot)
                                                                                                                                                                                    {
                                                                                                                                                                                    sessionManager_->removePlugin(trackId,
                                                                                                                                                                                    slot);
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::play()
                                                                                                                                                                                    {
                                                                                                                                                                                    transport_->play();
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::stop()
                                                                                                                                                                                    {
                                                                                                                                                                                    transport_->stop();
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::record()
                                                                                                                                                                                    {
                                                                                                                                                                                    transport_->record();
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::setTempo(double
                                                                                                                                                                                    bpm)
                                                                                                                                                                                    {
                                                                                                                                                                                    transport_->setTempo(bpm);
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::processAudio(float**
                                                                                                                                                                                    outputBuffers,
                                                                                                                                                                                    int
                                                                                                                                                                                    numChannels,
                                                                                                                                                                                    int
                                                                                                                                                                                    numSamples)
                                                                                                                                                                                    {
                                                                                                                                                                                    if
                                                                                                                                                                                    (initialized_)
                                                                                                                                                                                    {
                                                                                                                                                                                    //
                                                                                                                                                                                    Update
                                                                                                                                                                                    transport
                                                                                                                                                                                    position
                                                                                                                                                                                    transport_->updatePosition();
                                                                                                                                                                                    //
                                                                                                                                                                                    Process
                                                                                                                                                                                    audio
                                                                                                                                                                                    through
                                                                                                                                                                                    engine
                                                                                                                                                                                    audioEngine_->processAudio(outputBuffers,
                                                                                                                                                                                    numChannels,
                                                                                                                                                                                    numSamples,
                                                                                                                                                                                    transport_->getCurrentPosition());
                                                                                                                                                                                    }
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::setupDefaultSession()
                                                                                                                                                                                    {
                                                                                                                                                                                    //
                                                                                                                                                                                    Create
                                                                                                                                                                                    a
                                                                                                                                                                                    default
                                                                                                                                                                                    session
                                                                                                                                                                                    with
                                                                                                                                                                                    basic
                                                                                                                                                                                    tracks
                                                                                                                                                                                    addAudioTrack("Audio
                                                                                                                                                                                    1");
                                                                                                                                                                                    addAudioTrack("Audio
                                                                                                                                                                                    2");
                                                                                                                                                                                    addInstrumentTrack("Instrument
                                                                                                                                                                                    1");
                                                                                                                                                                                    addBus("Bus
                                                                                                                                                                                    1");
                                                                                                                                                                                    addVCA("VCA
                                                                                                                                                                                    1");
                                                                                                                                                                                    }
                                                                                                                                                                                    void
                                                                                                                                                                                    LogicController::connectComponents()
                                                                                                                                                                                    {
                                                                                                                                                                                    //
                                                                                                                                                                                    Connect
                                                                                                                                                                                    session
                                                                                                                                                                                    manager
                                                                                                                                                                                    to
                                                                                                                                                                                    audio
                                                                                                                                                                                    engine
                                                                                                                                                                                    sessionManager_->getAudioEngine()
                                                                                                                                                                                    =
                                                                                                                                                                                    audioEngine_;
                                                                                                                                                                                    sessionManager_->getEnvironment()
                                                                                                                                                                                    =
                                                                                                                                                                                    audioEngine_->getEnvironment();
                                                                                                                                                                                    }
                                                                                                                                                                                    }
                                                                                                                                                                                    //
                                                                                                                                                                                    namespace
                                                                                                                                                                                    ampl
                                                                                                                                                                                </LogicAudioEngine></LogicTransport
                                                                                                                                                                            ></LogicSessionManager
                                                                                                                                                                        ></LogicScoreEditor
                                                                                                                                                                    ></LogicStepSequencer
                                                                                                                                                                ></LogicFlexTime
                                                                                                                                                            ></LogicTrackAlternatives
                                                                                                                                                        ></LogicSmartControls
                                                                                                                                                    ></LogicEnvironment
                                                                                                                                                ></LogicScoreEditor
                                                                                                                                            ></LogicStepSequencer
                                                                                                                                        ></LogicFlexTime
                                                                                                                                    ></LogicTrackAlternatives
                                                                                                                                ></LogicSmartControls
                                                                                                                            ></LogicEnvironment
                                                                                                                        ></LogicAudioEngine
                                                                                                                    ></GainNode
                                                                                                                ></AudioNode
                                                                                                            ></GainNode
                                                                                                        ></AudioNode
                                                                                                    ></GainNode
                                                                                                ></AudioNode
                                                                                            ></GainNode
                                                                                        ></AudioNode
                                                                                    ></GainNode
                                                                                ></AudioNode
                                                                            ></TrackOutputNode
                                                                        ></AudioNode
                                                                    ></TrackInputNode
                                                                ></TrackInputNode
                                                            ></TrackInputNode
                                                        ></TrackInputNode
                                                    ></AudioNode
                                                ></TrackOutputNode
                                            ></LogicMixerChannel
                                        ></AutomationLane
                                    ></LogicMixerChannel
                                ></LogicMixerChannel
                            ></LogicMixerChannel
                        ></LogicMixerChannel
                    ></TrackOutputNode
                ></LogicEnvironment
            ></AudioGraph
        ></cmath
    ></algorithm
>
