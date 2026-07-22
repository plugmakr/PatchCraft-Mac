#include "PScriptShare.h"

namespace patchcraft
{
    namespace PScriptShare
    {
        bool isPscriptFile (const juce::File& file)
        {
            if (! file.existsAsFile())
                return false;

            const auto ext = file.getFileExtension().toLowerCase();
            return ext == ".pscript" || ext == ".psc" || ext == ".txt";
        }

        static juce::String rewriteKnobWhenLine (juce::String line,
                                                 const juce::String& parameterId)
        {
            auto trimmed = line.trim();
            if (! trimmed.startsWithIgnoreCase ("when knob"))
                return line;

            juce::String suffix = " moves:";
            const auto lower = trimmed.toLowerCase();
            const int movesIdx = lower.indexOf (" moves");
            if (movesIdx >= 0)
            {
                suffix = trimmed.substring (movesIdx);
                if (! suffix.endsWithChar (':'))
                    suffix << ":";
            }

            return "when knob \"" + parameterId + "\"" + suffix;
        }

        juce::String bindToKnobParameter (juce::String script,
                                          const juce::String& parameterId,
                                          const juce::String& knobLabel)
        {
            if (parameterId.isEmpty())
                return script;

            juce::StringArray lines;
            lines.addLines (script.replace ("\r", "\n"));

            juce::String header = "# Bound to knob parameter " + parameterId;
            if (knobLabel.isNotEmpty())
                header << " (\"" << knobLabel << "\")";

            juce::String out = header + "\n";
            for (auto& line : lines)
            {
                if (line.trimStart().startsWithChar ('#'))
                {
                    out << line << "\n";
                    continue;
                }

                out << rewriteKnobWhenLine (line, parameterId) << "\n";
            }

            return out.trimEnd() + "\n";
        }

        juce::String mergeSources (const juce::StringArray& sections)
        {
            juce::String merged;
            for (const auto& section : sections)
            {
                const auto trimmed = section.trim();
                if (trimmed.isEmpty())
                    continue;

                if (merged.isNotEmpty())
                    merged << "\n\n";
                merged << trimmed;
            }
            return merged;
        }
    }
}
