#include "PresetFiles.h"
#include <cmath>
#include <cstring>

namespace spectral::presets
{
namespace
{
using BankPoints = std::array<std::array<PointCurve, 3>, 2>;
constexpr int64_t maximumPresetBytes = 1024 * 1024;

// Read a bounded local preset file before parsing its tree.
juce::Result readTree(const juce::File &file, juce::ValueTree &tree)
{
    if (!file.existsAsFile() || file.getSize() <= 0 || file.getSize() > maximumPresetBytes)
        return juce::Result::fail("Preset file is missing or has an unexpected size.");
    juce::MemoryBlock bytes;
    if (!file.loadFileAsData(bytes))
        return juce::Result::fail("Could not read the preset file.");
    tree = juce::ValueTree::readFromData(bytes.getData(), bytes.getSize());
    return tree.isValid() ? juce::Result::ok()
                          : juce::Result::fail("Preset file is not valid.");
}

// Store a serialized tree through JUCE's temporary-file replacement helper.
juce::Result writeTree(const juce::File &file, const juce::ValueTree &tree)
{
    juce::MemoryOutputStream bytes;
    tree.writeToStream(bytes);
    return file.replaceWithData(bytes.getData(), bytes.getDataSize())
               ? juce::Result::ok() : juce::Result::fail("Could not save the preset file.");
}

// Confirm that the bank's saved grid contains finite normalized values.
bool validCurves(const Curves &curves)
{
    for (const auto &channel : curves.values)
        for (const auto &map : channel)
            for (float value : map)
                if (!std::isfinite(value) || value < 0 || value > 1)
                    return false;
    return true;
}

// Confirm that editable anchors cannot produce invalid point interpolation.
bool validPoints(const BankPoints &points)
{
    for (const auto &channel : points)
        for (const auto &curve : channel)
        {
            if (curve.count != 0 && (curve.count < 2 || curve.count > PointCurve::capacity))
                return false;
            float previous = -1;
            for (int i = 0; i < curve.count; ++i)
            {
                const auto &point = curve.points[size_t(i)];
                if (!std::isfinite(point.x) || !std::isfinite(point.value) ||
                    !std::isfinite(point.shape) || point.x < 0 || point.x > 1 ||
                    point.x <= previous || point.value < 0 || point.value > 1 ||
                    point.shape < -1 || point.shape > 1)
                    return false;
                previous = point.x;
            }
        }
    return true;
}
}

// Write the complete version 4 host state to a plug-in preset file.
juce::Result savePlugin(Processor &processor, const juce::File &file)
{
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    return file.replaceWithData(state.getData(), state.getSize())
               ? juce::Result::ok() : juce::Result::fail("Could not save the plug-in preset.");
}

// Validate and restore a complete plug-in preset file.
juce::Result loadPlugin(Processor &processor, const juce::File &file)
{
    juce::ValueTree tree;
    if (auto result = readTree(file, tree); result.failed()) return result;
    if (!tree.hasType("SpectralDelay") || int(tree.getProperty("version", 0)) != 4)
        return juce::Result::fail("This is not a supported plug-in preset.");
    const auto bankProperty = tree.getProperty("banks");
    const auto pointProperty = tree.getProperty("pointCurves");
    const auto *banks = bankProperty.getBinaryData();
    const auto *points = pointProperty.getBinaryData();
    if (!banks || banks->getSize() != sizeof(CurveBanks{}.banks) ||
        !points || points->getSize() != sizeof(PointCurves))
        return juce::Result::fail("Plug-in preset data is incomplete.");
    auto numeric = [](const juce::var &value)
    { return value.isInt() || value.isInt64() || value.isDouble() || value.isBool(); };
    for (auto [name, low, high] : {std::tuple{"bankCount", 1, 4}, {"selectedBank", 0, 3},
                                   {"curve", 0, 2}, {"channel", 0, 1},
                                   {"spectrogramSpeed", 0, 3}})
    {
        const auto value = tree.getProperty(name);
        if (!numeric(value) || !std::isfinite(double(value)) || double(value) < low ||
            double(value) > high || double(value) != std::floor(double(value)))
            return juce::Result::fail("Plug-in preset contains invalid editor settings.");
    }
    if (int(tree.getProperty("selectedBank")) >= int(tree.getProperty("bankCount")))
        return juce::Result::fail("Plug-in preset selects an inactive bank.");
    if (tree.hasProperty("pointSampleRate"))
    {
        const auto rate = tree.getProperty("pointSampleRate");
        if (!numeric(rate) || !std::isfinite(double(rate)) || double(rate) <= 0)
            return juce::Result::fail("Plug-in preset contains an invalid point sample rate.");
    }
    CurveBanks decodedBanks;
    PointCurves decodedPoints;
    std::memcpy(decodedBanks.banks.data(), banks->getData(), sizeof(decodedBanks.banks));
    std::memcpy(&decodedPoints, points->getData(), sizeof(decodedPoints));
    for (const auto &bank : decodedBanks.banks)
        if (!validCurves(bank)) return juce::Result::fail("Plug-in preset contains invalid curves.");
    for (const auto &bank : decodedPoints.banks)
        if (!validPoints(bank)) return juce::Result::fail("Plug-in preset contains invalid points.");
    for (auto *parameter : processor.getParameters())
    {
        auto *ranged = dynamic_cast<juce::RangedAudioParameter *>(parameter);
        if (!ranged) continue;
        int matches = 0;
        for (auto child : tree)
            if (child.hasType("PARAM") && child.getProperty("id").toString() == ranged->paramID)
            {
                ++matches;
                const auto stored = child.getProperty("value");
                if (!numeric(stored)) return juce::Result::fail("Plug-in preset contains invalid parameters.");
                const float value = float(stored);
                const auto &range = ranged->getNormalisableRange();
                if (!std::isfinite(value) || value < range.start || value > range.end)
                    return juce::Result::fail("Plug-in preset contains invalid parameters.");
            }
        if (matches != 1) return juce::Result::fail("Plug-in preset parameters are incomplete.");
    }
    juce::MemoryBlock state;
    juce::MemoryOutputStream output(state, false);
    tree.writeToStream(output);
    processor.setStateInformation(state.getData(), int(state.getSize()));
    return juce::Result::ok();
}

// Write only the selected bank's curves and point editor data.
juce::Result saveBank(Processor &processor, const juce::File &file)
{
    const auto index = size_t(juce::jlimit(0, 3, processor.selectedBank.load()));
    const auto curves = processor.getBanks().banks[index];
    const auto points = processor.getPointCurves().banks[index];
    juce::ValueTree tree("FreeqyBankPreset");
    tree.setProperty("version", 1, nullptr);
    tree.setProperty("curves", juce::MemoryBlock(&curves, sizeof(curves)), nullptr);
    tree.setProperty("points", juce::MemoryBlock(&points, sizeof(points)), nullptr);
    tree.setProperty("pointSampleRate", processor.pointSampleRate(), nullptr);
    return writeTree(file, tree);
}

// Validate a bank file, then replace only the currently selected bank.
juce::Result loadBank(Processor &processor, const juce::File &file)
{
    juce::ValueTree tree;
    if (auto result = readTree(file, tree); result.failed()) return result;
    if (!tree.hasType("FreeqyBankPreset") || int(tree.getProperty("version", 0)) != 1)
        return juce::Result::fail("This is not a supported selected-bank preset.");
    const auto curveProperty = tree.getProperty("curves");
    const auto pointProperty = tree.getProperty("points");
    const auto *curveBytes = curveProperty.getBinaryData();
    const auto *pointBytes = pointProperty.getBinaryData();
    if (!curveBytes || curveBytes->getSize() != sizeof(Curves) ||
        !pointBytes || pointBytes->getSize() != sizeof(BankPoints))
        return juce::Result::fail("Selected-bank preset data is incomplete.");
    Curves curves;
    BankPoints points;
    std::memcpy(&curves, curveBytes->getData(), sizeof(curves));
    std::memcpy(&points, pointBytes->getData(), sizeof(points));
    if (!validCurves(curves) || !validPoints(points))
        return juce::Result::fail("Selected-bank preset contains invalid values.");
    const auto index = size_t(juce::jlimit(0, 3, processor.selectedBank.load()));
    auto banks = processor.getBanks();
    auto allPoints = processor.getPointCurves();
    banks.banks[index] = curves;
    const double savedRate = double(tree.getProperty("pointSampleRate", 48000.0));
    allPoints.banks[index] = std::isfinite(savedRate) && savedRate == processor.pointSampleRate()
                                 ? points : BankPoints{};
    processor.updateBanks(banks);
    processor.setPointCurves(allPoints);
    return juce::Result::ok();
}
}
