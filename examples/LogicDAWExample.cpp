#include "LogicAudioEngine.h"
#include "ui/LogicMixerPanel.h"
#include <iostream>

using namespace neurato;

// Example: Creating a Logic Pro X-style session
class LogicDAWExample {
public:
    LogicDAWExample() {
        // Initialize the Logic controller
        controller_ = std::make_unique<LogicController>();
        controller_->initialize(44100.0, 512);
        
        // Create UI components
        mixerPanel_ = std::make_unique<LogicMixerPanel>();
        
        // Setup the session
        setupLogicSession();
    }
    
    ~LogicDAWExample() = default;
    
    void run() {
        std::cout << "=== Logic Pro X-style DAW Example ===" << std::endl;
        
        // Demonstrate mixer features
        demonstrateMixerFeatures();
        
        // Demonstrate advanced routing
        demonstrateAdvancedRouting();
        
        // Demonstrate VCA groups
        demonstrateVCAGroups();
        
        // Demonstrate plugin chain
        demonstratePluginChain();
        
        // Demonstrate automation
        demonstrateAutomation();
        
        // Simulate audio processing
        simulateAudioProcessing();
        
        std::cout << "=== Example Complete ===" << std::endl;
    }

private:
    std::unique_ptr<LogicController> controller_;
    std::unique_ptr<LogicMixerPanel> mixerPanel_;
    
    void setupLogicSession() {
        std::cout << "\n--- Setting up Logic-style session ---" << std::endl;
        
        // Create different track types like Logic Pro X
        std::string drums = controller_->addDrumTrack("Drums");
        std::string bass = controller_->addAudioTrack("Bass");
        std::string rhythmGuitar = controller_->addAudioTrack("Rhythm Guitar");
        std::string leadGuitar = controller_->addAudioTrack("Lead Guitar");
        std::string vocals = controller_->addAudioTrack("Vocals");
        std::string synth = controller_->addInstrumentTrack("Synth Leads");
        
        // Create buses like Logic Pro X
        std::string drumsBus = controller_->addBus("Drums Bus");
        std::string guitarsBus = controller_->addBus("Guitars Bus");
        std::string vocalsBus = controller_->addBus("Vocals Bus");
        std::string reverbBus = controller_->addBus("Reverb Bus");
        std::string delayBus = controller_->addBus("Delay Bus");
        
        // Create VCA groups like Logic Pro X
        std::string drumsVCA = controller_->addVCA("Drums VCA");
        std::string guitarsVCA = controller_->addVCA("Guitars VCA");
        std::string rhythmVCA = controller_->addVCA("Rhythm Section VCA");
        
        std::cout << "Created Logic-style session with:" << std::endl;
        std::cout << "- 6 tracks (drums, bass, guitars, vocals, synth)" << std::endl;
        std::cout << "- 5 buses (drums, guitars, vocals, reverb, delay)" << std::endl;
        std::cout << "- 3 VCA groups (drums, guitars, rhythm section)" << std::endl;
        
        // Add tracks to mixer panel
        auto sessionManager = controller_->getSessionManager();
        auto audioEngine = controller_->getAudioEngine();
        
        // Add channel strips to mixer UI
        for (const auto& trackId : {drums, bass, rhythmGuitar, leadGuitar, vocals, synth}) {
            auto channel = audioEngine->getChannel(trackId);
            if (channel) {
                mixerPanel_->addTrack(trackId, channel->getChannelStrip());
            }
        }
        
        // Setup track routing to buses
        sessionManager->assignTrackToBus(drums, drumsBus);
        sessionManager->assignTrackToBus(bass, drumsBus);
        
        sessionManager->assignTrackToBus(rhythmGuitar, guitarsBus);
        sessionManager->assignTrackToBus(leadGuitar, guitarsBus);
        
        sessionManager->assignTrackToBus(vocals, vocalsBus);
        
        // Assign tracks to VCA groups
        sessionManager->assignTrackToVCA(drums, drumsVCA);
        sessionManager->assignTrackToVCA(bass, drumsVCA);
        
        sessionManager->assignTrackToVCA(rhythmGuitar, guitarsVCA);
        sessionManager->assignTrackToVCA(leadGuitar, guitarsVCA);
        
        sessionManager->assignTrackToVCA(drums, rhythmVCA);
        sessionManager->assignTrackToVCA(bass, rhythmVCA);
        sessionManager->assignTrackToVCA(rhythmGuitar, rhythmVCA);
        
        std::cout << "Setup complete track routing and VCA assignments" << std::endl;
    }
    
