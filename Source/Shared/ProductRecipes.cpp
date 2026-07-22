#include "ProductRecipes.h"

#include "PatchCraftProject.h"
#include "SoundStack.h"

namespace patchcraft
{
    namespace
    {
        ProductTemplateSpec makeSpec (const juce::String& id,
                                    const juce::String& name,
                                    const juce::String& layout,
                                    ProductKind kind,
                                    bool chop = false)
        {
            return { id, name, layout, kind, chop };
        }
    }

    ProductRecipeInfo recipeInfoFor (ProductKind kind)
    {
        const auto spec = templateSpecFor (defaultTemplateForKind (kind));
        ProductRecipeInfo info;
        info.id = spec.templateId;
        info.displayName = kind == ProductKind::LoopChopInstrument ? "Loop / Chop Instrument"
                         : kind == ProductKind::SynthInstrument ? "Synth Instrument"
                         : kind == ProductKind::HybridInstrument ? "Hybrid Instrument"
                         : kind == ProductKind::FXPlugin ? "FX Plugin"
                         : kind == ProductKind::DrumMachine ? "Drum Machine"
                         : "Sample Instrument";
        info.subtitle = kind == ProductKind::LoopChopInstrument ? "Drum loops, sliced loops, glitch tools, beat engines."
                      : kind == ProductKind::SynthInstrument ? "Simple playable synth products."
                      : kind == ProductKind::HybridInstrument ? "Layer samples with synth oscillators, shared filters, macros, and FX."
                      : kind == ProductKind::FXPlugin ? "EQ, delay, reverb, lo-fi, mastering, vocal FX, and more."
                      : kind == ProductKind::DrumMachine ? "Pad-based kits and sequenced beat tools."
                      : "One-shots, keys, pads, guitars, cinematic tools.";
        info.engineId = kind == ProductKind::SynthInstrument ? "synth"
                      : kind == ProductKind::HybridInstrument ? "sample"
                      : kind == ProductKind::FXPlugin ? "fx"
                      : kind == ProductKind::DrumMachine ? "drum"
                      : "sample";
        info.category = info.displayName;
        info.layoutModuleId = spec.layoutModuleId;
        info.showChopStep = spec.showChopStep;
        return info;
    }

    juce::String defaultTemplateForKind (ProductKind kind)
    {
        switch (kind)
        {
            case ProductKind::LoopChopInstrument: return "loop_chopper";
            case ProductKind::SynthInstrument:    return "cinematic_pad_synth";
            case ProductKind::HybridInstrument:   return "hybrid_synth_sample";
            case ProductKind::FXPlugin:           return "simple_eq";
            case ProductKind::DrumMachine:        return "pad_kit_16";
            default:                              return "one_shot_pack";
        }
    }

