#pragma once

#include "PatchCraftTypes.h"
#include <cmath>

namespace patchcraft
{
    /** Shared layout + hit-testing for Arp Lane (circle, orbit, linear). */
    struct ArpLaneUi
    {
        struct Metrics
        {
            juce::Rectangle<int> bounds;
            juce::Rectangle<int> header;
            juce::Rectangle<int> stepsMinusBtn;
            juce::Rectangle<int> stepsPlusBtn;
            juce::Rectangle<int> stepsLabel;
            juce::Rectangle<int> playBtn;
            juce::Rectangle<int> content;
            juce::Point<float> centre {};
            float ringSize = 0.0f;
            float ringRadius = 0.0f;
            int steps = 16;
            int maxDrawSteps = 16;
            bool linear = false;
            bool orbitMultiRing = false;
            bool valid = false;
        };

        static bool isLinearMode (const LayoutElement& e)
        {
            return e.arpLaneMode.equalsIgnoreCase ("linear")
                || e.arpLaneMode.equalsIgnoreCase ("step")
                || e.arpLaneMode.equalsIgnoreCase ("steps");
        }

        static bool isOrbitMultiRing (const LayoutElement& e)
        {
            return e.arpLaneMode.equalsIgnoreCase ("multiRing")
                || e.arpLaneMode.equalsIgnoreCase ("orbit")
                || e.arpLaneMode.equalsIgnoreCase ("orbitMulti");
        }

        static float readLaneValue (const DspBlock* block, int lane, const juce::String& key, float fallback)
        {
            if (block == nullptr)
                return fallback;

            const auto bankPrefix = "mpBank" + juce::String (juce::jlimit (0, 15, lane) + 1) + "_";
            if (lane > 0)
                if (auto it = block->values.find (bankPrefix + key); it != block->values.end())
                    return it->second;

            if (auto it = block->values.find (key); it != block->values.end())
                return it->second;

            if (auto it = block->values.find (bankPrefix + key); it != block->values.end())
                return it->second;

            return fallback;
        }

        static Metrics layout (juce::Rectangle<int> r,
                               const LayoutElement& element,
                               const DspBlock* block,
                               bool includeTransportButtons = false)
        {
            Metrics m;
            m.bounds = r;
            m.linear = isLinearMode (element);
            m.orbitMultiRing = isOrbitMultiRing (element);

            auto area = r.reduced (10);
            m.header = area.removeFromTop (26);
            m.stepsMinusBtn = m.header.removeFromLeft (22).reduced (1);
            m.stepsPlusBtn = m.header.removeFromLeft (22).reduced (1);
            m.stepsLabel = m.header.removeFromLeft (juce::jmin (52, m.header.getWidth())).reduced (2, 4);

            if (includeTransportButtons)
            {
                m.playBtn = m.header.removeFromRight (58).reduced (2);
                m.header.removeFromRight (4);
            }

            m.content = area;
            m.content.removeFromTop (4);

            const int lane = juce::jlimit (0, 15, element.arpLaneIndex);
            m.steps = block != nullptr
                ? juce::jlimit (1, 128, juce::roundToInt (readLaneValue (block, lane, "arpSteps", (float) element.arpLaneSteps)))
                : juce::jlimit (1, 128, element.arpLaneSteps);
            m.maxDrawSteps = juce::jmin (m.steps, m.linear ? m.steps : 64);

            if (m.content.getWidth() >= 24 && m.content.getHeight() >= 24)
            {
                m.valid = true;
                if (m.linear)
                {
                    m.ringSize = (float) m.content.getWidth();
                    m.ringRadius = 0.0f;
                }
                else
                {
                    m.ringSize = (float) juce::jmin (m.content.getWidth(), m.content.getHeight() - (m.orbitMultiRing ? 34 : 10));
                    m.ringRadius = m.orbitMultiRing ? m.ringSize * 0.42f : m.ringSize * 0.40f;
                    m.centre = { (float) m.content.getCentreX(), (float) m.content.getY() + m.ringSize * 0.52f };
                }
            }

            return m;
        }

        static int stepFromAngle (float angle01, int maxDrawSteps)
        {
            if (maxDrawSteps <= 0)
                return 0;

            angle01 = std::fmod (angle01, 1.0f);
            if (angle01 < 0.0f)
                angle01 += 1.0f;

            // Steps are drawn at the centre of each angular slice.
            const float scaled = angle01 * (float) maxDrawSteps - 0.5f;
            return juce::jlimit (0, maxDrawSteps - 1, (int) std::floor (scaled + 1.0f));
        }

        static float angle01FromPoint (juce::Point<float> centre, juce::Point<float> pos)
        {
            const auto delta = pos - centre;
            float angle01 = (std::atan2 (delta.y, delta.x) + juce::MathConstants<float>::halfPi)
                / juce::MathConstants<float>::twoPi;
            angle01 = std::fmod (angle01, 1.0f);
            if (angle01 < 0.0f)
                angle01 += 1.0f;
            return angle01;
        }

        static int laneFromOrbitRadius (float distance, float ringSize)
        {
            constexpr int laneCount = 5;
            const float multiRadius = ringSize * 0.42f;
            const float innerRadius = multiRadius * 0.25f;
            const float outerRadius = multiRadius * 0.94f;
            if (distance < innerRadius - 6.0f || distance > outerRadius + 10.0f)
                return -1;

            const float lanePos = juce::jmap (distance, innerRadius, outerRadius, 0.0f, (float) laneCount);
            return juce::jlimit (0, laneCount - 1, (int) std::floor (lanePos + 0.5f));
        }

