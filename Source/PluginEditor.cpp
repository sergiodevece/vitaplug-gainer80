#include "PluginEditor.h"
#include "BinaryData.h"
#include "UI/VuScaleMap.h"

namespace
{
const auto cream = juce::Colour(0xffe4d9b7);
const auto scaleColour = juce::Colour(0xffd8d4bd);

// Canonical UI-01 geometry measured from the approved 1652 x 952 artwork and
// normalised to the fixed 760 x 438 editor. Component bounds deliberately
// retain generous hit areas; their centres are the visual control centres.
namespace G80Layout
{
constexpr auto editorWidth = 760;
constexpr auto editorHeight = 438;

constexpr juce::Point<float> gainCentre { 322.6f, 230.4f };
constexpr float gainDiameter = 115.0f;
constexpr int gainInteractionWidth = 201;
constexpr int gainInteractionHeight = 242;

constexpr juce::Point<float> selectorMechanicalCentre { 451.8f, 232.3f };
// The vector switch keeps its existing physical dimensions until its approved
// raster layers are integrated in the subsequent UI phase.
constexpr int selectorInteractionWidth = 42;
constexpr int selectorInteractionHeight = 136;
constexpr float selectorPivotYOffset = 1.5f;
constexpr float selectorBaseVisibleDiameter = 46.0f;
constexpr float selectorBaseSourceVisibleDiameter = 850.0f;
constexpr juce::Point<float> selectorBaseSourcePivot { 512.0f, 512.0f };
constexpr juce::Point<float> selectorUpperLeverSourcePivot { 350.0f, 780.0f };
constexpr juce::Point<float> selectorLowerLeverSourcePivot { 350.0f, 330.0f };
constexpr float selectorUpperLabelCentreY = 193.0f;
constexpr float selectorLowerLabelCentreY = 278.0f;

constexpr juce::Point<float> fattyCentre { 595.3f, 232.5f };
constexpr float fattyDiameter = 143.0f;
constexpr int fattyInteractionWidth = 235;
constexpr int fattyInteractionHeight = 291;

juce::Rectangle<int> centredInteractionBounds(juce::Point<float> centre, int width, int height)
{
    return { juce::roundToInt(centre.x - static_cast<float>(width) * 0.5f),
             juce::roundToInt(centre.y - static_cast<float>(height) * 0.5f), width, height };
}
}

juce::Point<float> polar(float x, float y, float radius, float angle)
{
    return { x + std::cos(angle) * radius, y + std::sin(angle) * radius };
}

float rotaryAngle(float proportion)
{
    return juce::degreesToRadians(135.0f + 270.0f * proportion);
}

juce::Image loadBundledPng(const void* data, int size)
{
    return juce::ImageFileFormat::loadFrom(data, static_cast<size_t>(size));
}

void drawImageAtLogicalSize(juce::Graphics& g, const juce::Image& image, juce::Rectangle<float> destination)
{
    if (image.isValid())
        g.drawImageWithin(image, juce::roundToInt(destination.getX()), juce::roundToInt(destination.getY()),
                          juce::roundToInt(destination.getWidth()), juce::roundToInt(destination.getHeight()),
                          juce::RectanglePlacement::stretchToFit, false);
}

void drawEngravedIndicator(juce::Graphics& g, juce::Point<float> centre, float radius, float angle)
{
    // A shallow radial slot near the outer face: 18% of radius long, never a
    // centre-to-edge line. The offset strokes create a restrained metal bevel.
    const auto inner = polar(centre.x, centre.y, radius * 0.58f, angle);
    const auto outer = polar(centre.x, centre.y, radius * 0.76f, angle);
    const auto perpendicular = juce::Point<float>(-std::sin(angle), std::cos(angle));
    const auto shadowOffset = perpendicular * 0.65f + juce::Point<float>(0.35f, 0.55f);

    g.setColour(juce::Colours::black.withAlpha(0.72f));
    g.drawLine({ inner + shadowOffset, outer + shadowOffset }, 2.8f);
    g.setColour(juce::Colour(0xff20221f));
    g.drawLine({ inner, outer }, 1.8f);
    g.setColour(juce::Colours::white.withAlpha(0.30f));
    g.drawLine({ inner - perpendicular * 0.5f, outer - perpendicular * 0.5f }, 0.65f);
}

}

