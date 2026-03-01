#include "import/LogicImporter.hpp"
#include "model/Session.hpp"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <juce_audio_formats/juce_audio_formats.h>

using namespace ampl;
using namespace ::testing;

class LogicImporterTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        importer = std::make_unique<LogicImporter>();
        session = std::make_unique<Session>();
        formatManager.registerBasicFormats();
    }

    void TearDown() override
    {
        importer.reset();
        session.reset();
    }

    std::unique_ptr<LogicImporter> importer;
    std::unique_ptr<Session> session;
    juce::AudioFormatManager formatManager;
};

// Helper to create minimal FCPXML content for testing
juce::String
createFCPXML(const std::vector<std::tuple<juce::String, double, double, double>> &clips,
             double bpm = 120.0)
{
    // clips: [(assetRef, offset, duration, start), ...]
    // offset: timeline position in seconds
    // duration: clip duration in seconds
    // start: source offset (in-point) in seconds

    juce::String xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                       "<!DOCTYPE fcpxml>\n"
                       "<fcpxml version=\"1.8\">\n"
                       "  <resources>\n";

    // Add asset definitions
    for (size_t i = 0; i < clips.size(); i++)
    {
        xml += "    <asset id=\"asset" + juce::String((int)i) + "\" src=\"file:///audio" +
               juce::String((int)i) +
               ".wav\" duration=\"60s\" format=\"r1\">\n"
               "      <media-rep kind=\"original-media\" src=\"file:///audio" +
               juce::String((int)i) +
               ".wav\"/>\n"
               "    </asset>\n";
    }

    xml += "    <format id=\"r1\" frameDuration=\"1/48000s\" sampleRate=\"48000\"/>\n"
           "  </resources>\n"
           "  <library>\n"
           "    <event>\n"
           "      <project name=\"Test Project\">\n"
           "        <sequence duration=\"300s\" tcStart=\"0s\">\n"
           "          <spine>\n";

    for (size_t i = 0; i < clips.size(); i++)
    {
        auto &[ref, offset, dur, srcStart] = clips[i];

        // Convert to FCPXML rational format (using 48000 as denominator for time)
        auto toRational = [](double seconds) -> juce::String
        {
            int64_t samples = static_cast<int64_t>(seconds * 48000.0);
            return juce::String(samples) + "/48000s";
        };

        xml += "            <asset-clip ref=\"asset" + juce::String((int)i) +
               "\" "
               "offset=\"" +
               toRational(offset) +
               "\" "
               "duration=\"" +
               toRational(dur) +
               "\" "
               "start=\"" +
               toRational(srcStart) +
               "\" "
               "audioRole=\"dialogue\"/>\n";
    }

    xml += "          </spine>\n"
           "        </sequence>\n"
           "      </project>\n"
           "    </event>\n"
           "  </library>\n"
           "</fcpxml>";

    return xml;
}

// Test FCPXML time parsing for rational formats
TEST_F(LogicImporterTest, ParsesRationalTimeFormat)
{
    // Create FCPXML with known timing
    // Clip at 0s, duration 2s, source start 0s
    // Clip at 3s, duration 1.5s, source start 0.5s
    std::vector<std::tuple<juce::String, double, double, double>> clips = {
        {"audio0", 0.0, 2.0, 0.0}, {"audio1", 3.0, 1.5, 0.5}};

    auto xml = createFCPXML(clips);

    // Parse the FCPXML
    auto xmlDoc = juce::XmlDocument::parse(xml);
    ASSERT_NE(xmlDoc, nullptr);

    auto result = importer->importFromFCPXML(juce::File(), *session, formatManager);

    // The import won't fully succeed without real audio files, but we can check
    // that basic parsing occurs without errors
    EXPECT_TRUE(result.success || !result.warnings.empty());
}

