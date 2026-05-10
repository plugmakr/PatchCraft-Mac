#include "LiveValueStore.h"

#include <cmath>

namespace patchcraft
{
    std::atomic<float>* LiveValueStore::getOrAddRaw (const juce::String& id, float def)
    {
        std::lock_guard<std::mutex> lock (mtx);
        auto it = values.find (id);
        if (it != values.end()) return it->second.get();
        auto slot = std::make_unique<std::atomic<float>> (def);
        auto* raw = slot.get();
        values.emplace (id, std::move (slot));
        return raw;
    }

    std::atomic<float>* LiveValueStore::getRaw (const juce::String& id) const
    {
        std::lock_guard<std::mutex> lock (mtx);
        auto it = values.find (id);
        return it == values.end() ? nullptr : it->second.get();
    }

    float LiveValueStore::getValue (const juce::String& id, float fallback) const
    {
        if (auto* v = getRaw (id)) return v->load (std::memory_order_relaxed);
        return fallback;
    }

    void LiveValueStore::setValue (const juce::String& id, float value)
    {
        auto* raw = getOrAddRaw (id, value);
        const float prev = raw->load (std::memory_order_relaxed);
        if (std::abs (prev - value) < 0.000001f) return;
        raw->store (value, std::memory_order_relaxed);
        listeners.call ([&] (Listener& l) { l.liveValueChanged (id, value); });
    }

    void LiveValueStore::clear()
    {
        std::lock_guard<std::mutex> lock (mtx);
        values.clear();
    }

    void LiveValueStore::addListener (Listener* l)    { listeners.add (l); }
    void LiveValueStore::removeListener (Listener* l) { listeners.remove (l); }

} // namespace patchcraft