G80Knob::G80Knob(juce::String label, bool isBass, const juce::Image& bodyImage,
                 juce::Point<float> imageHub, float sourceMechanicalDiameter)
    : caption(std::move(label)), bassKnob(isBass), body(bodyImage), bodyImageHub(imageHub),
      bodySourceMechanicalDiameter(sourceMechanicalDiameter)
{
    setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setWantsKeyboardFocus(true);
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::transparentBlack);
}

void G80Knob::paint(juce::Graphics& g)
{
    // The component bounds are symmetric around the approved physical centre.
    // Every mark is derived from that same centre, rather than from a separate
    // visual offset, so the two different sizes remain coherent.
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const auto bodyDiameter = bassKnob ? G80Layout::fattyDiameter : G80Layout::gainDiameter;
    const auto bodyRadius = bodyDiameter * 0.5f;
    const auto proportion = static_cast<float>(valueToProportionOfLength(getValue()));
    const auto pointerAngle = rotaryAngle(proportion);

    // Each control has its own approved metal asset. Scale from the measured
    // source mechanical diameter, then align the measured source hub to the
    // canonical control centre. Transparent margins may therefore differ
    // without shifting the rotating indicator or its interactive target.
    const auto sourceScale = bodyDiameter / bodySourceMechanicalDiameter;
    const auto renderedSize = juce::Point<float>(static_cast<float>(body.getWidth()) * sourceScale,
                                                 static_cast<float>(body.getHeight()) * sourceScale);
    const auto bodyTopLeft = centre - bodyImageHub * sourceScale;
    drawImageAtLogicalSize(g, body, { bodyTopLeft.x, bodyTopLeft.y, renderedSize.x, renderedSize.y });

    constexpr auto divisions = 24;
    for (int index = 0; index <= divisions; ++index)
    {
        const auto angle = rotaryAngle(static_cast<float>(index) / static_cast<float>(divisions));
        const auto isMajor = index % 6 == 0;
        const auto outer = polar(centre.x, centre.y, bodyRadius * 1.30f, angle);
        const auto inner = polar(centre.x, centre.y, bodyRadius * (isMajor ? 1.13f : 1.22f), angle);
        g.setColour(isMajor ? cream : juce::Colour(0xffbbc1ac));
        g.drawLine({ inner, outer }, isMajor ? 1.7f : 0.75f);
    }

    const auto labelValues = bassKnob ? std::array<double, 5> { 0.0, 3.0, 6.0, 9.0, 12.0 }
                                     : std::array<double, 5> { -24.0, -12.0, 0.0, 12.0, 24.0 };
    const auto labelRadius = bodyRadius * (bassKnob ? 1.43f : 1.48f);
    g.setFont(juce::FontOptions(bassKnob ? 11.0f : 10.0f, juce::Font::bold));
    for (const auto value : labelValues)
    {
        const auto point = polar(centre.x, centre.y, labelRadius,
                                 rotaryAngle(static_cast<float>(valueToProportionOfLength(value))));
        const auto text = bassKnob ? juce::String(value, 0)
                                   : juce::String(value > 0.0 ? "+" : "") + juce::String(value, 0);
        g.setColour(scaleColour);
        g.drawFittedText(text, juce::Rectangle<float>(point.x - 19.0f, point.y - 7.0f, 38.0f, 14.0f).toNearestInt(),
                         juce::Justification::centred, 1);
    }

    drawEngravedIndicator(g, centre, bodyRadius, pointerAngle);

    const auto captionY = centre.y + bodyRadius + (bassKnob ? 28.0f : 24.0f);
    g.setColour(cream);
    g.setFont(juce::FontOptions(bassKnob ? 14.0f : 13.0f, juce::Font::bold));
    g.drawFittedText(caption, juce::Rectangle<float>(centre.x - 58.0f, captionY - 9.0f, 116.0f, 18.0f).toNearestInt(),
                     juce::Justification::centred, 1);

    if (readoutVisible)
    {
        // Interaction-only readout: its parent-space centres are the approved
        // UI-05 positions. It is painted from Slider::getValue(), which is the
        // same value kept in sync by the existing SliderAttachment.
        const auto readoutCentreY = (bassKnob ? 350.0f : 334.0f) - static_cast<float>(getY());
        const auto readoutBounds = juce::Rectangle<float>(centre.x - 44.0f, readoutCentreY - 7.0f, 88.0f, 14.0f);
        const auto readout = formatReadout();
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.setColour(juce::Colour(0xff080c09).withAlpha(0.72f));
        g.drawFittedText(readout, readoutBounds.translated(0.7f, 1.0f).toNearestInt(), juce::Justification::centred, 1);
        g.setColour(cream);
        g.drawFittedText(readout, readoutBounds.toNearestInt(), juce::Justification::centred, 1);
    }
}