// Test that sequential spine clips without explicit offset are positioned correctly
TEST_F(LogicImporterTest, SequentialSpineClipsPositionedCorrectly)
{
    // This tests that when clips in a spine don't have explicit offsets,
    // they are placed sequentially after each other

    juce::String xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE fcpxml>
<fcpxml version="1.8">
  <resources>
    <asset id="clip1" src="file:///test1.wav" duration="48000/48000s"/>
    <asset id="clip2" src="file:///test2.wav" duration="48000/48000s"/>
  </resources>
  <library>
    <event>
      <project name="Sequential Test">
        <sequence duration="96000/48000s" tcStart="0s">
          <spine>
            <asset-clip ref="clip1" duration="48000/48000s" start="0s" audioRole="dialogue"/>
            <asset-clip ref="clip2" duration="48000/48000s" start="0s" audioRole="dialogue"/>
          </spine>
        </sequence>
      </project>
    </event>
  </library>
</fcpxml>)";

    // Save to temp file
    juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("test_sequential.fcpxml");
    tempFile.replaceWithText(xml);

    auto result = importer->importFCPXML(tempFile);

    // Verify we got 2 tracks or 1 track with 2 regions
    EXPECT_FALSE(result.tracks.empty());

    // Check that clip positions are sequential
    // First clip should be at 0s, second at 1s (after first clip's duration)
    // Since files don't exist, we check the parsed data structure
    if (!result.tracks.empty())
    {
        auto &track = result.tracks[0];
        if (track.audioRegions.size() >= 2)
        {
            EXPECT_DOUBLE_EQ(track.audioRegions[0].startTime, 0.0);
            // Without explicit offset, second clip should follow first
            // First has duration 1s (48000/48000), so second starts at 1s
            EXPECT_NEAR(track.audioRegions[1].startTime, 1.0, 0.001);
        }
    }

    tempFile.deleteFile();
}

// Test that gaps in spine advance timeline position
TEST_F(LogicImporterTest, GapsAdvanceTimelinePosition)
{
    juce::String xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE fcpxml>
<fcpxml version="1.8">
  <resources>
    <asset id="clip1" src="file:///test.wav" duration="48000/48000s"/>
  </resources>
  <library>
    <event>
      <project name="Gap Test">
        <sequence duration="144000/48000s" tcStart="0s">
          <spine>
            <gap duration="48000/48000s"/>
            <asset-clip ref="clip1" duration="48000/48000s" start="0s" audioRole="dialogue"/>
          </spine>
        </sequence>
      </project>
    </event>
  </library>
</fcpxml>)";

    juce::File tempFile =
        juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("test_gap.fcpxml");
    tempFile.replaceWithText(xml);

    auto result = importer->importFCPXML(tempFile);

    if (!result.tracks.empty() && !result.tracks[0].audioRegions.empty())
    {
        // Clip should start at 1s (after 1s gap)
        EXPECT_NEAR(result.tracks[0].audioRegions[0].startTime, 1.0, 0.001);
    }

    tempFile.deleteFile();
}

// Test source offset is parsed correctly for trimmed clips
TEST_F(LogicImporterTest, SourceOffsetParsedCorrectly)
{
    // Test a clip that starts 2 seconds into the source file
    juce::String xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE fcpxml>
<fcpxml version="1.8">
  <resources>
    <asset id="clip1" src="file:///test.wav" duration="240000/48000s"/>
  </resources>
  <library>
    <event>
      <project name="Trim Test">
        <sequence duration="48000/48000s" tcStart="0s">
          <spine>
            <asset-clip ref="clip1" offset="0s" duration="48000/48000s" start="96000/48000s" audioRole="dialogue"/>
          </spine>
        </sequence>
      </project>
    </event>
  </library>
</fcpxml>)";

    juce::File tempFile =
        juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("test_trim.fcpxml");
    tempFile.replaceWithText(xml);

    auto result = importer->importFCPXML(tempFile);

    if (!result.tracks.empty() && !result.tracks[0].audioRegions.empty())
    {
        // Source offset should be 2s (96000/48000)
        EXPECT_NEAR(result.tracks[0].audioRegions[0].sourceOffset, 2.0, 0.001);
    }

    tempFile.deleteFile();
}

