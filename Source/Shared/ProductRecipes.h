#pragma once

#include <juce_core/juce_core.h>

namespace patchcraft
{
    class PatchCraftProject;

    enum class ProductKind
    {
        SampleInstrument = 1,
        LoopChopInstrument,
        SynthInstrument,
        HybridInstrument,
        FXPlugin,
        DrumMachine
    };

    struct ProductRecipeInfo
    {
        juce::String id;
        juce::String displayName;
        juce::String subtitle;
        juce::String engineId;
        juce::String category;
        juce::String layoutModuleId;
        bool showChopStep = false;
    };

    struct ProductTemplateSpec
    {
        juce::String templateId;
        juce::String displayName;
        juce::String layoutModuleId;
        ProductKind kind = ProductKind::SampleInstrument;
        bool showChopStep = false;
    };

    ProductRecipeInfo recipeInfoFor (ProductKind kind);
    ProductTemplateSpec templateSpecFor (const juce::String& templateId);
    juce::String defaultTemplateForKind (ProductKind kind);
    juce::StringArray allTemplateIds();

    void applyProductRecipe (PatchCraftProject& project, ProductKind kind);
    void applyProductTemplate (PatchCraftProject& project, const juce::String& templateId);
    void applyTemplateLiveDefaults (PatchCraftProject& project, const juce::String& templateId);
    void ensureSimpleStackForEngine (PatchCraftProject& project, const juce::String& engineId);

} // namespace patchcraft
