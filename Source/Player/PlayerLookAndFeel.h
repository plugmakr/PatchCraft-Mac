#pragma once

#include "PatchCraftLookAndFeel.h"

namespace patchcraft
{
    /**
        Player runtime visual style. Inherits Studio palette but tweaked for
        the in-DAW plugin window.
    */
    class PlayerLookAndFeel : public PatchCraftLookAndFeel
    {
    public:
        PlayerLookAndFeel() = default;
    };

} // namespace patchcraft
