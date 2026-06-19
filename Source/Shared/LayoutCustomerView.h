#pragma once

#include "PatchCraftTypes.h"

namespace patchcraft
{
    /** Layout chrome used during authoring but hidden in Player / Brand Lab preview. */
    inline bool isAuthoringOnlyLayoutElement (const LayoutElement& e)
    {
        if (e.id == "presets"
            || e.id == "demo_badge"
            || e.id == "demo_badge_panel"
            || e.id == "header_panel"
            || e.id == "sequencer_panel"
            || e.id == "macro_panel")
            return true;

        if (e.type == ElementType::Dropdown && e.id == "presets")
            return true;

        if (e.type == ElementType::Panel && e.parameterId.isEmpty())
        {
            if (e.id.endsWith ("_panel") || e.label.equalsIgnoreCase ("DEMO"))
                return true;
        }

        if (e.type == ElementType::Shape)
        {
            if (e.id.endsWith ("_panel") || e.id.contains ("demo_badge"))
                return true;
        }

        return false;
    }
}