void G80Knob::mouseDown(const juce::MouseEvent& event)
{
    setReadoutVisible(true);
    // RotaryHorizontalVerticalDrag retains JUCE's relative drag semantics, so
    // a press used only to consult the value does not set a new parameter value.
    juce::Slider::mouseDown(event);
}

void G80Knob::mouseUp(const juce::MouseEvent& event)
{
    juce::Slider::mouseUp(event);
    setReadoutVisible(false);
}

bool G80Knob::keyPressed(const juce::KeyPress& key)
{
    const auto wasHandled = juce::Slider::keyPressed(key);
    if (wasHandled)
        setReadoutVisible(true);
    return wasHandled;
}

void G80Knob::focusLost(FocusChangeType cause)
{
    juce::Slider::focusLost(cause);
    setReadoutVisible(false);
}

void G80Knob::valueChanged()
{
    juce::Slider::valueChanged();
    if (readoutVisible)
        repaint();
}

void G80Knob::setReadoutVisible(bool shouldBeVisible)
{
    if (readoutVisible != shouldBeVisible)
    {
        readoutVisible = shouldBeVisible;
        repaint();
    }
}

juce::String G80Knob::formatReadout() const
{
    const auto rounded = std::round(getValue() * 10.0) / 10.0;
    const auto value = std::abs(rounded) < 0.05 ? 0.0 : rounded;
    return juce::String(value > 0.0 ? "+" : "") + juce::String(value, 1) + " dB";
}

G80VuMeter::G80VuMeter(juce::String label, std::atomic<float>& source, std::atomic<uint64_t>& updateCounter,
                       const juce::Image& housingImage)
    : caption(std::move(label)), sourceDb(source), sourceCounter(updateCounter), housing(housingImage)
{
    startTimerHz(30);
}

void G80VuMeter::timerCallback()
{
    const auto counter = sourceCounter.load(std::memory_order_relaxed);
    const auto target = counter != lastCounter ? sourceDb.load(std::memory_order_relaxed) : -72.0f;
    lastCounter = counter;
    const auto response = target > displayedDb ? 0.38f : 0.12f;
    displayedDb += (target - displayedDb) * response;
    repaint();
}

void G80VuMeter::paint(juce::Graphics& g)
{
    drawImageAtLogicalSize(g, housing, getLocalBounds().toFloat());

    // 0 VU remains -18 dBFS RMS, driven by the existing 300 ms detector.
    // This is the useful paper/glass rectangle inside the static housing.
    // Needle, scale and pivot share it, so no point can leave the glass.
    const auto face = juce::Rectangle<float>(18.0f, 27.0f, 114.0f, 54.0f);
    g.setColour(juce::Colour(0xff22251f));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawFittedText(caption, juce::Rectangle<float>(face.getX(), face.getY() + 1.0f, face.getWidth(), 11.0f).toNearestInt(),
                     juce::Justification::centred, 1);

    const auto centre = juce::Point<float>(face.getCentreX(), face.getBottom() - 5.0f);
    constexpr auto radius = 27.0f;
    const auto visualAngle = [] (float vu)
    {
        return juce::degreesToRadians(vitaplug::gainer80::ui::VuScaleMap::angleForVu(vu));
    };

    // The printed arc follows the reference face. The live needle still has
    // the full -30..+6 VU travel supplied by VuScaleMap below.
    for (int tick = -20; tick <= 3; ++tick)
    {
        const auto major = tick % 5 == 0 || tick == 3;
        const auto angle = visualAngle(static_cast<float>(tick));
        const auto outer = polar(centre.x, centre.y, radius, angle);
        const auto inner = polar(centre.x, centre.y, radius - (major ? 5.2f : 3.0f), angle);
        g.setColour(tick >= 0 ? juce::Colour(0xffb42e23) : juce::Colour(0xff403e2f));
        g.drawLine({ inner, outer }, major ? 1.2f : 0.65f);
    }

    const std::array<int, 4> labels { -20, -10, 0, 3 };
    g.setFont(juce::FontOptions(7.4f, juce::Font::bold));
    for (const auto vu : labels)
    {
        const auto point = polar(centre.x, centre.y, 31.0f, visualAngle(static_cast<float>(vu)));
        const auto text = juce::String(vu > 0 ? "+" : "") + juce::String(vu);
        g.setColour(vu >= 0 ? juce::Colour(0xffb02b21) : juce::Colour(0xff3a382b));
        g.drawFittedText(text, juce::Rectangle<float>(point.x - 14.0f, point.y - 6.0f, 28.0f, 12.0f).toNearestInt(),
                         juce::Justification::centred, 1);
    }

    const auto needle = juce::degreesToRadians(vitaplug::gainer80::ui::VuScaleMap::angleForDbfs(displayedDb));
    g.setColour(juce::Colour(0xff7b1715));
    // The pivot and physical needle length are unchanged. Only the shared
    // visual VU calibration changes its angle.
    g.drawLine({ centre, polar(centre.x, centre.y, radius * 0.72f, needle) }, 1.7f);
    g.setColour(juce::Colour(0xff27251f));
    g.fillEllipse(centre.x - 3.8f, centre.y - 3.8f, 7.6f, 7.6f);
}