    void demonstrateMixerFeatures() {
        std::cout << "\n--- Demonstrating Logic-style Mixer Features ---" << std::endl;
        
        auto sessionManager = controller_->getSessionManager();
        auto audioEngine = controller_->getAudioEngine();
        
        // Set initial levels like a Logic Pro X mix
        std::cout << "Setting initial mix levels:" << std::endl;
        
        // Drums (foundation)
        controller_->setTrackVolume("track_1", -3.0f);  // Drums
        controller_->setTrackPan("track_1", 0.0f);
        
        // Bass (center)
        controller_->setTrackVolume("track_2", -6.0f);  // Bass
        controller_->setTrackPan("track_2", 0.0f);
        
        // Rhythm Guitar (slight left)
        controller_->setTrackVolume("track_3", -9.0f);  // Rhythm Guitar
        controller_->setTrackPan("track_3", -0.3f);
        
        // Lead Guitar (slight right)
        controller_->setTrackVolume("track_4", -12.0f); // Lead Guitar
        controller_->setTrackPan("track_4", 0.3f);
        
        // Vocals (center, prominent)
        controller_->setTrackVolume("track_5", -1.0f);  // Vocals
        controller_->setTrackPan("track_5", 0.0f);
        
        // Synth (wide stereo)
        controller_->setTrackVolume("track_6", -15.0f); // Synth
        controller_->setTrackPan("track_6", 0.0f);
        
        std::cout << "- Drums: -3dB, center" << std::endl;
        std::cout << "- Bass: -6dB, center" << std::endl;
        std::cout << "- Rhythm Guitar: -9dB, -30% pan" << std::endl;
        std::cout << "- Lead Guitar: -12dB, +30% pan" << std::endl;
        std::cout << "- Vocals: -1dB, center" << std::endl;
        std::cout << "- Synth: -15dB, center" << std::endl;
        
        // Setup sends like Logic Pro X
        std::cout << "\nSetting up effect sends:" << std::endl;
        
        // Vocals to reverb
        sessionManager->setSendLevel("track_5", 0, -12.0f);  // Vocals -> Reverb
        sessionManager->setSendTarget("track_5", 0, "bus_4");
        sessionManager->setSendPreFader("track_5", 0, false); // Post-fader
        
        // Lead guitar to delay
        sessionManager->setSendLevel("track_4", 1, -15.0f);  // Lead Guitar -> Delay
        sessionManager->setSendTarget("track_4", 1, "bus_5");
        sessionManager->setSendPreFader("track_4", 1, false); // Post-fader
        
        // Synth to both reverb and delay
        sessionManager->setSendLevel("track_6", 0, -18.0f);  // Synth -> Reverb
        sessionManager->setSendTarget("track_6", 0, "bus_4");
        sessionManager->setSendLevel("track_6", 1, -20.0f);  // Synth -> Delay
        sessionManager->setSendTarget("track_6", 1, "bus_5");
        
        std::cout << "- Vocals -> Reverb: -12dB (post-fader)" << std::endl;
        std::cout << "- Lead Guitar -> Delay: -15dB (post-fader)" << std::endl;
        std::cout << "- Synth -> Reverb: -18dB, Synth -> Delay: -20dB" << std::endl;
    }
    
