#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <map>
#include <memory>
#include <mutex>

namespace patchcraft
{
    /**
        Live (current) parameter values, separate from ParameterDef which only
        describes the parameter's identity / range / unit.

        Thread-safety:
        - The map structure is mutated only on the message thread.
        - Each value is an std::atomic<float>, safe to read from the audio thread
          via getRaw(). The pointer returned by getOrAddRaw() is stable for the
          lifetime of the LiveValueStore (we never delete entries).

        Listeners are notified on the message thread when a value changes via
        a setValue() call. The audio thread typically reads the atomic without
        going through setValue(), so listeners are about *user* changes.
    */
    class LiveValueStore
    {
    public:
        LiveValueStore() = default;

        // Get or create a slot. Stable pointer for the life of this object.
        std::atomic<float>* getOrAddRaw (const juce::String& parameterId, float defaultValue = 0.0f);

        // Returns nullptr if the slot doesn't exist.
        std::atomic<float>* getRaw (const juce::String& parameterId) const;

        float getValue (const juce::String& parameterId, float fallback = 0.0f) const;
        void  setValue (const juce::String& parameterId, float value);

        void clear();

        struct Listener
        {
            virtual ~Listener() = default;
            // Called on message thread when a parameter's live value changes.
            virtual void liveValueChanged (const juce::String& parameterId, float newValue) = 0;
        };
        void addListener (Listener* l);
        void removeListener (Listener* l);

    private:
        mutable std::mutex mtx;
        std::map<juce::String, std::unique_ptr<std::atomic<float>>> values;
        juce::ListenerList<Listener> listeners;
    };

} // namespace patchcraft
