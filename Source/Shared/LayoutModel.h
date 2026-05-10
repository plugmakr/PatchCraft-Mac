#pragma once

#include "PatchCraftTypes.h"

namespace patchcraft
{
    /**
        Editable list of LayoutElement records, in z-order (front of vector = back of canvas).
    */
    class LayoutModel
    {
    public:
        LayoutModel();

        std::vector<LayoutElement>&       getAll()       { return elements; }
        const std::vector<LayoutElement>& getAll() const { return elements; }

        LayoutElement* find (const juce::String& id);
        int findIndex (const juce::String& id) const;

        juce::String generateUniqueId (const juce::String& prefix);
        LayoutElement& add (const LayoutElement& e);

        void remove (const juce::String& id);
        void bringForward (const juce::String& id);
        void sendBackward (const juce::String& id);

        void clear()                          { elements.clear(); }

        juce::var toVar (const CanvasSize& canvas) const;
        void fromVar (const juce::var& v, CanvasSize& outCanvas);

    private:
        std::vector<LayoutElement> elements;
    };

} // namespace patchcraft