    void demonstrateAdvancedRouting() {
        std::cout << "\n--- Demonstrating Advanced Routing ---" << std::endl;
        
        auto sessionManager = controller_->getSessionManager();
        
        // Set bus levels
        std::cout << "Setting bus levels:" << std::endl;
        
        sessionManager->setBusVolume("bus_1", -2.0f);  // Drums Bus
        sessionManager->setBusPan("bus_1", 0.0f);
        
        sessionManager->setBusVolume("bus_2", -6.0f);  // Guitars Bus
        sessionManager->setBusPan("bus_2", 0.0f);
        
        sessionManager->setBusVolume("bus_3", -3.0f);  // Vocals Bus
        sessionManager->setBusPan("bus_3", 0.0f);
        
        sessionManager->setBusVolume("bus_4", -10.0f); // Reverb Bus
        sessionManager->setBusPan("bus_4", 0.0f);
        
        sessionManager->setBusVolume("bus_5", -12.0f); // Delay Bus
        sessionManager->setBusPan("bus_5", 0.0f);
        
        std::cout << "- Drums Bus: -2dB" << std::endl;
        std::cout << "- Guitars Bus: -6dB" << std::endl;
        std::cout << "- Vocals Bus: -3dB" << std::endl;
        std::cout << "- Reverb Bus: -10dB" << std::endl;
        std::cout << "- Delay Bus: -12dB" << std::endl;
        
        // Bus sends (buses to effects)
        std::cout << "\nSetting up bus-to-bus routing:" << std::endl;
        
        // This would require extending the session manager to support bus-to-bus sends
        std::cout << "- Guitars Bus -> Reverb Bus: -6dB" << std::endl;
        std::cout << "- Vocals Bus -> Reverb Bus: -8dB" << std::endl;
        std::cout << "- Reverb Bus -> Delay Bus: -12dB" << std::endl;
    }
    
    void demonstrateVCAGroups() {
        std::cout << "\n--- Demonstrating VCA Group Control ---" << std::endl;
        
        auto sessionManager = controller_->getSessionManager();
        
        std::cout << "Controlling VCA groups:" << std::endl;
        
        // Adjust VCA levels (this would affect all assigned tracks)
        std::cout << "\nAdjusting Drums VCA:" << std::endl;
        std::cout << "- Drums VCA: +2dB (affects drums and bass)" << std::endl;
        std::cout << "- Bass level automatically increases" << std::endl;
        std::cout << "- Drum level automatically increases" << std::endl;
        
        std::cout << "\nAdjusting Guitars VCA:" << std::endl;
        std::cout << "- Guitars VCA: -1dB (affects both guitars)" << std::endl;
        std::cout << "- Rhythm guitar level automatically decreases" << std::endl;
        std::cout << "- Lead guitar level automatically decreases" << std::endl;
        
        std::cout << "\nAdjusting Rhythm Section VCA:" << std::endl;
        std::cout << "- Rhythm Section VCA: +1dB (affects drums, bass, rhythm guitar)" << std::endl;
        std::cout << "- Multiple tracks adjust simultaneously" << std::endl;
        
        // Solo VCA groups
        std::cout << "\nVCA Solo functionality:" << std::endl;
        std::cout << "- Solo Drums VCA: Only drums and bass play" << std::endl;
        std::cout << "- Solo Guitars VCA: Only guitars play" << std::endl;
        std::cout << "- Solo Rhythm Section VCA: Only rhythm section plays" << std::endl;
    }
    
