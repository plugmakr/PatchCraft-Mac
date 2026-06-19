#include "ControlNodeEditor.h"

#include "ControlNodeAuthoring.h"
#include "PatchCraftLookAndFeel.h"
#include "StudioMainComponent.h"

#include <map>

namespace patchcraft
{
    namespace
    {
        static juce::Colour sectionColour (const juce::String& section)
        {
            if (section == "source") return juce::Colour (0xff35b8d4);
            if (section == "filter" || section == "amp") return juce::Colour (0xff79c267);
            if (section == "mod" || section == "motion") return juce::Colour (0xffd889e8);
            if (section == "fx") return juce::Colour (0xffffb84d);
            if (section == "out") return juce::Colour (0xfff06b78);
            return PatchCraftLookAndFeel::accent();
        }

        static bool isControlElement (ElementType type)
        {
            return type == ElementType::Knob || type == ElementType::Slider
                || type == ElementType::Button || type == ElementType::Toggle
                || type == ElementType::Dropdown || type == ElementType::ValueDisplay
                || type == ElementType::MacroControl || type == ElementType::SampleDropZone;
        }

        static bool isParameterEnabled (const PatchCraftProject& project, const juce::String& parameterId)
        {
            if (const auto* def = project.getParameters().find (parameterId))
            {
                if (def->enabledBy.isEmpty())
                    return true;
                if (const auto* parent = project.getParameters().find (def->enabledBy))
                {
                    const auto val = project.getLiveValues().getValue (def->enabledBy, parent->defaultValue);
                    return val >= 0.5f;
                }
            }
            return true;
        }

        static bool nodeMatchesSearch (const DspBlock& block, const ControlNodeDefinition* definition,
                                       const juce::String& query)
        {
            if (query.trim().isEmpty())
                return true;

            const auto q = query.trim().toLowerCase();
            if (block.name.toLowerCase().contains (q))
                return true;
            if (block.id.toLowerCase().contains (q))
                return true;
            if (block.section.toLowerCase().contains (q))
                return true;
            if (definition != nullptr && definition->name.toLowerCase().contains (q))
                return true;
            return false;
        }

        static std::map<juce::String, juce::String> validationSeverityByBlock (const PatchCraftProject& project)
        {
            std::map<juce::String, juce::String> result;
            for (const auto& issue : ControlNodeAuthoring::validateNodeGraph (project))
            {
                if (issue.ownerId.isEmpty())
                    continue;

                auto& severity = result[issue.ownerId];
                if (issue.severity == "error" || severity != "error")
                    severity = issue.severity;
            }
            return result;
        }

        class ParameterRow final : public juce::Component
        {
        public:
            ParameterRow (StudioMainComponent& targetOwner, juce::String targetBlockId,
                          juce::String targetParameterId, juce::String boundParameterId,
                          std::function<void(const juce::String&)> bindCallback)
                : owner (targetOwner), blockId (std::move (targetBlockId)),
                  parameterId (std::move (targetParameterId)), onBind (std::move (bindCallback))
            {
                addAndMakeVisible (name);
                addAndMakeVisible (value);
                addAndMakeVisible (bind);

                name.setInterceptsMouseClicks (false, false);
                name.setFont (juce::FontOptions (10.5f).withStyle ("bold"));
                name.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
                value.setSliderStyle (juce::Slider::LinearHorizontal);
                value.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
                value.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff171b22));
                value.setColour (juce::Slider::trackColourId, PatchCraftLookAndFeel::accent().withAlpha (0.72f));
                value.setColour (juce::Slider::textBoxOutlineColourId, PatchCraftLookAndFeel::border());
                bind.setButtonText (parameterId == boundParameterId ? "LINKED" : "LINK UI");
                bind.getProperties().set ("smallButton", true);
                bind.setEnabled (parameterId != boundParameterId);
                bind.onClick = [this]
                {
                    if (onBind)
                        onBind (parameterId);
                };

                if (const auto* definition = owner.getProject().getParameters().find (parameterId))
                {
                    const auto interval = definition->step > 0.0f ? definition->step
                        : (definition->displayMode == "toggle" ? 1.0f : 0.0f);
                    value.setRange (definition->min, definition->max, interval);
                    value.setDoubleClickReturnValue (true, definition->defaultValue);
                    value.setValue (owner.getProject().getLiveValues().getValue (parameterId, definition->defaultValue),
                                    juce::dontSendNotification);
                    name.setText (definition->name, juce::dontSendNotification);
                }
                else
                {
                    name.setText (parameterId, juce::dontSendNotification);
                    value.setRange (0.0, 1.0, 0.0);
                }

                value.onValueChange = [this]
                {
                    if (! updating)
                        ControlNodeAuthoring::setNodeParameter (owner.getProject(), blockId, parameterId,
                                                                (float) value.getValue(), false);
                };
                value.onDragEnd = [this]
                {
                    ControlNodeAuthoring::setNodeParameter (owner.getProject(), blockId, parameterId,
                                                            (float) value.getValue(), true);
                };
            }

            void resized() override
            {
                auto r = getLocalBounds();
                auto header = r.removeFromTop (18);
                name.setBounds (header.removeFromLeft (140));
                bind.setBounds (header.removeFromRight (64).reduced (2, 2));
                value.setBounds (r.reduced (2, 2));
            }

            void paint (juce::Graphics&) override
            {
                const bool enabled = isParameterEnabled (owner.getProject(), parameterId);
                if (value.isEnabled() != enabled)
                {
                    value.setEnabled (enabled);
                    name.setAlpha (enabled ? 1.0f : 0.4f);
                    setAlpha (enabled ? 1.0f : 0.5f);
                }
            }