    ProductTemplateSpec templateSpecFor (const juce::String& templateId)
    {
        static const ProductTemplateSpec specs[] = {
            makeSpec ("one_shot_pack", "One-Shot Pack", "startersamplerinstrument", ProductKind::SampleInstrument),
            makeSpec ("cinematic_pads", "Cinematic Pads", "starteasysamplerworkstation", ProductKind::SampleInstrument),
            makeSpec ("guitar_textures", "Guitar Textures", "startersamplerinstrument", ProductKind::SampleInstrument),
            makeSpec ("vocal_chops", "Vocal Chops", "startervocalchopinstrument", ProductKind::SampleInstrument),
            makeSpec ("ambient_keys", "Ambient Keys", "startersamplerinstrument", ProductKind::SampleInstrument),
            makeSpec ("drum_hits", "Drum Hits", "starterdrummachine", ProductKind::SampleInstrument),
            makeSpec ("loop_chopper", "Loop Chopper", "starterchoplab", ProductKind::LoopChopInstrument, true),
            makeSpec ("analog_bass", "Analog Bass", "startersynthplugin", ProductKind::SynthInstrument),
            makeSpec ("cinematic_pad_synth", "Cinematic Pad", "startersynthplugin", ProductKind::SynthInstrument),
            makeSpec ("pluck_synth", "Pluck Synth", "startersynthplugin", ProductKind::SynthInstrument),
            makeSpec ("arp_synth", "Arp Synth", "starterchordprogressionplugin", ProductKind::SynthInstrument),
            makeSpec ("drone_synth", "Drone Synth", "startersynthplugin", ProductKind::SynthInstrument),
            makeSpec ("lead_synth", "Lead Synth", "startersynthplugin", ProductKind::SynthInstrument),
            makeSpec ("hybrid_synth_sample", "Hybrid Synth + Sample", "starterhybridinstrument", ProductKind::HybridInstrument),
            makeSpec ("simple_eq", "Simple EQ", "starterdelayfx", ProductKind::FXPlugin),
            makeSpec ("delay_workstation", "Delay Workstation", "starterdelayfx", ProductKind::FXPlugin),
            makeSpec ("lofi_color", "Lo-Fi Color", "starterloopremixfx", ProductKind::FXPlugin),
            makeSpec ("reverb_space", "Reverb Space", "startervocalmasterfx", ProductKind::FXPlugin),
            makeSpec ("vocal_fx", "Vocal FX", "startervocalmasterfx", ProductKind::FXPlugin),
            makeSpec ("master_bus", "Master Bus", "startervocalmasterfx", ProductKind::FXPlugin),
            makeSpec ("pad_kit_16", "16 Pad Kit", "startermpcpads", ProductKind::DrumMachine),
            makeSpec ("pad_kit_32", "32 Pad Kit", "starterdrummachine", ProductKind::DrumMachine),
            makeSpec ("step_drum_machine", "Step Drum Machine", "starterdrummachine", ProductKind::DrumMachine),
            makeSpec ("hybrid_drum_rack", "Hybrid Drum Rack", "startermpcpads", ProductKind::DrumMachine)
        };

        for (const auto& spec : specs)
            if (spec.templateId == templateId)
                return spec;

        return templateSpecFor (defaultTemplateForKind (ProductKind::SampleInstrument));
    }

    juce::StringArray allTemplateIds()
    {
        juce::StringArray ids;
        for (const auto& id : {
                 "one_shot_pack", "cinematic_pads", "guitar_textures", "vocal_chops", "ambient_keys", "drum_hits",
                 "analog_bass", "cinematic_pad_synth", "pluck_synth", "arp_synth", "drone_synth", "lead_synth",
                 "hybrid_synth_sample",
                 "simple_eq", "delay_workstation", "lofi_color", "reverb_space", "vocal_fx", "master_bus",
                 "pad_kit_16", "pad_kit_32", "loop_chopper", "step_drum_machine", "hybrid_drum_rack" })
            ids.add (id);
        return ids;
    }

    void ensureSimpleStackForEngine (PatchCraftProject& project, const juce::String& engineId)
    {
        auto& graph = project.getDspGraph();
        if (! graph.userConfigured)
            graph.resetForEngine (engineId);
    }