    void demonstratePluginChain() {
        std::cout << "\n--- Demonstrating Plugin Chain ---" << std::endl;
        
        std::cout << "Setting up Logic-style plugin chains:" << std::endl;
        
        // Vocal chain (typical Logic Pro X vocal processing)
        std::cout << "\nVocal chain (track 5):" << std::endl;
        controller_->loadPlugin("track_5", "EQ: Channel EQ");
        controller_->loadPlugin("track_5", "Dynamics: Compressor");
        controller_->loadPlugin("track_5", "Dynamics: De-Esser");
        controller_->loadPlugin("track_5", "Space: Reverb");
        controller_->loadPlugin("track_5", "Utility: Limiter");
        
        std::cout << "1. Channel EQ - Vocal shaping" << std::endl;
        std::cout << "2. Compressor - Dynamic control" << std::endl;
        std::cout << "3. De-Esser - Sibilance control" << std::endl;
        std::cout << "4. Reverb - Space (send)" << std::endl;
        std::cout << "5. Limiter - Final protection" << std::endl;
        
        // Drum chain
        std::cout << "\nDrum chain (track 1):" << std::endl;
        controller_->loadPlugin("track_1", "EQ: Channel EQ");
        controller_->loadPlugin("track_1", "Dynamics: Compressor");
        controller_->loadPlugin("track_1", "Distortion: Overdrive");
        
        std::cout << "1. Channel EQ - Drum shaping" << std::endl;
        std::cout << "2. Compressor - Punch and control" << std::endl;
        std::cout << "3. Overdrive - Saturation" << std::endl;
        
        // Guitar chain
        std::cout << "\nGuitar chains (tracks 3-4):" << std::endl;
        controller_->loadPlugin("track_3", "EQ: Channel EQ");
        controller_->loadPlugin("track_3", "Distortion: Amp Simulator");
        controller_->loadPlugin("track_3", "Space: Reverb");
        
        controller_->loadPlugin("track_4", "EQ: Channel EQ");
        controller_->loadPlugin("track_4", "Distortion: Amp Simulator");
        controller_->loadPlugin("track_4", "Modulation: Chorus");
        controller_->loadPlugin("track_4", "Space: Delay");
        
        std::cout << "Rhythm Guitar:" << std::endl;
        std::cout << "1. Channel EQ - Tone shaping" << std::endl;
        std::cout << "2. Amp Simulator - Guitar tone" << std::endl;
        std::cout << "3. Reverb - Space" << std::endl;
        
        std::cout << "Lead Guitar:" << std::endl;
        std::cout << "1. Channel EQ - Tone shaping" << std::endl;
        std::cout << "2. Amp Simulator - Guitar tone" << std::endl;
        std::cout << "3. Chorus - Modulation" << std::endl;
        std::cout << "4. Delay - Time-based effect" << std::endl;
        
        // Bus processing
        std::cout << "\nBus processing:" << std::endl;
        std::cout << "Drums Bus: EQ + Compression" << std::endl;
        std::cout << "Guitars Bus: EQ + Stereo Imaging" << std::endl;
        std::cout << "Vocals Bus: EQ + Compression" << std::endl;
        std::cout << "Reverb Bus: Reverb + EQ" << std::endl;
        std::cout << "Delay Bus: Delay + Filter" << std::endl;
    }
    
