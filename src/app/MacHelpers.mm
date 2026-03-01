#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>
#include <cstring>

namespace ampl
{
void hideDockIconForSubprocess(); // declaration so -Wmissing-prototypes is satisfied
void suppressDockIconEarly(int argc, char **argv);

void hideDockIconForSubprocess()
{
    // Belt-and-suspenders: also called from initialise() for any late path.
    [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
}

void suppressDockIconEarly(int argc, char **argv)
{
    // Called from main() BEFORE juce::JUCEApplicationBase::main() so that
    // the activation policy is set before NSApplication registers with the Dock.
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--scan-plugin") == 0)
        {
            // Creating NSApp here (before JUCE does) is safe — JUCE will get
            // the same singleton.  Setting Prohibited before finishLaunching
            // prevents the icon from ever appearing in the Dock.
            [[NSApplication sharedApplication]
                setActivationPolicy:NSApplicationActivationPolicyProhibited];
            return;
        }
    }
}

} // namespace ampl

#endif // __APPLE__