G80FattyModeSwitch::G80FattyModeSwitch(const juce::Image& baseImage, const juce::Image& upperLeverImage,
                                        const juce::Image& lowerLeverImage)
    : base(baseImage), upperLever(upperLeverImage), lowerLever(lowerLeverImage)
{
    setSliderStyle(juce::Slider::LinearVertical);
    setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    setRange(0.0, 1.0, 1.0);
    setMouseClickGrabsKeyboardFocus(false);
}

void G80FattyModeSwitch::mouseDown(const juce::MouseEvent&)
{
    setValue(getValue() < 0.5 ? 1.0 : 0.0, juce::sendNotificationSync);
}

void G80FattyModeSwitch::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto pivot = juce::Point<float>(bounds.getCentreX(), bounds.getCentreY() + G80Layout::selectorPivotYOffset);
    const auto upper = getValue() >= 0.5;

    g.setColour(cream);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    const auto upperLabelY = G80Layout::selectorUpperLabelCentreY - static_cast<float>(getY());
    const auto lowerLabelY = G80Layout::selectorLowerLabelCentreY - static_cast<float>(getY());
    g.drawFittedText("UPPER", juce::Rectangle<float>(bounds.getX() - 8.0f, upperLabelY - 7.0f, bounds.getWidth() + 16.0f, 14.0f).toNearestInt(),
                     juce::Justification::centred, 1);
    g.drawFittedText("LOWER", juce::Rectangle<float>(bounds.getX() - 8.0f, lowerLabelY - 7.0f, bounds.getWidth() + 16.0f, 14.0f).toNearestInt(),
                     juce::Justification::centred, 1);

    // Approved UI-03 raster layers. The base is always identical; only the
    // pre-rendered lever changes when the host automates fattyMode.
    const auto scale = G80Layout::selectorBaseVisibleDiameter / G80Layout::selectorBaseSourceVisibleDiameter;
    const auto drawLayerAtPivot = [&] (const juce::Image& image, juce::Point<float> sourcePivot)
    {
        const auto size = juce::Point<float>(static_cast<float>(image.getWidth()) * scale,
                                             static_cast<float>(image.getHeight()) * scale);
        const auto topLeft = pivot - sourcePivot * scale;
        drawImageAtLogicalSize(g, image, { topLeft.x, topLeft.y, size.x, size.y });
    };

    drawLayerAtPivot(base, G80Layout::selectorBaseSourcePivot);
    drawLayerAtPivot(upper ? upperLever : lowerLever,
                     upper ? G80Layout::selectorUpperLeverSourcePivot
                           : G80Layout::selectorLowerLeverSourcePivot);
}

