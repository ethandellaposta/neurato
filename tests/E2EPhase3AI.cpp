#include <gtest/gtest.h>

#include "JuceGuiFixture.hpp"
#include "ai/AIImplementation.hpp"
#include "model/Session.hpp"

#include <algorithm>
#include <string>

namespace ampl
{

TEST_F(JuceGuiFixture, EditPreviewDiffAndExplainAreHumanCentered)
{
    auto session = std::make_shared<Session>();
    const int trackIndex = session->addTrack("Lead Vox");
    ASSERT_GE(trackIndex, 0);

    auto *track = session->getTrack(trackIndex);
    ASSERT_NE(track, nullptr);
    const auto trackId = track->id.toStdString();

    auto sessionState = std::make_shared<SessionStateAPI>();
    sessionState->setSession(session);

    EditPreviewUI previewUI;
    ActionDSL::ActionSequence actions;

    auto gainAction = ActionDSL::setTrackGain(trackId, -8.0f);
    gainAction->confidence = 0.90f;
    actions.push_back(std::move(gainAction));

    auto muteAction = ActionDSL::setTrackMute(trackId, true);
    muteAction->confidence = 0.85f;
    actions.push_back(std::move(muteAction));

    auto preview = previewUI.generatePreview(std::move(actions), sessionState->generateSnapshot());
    const auto previewId = preview.id;
    previewUI.showPreview(std::move(preview));

    EXPECT_EQ(previewUI.pendingPreviewCount(), 1u);

    const auto diff = previewUI.getPreviewDiff(previewId);
    EXPECT_FALSE(diff.empty());

    const bool hasGainChange =
        std::any_of(diff.begin(), diff.end(), [](const EditPreviewUI::DiffItem &item)
                    { return item.type == "track" && item.property == "gain"; });
    const bool hasMuteChange =
        std::any_of(diff.begin(), diff.end(), [](const EditPreviewUI::DiffItem &item)
                    { return item.type == "track" && item.property == "muted"; });

    EXPECT_TRUE(hasGainChange);
    EXPECT_TRUE(hasMuteChange);

    const auto explanation = previewUI.explainPreview(previewId);
    EXPECT_NE(explanation.find("Confidence"), std::string::npos);
    EXPECT_NE(explanation.find("Actions"), std::string::npos);
    EXPECT_NE(explanation.find("Changes"), std::string::npos);
}

TEST_F(JuceGuiFixture, AcceptingPreviewAppliesActionsToSession)
{
    auto session = std::make_shared<Session>();
    const int trackIndex = session->addTrack("Bass");
    ASSERT_GE(trackIndex, 0);

    const auto initialTrackCount = session->getTracks().size();

    auto sessionState = std::make_shared<SessionStateAPI>();
    sessionState->setSession(session);

    EditPreviewUI previewUI;
    previewUI.setPreviewAcceptedCallback(
        [&](const EditPreviewUI::EditPreview &accepted)
        { EXPECT_TRUE(sessionState->applyActionSequence(accepted.actions)); });

    ActionDSL::ActionSequence actions;
    auto createTrack = ActionDSL::createTrack("AI Added Track", false);
    createTrack->confidence = 0.92f;
    actions.push_back(std::move(createTrack));

    auto preview = previewUI.generatePreview(std::move(actions), sessionState->generateSnapshot());
    EXPECT_GT(preview.confidence, 0.9f);

    previewUI.showPreview(std::move(preview));
    EXPECT_TRUE(previewUI.isVisible());
    EXPECT_EQ(previewUI.pendingPreviewCount(), 1u);
    EXPECT_EQ(session->getTracks().size(), initialTrackCount);

    previewUI.applyAllPreviews();
    EXPECT_EQ(session->getTracks().size(), initialTrackCount + 1);
}

} // namespace ampl
