#include "LayoutModel.h"

namespace patchcraft
{
    LayoutModel::LayoutModel() = default;

    LayoutElement* LayoutModel::find (const juce::String& id)
    {
        for (auto& e : elements)
            if (e.id == id) return &e;
        return nullptr;
    }

    int LayoutModel::findIndex (const juce::String& id) const
    {
        for (int i = 0; i < (int) elements.size(); ++i)
            if (elements[(size_t) i].id == id) return i;
        return -1;
    }

    juce::String LayoutModel::generateUniqueId (const juce::String& prefix)
    {
        for (int i = 1; i < 100000; ++i)
        {
            auto candidate = prefix + juce::String (i);
            if (find (candidate) == nullptr)
                return candidate;
        }
        return prefix + juce::Uuid().toDashedString();
    }

    LayoutElement& LayoutModel::add (const LayoutElement& e)
    {
        LayoutElement copy = e;
        if (copy.id.isEmpty())
            copy.id = generateUniqueId (elementTypeToString (copy.type) + "_");
        elements.push_back (copy);
        return elements.back();
    }

    void LayoutModel::remove (const juce::String& id)
    {
        for (auto it = elements.begin(); it != elements.end(); ++it)
            if (it->id == id) { elements.erase (it); return; }
    }

    void LayoutModel::bringForward (const juce::String& id)
    {
        auto idx = findIndex (id);
        if (idx < 0 || idx >= (int) elements.size() - 1) return;
        std::swap (elements[(size_t) idx], elements[(size_t) idx + 1]);
    }

    void LayoutModel::sendBackward (const juce::String& id)
    {
        auto idx = findIndex (id);
        if (idx <= 0) return;
        std::swap (elements[(size_t) idx], elements[(size_t) idx - 1]);
    }

    juce::var LayoutModel::toVar (const CanvasSize& canvas) const
    {
        auto* obj = new juce::DynamicObject();

        auto* canvasObj = new juce::DynamicObject();
        canvasObj->setProperty ("width",  canvas.width);
        canvasObj->setProperty ("height", canvas.height);
        obj->setProperty ("canvas", juce::var (canvasObj));

        juce::Array<juce::var> arr;
        for (auto& e : elements) arr.add (e.toVar());
        obj->setProperty ("elements", arr);

        return juce::var (obj);
    }

    void LayoutModel::fromVar (const juce::var& v, CanvasSize& outCanvas)
    {
        elements.clear();
        if (auto* o = v.getDynamicObject())
        {
            if (auto* c = o->getProperty ("canvas").getDynamicObject())
            {
                outCanvas.width  = (int) c->getProperty ("width");
                outCanvas.height = (int) c->getProperty ("height");
                if (outCanvas.width  <= 0) outCanvas.width  = 1280;
                if (outCanvas.height <= 0) outCanvas.height = 800;
            }
            if (auto* a = o->getProperty ("elements").getArray())
                for (auto& item : *a)
                    elements.push_back (LayoutElement::fromVar (item));
        }
    }

} // namespace patchcraft