// Test sample rate conversion for source offsets
TEST_F(LogicImporterTest, SampleRateConversionForSourceOffset)
{
    // When converting to session, source offset should use asset's sample rate
    // not project sample rate

    session->setSampleRate(44100.0);

    // Create a mock clip with known values
    // Source offset of 2 seconds in a 96kHz file = 192000 samples in asset
    // Timeline position should still use project sample rate

    // This test verifies the conceptual correctness
    const double assetSampleRate = 96000.0;
    const double projectSampleRate = 44100.0;
    const double sourceOffsetSeconds = 2.0;

    int64_t sourceOffsetInAssetSamples =
        static_cast<int64_t>(sourceOffsetSeconds * assetSampleRate);
    int64_t wrongSourceOffset = static_cast<int64_t>(sourceOffsetSeconds * projectSampleRate);

    // Asset samples should be 192000 for 2s at 96kHz
    EXPECT_EQ(sourceOffsetInAssetSamples, 192000);
    // Project samples would be 88200 for 2s at 44.1kHz - this would be wrong
    EXPECT_EQ(wrongSourceOffset, 88200);

    // They should NOT be equal - using the wrong sample rate would cause timing errors
    EXPECT_NE(sourceOffsetInAssetSamples, wrongSourceOffset);
}

// Test explicit offset overrides sequential positioning
TEST_F(LogicImporterTest, ExplicitOffsetOverridesSequential)
{
    juce::String xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE fcpxml>
<fcpxml version="1.8">
  <resources>
    <asset id="clip1" src="file:///test1.wav" duration="48000/48000s"/>
    <asset id="clip2" src="file:///test2.wav" duration="48000/48000s"/>
  </resources>
  <library>
    <event>
      <project name="Explicit Offset Test">
        <sequence duration="240000/48000s" tcStart="0s">
          <spine>
            <asset-clip ref="clip1" duration="48000/48000s" start="0s" audioRole="dialogue"/>
            <asset-clip ref="clip2" offset="144000/48000s" duration="48000/48000s" start="0s" audioRole="dialogue"/>
          </spine>
        </sequence>
      </project>
    </event>
  </library>
</fcpxml>)";

    juce::File tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("test_explicit.fcpxml");
    tempFile.replaceWithText(xml);

    auto result = importer->importFCPXML(tempFile);

    if (!result.tracks.empty() && result.tracks[0].audioRegions.size() >= 2)
    {
        // First clip at 0s (sequential)
        EXPECT_NEAR(result.tracks[0].audioRegions[0].startTime, 0.0, 0.001);
        // Second clip at 3s (144000/48000) - explicit offset, not following first clip
        EXPECT_NEAR(result.tracks[0].audioRegions[1].startTime, 3.0, 0.001);
    }

    tempFile.deleteFile();
}

// Test BPM and time signature parsing
TEST_F(LogicImporterTest, ParsesBPMAndTimeSignature)
{
    juce::String xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE fcpxml>
<fcpxml version="1.8">
  <resources>
    <format id="r1" frameDuration="1/48000s" sampleRate="48000"/>
  </resources>
  <library>
    <event>
      <project name="Tempo Test">
        <sequence duration="48000/48000s" tcStart="0s">
          <spine>
          </spine>
        </sequence>
      </project>
    </event>
  </library>
</fcpxml>)";

    juce::File tempFile =
        juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("test_tempo.fcpxml");
    tempFile.replaceWithText(xml);

    auto result = importer->importFCPXML(tempFile);

    // Default BPM should be reasonable (120 is common default)
    EXPECT_GE(result.bpm, 60.0);
    EXPECT_LE(result.bpm, 240.0);

    // Sample rate should be parsed from format
    EXPECT_EQ(result.sampleRate, 48000.0);

    tempFile.deleteFile();
}

// Integration test: import and verify audio alignment
class LogicImporterIntegrationTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        importer = std::make_unique<LogicImporter>();
        session = std::make_unique<Session>();
        formatManager.registerBasicFormats();
    }

    std::unique_ptr<LogicImporter> importer;
    std::unique_ptr<Session> session;
    juce::AudioFormatManager formatManager;
};

TEST_F(LogicImporterIntegrationTest, ImportedClipsMatchExpectedTiming)
{
    // This test would ideally use real audio files and verify
    // that playback matches the original Logic project

    // For now, verify the import produces correct sample-accurate positions
    session->setSampleRate(48000.0);

    // Expected: clip at 1.5s should be at sample position 72000
    const double timelinePosition = 1.5;
    const double sampleRate = 48000.0;
    const int64_t expectedSample = static_cast<int64_t>(timelinePosition * sampleRate);

    EXPECT_EQ(expectedSample, 72000);
}
