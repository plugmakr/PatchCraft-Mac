#include "TutorialHelpRegistry.h"

#include <map>

namespace patchcraft
{
    namespace
    {
        static juce::String normaliseKey (juce::String text)
        {
            return text.trim().toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz0123456789");
        }

        static std::map<juce::String, TutorialHelpEntry> buildRegistry()
        {
            std::map<juce::String, TutorialHelpEntry> entries;

            auto add = [&] (const juce::String& key, juce::String title, juce::String body)
            {
                entries[normaliseKey (key)] = { std::move (title), std::move (body) };
            };

            add ("inspector.type", "Control Type",
                 "Choose what kind of Player control this layer is: knob, slider, pad grid, tab panel, and so on. "
                 "The type controls which runtime behaviour and inspector fields are available.");
            add ("inspector.id", "Layer ID",
                 "Internal identifier used by pScript, presets, and exports. Keep IDs stable once a product is shipped.");
            add ("inspector.position", "Position",
                 "Canvas X/Y in design pixels. Drag on the Layout canvas or type exact coordinates here.");
            add ("inspector.size", "Size",
                 "Width and height of the control in design pixels. Hold Shift while dragging on canvas to resize proportionally.");
            add ("inspector.parameter", "Parameter Binding",
                 "Connect this control to a sound parameter from your instrument. Unbound controls look correct in Layout "
                 "but will not move audio until wired here or through the node graph.");
            add ("inspector.label", "Display Label",
                 "Text shown beneath or beside the control in the exported Player. Leave blank to hide the label.");
            add ("inspector.min", "Minimum Value",
                 "Lowest value the Player sends for this parameter. Match the real parameter range from the sound engine.");
            add ("min", "Minimum Value",
                 "Lowest value the Player sends for this parameter.");
            add ("max", "Maximum Value",
                 "Highest value the Player sends for this parameter.");
            add ("default", "Default Value",
                 "Starting value when the preset loads.");
            add ("step", "Step Size",
                 "Increment used by the control when adjusting this parameter.");
            add ("valuetype", "Value Type",
                 "How the Player interprets and displays values for this binding.");
            add ("inspector.max", "Maximum Value",
                 "Highest value the Player sends for this parameter.");
            add ("inspector.default", "Default Value",
                 "Starting value when the preset loads. Double-click a knob on the Layout canvas to reset to this value.");
            add ("inspector.smoothing", "Value Smoothing",
                 "How quickly the Player glides toward a new value. Higher smoothing reduces zipper noise on filters and levels.");

            add ("layout.controlbindings", "Control Bindings",
                 "This strip helps wire Layout controls to sound parameters. Select a knob or slider on the canvas, "
                 "then use Open Node Editor for deep routing or Add Knob to Canvas for quick placement. "
                 "Open the Graph tab for the full DSP node editor.");
            add ("layout.opennodeeditor", "Open Node Editor",
                 "Opens the focused node graph for the selected control so you can route modulation, FX, and sources.");
            add ("layout.addknob", "Add Knob To Canvas",
                 "Places a new knob on the Layout canvas and selects it for binding.");

            add ("graph.templates", "Graph Templates",
                 "Load a starter DSP graph (Init Synth, Pluck Arp, Drum Bus, and more). Your current graph is replaced "
                 "when you pick a template.");
            add ("graph.search", "Search Nodes",
                 "Filter visible nodes by name, section, or parameter. Useful in large graphs.");
            add ("graph.tidy", "Tidy Graph",
                 "Auto-arrange nodes into Source, Shape, Motion, FX, and Output columns.");
            add ("graph.fit", "Fit Graph",
                 "Zoom and scroll so the entire graph fits in the viewport.");

            add ("sampler.import", "Import Samples",
                 "Add WAV, AIFF, or FLAC files. Drop onto pads for one-shot kits or onto the keyboard map for pitched zones.");
            add ("sampler.easy", "Easy Sample Builder",
                 "Guided mapping workflow: import, choose keyboard/pad/slice mode, build the map, then test in the Player.");
            add ("sampler.advanced", "Advanced Sample Mapper",
                 "Full editing: trim, loops, velocity layers, round robin, MIDI-on-zone, and health checks.");
            add ("sampler.stackpads", "Stack Pads",
                 "When off, each pad holds one sample. When on, repeated drops on the same pad create round-robin layers.");
            add ("sampler.autotrim", "Auto Trim",
                 "Strip silence from selected zones (or all zones if nothing is selected) without destroying the source file.");
            add ("sampler.playmode", "Play Mapper",
                 "Live-play the map from the mapper keyboard or pad grid while editing.");
            add ("sampler.testplayer", "Test Player",
                 "Jump to runtime Test view with the current sample map loaded.");

            add ("toolbar.preview", "Preview",
                 "Hear the current project in Studio without exporting. Uses the same engine path as Test.");
            add ("toolbar.export", "Export Pack",
                 "Export a playable .patchcraft pack, VST3 plugin, or send content to an expansion pack.");
            add ("toolbar.settings", "Settings",
                 "Audio/MIDI devices, Plugin.club publishing, extensions (.pcexp), and UI preferences.");

            add ("tab.sound", "Sound Tab",
                 "Build the instrument voice: sample maps, keyzones, velocity layers, and sample tools.");
            add ("tab.graph", "Graph Tab",
                 "Author the DSP signal path — oscillators, filters, arps, FX, and output — with the node editor.");
            add ("tab.perform", "Perform Tab",
                 "Pattern, step, circle, and drum performance tools that drive the instrument at runtime.");
            add ("tab.layout", "Layout Tab",
                 "Design the Player UI: place controls, wire parameters, and preview the exported interface.");
            add ("tab.ship", "Ship Tab",
                 "Launch Center: export checks, Plugin.club publish, installers, and release readiness.");

            add ("help.tutorialmode", "Tutorial Mode",
                 "When enabled, hover any control for a few moments to open a guided explanation. "
                 "Works across Layout, Graph, Sound, Perform, and property panels.");

            return entries;
        }

        const std::map<juce::String, TutorialHelpEntry>& registry()
        {
            static const auto entries = buildRegistry();
            return entries;
        }
    }

    const TutorialHelpEntry* TutorialHelpRegistry::lookup (const juce::String& key)
    {
        const auto normalised = normaliseKey (key);
        if (normalised.isEmpty())
            return nullptr;

        const auto& entries = registry();
        if (auto it = entries.find (normalised); it != entries.end())
            return &it->second;

        return nullptr;
    }

    const TutorialHelpEntry* TutorialHelpRegistry::lookupForLabel (const juce::String& labelText)
    {
        const auto trimmed = labelText.trim();
        if (trimmed.isEmpty())
            return nullptr;

        if (auto* entry = lookup ("inspector." + normaliseKey (trimmed)))
            return entry;

        return lookup ("label:" + normaliseKey (trimmed));
    }

    const TutorialHelpEntry* TutorialHelpRegistry::lookupForButton (const juce::String& buttonText)
    {
        const auto trimmed = buttonText.trim();
        if (trimmed.isEmpty())
            return nullptr;

        return lookup ("button:" + normaliseKey (trimmed));
    }
}