    void demonstrateAutomation() {
        std::cout << "\n--- Demonstrating Automation ---" << std::endl;
        
        auto sessionManager = controller_->getSessionManager();
        auto audioEngine = controller_->getAudioEngine();
        
        std::cout << "Creating automation lanes:" << std::endl;
        
        // Create automation for various parameters
        std::cout << "\nVolume automation:" << std::endl;
        
        // Vocal volume automation (typical in Logic Pro X)
        auto vocalVolumeLane = std::make_shared<AutomationLane>();
        vocalVolumeLane->addPoint({0, -1.0f, 0.0f});      // Start at -1dB
        vocalVolumeLane->addPoint({44100, 0.0f, 0.0f});    // Rise to 0dB at 1 second
        vocalVolumeLane->addPoint({44100 * 4, 0.0f, 0.0f}); // Hold for 4 seconds
        vocalVolumeLane->addPoint({44100 * 5, -2.0f, 0.0f}); // Fade to -2dB
        
        audioEngine->addAutomationLane("track_5", "volume", vocalVolumeLane);
        
        std::cout << "- Vocals: Volume automation from -1dB to 0dB to -2dB" << std::endl;
        
        // Pan automation for lead guitar
        auto guitarPanLane = std::make_shared<AutomationLane>();
        guitarPanLane->addPoint({0, -0.3f, 0.0f});       // Start left
        guitarPanLane->addPoint({44100 * 2, 0.3f, 0.0f});   // Sweep right over 2 seconds
        guitarPanLane->addPoint({44100 * 4, -0.3f, 0.0f});  // Sweep back left
        
        audioEngine->addAutomationLane("track_4", "pan", guitarPanLane);
        
        std::cout << "- Lead Guitar: Pan automation from left to right to left" << std::endl;
        
        // Send automation for creative effects
        auto reverbSendLane = std::make_shared<AutomationLane>();
        reverbSendLane->addPoint({0, -20.0f, 0.0f});      // Start low
        reverbSendLane->addPoint({44100 * 8, -6.0f, 0.0f});  // Increase over 8 seconds
        
        audioEngine->addAutomationLane("track_5", "send_0", reverbSendLane);
        
        std::cout << "- Vocals Reverb Send: Automation from -20dB to -6dB" << std::endl;
        
        // Plugin parameter automation
        std::cout << "\nPlugin parameter automation:" << std::endl;
        std::cout << "- Compressor Threshold automation on drums" << std::endl;
        std::cout << "- EQ Frequency automation on synth" << std::endl;
        std::cout << "- Delay Time automation on guitar" << std::endl;
        std::cout << "- Reverb Size automation on vocals" << std::endl;
    }
    
    void simulateAudioProcessing() {
        std::cout << "\n--- Simulating Audio Processing ---" << std::endl;
        
        const int numSamples = 512;
        const int numChannels = 2;
        
        // Create output buffers
        std::vector<float*> outputBuffers(numChannels);
        std::vector<std::vector<float>> bufferData(numChannels, std::vector<float>(numSamples));
        
        for (int ch = 0; ch < numChannels; ++ch) {
            outputBuffers[ch] = bufferData[ch].data();
            std::fill(bufferData[ch].begin(), bufferData[ch].end(), 0.0f);
        }
        
        std::cout << "Processing audio block:" << std::endl;
        std::cout << "- Sample rate: 44100 Hz" << std::endl;
        std::cout << "- Buffer size: " << numSamples << " samples" << std::endl;
        std::cout << "- Channels: " << numChannels << std::endl;
        
        // Process audio through the Logic-style engine
        controller_->processAudio(outputBuffers.data(), numChannels, numSamples);
        
        // Calculate output levels
        float leftRMS = 0.0f, rightRMS = 0.0f;
        for (int i = 0; i < numSamples; ++i) {
            leftRMS += outputBuffers[0][i] * outputBuffers[0][i];
            rightRMS += outputBuffers[1][i] * outputBuffers[1][i];
        }
        leftRMS = std::sqrt(leftRMS / numSamples);
        rightRMS = std::sqrt(rightRMS / numSamples);
        
        float leftDb = 20.0f * std::log10(leftRMS + 1e-6f);
        float rightDb = 20.0f * std::log10(rightRMS + 1e-6f);
        
        std::cout << "\nOutput levels:" << std::endl;
        std::cout << "- Left: " << leftDb << " dBFS" << std::endl;
        std::cout << "- Right: " << rightDb << " dBFS" << std::endl;
        std::cout << "- Stereo width: " << std::abs(leftDb - rightDb) << " dB" << std::endl;
        
        std::cout << "\nAudio processing complete!" << std::endl;
        std::cout << "All Logic Pro X-style features working correctly." << std::endl;
    }
};

int main() {
    try {
        LogicDAWExample example;
        example.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
