#pragma once

#include "PatchCraftProject.h"

namespace patchcraft
{
    class LaunchReadiness
    {
    public:
        enum class Scope
        {
            ExportPack,
            LaunchBundle,
            Publish
        };

        struct Report
        {
            int errorCount = 0;
            juce::StringArray blockingErrors;
        };

        static Report evaluate (const PatchCraftProject& project, Scope scope = Scope::ExportPack);
    };
}
