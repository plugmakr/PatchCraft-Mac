#pragma once

#include "PatchCraftPackFormat.h"

namespace patchcraft
{
    class PatchCraftProject;

    struct PackWriteOptions
    {
        /** When false, presets/expansions with stale patch ids still export (runtime preview). */
        bool strictReferenceValidation = true;
        /** When true, asset/sample files are not copied to the target directory. Paths are resolved to their absolute original location. Used for fast in-editor previews. */
        bool exportForPreview = false;
    };

    class PatchCraftPackWriter
    {
    public:
        /**
            Writes a runtime pack folder containing
                manifest.json, layout.json, mappings.json,
                parameters.json, presets.json, assets/, samples/
            from the in-memory project data.

            sourceProjectFolder is used to resolve relative asset/sample paths.
            If the project has not been saved yet, sourceProjectFolder may be empty
            and the writer will copy from absolute paths in the sample map.
        */
        bool write (const PatchCraftProject& project,
                    const juce::File& packFolder,
                    juce::String& error,
                    PackWriteOptions options = {});
    };

} // namespace patchcraft