        static bool hitTestStep (const Metrics& m,
                               const LayoutElement& element,
                               juce::Point<int> pos,
                               int& lane,
                               int& step)
        {
            if (! m.valid)
                return false;

            lane = juce::jlimit (0, 15, element.arpLaneIndex);

            if (m.linear)
            {
                const int gridTop = m.content.getY() + 8;
                const int gridBottom = m.content.getBottom() - 8;
                if (pos.y < gridTop || pos.y > gridBottom)
                    return false;

                const float cellW = (float) m.content.getWidth() / (float) m.maxDrawSteps;
                step = juce::jlimit (0, m.maxDrawSteps - 1,
                                     (int) ((pos.x - m.content.getX()) / cellW));
                return true;
            }

            const auto delta = pos.toFloat() - m.centre;
            const float distance = delta.getDistanceFromOrigin();

            if (m.orbitMultiRing)
            {
                const int orbitLane = laneFromOrbitRadius (distance, m.ringSize);
                if (orbitLane < 0)
                    return false;
                lane = orbitLane;
            }
            else
            {
                const float radius = m.ringRadius;
                if (distance < radius * 0.12f || distance > radius * 1.18f)
                    return false;
            }

            step = stepFromAngle (angle01FromPoint (m.centre, pos.toFloat()), m.maxDrawSteps);
            return true;
        }

        static float velocityFromVerticalDrag (juce::Rectangle<int> contentArea, int y)
        {
            const int top = contentArea.getY() + 8;
            const int bottom = contentArea.getBottom() - 8;
            return juce::jlimit (0.05f, 1.0f, juce::jmap ((float) y, (float) bottom, (float) top, 0.05f, 1.0f));
        }

        static float storedStepVelocity (const DspBlock* block, int lane, int step, float fallback = 0.72f)
        {
            return juce::jlimit (0.05f, 1.0f,
                readLaneValue (block, lane, "mpVelocity" + juce::String (step), fallback));
        }

        static bool storedStepActive (const DspBlock* block, int lane, int step, bool fallback = false)
        {
            return readLaneValue (block, lane, "mpStep" + juce::String (step) + "On", fallback ? 1.0f : 0.0f) >= 0.5f;
        }

        static juce::Rectangle<float> linearStepRect (const Metrics& m, int step)
        {
            const float cellW = (float) m.content.getWidth() / (float) juce::jmax (1, m.maxDrawSteps);
            return { (float) m.content.getX() + (float) step * cellW + 1.0f,
                     (float) m.content.getY() + 8.0f,
                     juce::jmax (3.0f, cellW - 2.0f),
                     (float) m.content.getHeight() - 16.0f };
        }
    };

    /** Linear step-lane widget (HISE-style row). Uses same mpStep* keys as Arp Lane. */
    struct SeqLaneUi
    {
        struct Metrics
        {
            juce::Rectangle<int> bounds;
            juce::Rectangle<int> header;
            juce::Rectangle<int> stepsMinusBtn;
            juce::Rectangle<int> stepsPlusBtn;
            juce::Rectangle<int> grid;
            int steps = 16;
            bool valid = false;
        };

        static Metrics layout (juce::Rectangle<int> r, const LayoutElement& element, const DspBlock* block = nullptr)
        {
            Metrics m;
            m.bounds = r;
            auto area = r.reduced (8);
            m.header = area.removeFromTop (22);
            m.stepsMinusBtn = m.header.removeFromRight (22).reduced (1);
            m.stepsPlusBtn = m.header.removeFromRight (22).reduced (1);
            m.grid = area;
            const int lane = juce::jlimit (0, 15, element.seqLaneIndex);
            m.steps = block != nullptr
                ? juce::jlimit (1, 64, juce::roundToInt (ArpLaneUi::readLaneValue (block, lane, "arpSteps", (float) element.seqLaneSteps)))
                : juce::jlimit (1, 64, element.seqLaneSteps);
            m.valid = m.grid.getWidth() > 20 && m.grid.getHeight() > 12;
            return m;
        }

        static int stepAtX (const Metrics& m, int x)
        {
            const float cellW = (float) m.grid.getWidth() / (float) m.steps;
            return juce::jlimit (0, m.steps - 1, (int) ((x - m.grid.getX()) / cellW));
        }

        static float valueFromY (const Metrics& m, int y, const juce::String& laneType)
        {
            const float norm = juce::jlimit (0.0f, 1.0f,
                1.0f - ((float) y - (float) m.grid.getY()) / (float) juce::jmax (1, m.grid.getHeight()));
            if (laneType == "pitch")
                return juce::jmap (norm, 0.0f, 1.0f, -24.0f, 24.0f);
            return norm;
        }

        static float readStepValue (const DspBlock* block, int laneIndex, int step,
                                  const juce::String& laneType, float fallback)
        {
            if (block == nullptr)
                return fallback;

            const auto prefix = "seqLane" + juce::String (laneIndex);
            if (laneType == "gate")
                return ArpLaneUi::readLaneValue (block, laneIndex, "mpStep" + juce::String (step) + "On", fallback >= 0.5f ? 1.0f : 0.0f) >= 0.5f ? 1.0f : 0.0f;

            if (laneType == "pitch")
                return ArpLaneUi::readLaneValue (block, laneIndex, "arpNote" + juce::String (step), fallback);

            if (laneType == "chance")
                return ArpLaneUi::readLaneValue (block, laneIndex, "mpStepProb" + juce::String (step), fallback);

            if (auto it = block->values.find (prefix + "Step" + juce::String (step)); it != block->values.end())
                return it->second;

            return ArpLaneUi::readLaneValue (block, laneIndex, "mpVelocity" + juce::String (step), fallback);
        }
    };
}