VitaPlugGainer80AudioProcessorEditor::VitaPlugGainer80AudioProcessorEditor(VitaPlugGainer80AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      chassisImage(loadBundledPng(BinaryData::chassispaneltypographyv22x_png,
                                  BinaryData::chassispaneltypographyv22x_pngSize)),
      inputVuImage(loadBundledPng(BinaryData::inputvuhousing2x_png, BinaryData::inputvuhousing2x_pngSize)),
      outputVuImage(loadBundledPng(BinaryData::outputvuhousing2x_png, BinaryData::outputvuhousing2x_pngSize)),
      gainKnobImage(loadBundledPng(BinaryData::gainknobmetal2x_png, BinaryData::gainknobmetal2x_pngSize)),
      fattyKnobImage(loadBundledPng(BinaryData::fattyknobmetal2x_png, BinaryData::fattyknobmetal2x_pngSize)),
      switchBaseImage(loadBundledPng(BinaryData::switchbase_png, BinaryData::switchbase_pngSize)),
      switchUpperLeverImage(loadBundledPng(BinaryData::switchleverupper_png, BinaryData::switchleverupper_pngSize)),
      switchLowerLeverImage(loadBundledPng(BinaryData::switchleverlower_png, BinaryData::switchleverlower_pngSize)),
      inputVuSource(audioProcessor.getInputVuSource()), outputVuSource(audioProcessor.getOutputVuSource()),
      vuUpdateCounter(audioProcessor.getVuUpdateCounter()),
      gainAttachment(audioProcessor.parameters, "gain", gainSlider),
      bassAttachment(audioProcessor.parameters, "bass", bassSlider),
      fattyModeAttachment(audioProcessor.parameters, "fattyMode", fattyModeSwitch)
{
    addAndMakeVisible(inputMeter);
    addAndMakeVisible(outputMeter);
    addAndMakeVisible(gainSlider);
    addAndMakeVisible(bassSlider);
    addAndMakeVisible(fattyModeSwitch);
    setSize(G80Layout::editorWidth, G80Layout::editorHeight);
}

void VitaPlugGainer80AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    drawImageAtLogicalSize(g, chassisImage, getLocalBounds().toFloat());
}

void VitaPlugGainer80AudioProcessorEditor::resized()
{
    // Fixed approved 760 x 438 logical map. The component rectangles are
    // centred on the physical control centres, with enough un-clipped room for
    // every polar tick, label and the FATTY 6/top mark.
    inputMeter.setBounds(76, 122, 150, 106);
    outputMeter.setBounds(76, 240, 150, 106);
    gainSlider.setBounds(G80Layout::centredInteractionBounds(G80Layout::gainCentre,
                                                              G80Layout::gainInteractionWidth,
                                                              G80Layout::gainInteractionHeight));
    bassSlider.setBounds(G80Layout::centredInteractionBounds(G80Layout::fattyCentre,
                                                              G80Layout::fattyInteractionWidth,
                                                              G80Layout::fattyInteractionHeight));
    fattyModeSwitch.setBounds(G80Layout::centredInteractionBounds(
        { G80Layout::selectorMechanicalCentre.x,
          G80Layout::selectorMechanicalCentre.y - G80Layout::selectorPivotYOffset },
        G80Layout::selectorInteractionWidth, G80Layout::selectorInteractionHeight));

    // Integer JUCE bounds represent the measured floating-point anchors to
    // within half a logical pixel; retain the assertions as a guard against
    // future layout drift.
    jassert(std::abs(static_cast<float>(gainSlider.getBounds().getCentreX()) - G80Layout::gainCentre.x) <= 0.5f);
    jassert(std::abs(static_cast<float>(gainSlider.getBounds().getCentreY()) - G80Layout::gainCentre.y) <= 0.5f);
    jassert(std::abs(static_cast<float>(bassSlider.getBounds().getCentreX()) - G80Layout::fattyCentre.x) <= 0.5f);
    jassert(std::abs(static_cast<float>(bassSlider.getBounds().getCentreY()) - G80Layout::fattyCentre.y) <= 0.5f);
    jassert(std::abs(static_cast<float>(fattyModeSwitch.getBounds().getCentreX())
                     - G80Layout::selectorMechanicalCentre.x) <= 0.5f);
    jassert(std::abs(static_cast<float>(fattyModeSwitch.getBounds().getCentreY()) + 1.5f
                     - G80Layout::selectorMechanicalCentre.y) <= 0.5f);
}