        private:
            StudioMainComponent& owner;
            juce::String blockId;
            juce::String parameterId;
            juce::Label name;
            juce::Slider value;
            juce::TextButton bind;
            std::function<void(const juce::String&)> onBind;
            bool updating = false;
        };

        class NodeCard final : public juce::Component
        {
        public:
            NodeCard (StudioMainComponent& targetOwner, juce::String targetBlockId,
                      const ControlNodeDefinition* targetDefinition, juce::String boundParameterId,
                      std::function<void(const juce::String&)> bindCallback,
                      std::function<void(const juce::String&, bool)> routeCallback,
                      std::function<void(const juce::String&, juce::Point<int>)> movedCallback,
                      std::function<void(const juce::String&, juce::Point<float>)> cableStartCallback,
                      std::function<void(juce::Point<float>)> cableDragCallback,
                      std::function<void(juce::Point<float>)> cableEndCallback,
                      std::function<void(const juce::String&)> selectCallback,
                      std::function<void()> layoutChangedCallback,
                      std::function<void(const juce::String&)> deleteCallback)
                : owner (targetOwner), blockId (std::move (targetBlockId)), definition (targetDefinition),
                  boundParameter (std::move (boundParameterId)), onRoute (std::move (routeCallback)),
                  onMoved (std::move (movedCallback)), onCableStart (std::move (cableStartCallback)),
                  onCableDrag (std::move (cableDragCallback)), onCableEnd (std::move (cableEndCallback)),
                  onSelect (std::move (selectCallback)), onLayoutChanged (std::move (layoutChangedCallback)),
                  onDelete (std::move (deleteCallback))
            {
                if (const auto* block = ControlNodeAuthoring::findBlock (owner.getProject(), blockId))
                {
                    if (auto it = block->metadata.find ("uiCollapsed"); it != block->metadata.end())
                        collapsed = it->second == "1";
                    if (auto it = block->metadata.find ("uiW"); it != block->metadata.end())
                        cardWidth = juce::jmax (200, it->second.getIntValue());
                    if (auto it = block->metadata.find ("uiH"); it != block->metadata.end())
                        cardHeight = juce::jmax (48, it->second.getIntValue());
                }

                addAndMakeVisible (enabled);
                enabled.setButtonText ("On");
                enabled.setTooltip ("Enable or bypass this sound node.");
                if (const auto* block = ControlNodeAuthoring::findBlock (owner.getProject(), blockId))
                    enabled.setToggleState (block->enabled, juce::dontSendNotification);
                enabled.onClick = [this]
                {
                    if (auto* block = ControlNodeAuthoring::findBlock (owner.getProject(), blockId))
                    {
                        block->enabled = enabled.getToggleState();
                        owner.getProject().markDirty();
                        owner.getProject().notifyChanged();
                    }
                };

                if (definition != nullptr)
                {
                    for (const auto& parameterId : definition->parameterIds)
                    {
                        if (owner.getProject().getParameters().find (parameterId) == nullptr)
                            continue;
                        auto row = std::make_unique<ParameterRow> (owner, blockId, parameterId, boundParameter, bindCallback);
                        addAndMakeVisible (*row);
                        rows.push_back (std::move (row));
                    }
                }

                const bool canRoute = definition != nullptr && definition->modulationSource
                    && boundParameter.isNotEmpty() && ! definition->parameterIds.contains (boundParameter);
                hasRouteButton = canRoute;
                if (canRoute)
                {
                    addAndMakeVisible (route);
                    const bool connected = ControlNodeAuthoring::hasModulationRoute (owner.getProject(), blockId, boundParameter);
                    route.setButtonText (connected ? "DISCONNECT TARGET" : "MODULATE PARAMETER");
                    route.setTooltip ("Connect this node's movement to the parameter currently assigned to the selected UI control.");
                    route.onClick = [this, connected]
                    {
                        if (onRoute)
                            onRoute (blockId, ! connected);
                    };
                }

                expandedHeight = juce::jmax (108, preferredExpandedHeight());
                if (cardHeight < expandedHeight && ! collapsed)
                    cardHeight = expandedHeight;
                applyCollapsedState();
            }

            int preferredHeight() const
            {
                return collapsed ? headerHeight() : juce::jmax (expandedHeight, preferredExpandedHeight());
            }

            int preferredExpandedHeight() const
            {
                return 48 + (int) rows.size() * 46 + (route.isVisible() ? 34 : 8);
            }

            int headerHeight() const { return 48; }

            void setSelected (bool shouldSelect)
            {
                if (selected != shouldSelect)
                {
                    selected = shouldSelect;
                    repaint();
                }
            }

            juce::Rectangle<int> resizeHandleBounds() const
            {
                constexpr int handleSize = 12;
                return { getWidth() - handleSize - 2, getHeight() - handleSize - 2, handleSize, handleSize };
            }

            juce::Point<float> inputPoint() const  { return { (float) getX(), (float) getBounds().getCentreY() - (definition && definition->section == "fx" ? 15.0f : 0.0f) }; }
            juce::Point<float> outputPoint() const { return { (float) getRight(), (float) getBounds().getCentreY() - (definition && definition->modulationSource ? 15.0f : 0.0f) }; }
            juce::String getBlockId() const        { return blockId; }
            const ControlNodeDefinition* getDefinition() const { return definition; }

            void setValidationSeverity (const juce::String& severity)
            {
                validationSeverity = severity;
                repaint();
            }

            void applySearchVisibility (const DspBlock& block, const juce::String& query)
            {
                setVisible (nodeMatchesSearch (block, definition, query));
            }

            void paint (juce::Graphics& g) override
            {
                const auto* block = ControlNodeAuthoring::findBlock (owner.getProject(), blockId);
                const auto section = block != nullptr ? block->section : juce::String();
                const auto colour = sectionColour (section);
                auto bounds = getLocalBounds().toFloat().reduced (1.0f);
                g.setColour (juce::Colour (0xff11151b));
                g.fillRoundedRectangle (bounds, 6.0f);
                g.setColour (colour.withAlpha (0.85f));
                g.drawRoundedRectangle (bounds, 6.0f, 1.5f);
                g.fillRoundedRectangle (bounds.removeFromTop (34.0f), 6.0f);

                g.setColour (juce::Colours::black.withAlpha (0.72f));
                g.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
                const auto titleText = block != nullptr && block->name.isNotEmpty() ? block->name
                    : definition != nullptr ? definition->name : blockId;
                g.drawText (titleText, 12, 2, getWidth() - 92, 30, juce::Justification::centredLeft);
                if (validationSeverity.isNotEmpty())
                {
                    g.setColour (validationSeverity == "error" ? juce::Colour (0xffff5a5a)
                                                               : juce::Colour (0xffffb84d));
                    g.fillEllipse ((float) getWidth() - 58.0f, 10.0f, 10.0f, 10.0f);
                }
                g.setFont (juce::FontOptions (9.0f));
                g.drawText (collapsed ? "+" : "-", getWidth() - 24, 8, 16, 16, juce::Justification::centred);

                if (selected)
                {
                    g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.35f));
                    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 2.5f);
                }

                if (! collapsed)
                {
                auto drawPort = [&] (bool isInput, float yOffset, const juce::String& text, juce::Colour c) {
                    g.setColour (c);
                    const float y = getHeight() * 0.5f + yOffset;
                    if (isInput) {
                        g.fillEllipse (-4.0f, y - 5.0f, 10.0f, 10.0f);
                        g.drawText (text, 8, y - 9.0f, 60, 18, juce::Justification::centredLeft);
                    } else {
                        g.fillEllipse ((float) getWidth() - 6.0f, y - 5.0f, 10.0f, 10.0f);
                        g.drawText (text, getWidth() - 68, y - 9.0f, 60, 18, juce::Justification::centredRight);
                    }
                };

                g.setFont (juce::FontOptions (8.0f).withStyle ("bold"));
                if (definition && definition->modulationSource) {
                    drawPort (true, -15.0f, "EVENT IN", colour);
                    drawPort (false, -15.0f, definition->id == "arp" ? "EVENT OUT" : "MOD OUT", colour);
                    drawPort (false, 15.0f, "AUX OUT", colour.darker());
                } else if (definition && (definition->section == "fx" || definition->section == "filter")) {
                    drawPort (true, -15.0f, "AUDIO IN", colour);
                    drawPort (true, 15.0f, "SIDECHAIN", colour.darker());
                    drawPort (false, -15.0f, "AUDIO OUT", colour);
                } else {
                    drawPort (true, 0.0f, "MAIN IN", colour);
                    drawPort (false, 0.0f, "MAIN OUT", colour);
                }
                }

                if (! collapsed)
                {
                    auto handle = resizeHandleBounds().toFloat();
                    g.setColour (PatchCraftLookAndFeel::textDim().withAlpha (0.85f));
                    g.drawLine (handle.getRight() - 2.0f, handle.getBottom() - 2.0f,
                                handle.getX() + 2.0f, handle.getBottom() - 2.0f, 1.5f);
                    g.drawLine (handle.getRight() - 2.0f, handle.getBottom() - 2.0f,
                                handle.getRight() - 2.0f, handle.getY() + 2.0f, 1.5f);
                }
            }

            void resized() override
            {
                auto r = getLocalBounds().reduced (8);
                auto header = r.removeFromTop (28);
                enabled.setBounds (header.removeFromRight (50));
                if (collapsed)
                    return;

                r.removeFromTop (8);
                for (auto& row : rows)
                    row->setBounds (r.removeFromTop (46));
                if (route.isVisible())
                    route.setBounds (r.removeFromTop (30).reduced (0, 3));
            }

            void mouseDoubleClick (const juce::MouseEvent& event) override
            {
                if (event.y > 36.0f)
                    return;
                setCollapsed (! collapsed);
            }

            void mouseDown (const juce::MouseEvent& event) override
            {
                if (onSelect)
                    onSelect (blockId);

                if (event.mods.isPopupMenu())
                {
                    juce::PopupMenu menu;
                    menu.addItem (10, collapsed ? "Expand Node" : "Collapse Node");
                    menu.addSeparator();
                    menu.addItem (4, "Add Connected Node...");
                    menu.addSeparator();
                    menu.addItem (1, "Disconnect All Inputs");
                    menu.addItem (2, "Disconnect All Outputs");
                    menu.addSeparator();
                    menu.addItem (3, "Delete Node...");
                    menu.showMenuAsync (juce::PopupMenu::Options(), [this, event] (int result)
                    {
                        if (result == 10)
                        {
                            setCollapsed (! collapsed);
                            return;
                        }
                        if (result == 4)
                        {
                            if (auto* editor = findParentComponentOfClass<ControlNodeEditor>())
                            {
                                const auto pos = getBounds().getBottomRight() + juce::Point<int> (12, -40);
                                editor->showAddNodeMenu (blockId, pos, event.getScreenPosition());
                            }
                            return;
                        }
                        if (result == 3)
                        {
                            if (onDelete)
                                onDelete (blockId);
                            return;
                        }
                        if (result == 1 || result == 2)
                        {
                            auto& edges = owner.getProject().getDspGraph().edges;
                            edges.erase (std::remove_if (edges.begin(), edges.end(), [&] (const auto& e) {
                                return result == 1 ? e.targetNodeId == blockId : e.sourceNodeId == blockId;
                            }), edges.end());
                            auto& mods = owner.getProject().getDspGraph().modulation;
                            mods.erase (std::remove_if (mods.begin(), mods.end(), [&] (const auto& m) {
                                return result == 1 ? m.targetId == blockId : m.sourceId == blockId;
                            }), mods.end());
                            owner.getProject().markDirty();
                            owner.getProject().notifyChanged();
                            if (auto* parent = getParentComponent()) parent->repaint();
                        }
                    });
                    return;
                }

                if (! collapsed && resizeHandleBounds().contains (event.getPosition()))
                {
                    resizing = true;
                    resizeStart = event.getScreenPosition();
                    resizeOriginalSize = { getWidth(), getHeight() };
                    return;
                }

                if (event.position.x >= (float) getWidth() - 18.0f
                    && std::abs (event.position.y - (float) getHeight() * 0.5f) <= 18.0f)
                {
                    cableDragging = true;
                    if (onCableStart)
                        onCableStart (blockId, eventPointInParent (event));
                    return;
                }
                if (event.position.y <= 36.0f)
                {
                    dragging = true;
                    dragStart = event.getScreenPosition();
                    originalPosition = getPosition();
                }
            }

            void mouseDrag (const juce::MouseEvent& event) override
            {
                if (resizing)
                {
                    const auto delta = event.getScreenPosition() - resizeStart;
                    const int newW = juce::jmax (200, resizeOriginalSize.x + delta.x);
                    const int newH = juce::jmax (collapsed ? headerHeight() : 108,
                                                 resizeOriginalSize.y + delta.y);
                    cardWidth = newW;
                    cardHeight = newH;
                    if (! collapsed)
                        expandedHeight = newH;
                    setSize (newW, newH);
                    if (auto* parent = getParentComponent())
                        parent->repaint();
                    return;
                }

                if (cableDragging)
                {
                    if (onCableDrag)
                        onCableDrag (eventPointInParent (event));
                    return;
                }
                if (! dragging)
                    return;
                const auto delta = event.getScreenPosition() - dragStart;
                setTopLeftPosition (juce::jmax (270, originalPosition.x + delta.x),
                                    juce::jmax (20, originalPosition.y + delta.y));
                if (auto* parent = getParentComponent())
                    parent->repaint();
            }

            void mouseUp (const juce::MouseEvent& event) override
            {
                if (resizing)
                {
                    resizing = false;
                    persistLayoutMetadata();
                    if (onLayoutChanged)
                        onLayoutChanged();
                    return;
                }
                if (cableDragging)
                {
                    cableDragging = false;
                    if (onCableEnd)
                        onCableEnd (eventPointInParent (event));
                    return;
                }
                if (dragging && onMoved)
                    onMoved (blockId, getPosition());
                dragging = false;
            }

        private:
            void applyCollapsedState()
            {
                for (auto& row : rows)
                    row->setVisible (! collapsed);
                route.setVisible (! collapsed && hasRouteButton);
                const int h = collapsed ? headerHeight() : juce::jmax (expandedHeight, preferredExpandedHeight());
                cardHeight = h;
                setSize (cardWidth, h);
            }

            void setCollapsed (bool shouldCollapse)
            {
                if (collapsed == shouldCollapse)
                    return;

                collapsed = shouldCollapse;
                if (! collapsed)
                    cardHeight = juce::jmax (cardHeight, expandedHeight);
                applyCollapsedState();
                persistLayoutMetadata();
                if (onLayoutChanged)
                    onLayoutChanged();
            }

            void persistLayoutMetadata()
            {
                if (auto* block = ControlNodeAuthoring::findBlock (owner.getProject(), blockId))
                {
                    block->metadata["uiCollapsed"] = collapsed ? "1" : "0";
                    block->metadata["uiW"] = juce::String (getWidth());
                    block->metadata["uiH"] = juce::String (getHeight());
                    owner.getProject().markDirty();
                }
            }

            juce::Point<float> eventPointInParent (const juce::MouseEvent& event) const
            {
                if (auto* parent = getParentComponent())
                    return parent->getLocalPoint (nullptr, event.getScreenPosition()).toFloat();
                return {};
            }

            StudioMainComponent& owner;
            juce::String blockId;
            const ControlNodeDefinition* definition = nullptr;
            juce::String boundParameter;
            juce::ToggleButton enabled;
            juce::TextButton route;
            bool hasRouteButton = false;
            std::vector<std::unique_ptr<ParameterRow>> rows;
            std::function<void(const juce::String&, bool)> onRoute;
            std::function<void(const juce::String&, juce::Point<int>)> onMoved;
            std::function<void(const juce::String&, juce::Point<float>)> onCableStart;
            std::function<void(juce::Point<float>)> onCableDrag;
            std::function<void(juce::Point<float>)> onCableEnd;
            std::function<void(const juce::String&)> onSelect;
            std::function<void()> onLayoutChanged;
            std::function<void(const juce::String&)> onDelete;
            juce::Point<int> dragStart;
            juce::Point<int> originalPosition;
            juce::Point<int> resizeStart;
            juce::Point<int> resizeOriginalSize;
            int cardWidth = 320;
            int cardHeight = 108;
            int expandedHeight = 108;
            bool collapsed = false;
            bool selected = false;
            bool dragging = false;
            bool cableDragging = false;
            bool resizing = false;
            juce::String validationSeverity;
        };

        class TargetCard final : public juce::Component
        {
        public:
            TargetCard (juce::String controlName, juce::String controlType, juce::String parameterName)
                : name (std::move (controlName)), type (std::move (controlType)), parameter (std::move (parameterName)) {}

            juce::Point<float> outputPoint() const { return { (float) getRight(), (float) getBounds().getCentreY() }; }

            void paint (juce::Graphics& g) override
            {
                auto r = getLocalBounds().toFloat().reduced (1.0f);
                g.setColour (juce::Colour (0xff151a21));
                g.fillRoundedRectangle (r, 6.0f);
                g.setColour (PatchCraftLookAndFeel::accent());
                g.drawRoundedRectangle (r, 6.0f, 2.0f);
                g.fillRoundedRectangle (r.removeFromTop (34.0f), 6.0f);
                g.setColour (juce::Colours::black.withAlpha (0.75f));
                g.setFont (juce::FontOptions (12.0f).withStyle ("bold"));
                g.drawText ("UI CONTROL", 12, 2, getWidth() - 24, 30, juce::Justification::centredLeft);
                g.setColour (PatchCraftLookAndFeel::text());
                g.setFont (juce::FontOptions (15.0f).withStyle ("bold"));
                g.drawText (name, 12, 48, getWidth() - 24, 24, juce::Justification::centredLeft);
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (11.0f);
                g.drawText (type, 12, 74, getWidth() - 24, 18, juce::Justification::centredLeft);
                g.setColour (PatchCraftLookAndFeel::accent().brighter (0.15f));
                g.drawText (parameter.isNotEmpty() ? "DRIVES  " + parameter : "NOT CONNECTED",
                            12, 104, getWidth() - 24, 22, juce::Justification::centredLeft);
                g.fillEllipse ((float) getWidth() - 6.0f, getHeight() * 0.5f - 5.0f, 10.0f, 10.0f);
            }

        private:
            juce::String name, type, parameter;
        };

        static juce::String elementTypeName (ElementType type)
        {
            if (type == ElementType::Knob) return "Knob";
            if (type == ElementType::Slider) return "Slider";
            if (type == ElementType::Button) return "Button";
            if (type == ElementType::Toggle) return "Toggle";
            if (type == ElementType::Dropdown) return "Dropdown";
            if (type == ElementType::ValueDisplay) return "Value Display";
            if (type == ElementType::MacroControl) return "Macro";
            if (type == ElementType::SampleDropZone) return "Drop Zone";
            return "Control";
        }
    }

    class ControlNodeEditor::NodeCanvas final : public juce::Component, private juce::Timer
    {
    public:
        NodeCanvas (ControlNodeEditor& targetEditor, StudioMainComponent& targetOwner, const juce::String& targetElementId)
            : editor (targetEditor), owner (targetOwner), elementId (targetElementId)
        {
            setWantsKeyboardFocus (true);
            startTimerHz (30);
            const auto* element = owner.getProject().getLayout().find (elementId);
            const auto parameterId = element != nullptr ? element->parameterId : juce::String();
            const auto* parameter = owner.getProject().getParameters().find (parameterId);
            const auto controlName = element != nullptr && element->label.isNotEmpty() ? element->label : elementId;
            target = std::make_unique<TargetCard> (controlName,
                                                  element != nullptr ? elementTypeName (element->type) : "Control",
                                                  parameter != nullptr ? parameter->name : parameterId);
            addAndMakeVisible (*target);
            target->setBounds (20, 48, 220, 140);

            std::map<juce::String, int> sectionCounts;
            int maxRight = 1400;
            int maxBottom = 700;
            const auto validationByBlock = validationSeverityByBlock (owner.getProject());
            for (const auto& block : owner.getProject().getDspGraph().blocks)
            {
                const auto* definition = ControlNodeAuthoring::definitionForBlock (block);
                if (definition == nullptr)
                    continue;
                auto card = std::make_unique<NodeCard> (owner, block.id, definition, parameterId,
                    [this] (const juce::String& selectedParameter)
                    {
                        juce::String message;
                        if (ControlNodeAuthoring::bindControl (owner.getProject(), elementId, selectedParameter, message))
                            owner.setSelectedElementId (elementId);
                        editor.setStatus (message);
                        editor.rebuild();
                    },
                    [this, parameterId] (const juce::String& blockId, bool enabled)
                    {
                        juce::String message;
                        ControlNodeAuthoring::setModulationRoute (owner.getProject(), blockId, parameterId,
                                                                 enabled, 0.25f, message);
                        editor.setStatus (message);
                        editor.rebuild();
                    },
                    [this] (const juce::String& blockId, juce::Point<int> position)
                    {
                        if (auto* moved = ControlNodeAuthoring::findBlock (owner.getProject(), blockId))
                        {
                            moved->metadata["uiX"] = juce::String (position.x);
                            moved->metadata["uiY"] = juce::String (position.y);
                            owner.getProject().markDirty();
                        }
                    },
                    [this] (const juce::String& sourceId, juce::Point<float> point)
                    {
                        cableSourceId = sourceId;
                        cablePoint = point;
                        cableDragging = true;
                        editor.setStatus ("Drag the cable to another node's IN port.");
                        repaint();
                    },
                    [this] (juce::Point<float> point)
                    {
                        cablePoint = point;
                        repaint();
                    },
                    [this] (juce::Point<float> point)
                    {
                        finishCable (point);
                    },
                    [this] (const juce::String& blockId)
                    {
                        selectNode (blockId);
                    },
                    [this]
                    {
                        updateCanvasBounds();
                    },
                    [this] (const juce::String& blockId)
                    {
                        juce::String message;
                        if (ControlNodeAuthoring::deleteNode (owner.getProject(), blockId, message))
                        {
                            editor.setStatus (message);
                            editor.rebuild();
                        }
                    });

                int cardW = 320;
                if (auto it = block.metadata.find ("uiW"); it != block.metadata.end())
                    cardW = juce::jmax (200, it->second.getIntValue());

                const int cardHeight = card->preferredHeight();
                int x = ControlNodeAuthoring::defaultColumnForSection (block.section);
                int y = 38;
                if (auto it = block.metadata.find ("uiX"); it != block.metadata.end())
                    x = juce::jmax (20, it->second.getIntValue());

                if (auto it = block.metadata.find ("uiY"); it != block.metadata.end())
                    y = juce::jmax (20, it->second.getIntValue());
                else
                    y += sectionCounts[block.section]++ * (cardHeight + 18);

                card->applySearchVisibility (block, editor.searchQuery);
                if (auto it = validationByBlock.find (block.id); it != validationByBlock.end())
                    card->setValidationSeverity (it->second);

                card->setBounds (x, y, cardW, cardHeight);
                maxRight = juce::jmax (maxRight, x + cardW + 40);
                maxBottom = juce::jmax (maxBottom, y + cardHeight + 60);
                addAndMakeVisible (*card);
                cards.push_back (std::move (card));
            }

            editor.baseCanvasWidth = maxRight;
            editor.baseCanvasHeight = maxBottom;
            setSize (maxRight, maxBottom);
        }

        void applySearchFilter (const juce::String& query)
        {
            for (auto& card : cards)
            {
                if (const auto* block = ControlNodeAuthoring::findBlock (owner.getProject(), card->getBlockId()))
                    card->applySearchVisibility (*block, query);
            }
            repaint();
        }

        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
        {
            if (e.mods.isCtrlDown() || e.mods.isCommandDown())
            {
                editor.zoomScale += wheel.deltaY * 1.5f;
                editor.applyCanvasZoom();
                return;
            }

            if (auto* vp = findParentComponentOfClass<juce::Viewport>())
                vp->mouseWheelMove (e, wheel);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour (0xff0c1015));

            const juce::StringArray categoryOrder { "Sources", "Shape", "Motion", "FX", "Output" };
            for (const auto& category : categoryOrder)
            {
                int columnX = 40;
                if (category == "Shape") columnX = 340;
                else if (category == "Motion") columnX = 640;
                else if (category == "FX") columnX = 940;
                else if (category == "Output") columnX = 1240;

                auto lane = juce::Rectangle<int> (columnX - 12, 8, 280, getHeight() - 16);
                g.setColour (juce::Colour (0xff121820).withAlpha (0.55f));
                g.fillRoundedRectangle (lane.toFloat(), 8.0f);
                g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.45f));
                g.drawRoundedRectangle (lane.toFloat().reduced (0.5f), 8.0f, 1.0f);
                g.setColour (PatchCraftLookAndFeel::textDim());
                g.setFont (juce::FontOptions (10.5f).withStyle ("bold"));
                g.drawText (category.toUpperCase(), lane.removeFromTop (22).reduced (8, 2),
                            juce::Justification::centredLeft);
            }

            g.setColour (juce::Colour (0xff1b222b));
            for (int x = 0; x < getWidth(); x += 24)
                g.drawVerticalLine (x, 0.0f, (float) getHeight());
            for (int y = 0; y < getHeight(); y += 24)
                g.drawHorizontalLine (y, 0.0f, (float) getWidth());

            auto cardForId = [this] (const juce::String& id) -> NodeCard*
            {
                for (auto& card : cards)
                    if (card->getBlockId() == id)
                        return card.get();
                return nullptr;
            };
            auto drawCable = [&g, this] (juce::String sourceId, juce::String targetId, juce::Point<float> from, juce::Point<float> to, juce::Colour colour, float width)
            {
                juce::Path path;
                path.startNewSubPath (from);
                const auto bend = juce::jmax (50.0f, std::abs (to.x - from.x) * 0.42f);
                path.cubicTo (from.x + bend, from.y, to.x - bend, to.y, to.x, to.y);
                
                DrawnCable dc { sourceId, targetId, path, width };
                drawnCables.push_back (dc);
                
                bool isSelected = selectedCable.has_value() && selectedCable->sourceId == sourceId && selectedCable->targetId == targetId;

                g.setColour (juce::Colours::black.withAlpha (0.7f));
                g.strokePath (path, juce::PathStrokeType (width + (isSelected ? 6.0f : 3.0f)));
                g.setColour (isSelected ? PatchCraftLookAndFeel::accent() : colour);
                g.strokePath (path, juce::PathStrokeType (width + (isSelected ? 1.0f : 0.0f)));
            };

            drawnCables.clear();

            for (const auto& edge : owner.getProject().getDspGraph().buildAudioEdges (owner.getProject().getEngineType()))
                if (auto* source = cardForId (edge.sourceNodeId))
                    if (auto* destination = cardForId (edge.targetNodeId))
                        drawCable (edge.sourceNodeId, edge.targetNodeId, source->outputPoint(), destination->inputPoint(), juce::Colour (0xff627080), 2.0f);

            for (const auto& edge : owner.getProject().getDspGraph().edges)
                if (edge.enabled && edge.signalType == DspSignalType::event)
                    if (auto* source = cardForId (edge.sourceNodeId))
                        if (auto* destination = cardForId (edge.targetNodeId))
                            drawCable (edge.sourceNodeId, edge.targetNodeId, source->outputPoint(), destination->inputPoint(), juce::Colour (0xffffb84d), 2.5f);

            const auto* element = owner.getProject().getLayout().find (elementId);
            const auto boundParameter = element != nullptr ? element->parameterId : juce::String();
            for (auto& card : cards)
            {
                if (const auto* definition = card->getDefinition())
                    if (definition->parameterIds.contains (boundParameter))
                    {
                        drawCable (elementId, card->getBlockId(), target->outputPoint(), card->inputPoint(), PatchCraftLookAndFeel::accent(), 3.0f);
                        break;
                    }
            }

            for (const auto& route : owner.getProject().getDspGraph().modulation)
            {
                if (! route.enabled)
                    continue;
                auto* source = cardForId (route.sourceId);
                if (source == nullptr)
                    continue;

                for (auto& destination : cards)
                {
                    const auto* block = ControlNodeAuthoring::findBlock (owner.getProject(), destination->getBlockId());
                    const auto* definition = destination->getDefinition();
                    if (destination.get() != source && ((block != nullptr && block->targetId == route.targetId)
                        || (definition != nullptr && definition->parameterIds.contains (route.targetId))))
                    {
                        drawCable (route.sourceId, destination->getBlockId(), source->outputPoint(), destination->inputPoint(), juce::Colour (0xffd889e8), 2.5f);
                        break;
                    }
                }

                if (route.targetId == boundParameter)
                    drawCable (route.sourceId, "parameter_binding", source->outputPoint(), { target->getBounds().getCentreX() + 40.0f,
                                                       target->getBounds().getBottom() + 8.0f },
                               juce::Colour (0xffd889e8), 2.5f);
            }

            if (cableDragging)
            {
                if (auto* source = cardForId (cableSourceId))
                    drawCable (cableSourceId, "drag", source->outputPoint(), cablePoint, PatchCraftLookAndFeel::accent().brighter (0.25f), 3.0f);
                    
                for (auto& destination : cards)
                {
                    if (destination->getBlockId() == cableSourceId)
                        continue;
                    const auto pt = destination->inputPoint();
                    g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.4f + 0.2f * std::sin (juce::Time::getMillisecondCounter() * 0.005f)));
                    g.fillEllipse (pt.x - 12.0f, pt.y - 12.0f, 24.0f, 24.0f);
                    g.setColour (PatchCraftLookAndFeel::accent());
                    g.fillEllipse (pt.x - 6.0f, pt.y - 6.0f, 12.0f, 12.0f);
                }
            }
        }

    private:
        void timerCallback() override
        {
            if (cableDragging)
                repaint();
        }

        void mouseDown (const juce::MouseEvent& event) override
        {
            grabKeyboardFocus();

            if (event.mods.isPopupMenu() && event.eventComponent == this)
            {
                if (auto* e = findParentComponentOfClass<ControlNodeEditor>())
                    e->showAddNodeMenu ({}, event.getPosition(), event.getScreenPosition());
                return;
            }

            if (event.mods.isMiddleButtonDown() || event.mods.isAltDown()
                || juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey))
            {
                panning = true;
                panStart = event.getScreenPosition();
                return;
            }

            bool cableClicked = false;
            for (auto& c : drawnCables)
            {
                juce::Path strokePath;
                juce::PathStrokeType (c.width + 10.0f).createStrokedPath (strokePath, c.path);
                if (strokePath.contains (event.position))
                {
                    cableClicked = true;
                    selectedCable = c;
                    
                    if (event.mods.isPopupMenu())
                    {
                        juce::PopupMenu m;
                        m.addItem (1, "Disconnect");
                        auto pos = event.getScreenPosition();
                        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (juce::Rectangle<int>(pos.x, pos.y, 1, 1)),
                            [this, src = c.sourceId, tgt = c.targetId] (int res)
                            {
                                if (res == 1)
                                {
                                    juce::String status;
                                    ControlNodeAuthoring::disconnectNodes (owner.getProject(), src, tgt, status);
                                    if (auto* e = findParentComponentOfClass<ControlNodeEditor>())
                                        e->rebuild();
                                }
                            });
                        return;
                    }
                    break;
                }
            }

            if (! cableClicked)
            {
                selectedCable.reset();
                selectedBlockId.clear();
                for (auto& card : cards)
                    card->setSelected (false);
            }

            repaint();
        }

        void selectNode (const juce::String& blockId)
        {
            selectedBlockId = blockId;
            selectedCable.reset();
            for (auto& card : cards)
                card->setSelected (card->getBlockId() == blockId);
            repaint();
        }

        void updateCanvasBounds()
        {
            int maxRight = 1400;
            int maxBottom = 700;

            if (target != nullptr)
            {
                maxRight = juce::jmax (maxRight, target->getRight() + 40);
                maxBottom = juce::jmax (maxBottom, target->getBottom() + 60);
            }

            for (auto& card : cards)
            {
                maxRight = juce::jmax (maxRight, card->getRight() + 40);
                maxBottom = juce::jmax (maxBottom, card->getBottom() + 60);
            }

            editor.baseCanvasWidth = maxRight;
            editor.baseCanvasHeight = maxBottom;
            editor.applyCanvasZoom();
        }

        bool keyPressed (const juce::KeyPress& key) override
        {
            if (key.isKeyCode (juce::KeyPress::spaceKey))
            {
                spacePanActive = true;
                return true;
            }

            if (selectedCable.has_value() && (key.isKeyCode (juce::KeyPress::deleteKey) || key.isKeyCode (juce::KeyPress::backspaceKey)))
            {
                juce::String status;
                ControlNodeAuthoring::disconnectNodes (owner.getProject(), selectedCable->sourceId, selectedCable->targetId, status);
                if (auto* e = findParentComponentOfClass<ControlNodeEditor>())
                    e->rebuild();
                return true;
            }

            if (selectedBlockId.isNotEmpty()
                && (key.isKeyCode (juce::KeyPress::deleteKey) || key.isKeyCode (juce::KeyPress::backspaceKey)))
            {
                juce::String status;
                if (ControlNodeAuthoring::deleteNode (owner.getProject(), selectedBlockId, status))
                {
                    editor.setStatus (status);
                    editor.rebuild();
                }
                return true;
            }

            return false;
        }

        bool keyStateChanged (bool isKeyDown) override
        {
            if (! juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey))
                spacePanActive = false;
            return juce::Component::keyStateChanged (isKeyDown);
        }

        void mouseDrag (const juce::MouseEvent& event) override
        {
            if (panning || spacePanActive
                || juce::KeyPress::isKeyCurrentlyDown (juce::KeyPress::spaceKey))
            {
                const auto delta = panStart - event.getScreenPosition();
                panStart = event.getScreenPosition();
                if (auto* vp = findParentComponentOfClass<juce::Viewport>())
                    vp->setViewPosition (vp->getViewPositionX() + delta.x, vp->getViewPositionY() + delta.y);
                return;
            }
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            panning = false;
        }

        void finishCable (juce::Point<float> point)
        {
            cableDragging = false;
            NodeCard* destination = nullptr;
            float closestDistance = 60.0f;
            for (auto& card : cards)
            {
                if (card->getBlockId() == cableSourceId)
                    continue;
                const auto distance = card->inputPoint().getDistanceFrom (point);
                if (distance < closestDistance)
                {
                    closestDistance = distance;
                    destination = card.get();
                }
            }

            juce::String message;
            if (destination == nullptr)
                message = "Cable cancelled. Drop directly on a node's IN port.";
            else
                ControlNodeAuthoring::connectNodes (owner.getProject(), cableSourceId,
                                                     destination->getBlockId(), message);
            editor.setStatus (message);
            cableSourceId.clear();
            repaint();

            juce::Component::SafePointer<ControlNodeEditor> safeEditor (&editor);
            juce::MessageManager::callAsync ([safeEditor]
            {
                if (auto* targetEditor = safeEditor.getComponent())
                    targetEditor->rebuild();
            });
        }

        ControlNodeEditor& editor;
        StudioMainComponent& owner;
        juce::String elementId;
        std::unique_ptr<TargetCard> target;
        std::vector<std::unique_ptr<NodeCard>> cards;
        
        struct DrawnCable
        {
            juce::String sourceId;
            juce::String targetId;
            juce::Path path;
            float width;
        };
        mutable std::vector<DrawnCable> drawnCables;
        std::optional<DrawnCable> selectedCable;
        juce::String selectedBlockId;

        juce::String cableSourceId;
        juce::Point<float> cablePoint;
        bool cableDragging = false;
        bool panning = false;
        bool spacePanActive = false;
        juce::Point<int> panStart;
    };

    class ControlNodeEditor::GraphMiniMap final : public juce::Component
    {
    public:
        explicit GraphMiniMap (ControlNodeEditor& targetEditor) : editor (targetEditor) {}

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (juce::Colour (0xdd0b1016));
            g.fillRoundedRectangle (bounds, 6.0f);
            g.setColour (PatchCraftLookAndFeel::border().withAlpha (0.7f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

            if (editor.nodeCanvas == nullptr || editor.baseCanvasWidth <= 0 || editor.baseCanvasHeight <= 0)
                return;

            const float scale = juce::jmin (bounds.getWidth() / (float) editor.baseCanvasWidth,
                                            bounds.getHeight() / (float) editor.baseCanvasHeight);

            for (const auto& block : editor.owner.getProject().getDspGraph().blocks)
            {
                int x = ControlNodeAuthoring::defaultColumnForSection (block.section);
                int y = 38;
                int w = 320;
                int h = 108;
                if (auto it = block.metadata.find ("uiX"); it != block.metadata.end())
                    x = it->second.getIntValue();
                if (auto it = block.metadata.find ("uiY"); it != block.metadata.end())
                    y = it->second.getIntValue();
                if (auto it = block.metadata.find ("uiW"); it != block.metadata.end())
                    w = juce::jmax (120, it->second.getIntValue());
                if (auto it = block.metadata.find ("uiH"); it != block.metadata.end())
                    h = juce::jmax (48, it->second.getIntValue());

                auto rect = juce::Rectangle<float> (bounds.getX() + x * scale,
                                                    bounds.getY() + y * scale,
                                                    w * scale,
                                                    h * scale);
                g.setColour (sectionColour (block.section).withAlpha (0.9f));
                g.fillRect (rect);
            }

            const auto viewed = editor.viewport.getViewArea().toFloat();
            auto viewRect = juce::Rectangle<float> (bounds.getX() + viewed.getX() * scale,
                                                    bounds.getY() + viewed.getY() * scale,
                                                    viewed.getWidth() * scale,
                                                    viewed.getHeight() * scale);
            g.setColour (PatchCraftLookAndFeel::accent().withAlpha (0.22f));
            g.fillRect (viewRect);
            g.setColour (PatchCraftLookAndFeel::accent());
            g.drawRect (viewRect, 1.0f);
        }

        void mouseDown (const juce::MouseEvent& event) override
        {
            if (editor.nodeCanvas == nullptr || editor.baseCanvasWidth <= 0 || editor.baseCanvasHeight <= 0)
                return;

            const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
            const float scale = juce::jmin (bounds.getWidth() / (float) editor.baseCanvasWidth,
                                            bounds.getHeight() / (float) editor.baseCanvasHeight);
            const auto local = event.position - bounds.getPosition();
            const int targetX = juce::roundToInt (local.x / scale - editor.viewport.getViewWidth() * 0.5f);
            const int targetY = juce::roundToInt (local.y / scale - editor.viewport.getViewHeight() * 0.5f);
            editor.viewport.setViewPosition (juce::jmax (0, targetX), juce::jmax (0, targetY));
            repaint();
        }

    private:
        ControlNodeEditor& editor;
    };

    ControlNodeEditor::ControlNodeEditor (StudioMainComponent& targetOwner, juce::String targetElementId)
        : owner (targetOwner), elementId (std::move (targetElementId))
    {
        setSize (1080, 720);
        addAndMakeVisible (title);
        addAndMakeVisible (targetSummary);
        addAndMakeVisible (status);
        addAndMakeVisible (viewport);

        title.setText (elementId.isEmpty() ? "GLOBAL DSP GRAPH" : "CONTROL NODE EDITOR", juce::dontSendNotification);
        title.setFont (juce::FontOptions (18.0f).withStyle ("bold"));
        title.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::text());
        targetSummary.setFont (12.0f);
        targetSummary.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
        status.setFont (11.5f);
        status.setColour (juce::Label::textColourId, PatchCraftLookAndFeel::accent());
        status.setText ("Add a node, tweak it, then bind this control to one of its parameters.", juce::dontSendNotification);

        for (auto* button : { &tidyButton, &fitButton })
        {
            button->getProperties().set ("smallButton", true);
            addAndMakeVisible (*button);
        }
        tidyButton.setTooltip ("Auto-layout nodes into category columns.");
        tidyButton.onClick = [this]
        {
            ControlNodeAuthoring::tidyGraphLayout (owner.getProject());
            owner.getProject().notifyChanged();
            setStatus ("Graph tidied into category columns.");
            rebuild();
        };
        fitButton.setTooltip ("Fit the full graph into the viewport.");
        fitButton.onClick = [this] { fitCanvasToView(); };

        searchField.setTextToShowWhenEmpty ("Search nodes...", PatchCraftLookAndFeel::textDim());
        searchField.setFont (juce::FontOptions (12.0f));
        searchField.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff151a21));
        searchField.setColour (juce::TextEditor::outlineColourId, PatchCraftLookAndFeel::border());
        searchField.setColour (juce::TextEditor::textColourId, PatchCraftLookAndFeel::text());
        searchField.onTextChange = [this] { applySearchFilter(); };
        addAndMakeVisible (searchField);

        graphTemplateBox.addItemList (ControlNodeAuthoring::getGraphTemplateNames(), 1);
        graphTemplateBox.setTextWhenNothingSelected ("Graph Templates...");
        graphTemplateBox.onChange = [this]
        {
            if (graphTemplateBox.getSelectedId() == 0)
                return;

            juce::String message;
            if (ControlNodeAuthoring::applyGraphTemplate (owner.getProject(), graphTemplateBox.getText(), message))
            {
                graphTemplateBox.setSelectedId (0, juce::dontSendNotification);
                graphTemplateBox.setTextWhenNothingSelected ("Graph Templates...");
                setStatus (message);
                rebuild();
            }
            else
            {
                setStatus (message);
            }
        };
        addAndMakeVisible (graphTemplateBox);

        validationBadge.setFont (juce::FontOptions (11.0f).withStyle ("bold"));
        validationBadge.setJustificationType (juce::Justification::centredRight);
        validationBadge.setInterceptsMouseClicks (true, false);
        validationBadge.setMouseCursor (juce::MouseCursor::PointingHandCursor);
        addAndMakeVisible (validationBadge);

        miniMap = std::make_unique<GraphMiniMap> (*this);
        addAndMakeVisible (*miniMap);

        viewport.setScrollBarsShown (true, true);
        viewport.setScrollBarThickness (12);
        viewport.setViewedComponent (nullptr, false);

        rebuild();
    }

    ControlNodeEditor::~ControlNodeEditor()
    {
        viewport.setViewedComponent (nullptr, false);
    }

    void ControlNodeEditor::paint (juce::Graphics& g)
    {
        g.fillAll (PatchCraftLookAndFeel::bg());
        auto top = getLocalBounds().removeFromTop (140);
        g.setColour (PatchCraftLookAndFeel::panel());
        g.fillRect (top);
        g.setColour (PatchCraftLookAndFeel::border());
        g.drawHorizontalLine (top.getBottom() - 1, 0.0f, (float) getWidth());
    }

    void ControlNodeEditor::resized()
    {
        auto r = getLocalBounds();
        auto header = r.removeFromTop (64).reduced (14, 6);
        title.setBounds (header.removeFromTop (26));
        targetSummary.setBounds (header.removeFromTop (24));

        auto tools = r.removeFromTop (36).reduced (14, 4);
        tidyButton.setBounds (tools.removeFromLeft (58));
        tools.removeFromLeft (6);
        fitButton.setBounds (tools.removeFromLeft (48));
        tools.removeFromLeft (10);
        searchField.setBounds (tools.removeFromLeft (juce::jmin (220, tools.getWidth() / 2)));
        tools.removeFromLeft (8);
        graphTemplateBox.setBounds (tools.removeFromLeft (juce::jmin (180, tools.getWidth() / 2)));
        tools.removeFromLeft (8);
        validationBadge.setBounds (tools);

        auto palette = r.removeFromTop (40).reduced (14, 4);
        for (size_t i = 0; i < addButtons.size(); ++i)
        {
            if (i < paletteLabels.size() && paletteLabels[i] != nullptr)
            {
                paletteLabels[i]->setBounds (palette.removeFromLeft (paletteLabels[i]->getFont().getStringWidth(paletteLabels[i]->getText()) + 10));
            }
            if (addButtons[i] != nullptr)
            {
                const int width = juce::jlimit (64, 102, 28 + addButtons[i]->getButtonText().length() * 7);
                addButtons[i]->setBounds (palette.removeFromLeft (width));
                palette.removeFromLeft (5);
            }
        }

        auto statusArea = r.removeFromBottom (30).reduced (14, 3);
        status.setBounds (statusArea);
        viewport.setBounds (r.reduced (10, 6));

        if (miniMap != nullptr)
        {
            auto mapBounds = viewport.getBounds().removeFromRight (148).removeFromBottom (108).reduced (8);
            miniMap->setBounds (mapBounds);
            miniMap->toFront (false);
        }
    }

    void ControlNodeEditor::updateValidationBadge()
    {
        const auto issues = ControlNodeAuthoring::validateNodeGraph (owner.getProject());
        int errors = 0;
        int warnings = 0;
        for (const auto& issue : issues)
        {
            if (issue.severity == "error") ++errors;
            else if (issue.severity == "warning") ++warnings;
        }

        if (errors > 0)
        {
            validationBadge.setText (juce::String (errors) + " error"
                                     + (errors == 1 ? "" : "s")
                                     + (warnings > 0 ? "  |  " + juce::String (warnings) + " warn" : ""),
                                     juce::dontSendNotification);
            validationBadge.setColour (juce::Label::textColourId, juce::Colour (0xffff5a5a));
        }
        else if (warnings > 0)
        {
            validationBadge.setText (juce::String (warnings) + " warning" + (warnings == 1 ? "" : "s"),
                                     juce::dontSendNotification);
            validationBadge.setColour (juce::Label::textColourId, juce::Colour (0xffffb84d));
        }
        else
        {
            validationBadge.setText ("Graph OK", juce::dontSendNotification);
            validationBadge.setColour (juce::Label::textColourId, juce::Colour (0xff79c267));
        }
    }

    void ControlNodeEditor::applySearchFilter()
    {
        searchQuery = searchField.getText();
        if (nodeCanvas != nullptr)
            nodeCanvas->applySearchFilter (searchQuery);
        if (miniMap != nullptr)
            miniMap->repaint();
    }

    void ControlNodeEditor::rebuild()
    {
        if (elementId.isNotEmpty())
        {
            const auto* element = owner.getProject().getLayout().find (elementId);
            if (element == nullptr || ! isControlElement (element->type))
            {
                targetSummary.setText ("The selected element is not a bindable runtime control.", juce::dontSendNotification);
                return;
            }

            const auto* parameter = owner.getProject().getParameters().find (element->parameterId);
            const auto parameterName = parameter != nullptr ? parameter->name : element->parameterId;
            targetSummary.setText ((element->label.isNotEmpty() ? element->label : element->id)
                                   + "  |  " + elementTypeName (element->type)
                                   + "  |  " + (parameterName.isNotEmpty() ? "Drives: " + parameterName : "Not connected"),
                                   juce::dontSendNotification);
        }
        else
        {
            targetSummary.setText ("GLOBAL GRAPH VIEW  |  All nodes and connections in the instrument", juce::dontSendNotification);
        }

        viewport.setViewedComponent (nullptr, false);
        nodeCanvas = std::make_unique<NodeCanvas> (*this, owner, elementId);
        viewport.setViewedComponent (nodeCanvas.get(), false);
        applyCanvasZoom();

        juce::Component::SafePointer<ControlNodeEditor> safeThis (this);
        juce::MessageManager::callAsync ([safeThis]
        {
            if (auto* editor = safeThis.getComponent())
                editor->fitCanvasToView();
        });

        for (auto& btn : addButtons) if (btn) removeChildComponent(btn.get());
        for (auto& lbl : paletteLabels) if (lbl) removeChildComponent(lbl.get());
        addButtons.clear();
        paletteLabels.clear();

        // Add Existing Nodes
        auto addLabel = [&](const juce::String& text) {
            auto lbl = std::make_unique<juce::Label>();
            lbl->setText(text, juce::dontSendNotification);
            lbl->setFont(juce::FontOptions(12.0f).withStyle("bold"));
            lbl->setColour(juce::Label::textColourId, PatchCraftLookAndFeel::textDim());
            addAndMakeVisible(*lbl);
            paletteLabels.push_back(std::move(lbl));
            addButtons.push_back(nullptr); // padding
        };

        bool hasExisting = false;
        for (const auto& block : owner.getProject().getDspGraph().blocks)
        {
            if (!hasExisting)
            {
                addLabel("EXISTING:");
                hasExisting = true;
            }
            auto button = std::make_unique<juce::TextButton>(block.name);
            button->getProperties().set("smallButton", true);
            const auto* def = ControlNodeAuthoring::definitionForBlock(block);
            const auto blockDefId = def != nullptr ? def->id : block.type;
            // Actually just reveal it.
            button->onClick = [this, blockDefId] { addNode(blockDefId, false); };
            addAndMakeVisible(*button);
            addButtons.push_back(std::move(button));
            paletteLabels.push_back(nullptr);
        }

        addLabel("NEW:");
        const juce::StringArray categoryOrder { "Sources", "Shape", "Motion", "FX", "Output" };
        for (const auto& category : categoryOrder)
        {
            bool addedCategory = false;
            for (const auto* definition : ControlNodeAuthoring::definitionsForEngine (owner.getProject().getEngineType()))
            {
                if (ControlNodeAuthoring::sectionCategoryLabel (definition->section) != category)
                    continue;

                if (! addedCategory)
                {
                    addLabel (category + ":");
                    addedCategory = true;
                }

                auto button = std::make_unique<juce::TextButton> ("+ " + definition->name);
                button->getProperties().set ("smallButton", true);
                button->setTooltip ("Create a new " + definition->name + " node in the graph.");
                const auto definitionId = definition->id;
                button->onClick = [this, definitionId] { addNode (definitionId, true); };
                addAndMakeVisible (*button);
                addButtons.push_back (std::move (button));
                paletteLabels.push_back (nullptr);
            }
        }

        resized();
        updateValidationBadge();
        if (miniMap != nullptr)
            miniMap->repaint();
        repaint();
    }

    void ControlNodeEditor::addNodeAt (const juce::String& definitionId, juce::Point<int> canvasPosition,
                                       const juce::String& anchorBlockId)
    {
        juce::String message;
        if (auto* block = ControlNodeAuthoring::createNodeAt (owner.getProject(), definitionId,
                                                              canvasPosition, message))
        {
            if (anchorBlockId.isNotEmpty())
            {
                juce::String connectStatus;
                if (! ControlNodeAuthoring::connectNodes (owner.getProject(), anchorBlockId, block->id, connectStatus))
                    ControlNodeAuthoring::connectNodes (owner.getProject(), block->id, anchorBlockId, connectStatus);
                message += "  " + connectStatus;
            }
            setStatus (message);
            rebuild();
            return;
        }
        setStatus (message);
    }

    void ControlNodeEditor::showAddNodeMenu (const juce::String& anchorBlockId,
                                              juce::Point<int> canvasPosition,
                                              juce::Point<int> screenPosition)
    {
        const auto definitions = ControlNodeAuthoring::connectableDefinitions (owner.getProject(), anchorBlockId);
        if (definitions.empty())
        {
            setStatus ("No compatible nodes can connect to the current graph.");
            return;
        }

        juce::PopupMenu root;
        const juce::StringArray categoryOrder { "Sources", "Shape", "Motion", "FX", "Output" };
        auto idMap = std::make_shared<std::map<int, juce::String>>();
        int itemId = 100;

        for (const auto& category : categoryOrder)
        {
            juce::PopupMenu sectionMenu;
            bool hasItems = false;

            for (const auto* definition : definitions)
            {
                if (ControlNodeAuthoring::sectionCategoryLabel (definition->section) != category)
                    continue;

                sectionMenu.addItem (itemId, definition->name);
                (*idMap)[itemId++] = definition->id;
                hasItems = true;
            }

            if (hasItems)
                root.addSubMenu (category, sectionMenu);
        }

        root.showMenuAsync (juce::PopupMenu::Options()
                                .withTargetScreenArea ({ screenPosition.x, screenPosition.y, 1, 1 }),
            [this, anchorBlockId, canvasPosition, idMap] (int result)
            {
                const auto it = idMap->find (result);
                if (it != idMap->end())
                    addNodeAt (it->second, canvasPosition, anchorBlockId);
            });
    }

    void ControlNodeEditor::addNode (const juce::String& definitionId, bool forceNew)
    {
        juce::String message;
        if (forceNew)
            ControlNodeAuthoring::createNode (owner.getProject(), definitionId, message);
        else
            ControlNodeAuthoring::ensureNode (owner.getProject(), definitionId, message);
        setStatus (message);
        rebuild();
    }

    void ControlNodeEditor::setStatus (const juce::String& message)
    {
        status.setText (message, juce::dontSendNotification);
    }

    void ControlNodeEditor::applyCanvasZoom()
    {
        zoomScale = juce::jlimit (0.25f, 4.0f, zoomScale);
        if (nodeCanvas == nullptr)
            return;

        const float scrollMult = zoomScale >= 1.0f ? zoomScale : (1.0f / zoomScale);
        nodeCanvas->setTransform (juce::AffineTransform::scale (zoomScale));
        nodeCanvas->setSize (juce::jmax (baseCanvasWidth, juce::roundToInt ((float) baseCanvasWidth * scrollMult)),
                             juce::jmax (baseCanvasHeight, juce::roundToInt ((float) baseCanvasHeight * scrollMult)));
        viewport.resized();
    }

    void ControlNodeEditor::fitCanvasToView()
    {
        if (nodeCanvas == nullptr || baseCanvasWidth <= 0 || baseCanvasHeight <= 0)
            return;

        const auto area = viewport.getViewArea();
        if (area.isEmpty())
            return;

        const float fitZoom = juce::jmin ((float) area.getWidth() / (float) baseCanvasWidth,
                                          (float) area.getHeight() / (float) baseCanvasHeight) * 0.92f;
        zoomScale = juce::jlimit (0.35f, 1.25f, fitZoom);
        applyCanvasZoom();
        viewport.setViewPosition (0, 0);
        if (miniMap != nullptr)
            miniMap->repaint();
    }

    void ControlNodeEditor::mouseDown (const juce::MouseEvent& e)
    {
        if (validationBadge.getBounds().contains (e.getPosition()))
        {
            const auto issues = ControlNodeAuthoring::validateNodeGraph (owner.getProject());
            juce::PopupMenu menu;
            if (issues.empty())
            {
                menu.addItem (1, "Graph routing looks good.", false, false);
            }
            else
            {
                int id = 10;
                for (const auto& issue : issues)
                    menu.addItem (id++, issue.severity.toUpperCase() + ": " + issue.message);
            }
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&validationBadge));
        }
    }

    void ControlNodeEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
    {
        if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        {
            zoomScale += wheel.deltaY * 1.5f;
            applyCanvasZoom();
            return;
        }
        juce::Component::mouseWheelMove (e, wheel);
    }
}
