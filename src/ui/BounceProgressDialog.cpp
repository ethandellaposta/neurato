#include "ui/BounceProgressDialog.h"

namespace neurato {

BounceProgressDialog::BounceProgressDialog(const Session& session,
                                             const juce::File& outputFile,
                                             const OfflineRenderer::Settings& settings)
    : sessionCopy_(session),
      outputFile_(outputFile),
      settings_(settings)
{
    statusLabel_ = std::make_unique<juce::Label>("status", "Bouncing...");
    statusLabel_->setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel_->setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel_.get());

    progressBar_ = std::make_unique<juce::ProgressBar>(progress_);
    addAndMakeVisible(progressBar_.get());

    cancelButton_ = std::make_unique<juce::TextButton>("Cancel");
    cancelButton_->onClick = [this] {
        cancelFlag_.store(true, std::memory_order_release);
        cancelButton_->setEnabled(false);
        statusLabel_->setText("Cancelling...", juce::dontSendNotification);
    };
    addAndMakeVisible(cancelButton_.get());

    setSize(360, 120);
    startRender();
    startTimerHz(20);
}

BounceProgressDialog::~BounceProgressDialog()
{
    stopTimer();
    cancelFlag_.store(true, std::memory_order_release);
    if (renderThread_.joinable())
        renderThread_.join();
}

void BounceProgressDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1a1a2e));
}

void BounceProgressDialog::resized()
{
    auto area = getLocalBounds().reduced(16);
    statusLabel_->setBounds(area.removeFromTop(24));
    area.removeFromTop(8);
    progressBar_->setBounds(area.removeFromTop(20));
    area.removeFromTop(12);
    cancelButton_->setBounds(area.removeFromBottom(28).withSizeKeepingCentre(80, 28));
}

void BounceProgressDialog::timerCallback()
{
    progress_ = atomicProgress_.load(std::memory_order_acquire);

    if (renderDone_.load(std::memory_order_acquire))
    {
        stopTimer();
        onRenderComplete();
    }
}

void BounceProgressDialog::startRender()
{
    renderThread_ = std::thread([this] {
        bool success = OfflineRenderer::render(
            sessionCopy_, outputFile_, settings_,
            [this](const OfflineRenderer::Progress& p) {
                atomicProgress_.store(p.fraction, std::memory_order_release);
                if (!p.error.isEmpty())
                {
                    errorMessage_ = p.error;
                    hasError_.store(true, std::memory_order_release);
                }
            },
            &cancelFlag_);

        renderSuccess_.store(success, std::memory_order_release);
        renderDone_.store(true, std::memory_order_release);
    });
}

void BounceProgressDialog::onRenderComplete()
{
    bool success = renderSuccess_.load(std::memory_order_acquire);

    if (hasError_.load(std::memory_order_acquire))
    {
        statusLabel_->setText("Error: " + errorMessage_, juce::dontSendNotification);
        cancelButton_->setButtonText("Close");
        cancelButton_->setEnabled(true);
        cancelButton_->onClick = [this] {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->closeButtonPressed();
        };
    }
    else if (cancelFlag_.load(std::memory_order_acquire))
    {
        statusLabel_->setText("Cancelled", juce::dontSendNotification);
        cancelButton_->setButtonText("Close");
        cancelButton_->setEnabled(true);
        cancelButton_->onClick = [this] {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->closeButtonPressed();
        };
    }
    else if (success)
    {
        statusLabel_->setText("Bounce complete!", juce::dontSendNotification);
        progress_ = 1.0;
        cancelButton_->setButtonText("Close");
        cancelButton_->onClick = [this] {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->closeButtonPressed();
        };
    }
}

void BounceProgressDialog::launch(const Session& session,
                                    const juce::File& outputFile,
                                    const OfflineRenderer::Settings& settings)
{
    auto* dialog = new BounceProgressDialog(session, outputFile, settings);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(dialog);
    options.dialogTitle = "Exporting Audio";
    options.dialogBackgroundColour = juce::Colour(0xff1a1a2e);
    options.escapeKeyTriggersCloseButton = false;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.launchAsync();
}

} // namespace neurato