    void applyTemplateLiveDefaults (PatchCraftProject& project, const juce::String& templateId)
    {
        auto& lv = project.getLiveValues();
        lv.setValue ("outputLimiter", 1.0f);
        lv.setValue ("outputCeilingDb", -0.8f);

        if (templateId == "loop_chopper")
        {
            lv.setValue ("sampleSliceCount", 32.0f);
            lv.setValue ("bpmSync", 1.0f);
            lv.setValue ("tapeMix", 0.12f);
            lv.setValue ("multiTapMix", 0.14f);
            lv.setValue ("volume", 0.84f);
        }
        else if (templateId == "cinematic_pads" || templateId == "cinematic_pad_synth")
        {
            lv.setValue ("filterCutoff", 4800.0f);
            lv.setValue ("reverbMix", 0.32f);
            lv.setValue ("delayMix", 0.18f);
            lv.setValue ("attack", 0.35f);
            lv.setValue ("release", 0.72f);
            lv.setValue ("volume", 0.82f);
        }
        else if (templateId == "one_shot_pack" || templateId == "drum_hits")
        {
            lv.setValue ("filterCutoff", 12000.0f);
            lv.setValue ("reverbMix", 0.10f);
            lv.setValue ("delayMix", 0.06f);
            lv.setValue ("volume", 0.88f);
        }
        else if (templateId == "vocal_chops")
        {
            lv.setValue ("sampleSliceCount", 12.0f);
            lv.setValue ("bpmSync", 1.0f);
            lv.setValue ("vocalMix", 0.10f);
            lv.setValue ("volume", 0.82f);
        }
        else if (templateId.contains ("drum") || templateId.contains ("pad_kit") || templateId == "step_drum_machine")
        {
            lv.setValue ("volume", 0.86f);
            lv.setValue ("lofiMix", 0.08f);
            lv.setValue ("tapeMix", 0.10f);
        }
        else if (templateId == "hybrid_synth_sample")
        {
            lv.setValue ("sampleLength", 1.0f);
            lv.setValue ("samplePitch", 0.0f);
            lv.setValue ("oscBlend", 0.36f);
            lv.setValue ("wtEnabled", 1.0f);
            lv.setValue ("wtLevel", 0.34f);
            lv.setValue ("filterCutoff", 5200.0f);
            lv.setValue ("filterResonance", 0.16f);
            lv.setValue ("delayMix", 0.14f);
            lv.setValue ("reverbMix", 0.24f);
            lv.setValue ("volume", 0.82f);
        }
        else if (templateId.contains ("synth") || templateId == "analog_bass" || templateId == "lead_synth")
        {
            lv.setValue ("filterCutoff", 6200.0f);
            lv.setValue ("filterResonance", 0.18f);
            lv.setValue ("reverbMix", 0.22f);
            lv.setValue ("volume", 0.80f);
        }
        else if (templateId.contains ("eq") || templateId.contains ("fx") || templateId.contains ("delay")
              || templateId.contains ("reverb") || templateId.contains ("lofi") || templateId == "master_bus")
        {
            lv.setValue ("volume", 0.90f);
            lv.setValue ("delayMix", templateId.contains ("delay") ? 0.35f : 0.12f);
            lv.setValue ("reverbMix", templateId.contains ("reverb") ? 0.40f : 0.10f);
        }
    }

    void applyProductRecipe (PatchCraftProject& project, ProductKind kind)
    {
        applyProductTemplate (project, defaultTemplateForKind (kind));
    }

    void applyProductTemplate (PatchCraftProject& project, const juce::String& templateId)
    {
        const auto spec = templateSpecFor (templateId);
        const juce::String engineId = spec.kind == ProductKind::SynthInstrument ? "synth"
                                    : spec.kind == ProductKind::HybridInstrument ? "sample"
                                    : spec.kind == ProductKind::FXPlugin ? "fx"
                                    : spec.kind == ProductKind::DrumMachine ? "drum"
                                    : "sample";

        project.resetCanvasToBlank();
        project.setEngineType (engineId);
        project.getSampleMap().clear();
        ensureSimpleStackForEngine (project, engineId);

        auto& manifest = project.getManifest();
        manifest.category = spec.displayName;
        manifest.quickBuildMode = true;
        manifest.productRecipeId = spec.templateId;
        manifest.productKindLabel = spec.displayName;
        manifest.instrumentName = "Untitled " + spec.displayName;
        manifest.description = recipeInfoFor (spec.kind).subtitle;
        manifest.tags.addIfNotAlreadyThere ("QuickBuild");
        manifest.tags.addIfNotAlreadyThere (spec.templateId);

        applyTemplateLiveDefaults (project, templateId);
        project.notifyChanged();
        project.markClean();
    }

} // namespace patchcraft
