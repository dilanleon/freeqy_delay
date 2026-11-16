#include <clap/clap.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <cmath>
#include <chrono>
#include <cstring>
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Stop the CLAP smoke test at the first failed host assertion.
static void check(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

// Report that the minimal test host exposes no optional extensions.
static const void *CLAP_ABI hostExtension(const clap_host_t *, const char *)
{
    return nullptr;
}
// Accept a host request without scheduling any extra work in this smoke test.
static void CLAP_ABI hostRequest(const clap_host_t *) {}
// Present an empty input-event list to the plug-in.
static uint32_t CLAP_ABI eventCount(const clap_input_events_t *)
{
    return 0;
}
// Return no event because the test host provides no input events.
static const clap_event_header_t *CLAP_ABI eventGet(const clap_input_events_t *, uint32_t)
{
    return nullptr;
}
// Accept output events without retaining them in this audio-only smoke test.
static bool CLAP_ABI eventPush(const clap_output_events_t *, const clap_event_header_t *)
{
    return true;
}
// Append serialized CLAP state bytes to the test-owned buffer.
static int64_t CLAP_ABI streamWrite(const clap_ostream_t *stream, const void *data, uint64_t size)
{
    auto &bytes = *static_cast<std::vector<uint8_t> *>(stream->ctx);
    const auto *first = static_cast<const uint8_t *>(data);
    bytes.insert(bytes.end(), first, first + size);
    return int64_t(size);
}
struct Reader
{
    const std::vector<uint8_t> &bytes;
    size_t offset = 0;
};
// Read serialized state bytes from the test-owned buffer.
static int64_t CLAP_ABI streamRead(const clap_istream_t *stream, void *data, uint64_t size)
{
    auto &reader = *static_cast<Reader *>(stream->ctx);
    const auto available = reader.bytes.size() - reader.offset;
    const auto count = std::min<size_t>(size_t(size), available);
    std::memcpy(data, reader.bytes.data() + reader.offset, count);
    reader.offset += count;
    return int64_t(count);
}

struct StagedBundle
{
    // Copy the bundle outside the workspace before loading it through dyld.
    explicit StagedBundle(const std::filesystem::path &source)
    {
        const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
        root = std::filesystem::temp_directory_path() /
               ("freeqy-clap-check-" + std::to_string(nonce));
        bundle = root / source.filename();
        std::filesystem::create_directories(root);
        std::filesystem::copy(source, bundle,
                              std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::copy_symlinks);
    }

    // Remove the temporary host-loading copy after the test.
    ~StagedBundle()
    {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }

    std::filesystem::path root;
    std::filesystem::path bundle;
};

// Scan, instantiate, process, and round-trip state through native CLAP.
int main(int argc, char **argv)
{
    try
    {
        juce::ScopedJuceInitialiser_GUI juceInit;
        auto progress = [](const char *message)
        {
            std::cerr << message << '\n';
            std::cerr.flush();
        };
        check(argc == 2, "expected CLAP bundle path");
        // dyld on macOS 26 may indefinitely validate signed modules opened directly
        // from a sync-managed Documents workspace. Real hosts use installed bundles,
        // so stage an identical copy in the system temporary directory for this smoke test.
        StagedBundle staged(std::filesystem::absolute(argv[1]));
        const std::string bundle = staged.bundle.string();
        const auto executable = bundle + "/Contents/MacOS/FREEQY DELAY";
        void *library = dlopen(executable.c_str(), RTLD_NOW | RTLD_LOCAL);
        const auto *loadError = library == nullptr ? dlerror() : nullptr;
        check(library != nullptr, loadError != nullptr ? loadError : "could not load CLAP");
        progress("loaded");
        const auto *entry = static_cast<const clap_plugin_entry_t *>(dlsym(library, "clap_entry"));
        check(entry && clap_version_is_compatible(entry->clap_version), "invalid clap_entry");
        check(entry->init(bundle.c_str()), "CLAP entry initialization failed");
        progress("entry initialized");
        const auto *factory = static_cast<const clap_plugin_factory_t *>(
            entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
        check(factory && factory->get_plugin_count(factory) == 1, "CLAP factory mismatch");
        const auto *descriptor = factory->get_plugin_descriptor(factory, 0);
        check(descriptor && std::string(descriptor->id) == "com.stationsound.freeqydelay" &&
                  std::string(descriptor->name) == "FREEQY DELAY" &&
                  std::string(descriptor->vendor) == "Station Sound",
              "CLAP descriptor mismatch");

        const clap_host_t host{CLAP_VERSION,
                               nullptr,
                               "FREEQY DELAY CLAP Check",
                               "Station Sound",
                               "",
                               "1",
                               hostExtension,
                               hostRequest,
                               hostRequest,
                               hostRequest};
        const auto *plugin = factory->create_plugin(factory, &host, descriptor->id);
        check(plugin && plugin->init(plugin), "CLAP instance initialization failed");
        progress("instance initialized");
        const auto *ports = static_cast<const clap_plugin_audio_ports_t *>(
            plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS));
        clap_audio_port_info_t port{};
        check(ports && ports->count(plugin, true) == 1 && ports->count(plugin, false) == 1 &&
                  ports->get(plugin, 0, true, &port) && port.channel_count == 2 &&
                  ports->get(plugin, 0, false, &port) && port.channel_count == 2,
              "CLAP stereo ports mismatch");

        const auto *state = static_cast<const clap_plugin_state_t *>(
            plugin->get_extension(plugin, CLAP_EXT_STATE));
        std::vector<uint8_t> stateBytes;
        const clap_ostream_t writer{&stateBytes, streamWrite};
        check(state && state->save(plugin, &writer) && !stateBytes.empty(), "CLAP state save failed");
        Reader reader{stateBytes};
        const clap_istream_t input{&reader, streamRead};
        check(state->load(plugin, &input), "CLAP state load failed");
        progress("state recalled");

        check(plugin->activate(plugin, 48000, 1, 512), "CLAP activation failed");
        check(plugin->start_processing(plugin), "CLAP processing start failed");
        progress("processing started");
        std::array<std::array<float, 512>, 2> source{}, result{};
        for (int n = 0; n < 512; ++n)
        {
            source[0][size_t(n)] = float(.1 * std::sin(6.283185307179586 * 440 * n / 48000));
            source[1][size_t(n)] = source[0][size_t(n)];
        }
        float *sourceChannels[]{source[0].data(), source[1].data()};
        float *resultChannels[]{result[0].data(), result[1].data()};
        clap_audio_buffer_t sourceBuffer{sourceChannels, nullptr, 2, 0, 0};
        clap_audio_buffer_t resultBuffer{resultChannels, nullptr, 2, 0, 0};
        const clap_input_events_t inEvents{nullptr, eventCount, eventGet};
        const clap_output_events_t outEvents{nullptr, eventPush};
        const clap_process_t process{0, 512, nullptr, &sourceBuffer, &resultBuffer, 1, 1,
                                     &inEvents, &outEvents};
        check(plugin->process(plugin, &process) != CLAP_PROCESS_ERROR, "CLAP process failed");
        progress("processed");
        plugin->stop_processing(plugin);
        plugin->deactivate(plugin);
        plugin->destroy(plugin);
        progress("destroyed");
        entry->deinit();
        progress("entry deinitialized");
        dlclose(library);
        std::cout << "CLAP scanned, instantiated, processed and recalled state\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
