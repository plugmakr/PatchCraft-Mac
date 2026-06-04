#pragma once

#include <juce_graphics/juce_graphics.h>

namespace patchcraft
{
    /**
        Loads and caches images and audio buffers used by the runtime.
        Lives entirely on the message thread.
    */
    class AssetManager
    {
    public:
        AssetManager() = default;

        juce::Image loadImage (const juce::File& path);
        juce::Image loadControlFilmstrip (const juce::File& path, int frames, bool vertical, int maxFrameSize = 192);
        void clear();

        // Renders the default cinematic mountain background of any size.
        // Used when an instrument has no background image.
        static juce::Image renderDefaultHeroImage (int width, int height);

    private:
        juce::HashMap<juce::String, juce::Image> imageCache;
        juce::HashMap<juce::String, juce::Image> controlFilmstripCache;
    };

} // namespace patchcraft
