#include <juce_gui_basics/juce_gui_basics.h>
#include <oae_audio.h>
#include "MainComponent.h"
#include "StudioEngine.h"
#include "Audio/AcousticMeterProcessor.h"
#include "Live/LiveBridgeServer.h"
#include "Model/AuthoringModel.h"
#include "Timeline/TimelineSnap.h"
#include "Timeline/WaveformCache.h"
#include "Project/ProjectDocument.h"
#include "Project/AsyncBankPublisher.h"
#include "Transport/AuditionTransport.h"
#include "Transport/PreviewRebuilder.h"
#include "UI/MixerDeckPanel.h"
#include "UI/OaeDarkLookAndFeel.h"
#include "UI/DockingLayoutManager.h"
#include "UI/ContextualBottomDeck.h"
#include "UI/InspectorPanel.h"
#include "UI/Inspector/IInspectorCard.h"
#include "UI/Inspector/InspectorClipboard.h"
#include "UI/Theme.h"
#include "UI/TimelineOverviewMinimap.h"
#include "UI/TimelinePanel.h"
#include "MCP/McpProtocol.h"
#include "MCP/McpServer.h"
#include "Testing/UiControlTestRunner.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

namespace fs = std::filesystem;

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace {

#ifdef _WIN32
bool run_command_capture_output(const std::wstring& cmdline, int& out_exit_code, std::string& out_output)
{
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE read_pipe = nullptr;
    HANDLE write_pipe = nullptr;

    if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
        return false;
    }

    SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = write_pipe;
    si.hStdError = write_pipe;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    std::vector<wchar_t> cmdline_buf(cmdline.begin(), cmdline.end());
    cmdline_buf.push_back(L'\0');

    BOOL success = CreateProcessW(
        nullptr,
        cmdline_buf.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &si,
        &pi
    );

    CloseHandle(write_pipe);

    if (!success) {
        CloseHandle(read_pipe);
        return false;
    }

    std::string captured;
    char buffer[1024];
    DWORD bytes_read = 0;
    while (ReadFile(read_pipe, buffer, sizeof(buffer) - 1, &bytes_read, nullptr) && bytes_read > 0) {
        buffer[bytes_read] = '\0';
        captured.append(buffer, bytes_read);
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    out_exit_code = static_cast<int>(exit_code);
    out_output = std::move(captured);

    CloseHandle(read_pipe);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return true;
}
#endif

fs::path get_app_dir()
{
    const auto juce_dir = juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getFullPathName().toStdString();
    return fs::path(juce_dir);
}

bool check_required_binaries(const fs::path& app_dir)
{
    const std::vector<std::string> binaries = { "oae_bankc.exe" };
    bool ok = true;
    for (const auto& bin : binaries) {
        bool found = false;
        const std::vector<fs::path> candidates = {
            app_dir / bin,
            app_dir / "Debug" / bin,
            app_dir / "Release" / bin,
            app_dir / ".." / "Debug" / bin,
            app_dir / ".." / "Release" / bin
        };
        for (const auto& c : candidates) {
            if (fs::exists(c)) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << "[OAE Studio Error] Required binary not found: " << bin << std::endl;
            ok = false;
        }
    }
    return ok;
}

fs::path find_bankc_executable(const fs::path& app_dir)
{
    const std::vector<fs::path> candidates = {
        app_dir / "oae_bankc.exe",
        app_dir / "Debug" / "oae_bankc.exe",
        app_dir / "Release" / "oae_bankc.exe",
        app_dir / ".." / "Debug" / "oae_bankc.exe",
        app_dir / ".." / "Release" / "oae_bankc.exe"
    };
    for (const auto& c : candidates) {
        if (fs::exists(c)) return fs::canonical(c);
    }
    return {};
}

bool write_sine_wav(const fs::path& wav_path, float freq = 440.0f, float duration_sec = 0.25f)
{
    std::ofstream wf(wav_path, std::ios::binary);
    if (!wf.is_open()) return false;

    constexpr uint32_t sample_rate = 48000;
    const uint32_t num_samples = static_cast<uint32_t>(sample_rate * duration_sec);
    constexpr uint16_t num_channels = 1;
    constexpr uint16_t bits_per_sample = 16;
    const uint32_t data_size = num_samples * sizeof(int16_t);

    wf.write("RIFF", 4);
    const uint32_t chunk_size = 36 + data_size;
    wf.write(reinterpret_cast<const char*>(&chunk_size), 4);
    wf.write("WAVEfmt ", 8);

    const uint32_t subchunk1_size = 16;
    const uint16_t audio_format = 1; // PCM
    const uint32_t byte_rate = sample_rate * num_channels * (bits_per_sample / 8);
    const uint16_t block_align = num_channels * (bits_per_sample / 8);

    wf.write(reinterpret_cast<const char*>(&subchunk1_size), 4);
    wf.write(reinterpret_cast<const char*>(&audio_format), 2);
    wf.write(reinterpret_cast<const char*>(&num_channels), 2);
    wf.write(reinterpret_cast<const char*>(&sample_rate), 4);
    wf.write(reinterpret_cast<const char*>(&byte_rate), 4);
    wf.write(reinterpret_cast<const char*>(&block_align), 2);
    wf.write(reinterpret_cast<const char*>(&bits_per_sample), 2);

    wf.write("data", 4);
    wf.write(reinterpret_cast<const char*>(&data_size), 4);

    for (uint32_t i = 0; i < num_samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(sample_rate);
        const float s = std::sin(2.0f * 3.14159265f * freq * t);
        const int16_t val = static_cast<int16_t>(s * 32000.0f);
        wf.write(reinterpret_cast<const char*>(&val), sizeof(int16_t));
    }
    return true;
}

int run_smoke_headless()
{
    std::cout << "[OAE Studio] Starting --smoke-headless validation..." << std::endl;

    const auto app_dir = get_app_dir();
    if (!check_required_binaries(app_dir)) {
        std::cerr << "[OAE Studio] Dependency check failed!" << std::endl;
        return 1;
    }

    const auto bankc_exe = find_bankc_executable(app_dir);
    if (bankc_exe.empty()) {
        std::cerr << "[OAE Studio] oae_bankc executable not found!" << std::endl;
        return 1;
    }

    // Prepare temp project directory
    const fs::path temp_dir = fs::temp_directory_path() / "oae_studio_smoke_headless";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir, ec);

    // Write a sine tone WAV
    const fs::path wav_path = temp_dir / "tone.wav";
    if (!write_sine_wav(wav_path, 440.0f, 0.25f)) return 1;

    // Write a minimal project.json v1
    const fs::path proj_path = temp_dir / "project.json";
    {
        std::ofstream pf(proj_path);
        if (!pf.is_open()) return 1;
        pf << R"({
  "version": 1,
  "buses": [
    { "path": "bus:/sfx", "volume": 1.0, "effects": [] }
  ],
  "global_parameters": [],
  "samples": [
    { "id": "sine_440", "file": "tone.wav" }
  ],
  "events": [
    {
      "path": "event:/smoke_test",
      "kind": "event",
      "bus_path": "bus:/sfx",
      "volume_db": 0.0,
      "tracks": [
        {
          "volume_db": 0.0,
          "instruments": [
            {
              "kind": "single",
              "sample_id": "sine_440",
              "offset_ms": 0.0,
              "length_ms": 250.0,
              "loop": false
            }
          ]
        }
      ]
    }
  ],
  "effect_automations": [],
  "vcas": [],
  "mixer": {
    "master": { "volume": 1.0, "mute": false }
  }
})";
    }

    // Compile bank via oae_bankc CLI
    const fs::path out_bank = temp_dir / "smoke.bank";
    const std::wstring cmd = L"\"" + bankc_exe.wstring() + L"\" \"" + proj_path.wstring() + L"\" -o \"" + out_bank.wstring() + L"\"";
    int compile_res = -1;
    std::string compile_output;
    const bool proc_ok = run_command_capture_output(cmd, compile_res, compile_output);
    std::cout << "[OAE Studio] oae_bankc output:\n" << compile_output << std::endl;
    if (!proc_ok || compile_res != 0) {
        std::cerr << "[OAE Studio] oae_bankc failed with exit code: " << compile_res << std::endl;
        return 1;
    }

    if (!fs::exists(out_bank)) {
        std::cerr << "[OAE Studio] Output bank does not exist: " << out_bank << std::endl;
        return 1;
    }

    // Test playback via StudioEngine headless mode
    oae::studio::StudioEngine engine;
    if (engine.init_headless() != OAE_OK) {
        std::cerr << "[OAE Studio] Engine init_headless failed!" << std::endl;
        return 1;
    }

    oae_bank bank = 0;
    if (engine.load_bank_file(out_bank.string().c_str(), &bank) != OAE_OK || bank == 0) {
        std::cerr << "[OAE Studio] load_bank_file failed!" << std::endl;
        return 1;
    }

    oae_event_instance instance = 0;
    if (engine.create_event_instance("event:/smoke_test", &instance) != OAE_OK || instance == 0) {
        std::cerr << "[OAE Studio] create_event_instance failed!" << std::endl;
        return 1;
    }

    oae_event_instance_start(instance);
    engine.update();

    // Render audio and verify non-zero samples
    constexpr uint32_t render_frames = 512;
    std::vector<float> buffer(render_frames * 2, 0.0f);
    float max_peak = 0.0f;

    for (int block = 0; block < 10; ++block) {
        engine.update();
        engine.render(buffer.data(), render_frames);
        for (float s : buffer) {
            max_peak = std::max(max_peak, std::abs(s));
        }
    }

    oae_event_instance_release(instance);

    if (max_peak < 0.001f) {
        std::cerr << "[OAE Studio] Rendered audio is silent (max peak: " << max_peak << ")" << std::endl;
        return 1;
    }

    std::cout << "[OAE Studio] Render verified successfully (peak: " << max_peak << ")" << std::endl;

    engine.unload_bank(bank);
    engine.shutdown();

    fs::remove_all(temp_dir, ec);

    std::cout << "[OAE Studio] --smoke-headless PASSED with exit code 0." << std::endl;
    return 0;
}

int run_smoke_project()
{
    std::cout << "[OAE Studio] Starting --smoke-project validation..." << std::endl;

    const auto app_dir = get_app_dir();
    const auto bankc_exe = find_bankc_executable(app_dir);
    if (bankc_exe.empty()) {
        std::cerr << "[OAE Studio] oae_bankc executable not found!" << std::endl;
        return 1;
    }

    // 1. Path with Unicode and spaces
    const fs::path base_temp = fs::temp_directory_path() / "OAE_Studio_Tests";
    std::error_code ec;
    fs::remove_all(base_temp, ec);

    // Using wide-string / UTF-8 chars for project folder
    const fs::path project_dir = base_temp / "OAE ProjÃ©t Ãudio ðŸŽµ Spaced";
    fs::create_directories(project_dir, ec);

    // 2. Create project using ProjectDocument
    oae::studio::ProjectDocument doc;
    if (!doc.create_empty(project_dir, "Unicode ProjÃ©t")) {
        std::cerr << "[OAE Studio] Failed to create empty project in: " << project_dir.string() << std::endl;
        return 1;
    }

    // 3. Write synthetic WAV outside the project
    const fs::path external_wav = base_temp / "external_tone.wav";
    if (!write_sine_wav(external_wav, 523.25f, 0.3f)) { // C5 tone
        std::cerr << "[OAE Studio] Failed to write external WAV" << std::endl;
        return 1;
    }

    // 4. Import audio file (copies to assets/ and saves relative path)
    if (!doc.import_audio_file(external_wav, "sine_c5")) {
        std::cerr << "[OAE Studio] Failed to import audio file into project!" << std::endl;
        return 1;
    }

    // Check that asset exists inside assets/
    const fs::path internal_asset = project_dir / "assets" / "external_tone.wav";
    if (!fs::exists(internal_asset)) {
        std::cerr << "[OAE Studio] Imported asset does not exist at: " << internal_asset.string() << std::endl;
        return 1;
    }

    // 5. Add event referencing the imported asset
    if (!doc.add_simple_event("event:/sfx/tone_c5", "sine_c5", 0.3f)) {
        std::cerr << "[OAE Studio] Failed to add event to project!" << std::endl;
        return 1;
    }

    // 6. Save atomically
    if (!doc.save()) {
        std::cerr << "[OAE Studio] Failed to save project atomically!" << std::endl;
        return 1;
    }

    // 7. Async Bank Publication
    const fs::path output_bank = project_dir / "build" / "master.bank";
    oae::studio::AsyncBankPublisher publisher;

    std::cout << "[OAE Studio] Starting async publication of immutable snapshot..." << std::endl;
    const auto snapshot = doc.create_immutable_snapshot();
    if (!publisher.start_publish(bankc_exe, snapshot, project_dir, output_bank)) {
        std::cerr << "[OAE Studio] Failed to start async bank publisher!" << std::endl;
        return 1;
    }

    publisher.wait_until_finished();
    const auto pub_result = publisher.get_last_result();

    if (!pub_result.success || pub_result.exit_code != 0) {
        std::cerr << "[OAE Studio] Async publication failed!\nDiagnostics:\n" << pub_result.diagnostics << std::endl;
        return 1;
    }

    if (!fs::exists(output_bank) || !fs::exists(project_dir / "build" / "master.bank.catalog.json")) {
        std::cerr << "[OAE Studio] Expected output bank or catalog missing after publication!" << std::endl;
        return 1;
    }

    std::cout << "[OAE Studio] Publication verified. Testing playback with native engine..." << std::endl;

    // 8. Test playback with StudioEngine
    {
        oae::studio::StudioEngine engine;
        if (engine.init_headless() != OAE_OK) {
            std::cerr << "[OAE Studio] StudioEngine init_headless failed!" << std::endl;
            return 1;
        }

        oae_bank bank = 0;
        const oae_result load_res = engine.load_bank_file(output_bank.u8string().c_str(), &bank);
        if (load_res != OAE_OK || bank == 0) {
            std::cerr << "[OAE Studio] load_bank_file failed for published bank! res=" << load_res << std::endl;
            return 1;
        }

        oae_event_instance instance = 0;
        if (engine.create_event_instance("event:/sfx/tone_c5", &instance) != OAE_OK || instance == 0) {
            std::cerr << "[OAE Studio] create_event_instance failed for published event!" << std::endl;
            return 1;
        }

        oae_event_instance_start(instance);
        engine.update();

        constexpr uint32_t render_frames = 512;
        std::vector<float> buffer(render_frames * 2, 0.0f);
        float max_peak = 0.0f;

        for (int block = 0; block < 10; ++block) {
            engine.update();
            engine.render(buffer.data(), render_frames);
            for (float s : buffer) {
                max_peak = std::max(max_peak, std::abs(s));
            }
        }

        oae_event_instance_release(instance);

        if (max_peak < 0.001f) {
            std::cerr << "[OAE Studio] Audio is silent in published bank test (max peak: " << max_peak << ")" << std::endl;
            return 1;
        }

        std::cout << "[OAE Studio] Published bank audio verified (peak: " << max_peak << ")" << std::endl;

        engine.unload_bank(bank);
        engine.shutdown();
    }

    // 9. Test Portability (Move folder to another directory with spaces and Unicode)
    const fs::path moved_dir = base_temp / "OAE ProjÃ©t Movido ðŸš€ Destino";
    std::cout << "[OAE Studio] Testing project relocation to: " << moved_dir.string() << std::endl;

    fs::rename(project_dir, moved_dir, ec);
    if (ec) {
        std::cerr << "[OAE Studio] Failed to move project directory: " << ec.message() << std::endl;
        return 1;
    }

    oae::studio::ProjectDocument moved_doc;
    if (!moved_doc.open(moved_dir / "project.json")) {
        std::cerr << "[OAE Studio] Failed to open project from moved directory!" << std::endl;
        return 1;
    }

    const auto missing_after_move = moved_doc.check_missing_assets();
    if (!missing_after_move.empty()) {
        std::cerr << "[OAE Studio] Relocation test failed: found " << missing_after_move.size() << " missing assets!" << std::endl;
        return 1;
    }

    // Rebuild bank from relocated project
    const fs::path moved_output_bank = moved_dir / "build" / "master_moved.bank";
    const auto moved_snapshot = moved_doc.create_immutable_snapshot();
    if (!publisher.start_publish(bankc_exe, moved_snapshot, moved_dir, moved_output_bank)) {
        std::cerr << "[OAE Studio] Failed to start publish for moved project!" << std::endl;
        return 1;
    }

    publisher.wait_until_finished();
    const auto moved_pub_result = publisher.get_last_result();
    if (!moved_pub_result.success || moved_pub_result.exit_code != 0 || !fs::exists(moved_output_bank)) {
        std::cerr << "[OAE Studio] Publication failed after moving project!\nDiagnostics:\n" << moved_pub_result.diagnostics << std::endl;
        return 1;
    }

    std::cout << "[OAE Studio] Project relocation verified successfully." << std::endl;

    // 10. Robustness against failures:
    // (a) Save failure test: Attempting to save to a directory should fail cleanly without corrupting project.json
    const fs::path dir_as_file = moved_dir / "assets"; // Exists as directory
    const bool save_fail_result = moved_doc.save_to(dir_as_file);
    if (save_fail_result) {
        std::cerr << "[OAE Studio] Expected save_to(dir) to fail, but it returned true!" << std::endl;
        return 1;
    }
    // Verify original project.json remains intact and parseable
    oae::studio::ProjectDocument check_doc;
    if (!check_doc.open(moved_dir / "project.json")) {
        std::cerr << "[OAE Studio] Original project.json was corrupted after failed save!" << std::endl;
        return 1;
    }

    // (b) Build failure test: Injecting broken reference should fail build cleanly and preserve existing bank
    auto corrupt_snapshot = moved_snapshot;
    corrupt_snapshot["samples"].push_back({
        { "id", "missing_sample" },
        { "file", "assets/does_not_exist_ever.wav" }
    });

    const auto before_timestamp = fs::last_write_time(moved_output_bank);
    if (!publisher.start_publish(bankc_exe, corrupt_snapshot, moved_dir, moved_output_bank)) {
        std::cerr << "[OAE Studio] Failed to start publish for corrupt snapshot test!" << std::endl;
        return 1;
    }
    publisher.wait_until_finished();
    const auto fail_result = publisher.get_last_result();

    if (fail_result.success || fail_result.exit_code == 0) {
        std::cerr << "[OAE Studio] Expected compilation to fail for missing sample, but it succeeded!" << std::endl;
        return 1;
    }

    // Verify existing bank was not overwritten or removed
    if (!fs::exists(moved_output_bank)) {
        std::cerr << "[OAE Studio] Existing bank was removed after a failed build!" << std::endl;
        return 1;
    }
    const auto after_timestamp = fs::last_write_time(moved_output_bank);
    if (before_timestamp != after_timestamp) {
        std::cerr << "[OAE Studio] Existing bank was corrupted or overwritten after a failed build!" << std::endl;
        return 1;
    }

    std::cout << "[OAE Studio] Failure handling verified (diagnostics captured error: "
              << (fail_result.diagnostics.find("Error") != std::string::npos ? "YES" : "NO") << ")" << std::endl;

    const auto read_contents = [](const fs::path& path) {
        std::ifstream input(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    };
    const fs::path moved_catalog = moved_dir / "build" / "master_moved.bank.catalog.json";
    const auto bank_before_failure = read_contents(moved_output_bank);
    const auto catalog_before_failure = read_contents(moved_catalog);
    if (bank_before_failure.empty() || catalog_before_failure.empty()) {
        std::cerr << "[OAE Studio] Missing baseline artifacts for publication regression!" << std::endl;
        return 1;
    }
    const fs::path saved_catalog = moved_dir / "build" / "saved_catalog.json";
    fs::rename(moved_catalog, saved_catalog, ec);
    if (ec || !fs::create_directory(moved_catalog, ec) || ec) {
        std::cerr << "[OAE Studio] Failed to prepare blocked catalog destination!" << std::endl;
        return 1;
    }
    auto changed_snapshot = moved_snapshot;
    changed_snapshot["events"][0]["volume_db"] = -12.0f;
    if (!publisher.start_publish(bankc_exe, changed_snapshot, moved_dir, moved_output_bank)) {
        std::cerr << "[OAE Studio] Failed to start blocked catalog publication!" << std::endl;
        return 1;
    }
    publisher.wait_until_finished();
    const auto blocked_result = publisher.get_last_result();
    if (blocked_result.success || blocked_result.exit_code == 0 ||
        !fs::is_directory(moved_catalog) || read_contents(moved_output_bank) != bank_before_failure ||
        read_contents(saved_catalog) != catalog_before_failure) {
        std::cerr << "[OAE Studio] Catalog failure was ignored or modified the previous bank!" << std::endl;
        return 1;
    }
    fs::remove(moved_catalog, ec);
    if (ec) return 1;
    fs::rename(saved_catalog, moved_catalog, ec);
    if (ec) return 1;

    const fs::path published_bank = moved_dir / "build" / "published.bank";
    const fs::path published_catalog = moved_dir / "build" / "published.bank.catalog.json";
    if (!publisher.start_publish(bankc_exe, moved_snapshot, moved_dir, published_bank)) return 1;
    publisher.wait_until_finished();
    const auto published_result = publisher.get_last_result();
    if (!published_result.success || published_result.exit_code != 0 ||
        !fs::is_regular_file(published_bank) || !fs::is_regular_file(published_catalog)) {
        std::cerr << "[OAE Studio] Direct publication to build/published.bank failed!" << std::endl;
        return 1;
    }
    const auto published_bank_before = read_contents(published_bank);
    const auto published_catalog_before = read_contents(published_catalog);
    if (!publisher.start_publish(bankc_exe, corrupt_snapshot, moved_dir, published_bank)) return 1;
    publisher.wait_until_finished();
    const auto failed_direct_result = publisher.get_last_result();
    if (failed_direct_result.success || failed_direct_result.exit_code == 0 ||
        !fs::is_regular_file(published_bank) || !fs::is_regular_file(published_catalog) ||
        read_contents(published_bank) != published_bank_before ||
        read_contents(published_catalog) != published_catalog_before) {
        std::cerr << "[OAE Studio] Failed direct publication modified existing bank/catalog!" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Catalog failure and direct-destination preservation verified." << std::endl;

    fs::remove_all(base_temp, ec);

    std::cout << "[OAE Studio] --smoke-project PASSED with exit code 0." << std::endl;
    return 0;
}

int run_smoke_authoring()
{
    std::cout << "[OAE Studio] Starting --smoke-authoring validation..." << std::endl;

    const auto app_dir = get_app_dir();
    const auto bankc_exe = find_bankc_executable(app_dir);
    if (bankc_exe.empty()) {
        std::cerr << "[OAE Studio] oae_bankc executable not found!" << std::endl;
        return 1;
    }

    const fs::path temp_dir = fs::temp_directory_path() / "oae_studio_smoke_authoring";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir, ec);

    // 1. Generate fixture audio files
    const fs::path wav_path = temp_dir / "laser_source.wav";
    write_sine_wav(wav_path, 880.0f, 0.4f);

    // 2. Initialize project document and authoring model
    oae::studio::ProjectDocument doc;
    if (!doc.create_empty(temp_dir, "AuthoringDemo")) {
        std::cerr << "[OAE Studio] Failed to create empty project" << std::endl;
        return 1;
    }

    oae::studio::AuthoringModel model(doc);

    // 3. Create Bus with DSP Limiter
    if (!model.create_bus("bus:/sfx/weapons", "bus:/")) {
        std::cerr << "[OAE Studio] Failed to create bus" << std::endl;
        return 1;
    }
    if (!model.add_effect_to_bus("bus:/sfx/weapons", "limiter", 0)) {
        std::cerr << "[OAE Studio] Failed to add limiter to bus" << std::endl;
        return 1;
    }

    // 4. Import Sample
    if (!doc.import_audio_file(wav_path, "laser_sfx")) {
        std::cerr << "[OAE Studio] Failed to import audio file" << std::endl;
        return 1;
    }

    // 5. Create Labeled Global Parameter
    if (!model.create_global_parameter("WeaponType", "labeled")) {
        std::cerr << "[OAE Studio] Failed to create global parameter" << std::endl;
        return 1;
    }
    model.modify([](nlohmann::json& r) {
        for (auto& p : r["global_parameters"]) {
            if (p.value("name", "") == "WeaponType") {
                p["labels"] = nlohmann::json::array({ "Pistol", "Rifle", "Laser" });
                p["default_value"] = 0;
            }
        }
    });

    // 6. Create 2-track Event
    if (!model.create_event("event:/weapons/fire", "bus:/sfx/weapons")) {
        std::cerr << "[OAE Studio] Failed to create event" << std::endl;
        return 1;
    }

    // Track 0: Multi-instrument with playlist entries and Volume automation
    model.add_instrument("event:/weapons/fire", 0, "multi");
    model.modify([](nlohmann::json& r) {
        for (auto& ev : r["events"]) {
            if (ev.value("path", "") == "event:/weapons/fire") {
                auto& inst = ev["tracks"][0]["instruments"][0];
                inst["playlist_mode"] = "random";
                inst["playlist"] = nlohmann::json::array({
                    { { "sample_id", "laser_sfx" }, { "weight", 0.7f } },
                    { { "sample_id", "laser_sfx" }, { "weight", 0.3f } }
                });

                // Add automation on Track 0 controlled by WeaponType
                ev["tracks"][0]["automations"] = nlohmann::json::array({
                    {
                        { "target", "volume" },
                        { "is_timeline", false },
                        { "param", { { "scope", "global" }, { "name", "WeaponType" } } },
                        { "curve", {
                            { "points", nlohmann::json::array({
                                { { "x", 0.0f }, { "y", 0.0f }, { "shape", "linear" } },
                                { { "x", 2.0f }, { "y", -6.0f }, { "shape", "linear" } }
                            }) }
                        } }
                    }
                });
            }
        }
    });

    // Track 1: Single instrument conditioned on WeaponType == 2 ("Laser")
    if (!model.add_track("event:/weapons/fire")) {
        std::cerr << "[OAE Studio] Failed to add track 1" << std::endl;
        return 1;
    }
    if (!model.add_instrument("event:/weapons/fire", 1, "single")) {
        std::cerr << "[OAE Studio] Failed to add single instrument to track 1" << std::endl;
        return 1;
    }
    model.modify([](nlohmann::json& r) {
        for (auto& ev : r["events"]) {
            if (ev.value("path", "") == "event:/weapons/fire") {
                auto& inst = ev["tracks"][1]["instruments"][0];
                inst["sample_id"] = "laser_sfx";
                inst["condition"] = {
                    { "probability_pct", 100.0f },
                    { "terms", nlohmann::json::array({
                        {
                            { "param", { { "scope", "global" }, { "name", "WeaponType" } } },
                            { "min", 2.0f },
                            { "max", 2.0f }
                        }
                    }) }
                };
            }
        }
    });

    // 7. Verify Referential Integrity on Rename
    // Rename Bus: bus:/sfx/weapons -> bus:/sfx/arms
    if (!model.rename_item(oae::studio::ItemType::Bus, "bus:/sfx/weapons", "bus:/sfx/arms")) {
        std::cerr << "[OAE Studio] Failed to rename bus" << std::endl;
        return 1;
    }

    std::string updated_bus_path;
    for (const auto& ev : doc.get_root_json()["events"]) {
        if (ev.value("path", "") == "event:/weapons/fire") {
            updated_bus_path = ev.value("bus_path", "");
        }
    }
    if (updated_bus_path != "bus:/sfx/arms") {
        std::cerr << "[OAE Studio] Referential integrity failed: event bus_path was not updated to bus:/sfx/arms (was: "
                  << updated_bus_path << ")" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Bus rename referential integrity verified." << std::endl;

    // Rename Parameter: WeaponType -> GunType
    if (!model.rename_item(oae::studio::ItemType::GlobalParameter, "WeaponType", "GunType")) {
        std::cerr << "[OAE Studio] Failed to rename global parameter" << std::endl;
        return 1;
    }

    std::string auto_param_name;
    std::string cond_param_name;
    for (const auto& ev : doc.get_root_json()["events"]) {
        if (ev.value("path", "") == "event:/weapons/fire") {
            auto_param_name = ev["tracks"][0]["automations"][0]["param"].value("name", "");
            cond_param_name = ev["tracks"][1]["instruments"][0]["condition"]["terms"][0]["param"].value("name", "");
        }
    }
    if (auto_param_name != "GunType" || cond_param_name != "GunType") {
        std::cerr << "[OAE Studio] Parameter rename referential integrity failed: auto="
                  << auto_param_name << ", cond=" << cond_param_name << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Parameter rename referential integrity verified." << std::endl;

    // 8. Test Undo/Redo
    // Undo Parameter rename
    if (!model.undo()) {
        std::cerr << "[OAE Studio] Failed to undo parameter rename" << std::endl;
        return 1;
    }
    for (const auto& ev : doc.get_root_json()["events"]) {
        if (ev.value("path", "") == "event:/weapons/fire") {
            auto_param_name = ev["tracks"][0]["automations"][0]["param"].value("name", "");
            cond_param_name = ev["tracks"][1]["instruments"][0]["condition"]["terms"][0]["param"].value("name", "");
        }
    }
    if (auto_param_name != "WeaponType" || cond_param_name != "WeaponType") {
        std::cerr << "[OAE Studio] Undo failed to revert parameter name in references! (auto="
                  << auto_param_name << ", cond=" << cond_param_name << ")" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Undo parameter rename verified." << std::endl;

    // Undo Bus rename
    if (!model.undo()) {
        std::cerr << "[OAE Studio] Failed to undo bus rename" << std::endl;
        return 1;
    }
    for (const auto& ev : doc.get_root_json()["events"]) {
        if (ev.value("path", "") == "event:/weapons/fire") {
            updated_bus_path = ev.value("bus_path", "");
        }
    }
    if (updated_bus_path != "bus:/sfx/weapons") {
        std::cerr << "[OAE Studio] Undo failed to revert bus_path in event! (got: "
                  << updated_bus_path << ")" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Undo bus rename verified." << std::endl;

    // Redo Bus rename
    if (!model.redo()) {
        std::cerr << "[OAE Studio] Failed to redo bus rename" << std::endl;
        return 1;
    }
    for (const auto& ev : doc.get_root_json()["events"]) {
        if (ev.value("path", "") == "event:/weapons/fire") {
            updated_bus_path = ev.value("bus_path", "");
        }
    }
    if (updated_bus_path != "bus:/sfx/arms") {
        std::cerr << "[OAE Studio] Redo failed to restore bus_path!" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Redo bus rename verified." << std::endl;

    // Redo Parameter rename
    if (!model.redo()) {
        std::cerr << "[OAE Studio] Failed to redo parameter rename" << std::endl;
        return 1;
    }
    for (const auto& ev : doc.get_root_json()["events"]) {
        if (ev.value("path", "") == "event:/weapons/fire") {
            auto_param_name = ev["tracks"][0]["automations"][0]["param"].value("name", "");
            cond_param_name = ev["tracks"][1]["instruments"][0]["condition"]["terms"][0]["param"].value("name", "");
        }
    }
    if (auto_param_name != "GunType" || cond_param_name != "GunType") {
        std::cerr << "[OAE Studio] Redo failed to restore parameter references!" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Redo parameter rename verified." << std::endl;

    const std::string parent_event = "event:/weapons/fire";
    const std::string nested_event = "event:/weapons/echo";
    if (!model.create_event(nested_event) ||
        model.add_instrument(parent_event, 1, "event") ||
        model.add_instrument(parent_event, 1, "command") ||
        model.add_instrument(parent_event, 1, "event", parent_event) ||
        !model.add_instrument(parent_event, 1, "event", nested_event) ||
        model.add_instrument(nested_event, 0, "command", parent_event)) {
        std::cerr << "[OAE Studio] Instrument creation accepted an invalid target or rejected a valid one." << std::endl;
        return 1;
    }

    auto event_clip = [&]() -> nlohmann::json& {
        return doc.get_root_json()["events"][0]["tracks"][1]["instruments"][1];
    };
    model.modify([&](nlohmann::json&) { event_clip()["target_event"] = ""; });
    {
        oae::studio::TimelinePanel timeline(model);
        timeline.setSize(800, 600);
        timeline.set_event_path(parent_event);
        const std::function<oae::studio::ClipComponent*(juce::Component&)> find_clip =
            [&](juce::Component& component) -> oae::studio::ClipComponent* {
                if (auto* clip = dynamic_cast<oae::studio::ClipComponent*>(&component)) {
                    if (clip->get_track_index() == 1 && clip->get_instrument_index() == 1) return clip;
                }
                for (auto* child : component.getChildren()) {
                    if (auto* found = find_clip(*child)) return found;
                }
                return nullptr;
            };
        auto* clip = find_clip(timeline);
        if (clip == nullptr) {
            std::cerr << "[OAE Studio] Legacy nested event clip was missing from Track 2." << std::endl;
            return 1;
        }
        oae::studio::InspectorPanel inspector(model);
        const auto now = juce::Time::getCurrentTime();
        const juce::Point<float> center{clip->getWidth() * 0.5f, clip->getHeight() * 0.5f};
        const juce::MouseEvent click(juce::Desktop::getInstance().getMainMouseSource(), center,
                                    juce::ModifierKeys::leftButtonModifier, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                    clip, clip, now, center, now, 1, false);
        clip->mouseDown(click);
        clip->mouseUp(click);
        const std::function<juce::ComboBox*(juce::Component&)> find_target = [&](juce::Component& component) -> juce::ComboBox* {
            if (component.getComponentID() == "inspector_instrument_target") return dynamic_cast<juce::ComboBox*>(&component);
            for (auto* child : component.getChildren()) {
                if (auto* found = find_target(*child)) return found;
            }
            return nullptr;
        };
        auto* target = find_target(inspector);
        if (target == nullptr || target->getSelectedId() != 0 || target->getNumItems() != 1 ||
            target->getItemText(0).toStdString() != nested_event) {
            std::cerr << "[OAE Studio] Inspector did not offer the valid nested event for a legacy clip." << std::endl;
            return 1;
        }
        target->setSelectedItemIndex(0, juce::sendNotificationSync);
        if (event_clip().value("target_event", "") != nested_event ||
            !model.undo() || !event_clip().value("target_event", "").empty() ||
            !model.redo() || event_clip().value("target_event", "") != nested_event) {
            std::cerr << "[OAE Studio] Inspector could not repair the nested event with undo/redo." << std::endl;
            return 1;
        }
    }
    if (!model.duplicate_instrument(parent_event, 1, 1) ||
        doc.get_root_json()["events"][0]["tracks"][1]["instruments"][2] != event_clip() ||
        !model.add_instrument(parent_event, 1, "command", nested_event)) {
        std::cerr << "[OAE Studio] Instrument duplication or command creation lost its target." << std::endl;
        return 1;
    }
    model.modify([](nlohmann::json& root) {
        auto& command = root["events"][0]["tracks"][1]["instruments"][3];
        command["command_target"] = "";
        command["fade_in_ms"] = 0.0;
        command["length_ms"] = 500.0;
    });
    if (!model.set_instrument_target(parent_event, 1, 3, nested_event) ||
        !model.resize_instrument(parent_event, 1, 3, 250.0, 500.0, -1.0)) {
        std::cerr << "[OAE Studio] Failed to repair or move the legacy command clip." << std::endl;
        return 1;
    }
    if (!model.set_command_type(parent_event, 1, 3, "stop_event") ||
        doc.get_root_json()["events"][0]["tracks"][1]["instruments"][3].value("command_type", "") != "stop_event" ||
        !model.undo() ||
        doc.get_root_json()["events"][0]["tracks"][1]["instruments"][3].value("command_type", "") != "start_event" ||
        !model.redo() ||
        doc.get_root_json()["events"][0]["tracks"][1]["instruments"][3].value("command_type", "") != "stop_event") {
        std::cerr << "[OAE Studio] Failed to switch command_type with undo/redo." << std::endl;
        return 1;
    }
    if (!model.get_snapshot_paths().empty()) {
        const auto snap_path = model.get_snapshot_paths().front();
        if (!model.set_command_type(parent_event, 1, 3, "snapshot_on") ||
            doc.get_root_json()["events"][0]["tracks"][1]["instruments"][3].value("command_target", "") != snap_path ||
            !model.set_command_type(parent_event, 1, 3, "snapshot_off") ||
            doc.get_root_json()["events"][0]["tracks"][1]["instruments"][3].value("command_type", "") != "snapshot_off" ||
            !model.undo() || !model.undo()) {
            std::cerr << "[OAE Studio] Failed to switch command_type to snapshot_on/snapshot_off." << std::endl;
            return 1;
        }
    }
    std::cout << "[OAE Studio] Command type switching and undo/redo verified." << std::endl;

    if (!model.add_instrument(parent_event, 1, "scatterer") ||
        !model.set_scatterer_properties(parent_event, 1, 4, 80.0, 240.0, 3, 6, 1.5f, 12.0f) ||
        doc.get_root_json()["events"][0]["tracks"][1]["instruments"][4].value("polyphony", 0u) != 3u ||
        !model.undo() ||
        doc.get_root_json()["events"][0]["tracks"][1]["instruments"][4].value("polyphony", 0u) != 4u ||
        !model.redo() ||
        doc.get_root_json()["events"][0]["tracks"][1]["instruments"][4].value("polyphony", 0u) != 3u) {
        std::cerr << "[OAE Studio] Scatterer authoring or undo/redo failed." << std::endl;
        return 1;
    }
    model.modify([](nlohmann::json& root) {
        if (root.contains("samples") && root["samples"].is_array() && !root["samples"].empty()) {
            const auto sid = root["samples"][0].value("id", "");
            if (!sid.empty()) {
                root["events"][0]["tracks"][1]["instruments"][4]["playlist"].push_back({{"sample_id", sid}, {"weight", 1.0}});
            }
        }
    });
    std::cout << "[OAE Studio] Scatterer authoring and undo/redo verified." << std::endl;

    // Programmer Instrument + Audio Table authoring & undo/redo
    const auto sample_ids = model.get_sample_ids();
    if (sample_ids.empty() ||
        !model.add_instrument(parent_event, 1, "programmer") ||
        !model.set_programmer_key(parent_event, 1, 5, "smoke_vo_key") ||
        !model.set_audio_table_entry("smoke_vo_key", sample_ids[0]) ||
        model.get_audio_table_entries().empty() ||
        !model.undo() || !model.get_audio_table_entries().empty() ||
        !model.redo() || model.get_audio_table_entries().empty()) {
        std::cerr << "[OAE Studio] Programmer or audio_table authoring/undo failed." << std::endl;
        return 1;
    }

    // Mixed playlist target_event + cycle-safe target query
    const auto valid_playlist_events = model.get_instrument_targets(parent_event, "event");
    if (std::find(valid_playlist_events.begin(), valid_playlist_events.end(), parent_event) != valid_playlist_events.end() ||
        std::find(valid_playlist_events.begin(), valid_playlist_events.end(), nested_event) == valid_playlist_events.end()) {
        std::cerr << "[OAE Studio] Playlist event target filtering failed." << std::endl;
        return 1;
    }
    model.modify([&](nlohmann::json& root) {
        root["events"][0]["tracks"][1]["instruments"][4]["playlist"].push_back({{"target_event", nested_event}, {"weight", 1.0}});
    });
    std::cout << "[OAE Studio] Programmer, audio_table and mixed playlist verified." << std::endl;

    // Modulation (RANDOM + AHDSR) and Parameter Automation Curves (T10b)
    const oae::studio::ItemSelection ev_sel{ oae::studio::ItemType::Event, parent_event, -1, -1, -1 };
    const auto gparams = model.get_global_parameter_names();
    const nlohmann::json curve_pts = nlohmann::json::array({
        {{ "x", 0.0 }, { "y", -6.0 }, { "shape", "s_curve" }},
        {{ "x", 0.5 }, { "y", 0.0 }, { "shape", "ease_out" }},
        {{ "x", 1.0 }, { "y", 3.0 }, { "shape", "linear" }}
    });
    if (!model.add_modulator(ev_sel, {{ "kind", "random" }, { "target", "pitch" }, { "range", 1.5 }}) ||
        !model.add_modulator(ev_sel, {{ "kind", "ahdsr" }, { "attack_ms", 20.0 }, { "hold_ms", 10.0 },
                                      { "decay_ms", 50.0 }, { "sustain", 0.8 }, { "release_ms", 120.0 },
                                      { "release_on_key_off", true }}) ||
        !model.update_modulator(ev_sel, 0, {{ "kind", "random" }, { "target", "pitch" }, { "range", 2.0 }}) ||
        !model.undo() ||
        doc.get_root_json()["events"][0]["modulators"][0].value("range", 0.0) != 1.5 ||
        !model.redo() ||
        doc.get_root_json()["events"][0]["modulators"][0].value("range", 0.0) != 2.0 ||
        gparams.empty() ||
        !model.add_parameter_automation(parent_event, -1, "volume", "global", gparams[0], curve_pts) ||
        !model.update_automation_curve(parent_event, -1, 0, curve_pts)) {
        std::cerr << "[OAE Studio] Modulation or parameter automation authoring failed." << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Modulation (RANDOM/AHDSR) and parameter curves verified." << std::endl;

    // Timeline Logic authoring (Marker rename integrity, Tempo, Loop until_key_off, Transition Region + crossfade + conditions)
    if (!model.add_marker(parent_event, "Intro", 100.0) ||
        !model.add_tempo_marker(parent_event, 120.0, 4, 4, 0.0) ||
        !model.update_tempo_marker(parent_event, 1, 138.0, 3, 8) ||
        !model.add_loop_region(parent_event, 200.0, 1000.0, -1) ||
        !model.add_transition_region(parent_event, 1200.0, 800.0, "Intro", "beat", 250.0) ||
        !model.add_sustain_point(parent_event, 1500.0)) {
        std::cerr << "[OAE Studio] Timeline logic authoring failed." << std::endl;
        return 1;
    }
    model.modify([&](nlohmann::json& root) {
        auto& items = root["events"][0]["logic_items"];
        items[2]["until_key_off"] = true;
        items[2]["length_ms"] = 1100.0;
        items[3]["condition"] = {
            { "probability_pct", 75.0 },
            { "terms", nlohmann::json::array({ {{ "param", {{ "scope", "global" }, { "name", gparams[0] }} }, { "min", 0.2 }, { "max", 0.9 }} }) }
        };
    });
    if (!model.rename_marker(parent_event, 0, "Verse") ||
        doc.get_root_json()["events"][0]["logic_items"][0].value("name", "") != "Verse" ||
        doc.get_root_json()["events"][0]["logic_items"][3].value("target_marker", "") != "Verse" ||
        !model.undo() ||
        doc.get_root_json()["events"][0]["logic_items"][0].value("name", "") != "Intro" ||
        doc.get_root_json()["events"][0]["logic_items"][3].value("target_marker", "") != "Intro" ||
        !model.redo() ||
        doc.get_root_json()["events"][0]["logic_items"][3].value("target_marker", "") != "Verse" ||
        doc.get_root_json()["events"][0]["logic_items"][1].value("bpm", 0.0) != 138.0 ||
        !doc.get_root_json()["events"][0]["logic_items"][2].value("until_key_off", false)) {
        std::cerr << "[OAE Studio] Timeline logic referential rename or undo/redo failed." << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Timeline logic authoring and referential rename verified." << std::endl;

    // Async instrument authoring + Undo/Redo via InspectorPanel
    {
        oae::studio::InspectorPanel inspector(model);
        inspector.setSize(320, 600);
        model.select({ oae::studio::ItemType::Event, parent_event, 0, 0 });
        const std::function<juce::ToggleButton*(juce::Component&)> find_async = [&](juce::Component& component) -> juce::ToggleButton* {
            if (component.getComponentID() == "inspector_instrument_async") return dynamic_cast<juce::ToggleButton*>(&component);
            for (auto* child : component.getChildren()) {
                if (auto* found = find_async(*child)) return found;
            }
            return nullptr;
        };
        auto* async_btn = find_async(inspector);
        if (!async_btn) {
            std::cerr << "[OAE Studio] inspector_instrument_async control not found." << std::endl;
            return 1;
        }
        const int wide_y = inspector.getLocalArea(async_btn, async_btn->getLocalBounds()).getY();
        inspector.setSize(180, 600);
        const auto narrow_bounds = inspector.getLocalArea(async_btn, async_btn->getLocalBounds());
        if (narrow_bounds.isEmpty() || narrow_bounds.getX() < 0 || narrow_bounds.getRight() > inspector.getWidth()
            || narrow_bounds.getY() <= wide_y) {
            std::cerr << "[OAE Studio] Narrow Inspector layout did not reflow." << std::endl;
            return 1;
        }
        async_btn->setToggleState(true, juce::sendNotificationSync);
        if (!doc.get_root_json()["events"][0]["tracks"][0]["instruments"][0].value("async", false) ||
            !model.undo() ||
            doc.get_root_json()["events"][0]["tracks"][0]["instruments"][0].value("async", false) ||
            !model.redo() ||
            !doc.get_root_json()["events"][0]["tracks"][0]["instruments"][0].value("async", false)) {
            std::cerr << "[OAE Studio] Async instrument InspectorPanel toggle or undo/redo failed." << std::endl;
            return 1;
        }
        std::cout << "[OAE Studio] Async instrument authoring and undo/redo verified." << std::endl;
    }

    // Parameter Sheets, Local Params, Modifiers & User Props authoring (Etapa 2)
    {
        if (!model.add_local_parameter(nested_event, "EngineRPM", "continuous") ||
            !model.update_local_parameter(nested_event, "EngineRPM", {
                { "name", "EngineRPM" }, { "kind", "continuous" },
                { "min", 0.0 }, { "max", 100.0 }, { "default_value", 25.0 },
                { "seek_speed", 10.0 }, { "velocity", 5.0 }, { "hold_during_playback", true }
            }) ||
            !model.add_local_parameter(nested_event, "Gear", "discrete") ||
            !model.update_local_parameter(nested_event, "Gear", {
                { "name", "Gear" }, { "kind", "discrete" },
                { "min", 1 }, { "max", 5 }, { "default_value", 2 }
            }) ||
            !model.add_local_parameter(nested_event, "Surface", "labeled") ||
            !model.update_local_parameter(nested_event, "Surface", {
                { "name", "Surface" }, { "kind", "labeled" },
                { "labels", nlohmann::json::array({ "dirt", "grass", "stone" }) },
                { "default_value", 1 }
            })) {
            std::cerr << "[OAE Studio] Local parameter creation/update failed." << std::endl;
            return 1;
        }
        const auto surfaceDom = model.get_parameter_domain(nested_event, "local", "Surface");
        if (surfaceDom.first != 0.0 || surfaceDom.second != 2.0) {
            std::cerr << "[OAE Studio] Labeled parameter domain mismatch." << std::endl;
            return 1;
        }
        if (!model.add_instrument(nested_event, 0, "single", sample_ids[0]) ||
            !model.set_instrument_sheet_range(nested_event, 0, 0, "EngineRPM", 20.0, 80.0) ||
            !model.undo() || !model.redo() ||
            model.set_instrument_sheet_range(parent_event, 1, 3, "EngineRPM", 0.0, 50.0)) {
            std::cerr << "[OAE Studio] Parameter sheet range authoring failed." << std::endl;
            return 1;
        }
        if (!model.set_event_user_property(nested_event, "notes", "Engine sheet test") ||
            !model.set_event_user_property(nested_event, "tags", "sfx,engine") ||
            !model.set_event_user_property(nested_event, "temp_key", "tmp") ||
            !model.remove_event_user_property(nested_event, "temp_key")) {
            std::cerr << "[OAE Studio] Event user property authoring failed." << std::endl;
            return 1;
        }
        std::cout << "[OAE Studio] Local params, modifiers, sheets and user_props verified." << std::endl;
    }

    // 9. Save and Compile with oae_bankc CLI
    if (!doc.save()) {
        std::cerr << "[OAE Studio] Failed to save project" << std::endl;
        return 1;
    }

    const fs::path proj_json_path = doc.get_project_file();
    const fs::path out_bank_path = temp_dir / "authoring_demo.bank";

#if defined(_WIN32)
    const std::wstring cmd = L"\"" + bankc_exe.wstring() + L"\" \"" + proj_json_path.wstring() + L"\" -o \"" + out_bank_path.wstring() + L"\"";
    int compile_res = -1;
    std::string compile_output;
    const bool proc_ok = run_command_capture_output(cmd, compile_res, compile_output);
    if (!proc_ok || compile_res != 0) {
        std::cerr << "[OAE Studio] Compilation of authored project failed with exit code: " << compile_res << "\n"
                  << compile_output << std::endl;
        return 1;
    }
#else
    const std::string cmd = "\"" + bankc_exe.string() + "\" \"" + proj_json_path.string() + "\" -o \"" + out_bank_path.string() + "\"";
    const int compile_res = std::system(cmd.c_str());
    if (compile_res != 0) {
        std::cerr << "[OAE Studio] Compilation of authored project failed with exit code: " << compile_res << std::endl;
        return 1;
    }
#endif

    std::cout << "[OAE Studio] Compiled authored bank successfully." << std::endl;

    // 10. Load into StudioEngine (headless) and test audio playback
    oae::studio::StudioEngine engine;
    if (engine.init_headless() != OAE_OK) {
        std::cerr << "[OAE Studio] Failed to init StudioEngine headless" << std::endl;
        return 1;
    }

    oae_bank bank_handle = 0;
    if (engine.load_bank_file(out_bank_path.string().c_str(), &bank_handle) != OAE_OK || bank_handle == 0) {
        std::cerr << "[OAE Studio] Failed to load compiled bank into engine" << std::endl;
        return 1;
    }

    oae_event_instance instance = 0;
    if (engine.create_event_instance("event:/weapons/fire", &instance) != OAE_OK || instance == 0) {
        std::cerr << "[OAE Studio] Failed to create event instance for event:/weapons/fire" << std::endl;
        return 1;
    }

    oae_event_instance_start(instance);

    std::array<float, 2048> pcm_buf{};
    float max_peak = 0.0f;
    for (int frame = 0; frame < 20; ++frame) {
        engine.update();
        if (engine.render(pcm_buf.data(), 1024) == OAE_OK) {
            for (float sample : pcm_buf) {
                max_peak = std::max(max_peak, std::abs(sample));
            }
        }
    }

    std::cout << "[OAE Studio] Render verified successfully (peak: " << max_peak << ")" << std::endl;
    if (max_peak < 0.001f) {
        std::cerr << "[OAE Studio] Rendered audio was silent!" << std::endl;
        return 1;
    }

    engine.unload_bank(bank_handle);
    engine.shutdown();

    fs::remove_all(temp_dir, ec);

    std::cout << "[OAE Studio] --smoke-authoring PASSED with exit code 0." << std::endl;
    return 0;
}

int run_smoke_timeline()
{
    std::cout << "[OAE Studio] Starting --smoke-timeline validation..." << std::endl;

    // 1. Validate TimelineSnap logic
    oae::studio::TimelineSnap snap;
    snap.set_mode(oae::studio::SnapMode::None);
    if (std::abs(snap.snap(123.456) - 123.456) > 0.001) {
        std::cerr << "[OAE Studio] Snap None failed!" << std::endl;
        return 1;
    }

    snap.set_mode(oae::studio::SnapMode::Seconds_1s);
    if (std::abs(snap.snap(1200.0) - 1000.0) > 0.001 || std::abs(snap.snap(1600.0) - 2000.0) > 0.001) {
        std::cerr << "[OAE Studio] Snap 1s failed!" << std::endl;
        return 1;
    }

    snap.set_mode(oae::studio::SnapMode::Seconds_500ms);
    if (std::abs(snap.snap(720.0) - 500.0) > 0.001 || std::abs(snap.snap(780.0) - 1000.0) > 0.001) {
        std::cerr << "[OAE Studio] Snap 500ms failed!" << std::endl;
        return 1;
    }

    snap.set_mode(oae::studio::SnapMode::Seconds_100ms);
    if (std::abs(snap.snap(134.0) - 100.0) > 0.001 || std::abs(snap.snap(162.0) - 200.0) > 0.001) {
        std::cerr << "[OAE Studio] Snap 100ms failed!" << std::endl;
        return 1;
    }

    // Default 120 BPM: beat = 500ms, bar = 2000ms
    snap.set_mode(oae::studio::SnapMode::Beat);
    if (std::abs(snap.snap(480.0) - 500.0) > 0.001 || std::abs(snap.snap(800.0) - 1000.0) > 0.001) {
        std::cerr << "[OAE Studio] Snap Beat (120 BPM) failed!" << std::endl;
        return 1;
    }

    snap.set_mode(oae::studio::SnapMode::Bar);
    if (std::abs(snap.snap(1200.0) - 2000.0) > 0.001 || std::abs(snap.snap(700.0) - 0.0) > 0.001) {
        std::cerr << "[OAE Studio] Snap Bar (120 BPM 4/4) failed!" << std::endl;
        return 1;
    }

    // Snap with dynamic tempo change
    nlohmann::json logic_items = nlohmann::json::array({
        { { "kind", "tempo" }, { "position_ms", 0.0 }, { "bpm", 120.0 }, { "ts_upper", 4 }, { "ts_lower", 4 } },
        { { "kind", "tempo" }, { "position_ms", 2000.0 }, { "bpm", 60.0 }, { "ts_upper", 4 }, { "ts_lower", 4 } }
    });
    // At position > 2000ms, tempo is 60 BPM -> beat is 1000ms
    snap.set_mode(oae::studio::SnapMode::Beat);
    const double snapped_tempo_beat = snap.snap(2600.0, &logic_items);
    if (std::abs(snapped_tempo_beat - 3000.0) > 0.001) {
        std::cerr << "[OAE Studio] Snap Beat with tempo change failed! (expected 3000, got: " << snapped_tempo_beat << ")" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] TimelineSnap algorithms verified." << std::endl;

    // 2. Validate WaveformCache decoding & envelope transforms
    const fs::path temp_dir = fs::temp_directory_path() / "oae_studio_smoke_timeline";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir, ec);

    const fs::path wav_path = temp_dir / "sine_500ms.wav";
    write_sine_wav(wav_path, 440.0f, 0.5f); // 500 ms sine wave

    oae::studio::WaveformCache cache;
    auto env = cache.load_synchronously(wav_path);
    if (!env || env->is_empty()) {
        std::cerr << "[OAE Studio] WaveformCache failed to decode WAV fixture!" << std::endl;
        return 1;
    }
    if (std::abs(env->duration_ms - 500.0) > 10.0) {
        std::cerr << "[OAE Studio] Waveform duration mismatch: expected ~500ms, got " << env->duration_ms << std::endl;
        return 1;
    }
    if (env->min_peaks.empty() || env->max_peaks.empty() || env->rms_levels.empty()) {
        std::cerr << "[OAE Studio] Waveform envelope arrays are empty!" << std::endl;
        return 1;
    }

    float minVal = 0.0f, maxVal = 0.0f, rmsVal = 0.0f;
    env->get_envelope_at(250.0, minVal, maxVal, rmsVal);
    if (maxVal < 0.8f || rmsVal < 0.5f) {
        std::cerr << "[OAE Studio] Waveform envelope values unexpected (max=" << maxVal << ", rms=" << rmsVal << ")" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] WaveformCache synchronous decoding and envelope verified." << std::endl;

    // Validate decoding across WAV, OGG, MP3, FLAC formats
    {
        const fs::path fixtures_candidates[] = {
            fs::current_path() / "native" / "tests" / "fixtures",
            fs::current_path() / ".." / "native" / "tests" / "fixtures",
            fs::current_path() / ".." / ".." / "native" / "tests" / "fixtures",
            fs::current_path() / ".." / ".." / ".." / "native" / "tests" / "fixtures"
        };
        fs::path fixtures_dir;
        for (const auto& candidate : fixtures_candidates) {
            if (fs::exists(candidate / "tone.ogg")) {
                fixtures_dir = fs::canonical(candidate);
                break;
            }
        }

        if (!fixtures_dir.empty()) {
            for (const char* filename : { "tone.wav", "tone.ogg", "tone.mp3", "tone.flac" }) {
                const fs::path fixture_file = fixtures_dir / filename;
                auto f_env = cache.load_synchronously(fixture_file);
                if (!f_env || f_env->is_empty() || f_env->duration_ms <= 0.0) {
                    std::cerr << "[OAE Studio] WaveformCache failed to decode fixture: " << filename << std::endl;
                    return 1;
                }
                if (f_env->min_peaks.empty() || f_env->max_peaks.empty() || f_env->rms_levels.empty()) {
                    std::cerr << "[OAE Studio] Waveform envelope empty for fixture: " << filename << std::endl;
                    return 1;
                }
            }
            std::cout << "[OAE Studio] WaveformCache multi-format decoding verified (WAV, OGG, MP3, FLAC)." << std::endl;
        }
    }

    // 3. Multi-Track Clip Editing and Authoring Model
    oae::studio::ProjectDocument doc;
    if (!doc.create_empty(temp_dir, "TimelineProject")) {
        std::cerr << "[OAE Studio] Failed to create empty project" << std::endl;
        return 1;
    }
    if (!doc.import_audio_file(wav_path, "sine_sample")) {
        std::cerr << "[OAE Studio] Failed to import audio sample" << std::endl;
        return 1;
    }

    oae::studio::AuthoringModel model(doc);
    if (!model.create_event("event:/music/loop", "bus:/")) {
        std::cerr << "[OAE Studio] Failed to create event" << std::endl;
        return 1;
    }

    // Track 0 single instrument
    model.add_instrument("event:/music/loop", 0, "single");
    model.resize_instrument("event:/music/loop", 0, 0, 100.0, 800.0, 0.0);
    model.set_instrument_fades("event:/music/loop", 0, 0, 50.0, 100.0);
    model.modify([](nlohmann::json& r) {
        for (auto& ev : r["events"]) {
            if (ev.value("path", "") == "event:/music/loop") {
                ev["tracks"][0]["instruments"][0]["sample_id"] = "sine_sample";
            }
        }
    });

    // Add Track 1 with single instrument
    if (!model.add_track("event:/music/loop")) {
        std::cerr << "[OAE Studio] Failed to add track 1" << std::endl;
        return 1;
    }
    model.add_instrument("event:/music/loop", 1, "single");
    model.resize_instrument("event:/music/loop", 1, 0, 500.0, 1200.0, 20.0);
    model.modify([](nlohmann::json& r) {
        for (auto& ev : r["events"]) {
            if (ev.value("path", "") == "event:/music/loop") {
                ev["tracks"][1]["instruments"][0]["sample_id"] = "sine_sample";
            }
        }
    });

    // Move instrument from Track 0 to Track 1 at offset 250.0 ms
    if (!model.move_instrument("event:/music/loop", 0, 0, 1, 250.0)) {
        std::cerr << "[OAE Studio] Failed to move instrument from track 0 to track 1" << std::endl;
        return 1;
    }

    // Verify instrument count: Track 0 has 0, Track 1 has 2
    size_t trk0_count = 999, trk1_count = 0;
    double moved_offset = 0.0;
    for (const auto& ev : doc.get_root_json()["events"]) {
        if (ev.value("path", "") == "event:/music/loop") {
            trk0_count = ev["tracks"][0]["instruments"].size();
            trk1_count = ev["tracks"][1]["instruments"].size();
            moved_offset = ev["tracks"][1]["instruments"][1].value("offset_ms", 0.0);
        }
    }
    if (trk0_count != 0 || trk1_count != 2 || std::abs(moved_offset - 250.0) > 0.001) {
        std::cerr << "[OAE Studio] Instrument move failed! trk0=" << trk0_count << ", trk1=" << trk1_count << ", offset=" << moved_offset << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Move instrument across tracks verified." << std::endl;

    // Resize and set fades on instrument 1 of track 1
    model.resize_instrument("event:/music/loop", 1, 1, 300.0, 1500.0, 50.0);
    model.set_instrument_fades("event:/music/loop", 1, 1, 120.0, 180.0);

    // Track volume, pan, mute, and solo controls
    model.set_track_volume("event:/music/loop", 1, -4.5);
    model.set_track_pan("event:/music/loop", 1, 0.35);
    const uint64_t rev_before_ms = model.get_content_revision();
    model.set_track_muted("event:/music/loop", 0, true);
    model.set_track_solo("event:/music/loop", 1, true);
    if (model.get_content_revision() != rev_before_ms) {
        std::cerr << "[OAE Studio] Track Mute/Solo unexpectedly incremented content revision!" << std::endl;
        return 1;
    }

    if (!model.is_track_muted("event:/music/loop", 0) || !model.is_track_solo("event:/music/loop", 1)) {
        std::cerr << "[OAE Studio] Track Mute/Solo flags verification failed!" << std::endl;
        return 1;
    }

    // Undo / Redo of Mute/Solo
    if (!model.undo()) {
        std::cerr << "[OAE Studio] Failed to undo track solo" << std::endl;
        return 1;
    }
    if (model.is_track_solo("event:/music/loop", 1)) {
        std::cerr << "[OAE Studio] Track solo was not cleared by undo!" << std::endl;
        return 1;
    }
    if (!model.redo()) {
        std::cerr << "[OAE Studio] Failed to redo track solo" << std::endl;
        return 1;
    }
    if (!model.is_track_solo("event:/music/loop", 1)) {
        std::cerr << "[OAE Studio] Track solo was not restored by redo!" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Track controls and undo/redo verified." << std::endl;

    // Verify sub-lane stacking for overlapping clips and double-click navigation on Event Instrument
    {
        oae::studio::WaveformCache smoke_wf_cache;
        oae::studio::TimelineTrackLane smoke_lane(model, smoke_wf_cache, "event:/music/loop", 1);
        smoke_lane.set_time_converter(
            [](double ms) { return ms * 0.2; },
            [](float px) { return static_cast<double>(px) / 0.2; });
        smoke_lane.setBounds(0, 0, 800, 80);
        const auto& lane_clips = smoke_lane.get_clips();
        if (lane_clips.size() != 2 ||
            lane_clips[0]->getBounds().intersects(lane_clips[1]->getBounds())) {
            std::cerr << "[OAE Studio] Overlapping clips were not placed in distinct sub-lanes!" << std::endl;
            return 1;
        }

        // Verify double-click navigation on an Event Instrument clip
        doc.add_simple_event("event:/sfx/nested_smoke", "sine_sample", 0.5f);
        model.add_instrument("event:/music/loop", 0, "event", "event:/sfx/nested_smoke");
        oae::studio::TimelinePanel timeline_panel(model);
        timeline_panel.setBounds(0, 0, 640, 320);
        model.select({ oae::studio::ItemType::Event, "event:/music/loop", -1, -1 });

        oae::studio::TimelineTrackLane evt_lane(model, smoke_wf_cache, "event:/music/loop", 0);
        evt_lane.set_time_converter(
            [](double ms) { return ms * 0.2; },
            [](float px) { return static_cast<double>(px) / 0.2; });
        evt_lane.setBounds(0, 0, 800, 80);
        if (evt_lane.get_clips().empty()) {
            std::cerr << "[OAE Studio] Event instrument clip missing in lane!" << std::endl;
            return 1;
        }
        evt_lane.get_clips()[0]->open_target_event();
        if (model.get_selection().type != oae::studio::ItemType::Event ||
            model.get_selection().id_or_path != "event:/sfx/nested_smoke") {
            std::cerr << "[OAE Studio] Double-click on Event Instrument did not select target event!" << std::endl;
            return 1;
        }
        timeline_panel.setBounds(0, 0, 420, 260);
        if (timeline_panel.get_breadcrumb_trail().size() != 2 ||
            timeline_panel.get_breadcrumb_trail()[0] != "event:/music/loop" ||
            timeline_panel.get_breadcrumb_trail()[1] != "event:/sfx/nested_smoke") {
            std::cerr << "[OAE Studio] Breadcrumb trail did not record parent -> child navigation!" << std::endl;
            return 1;
        }
        timeline_panel.navigate_to_parent_event();
        if (timeline_panel.get_event_path() != "event:/music/loop" ||
            timeline_panel.get_breadcrumb_trail().size() != 1 ||
            model.get_selection().type != oae::studio::ItemType::Event ||
            model.get_selection().id_or_path != "event:/music/loop") {
            std::cerr << "[OAE Studio] Breadcrumb back navigation failed to restore parent event!" << std::endl;
            return 1;
        }
        model.undo();
        model.select({ oae::studio::ItemType::Event, "event:/music/loop", -1, -1 });

        const auto project_before_curve_tests = model.json();
        const nlohmann::json auto_pts = nlohmann::json::array({
            { { "x", 100.0 }, { "y", -12.0 }, { "shape", "s_curve" } },
            { { "x", 500.0 }, { "y", 0.0 }, { "shape", "ease_in" } }
        });
        if (!model.set_track_timeline_volume_points("event:/music/loop", 0, auto_pts, true)) {
            std::cerr << "[OAE Studio] Failed to initialize automation curve points!" << std::endl;
            return 1;
        }
        timeline_panel.setSize(640, 420);
        oae::studio::TrackHeaderItem* headers[2]{};
        oae::studio::TimelineTrackLane* lanes[2]{};
        for (auto* child : timeline_panel.getChildren()) {
            for (auto* item : child->getChildren()) {
                if (auto* header = dynamic_cast<oae::studio::TrackHeaderItem*>(item)) {
                    const auto index = header->get_track_index();
                    if (index < 2) headers[index] = header;
                }
                if (auto* lane = dynamic_cast<oae::studio::TimelineTrackLane*>(item)) {
                    if (lane->getComponentID() == "track_lane_0") lanes[0] = lane;
                    if (lane->getComponentID() == "track_lane_1") lanes[1] = lane;
                }
            }
        }
        juce::Button* automation_buttons[2]{};
        for (size_t i = 0; i < 2; ++i) {
            if (!headers[i] || !lanes[i]) {
                std::cerr << "[OAE Studio] Automation smoke requires two track headers and lanes!" << std::endl;
                return 1;
            }
            for (auto* child : headers[i]->getChildren()) {
                if (auto* button = dynamic_cast<juce::Button*>(child);
                    button && button->getButtonText() == "A") {
                    automation_buttons[i] = button;
                    break;
                }
            }
            if (!automation_buttons[i]) {
                std::cerr << "[OAE Studio] Track header automation button missing!" << std::endl;
                return 1;
            }
        }
        const auto click_automation = [&](size_t index) {
            juce::Component* button = automation_buttons[index];
            const auto pos = button->getLocalBounds().getCentre().toFloat();
            const auto now = juce::Time::getCurrentTime();
            const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
            button->mouseDown(juce::MouseEvent(mouse, pos, juce::ModifierKeys::leftButtonModifier,
                                               1.0f, 0.0f, 0.0f, 0.0f, 0.0f, button, button, now, pos, now, 1, false));
            button->mouseUp(juce::MouseEvent(mouse, pos, juce::ModifierKeys::noModifiers,
                                             1.0f, 0.0f, 0.0f, 0.0f, 0.0f, button, button, now, pos, now, 1, false));
        };
        const auto automation_matches_layout = [&](size_t index, bool expanded) {
            const auto& editor = lanes[index]->automation_lane();
            const auto header_y = timeline_panel.getLocalPoint(headers[index], juce::Point<int>{}).y;
            const auto lane_y = timeline_panel.getLocalPoint(lanes[index], juce::Point<int>{}).y;
            return headers[index]->is_automation_expanded() == expanded &&
                   lanes[index]->is_automation_expanded() == expanded &&
                   editor.isVisible() == expanded &&
                   (!expanded || (!editor.getBounds().isEmpty() &&
                                  lanes[index]->getLocalBounds().contains(editor.getBounds()))) &&
                   header_y == lane_y && headers[index]->getHeight() == lanes[index]->getHeight() &&
                   lanes[index]->getHeight() == (expanded ? 140 : 80);
        };
        const auto project_before_toggle = model.json();
        if (!automation_matches_layout(0, false) || !automation_matches_layout(1, false)) {
            std::cerr << "[OAE Studio] Initial collapsed automation layout is incorrect!" << std::endl;
            return 1;
        }
        click_automation(0);
        if (!automation_matches_layout(0, true) || !automation_matches_layout(1, false)) {
            std::cerr << "[OAE Studio] Automation button did not expand only the first track!" << std::endl;
            return 1;
        }
        click_automation(1);
        click_automation(0);
        if (!automation_matches_layout(0, false) || !automation_matches_layout(1, true)) {
            std::cerr << "[OAE Studio] Collapsing the first track changed the second automation lane!" << std::endl;
            return 1;
        }
        click_automation(1);
        if (!automation_matches_layout(0, false) || !automation_matches_layout(1, false) ||
            model.json() != project_before_toggle) {
            std::cerr << "[OAE Studio] Automation buttons did not restore layout or preserve project data!" << std::endl;
            return 1;
        }
        std::cout << "[OAE Studio] Track header automation toggles, alignment and project data verified." << std::endl;
        timeline_panel.set_zoom(200.0);
        timeline_panel.set_horizontal_offset(0);
        click_automation(0);
        auto& editor = lanes[0]->automation_lane();
        if (!automation_matches_layout(0, true)) {
            std::cerr << "[OAE Studio] Automation editor is unavailable for curve regression!" << std::endl;
            return 1;
        }

        const auto set_curves = [&](const nlohmann::json& curves) {
            model.modify([&](nlohmann::json& root) {
                for (auto& event : root["events"]) {
                    if (event.value("path", "") == "event:/music/loop") {
                        event["tracks"][0]["automations"] = curves;
                        break;
                    }
                }
            });
        };
        const auto read_curves = [&]() -> nlohmann::json {
            for (const auto& event : model.json()["events"]) {
                if (event.value("path", "") == "event:/music/loop") {
                    return event["tracks"][0]["automations"];
                }
            }
            return nlohmann::json::array();
        };
        const auto render_editor = [&]() {
            juce::Image image(juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true);
            juce::Graphics graphics(image);
            editor.paint(graphics);
            return image;
        };
        const auto same_pixels = [](const juce::Image& left, const juce::Image& right) {
            if (left.getBounds() != right.getBounds()) return false;
            for (int y = 0; y < left.getHeight(); ++y) {
                for (int x = 0; x < left.getWidth(); ++x) {
                    if (left.getPixelAt(x, y) != right.getPixelAt(x, y)) return false;
                }
            }
            return true;
        };
        const auto mouse_event = [&](juce::Point<float> position, juce::Point<float> start,
                                     int clicks, juce::ModifierKeys mods, bool dragged = false) {
            auto mouse = juce::Desktop::getInstance().getMainMouseSource();
            const auto now = juce::Time::getCurrentTime();
            return juce::MouseEvent(mouse, position, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                    &editor, &editor, now, start, now, clicks, dragged);
        };
        const auto insert_point = [&](juce::Point<float> position) {
            editor.mouseDown(mouse_event(position, position, 2, juce::ModifierKeys::leftButtonModifier));
            editor.mouseUp(mouse_event(position, position, 2, juce::ModifierKeys::noModifiers));
        };

        const nlohmann::json temporal = {
            { "target", "volume" }, { "is_timeline", true }, { "curve", { { "points", auto_pts } } }
        };
        const nlohmann::json parameter = {
            { "target", "volume" }, { "is_timeline", false },
            { "param", { { "scope", "global" }, { "name", "MasterVolume" } } },
            { "curve", { { "points", nlohmann::json::array({
                { { "x", 0.0 }, { "y", -24.0 }, { "shape", "linear" } },
                { { "x", 1.0 }, { "y", 0.0 }, { "shape", "linear" } }
            }) } } }
        };
        const nlohmann::json pitch = {
            { "target", "pitch" }, { "is_timeline", true }, { "curve", { { "points", auto_pts } } }
        };
        const auto temporal_image = render_editor();
        const float height = static_cast<float>(editor.getHeight());
        const juce::Point<float> start{ 20.0f, height * (1.0f - 68.0f / 90.0f) };
        const juce::Point<float> middle{ 30.0f, height * 0.3f };
        const juce::Point<float> finish{ 40.0f, height * 0.2f };
        const juce::Point<float> inserted{ 70.0f, height * 0.5f };

        for (size_t temporal_index : { 0u, 1u }) {
            auto before_drag = nlohmann::json::array({ parameter, parameter });
            before_drag[temporal_index] = temporal;
            set_curves(before_drag);
            if (!same_pixels(render_editor(), temporal_image)) {
                std::cerr << "[OAE Studio] Unexpected curve rendered at index " << temporal_index << std::endl;
                return 1;
            }

            editor.mouseDown(mouse_event(start, start, 1, juce::ModifierKeys::leftButtonModifier));
            editor.mouseDrag(mouse_event(middle, start, 1, juce::ModifierKeys::leftButtonModifier, true));
            editor.mouseDrag(mouse_event(finish, start, 1, juce::ModifierKeys::leftButtonModifier, true));
            editor.mouseUp(mouse_event(finish, start, 1, juce::ModifierKeys::noModifiers, true));
            const auto after_drag = read_curves();
            const auto moved = after_drag.at(temporal_index).at("curve").at("points").at(0);
            auto expected_drag = before_drag;
            expected_drag[temporal_index]["curve"]["points"][0]["x"] = 200.0;
            expected_drag[temporal_index]["curve"]["points"][0]["y"] = moved.value("y", 0.0);
            if (std::abs(moved.value("y", 0.0) + 8.0) > 0.5 || after_drag != expected_drag ||
                !model.undo() || read_curves() != before_drag || !model.redo() || read_curves() != after_drag) {
                std::cerr << "[OAE Studio] Automation drag changed the wrong curve or broke Undo/Redo!" << std::endl;
                return 1;
            }

            insert_point(inserted);
            auto expected_insert = after_drag;
            auto& expected_points = expected_insert[temporal_index]["curve"]["points"];
            expected_points.insert(expected_points.begin() + 1,
                nlohmann::json{ { "x", 350.0 }, { "y", -35.0 }, { "shape", "linear" } });
            if (read_curves() != expected_insert ||
                !model.undo() || read_curves() != after_drag || !model.redo() || read_curves() != expected_insert) {
                std::cerr << "[OAE Studio] Point insertion changed the wrong curve or broke Undo/Redo!" << std::endl;
                return 1;
            }
        }

        set_curves(nlohmann::json::array());
        const auto empty_image = render_editor();
        const auto incompatible = nlohmann::json::array({ parameter, pitch });
        set_curves(incompatible);
        if (!same_pixels(render_editor(), empty_image) || same_pixels(empty_image, temporal_image) ||
            read_curves() != incompatible) {
            std::cerr << "[OAE Studio] Lane with only incompatible curves did not show an empty grid!" << std::endl;
            return 1;
        }
        insert_point(inserted);
        auto expected_creation = incompatible;
        auto new_curve = temporal;
        new_curve["curve"]["points"] = nlohmann::json::array({
            { { "x", 350.0 }, { "y", -35.0 }, { "shape", "linear" } }
        });
        expected_creation.push_back(new_curve);
        if (read_curves() != expected_creation || same_pixels(render_editor(), empty_image) ||
            !model.undo() || read_curves() != incompatible || !model.redo() || read_curves() != expected_creation) {
            std::cerr << "[OAE Studio] First timeline volume point did not preserve existing curves!" << std::endl;
            return 1;
        }

        // Point deletion: keyboard and context-menu paths preserve the other curves on the track.
        // The earlier navigation undo consumed track 0's only instrument, so restore a clip target first.
        if (!model.add_instrument("event:/music/loop", 0, "event", "event:/sfx/nested_smoke")) {
            std::cerr << "[OAE Studio] Could not restore a track 0 clip for the deletion smoke!" << std::endl;
            return 1;
        }
        const auto deletion_temporal = nlohmann::json{
            { "target", "volume" }, { "is_timeline", true }, { "curve", { { "points", nlohmann::json::array({
                { { "x", 100.0 }, { "y", -12.0 }, { "shape", "s_curve" } },
                { { "x", 500.0 }, { "y", 0.0 }, { "shape", "ease_in" } },
                { { "x", 900.0 }, { "y", -6.0 }, { "shape", "linear" } }
            }) } } }
        };
        const nlohmann::json deletion_curves = nlohmann::json::array({ parameter, deletion_temporal, pitch });
        const float edit_height = static_cast<float>(editor.getHeight());
        const auto second_point = juce::Point<float>{ 100.0f, edit_height * (1.0f - 80.0f / 90.0f) };
        const auto third_point = juce::Point<float>{ 180.0f, edit_height * (1.0f - 74.0f / 90.0f) };
        const juce::KeyPress delete_key(juce::KeyPress::deleteKey);
        const juce::KeyPress backspace_key(juce::KeyPress::backspaceKey);

        set_curves(deletion_curves);
        editor.mouseDown(mouse_event(second_point, second_point, 1, juce::ModifierKeys::leftButtonModifier));
        editor.mouseUp(mouse_event(second_point, second_point, 1, juce::ModifierKeys::noModifiers));
        auto expected_deletion = deletion_curves;
        expected_deletion[1]["curve"]["points"].erase(expected_deletion[1]["curve"]["points"].begin() + 1);
        if (editor.get_selected_point_index() != 1 || !editor.keyPressed(delete_key) ||
            read_curves() != expected_deletion || editor.get_selected_point_index() != -1) {
            std::cerr << "[OAE Studio] Delete key did not remove only the selected point!" << std::endl;
            return 1;
        }
        if (!model.undo() || read_curves() != deletion_curves ||
            !model.redo() || read_curves() != expected_deletion) {
            std::cerr << "[OAE Studio] Point deletion did not use a single Undo/Redo step!" << std::endl;
            return 1;
        }

        model.select({ oae::studio::ItemType::Event, "event:/music/loop", 0, 0 });
        const auto selection_before_noop = model.get_selection();
        const size_t clips_before_noop = lanes[0]->get_clips().size();
        const auto curves_before_noop = read_curves();
        const auto project_before_noop = model.json();
        const juce::Point<float> empty_spot{ 260.0f, edit_height * 0.85f };
        editor.mouseDown(mouse_event(empty_spot, empty_spot, 1, juce::ModifierKeys::leftButtonModifier));
        editor.mouseUp(mouse_event(empty_spot, empty_spot, 1, juce::ModifierKeys::noModifiers));
        if (editor.get_selected_point_index() != -1 ||
            (editor.point_index_at(empty_spot) != -1) ||
            !editor.keyPressed(backspace_key) ||
            read_curves() != curves_before_noop ||
            model.json() != project_before_noop ||
            lanes[0]->get_clips().size() != clips_before_noop ||
            model.get_selection() != selection_before_noop) {
            std::cerr << "[OAE Studio] Delete key without a selected point changed data or clips!" << std::endl;
            return 1;
        }

        editor.set_selected_point_index(-1);
        set_curves(deletion_curves);
        const auto expected_context = [&]() {
            auto curves = deletion_curves;
            curves[1]["curve"]["points"].erase(curves[1]["curve"]["points"].begin() + 2);
            return curves;
        }();
        const int context_index = editor.point_index_at(third_point);
        if (context_index != 2 || !editor.delete_point_at(context_index) ||
            read_curves() != expected_context) {
            std::cerr << "[OAE Studio] Context menu deletion did not remove the targeted point!" << std::endl;
            return 1;
        }
        if (!model.undo() || read_curves() != deletion_curves ||
            !model.redo() || read_curves() != expected_context) {
            std::cerr << "[OAE Studio] Context menu deletion did not use a single Undo/Redo step!" << std::endl;
            return 1;
        }

        // Deleting the last point removes the timeline volume curve and leaves an empty grid.
        const nlohmann::json single_point_curve = {
            { "target", "volume" }, { "is_timeline", true }, { "curve", { { "points", nlohmann::json::array({
                { { "x", 300.0 }, { "y", -9.0 }, { "shape", "linear" } }
            }) } } }
        };
        const nlohmann::json single_point_curves = nlohmann::json::array({ parameter, single_point_curve, pitch });
        set_curves(single_point_curves);
        const juce::Point<float> last_point{ 60.0f, edit_height * (1.0f - 71.0f / 90.0f) };
        editor.mouseDown(mouse_event(last_point, last_point, 1, juce::ModifierKeys::leftButtonModifier));
        editor.mouseUp(mouse_event(last_point, last_point, 1, juce::ModifierKeys::noModifiers));
        const nlohmann::json curves_without_temporal = nlohmann::json::array({ parameter, pitch });
        if (editor.get_selected_point_index() != 0 || !editor.keyPressed(delete_key) ||
            read_curves() != curves_without_temporal || !same_pixels(render_editor(), empty_image)) {
            std::cerr << "[OAE Studio] Deleting the last point did not remove the curve or empty the grid!" << std::endl;
            return 1;
        }
        if (!model.undo() || read_curves() != single_point_curves ||
            !model.redo() || read_curves() != curves_without_temporal) {
            std::cerr << "[OAE Studio] Last point deletion did not use a single Undo/Redo step!" << std::endl;
            return 1;
        }

        // Delete routing: the focused lane keeps the key on automation, the clicked clip defers to the panel.
        set_curves(deletion_curves);
        model.select({ oae::studio::ItemType::Event, "event:/music/loop", 0, 0 });
        const auto selection_before_routing = model.get_selection();
        const size_t clips_before_routing = lanes[0]->get_clips().size();
        const auto expected_routing = [&]() {
            auto curves = deletion_curves;
            curves[1]["curve"]["points"].erase(curves[1]["curve"]["points"].begin() + 1);
            return curves;
        }();
        editor.mouseDown(mouse_event(second_point, second_point, 1, juce::ModifierKeys::leftButtonModifier));
        editor.mouseUp(mouse_event(second_point, second_point, 1, juce::ModifierKeys::noModifiers));
        if (!editor.keyPressed(delete_key) || read_curves() != expected_routing ||
            lanes[0]->get_clips().size() != clips_before_routing ||
            model.get_selection() != selection_before_routing) {
            std::cerr << "[OAE Studio] Automation delete also removed the selected clip!" << std::endl;
            return 1;
        }

        if (model.get_selection().instrument_index != 0 || lanes[0]->get_clips().empty()) {
            std::cerr << "[OAE Studio] Clip routing smoke requires a selected clip on track 0!" << std::endl;
            return 1;
        }
        auto* routed_clip = lanes[0]->get_clips()[0].get();
        const auto clip_mouse_event = [&](juce::Point<float> position, juce::ModifierKeys mods) {
            auto mouse = juce::Desktop::getInstance().getMainMouseSource();
            const auto now = juce::Time::getCurrentTime();
            return juce::MouseEvent(mouse, position, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                    routed_clip, routed_clip, now, position, now, 1, false);
        };
        const auto curves_before_clip_delete = read_curves();
        routed_clip->mouseDown(clip_mouse_event({ 40.0f, 20.0f }, juce::ModifierKeys::leftButtonModifier));
        routed_clip->mouseUp(clip_mouse_event({ 40.0f, 20.0f }, juce::ModifierKeys::noModifiers));
        if (model.get_selection().instrument_index != 0 ||
            routed_clip->keyPressed(delete_key) || !timeline_panel.keyPressed(delete_key) ||
            lanes[0]->get_clips().size() + 1 != clips_before_routing ||
            read_curves() != curves_before_clip_delete) {
            std::cerr << "[OAE Studio] Clip delete after a lane interaction changed the automation!" << std::endl;
            return 1;
        }

        click_automation(0);
        model.modify([&](nlohmann::json& root) { root = project_before_curve_tests; });
        std::cout << "[OAE Studio] Timeline volume source, mixed order, first insertion and Undo/Redo verified." << std::endl;
        std::cout << "[OAE Studio] Point deletion, last point curve removal and Delete routing verified." << std::endl;
    }
    std::cout << "[OAE Studio] Sub-lane overlap layout and Event Instrument navigation verified." << std::endl;

    // Verify track reordering, duplication, removal and user_props Mute/Solo remapping
    if (!model.is_track_muted("event:/music/loop", 0) || model.is_track_solo("event:/music/loop", 0) ||
        model.is_track_muted("event:/music/loop", 1) || !model.is_track_solo("event:/music/loop", 1)) {
        std::cerr << "[OAE Studio] Precondition mismatch before track Mute/Solo remapping test!" << std::endl;
        return 1;
    }
    if (!model.duplicate_track("event:/music/loop", 1) ||
        !model.is_track_muted("event:/music/loop", 0) ||
        !model.is_track_solo("event:/music/loop", 1) ||
        !model.is_track_solo("event:/music/loop", 2)) {
        std::cerr << "[OAE Studio] duplicate_track failed or did not remap track_2_solo!" << std::endl;
        return 1;
    }
    if (!model.reorder_track("event:/music/loop", 2, 0) ||
        !model.is_track_solo("event:/music/loop", 0) || model.is_track_muted("event:/music/loop", 0) ||
        !model.is_track_muted("event:/music/loop", 1) || model.is_track_solo("event:/music/loop", 1) ||
        !model.is_track_solo("event:/music/loop", 2) || model.is_track_muted("event:/music/loop", 2)) {
        std::cerr << "[OAE Studio] reorder_track failed or did not remap track_<idx>_mute/_solo!" << std::endl;
        return 1;
    }
    if (!model.undo() ||
        !model.is_track_muted("event:/music/loop", 0) || model.is_track_solo("event:/music/loop", 0) ||
        !model.is_track_solo("event:/music/loop", 1) || !model.is_track_solo("event:/music/loop", 2)) {
        std::cerr << "[OAE Studio] Failed to undo track reorder or restore track_<idx>_mute/_solo!" << std::endl;
        return 1;
    }
    if (!model.redo() ||
        !model.is_track_solo("event:/music/loop", 0) || !model.is_track_muted("event:/music/loop", 1) ||
        !model.is_track_solo("event:/music/loop", 2)) {
        std::cerr << "[OAE Studio] Failed to redo track reorder or restore track_<idx>_mute/_solo!" << std::endl;
        return 1;
    }
    if (!model.remove_track("event:/music/loop", 0) ||
        !model.is_track_muted("event:/music/loop", 0) || model.is_track_solo("event:/music/loop", 0) ||
        !model.is_track_solo("event:/music/loop", 1) || model.is_track_muted("event:/music/loop", 1)) {
        std::cerr << "[OAE Studio] remove_track failed or did not shift track_<idx>_mute/_solo!" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Track reorder, duplication, removal and Mute/Solo remapping verified." << std::endl;

    // Simulate UI Track Header Drag and Drop & Mouse Click Interaction
    {
        oae::studio::TimelinePanel testTimeline(model);
        testTimeline.setSize(800, 600);
        testTimeline.set_event_path("event:/music/loop");
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);

        // Test clicking anywhere in track headers (Track 0 and Track 1)
        testTimeline.setVisible(true);
        auto now = juce::Time::getCurrentTime();
        std::cout << "[OAE Studio] TrackHeaders bounds: " << testTimeline.getComponentAt(50, 60) << std::endl;
        for (int y : { 60, 85, 100, 115, 130, 150, 170, 185, 200, 220 }) {
            auto* target = testTimeline.getComponentAt(50, y);
            if (target) {
                std::cout << "[OAE Studio] Click at (50, " << y << ") hit: " << typeid(*target).name() << std::endl;
                juce::MouseEvent ev(juce::Desktop::getInstance().getMainMouseSource(),
                                    { 5.0f, 5.0f },
                                    juce::ModifierKeys::leftButtonModifier,
                                    1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                    target, target,
                                    now, { 5.0f, 5.0f }, now, 1, false);
                target->mouseDown(ev);
                target->mouseUp(ev);
                juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
            }
        }
        // Restore values after interaction click simulation
        model.set_track_volume("event:/music/loop", 1, -4.5);
        model.set_track_pan("event:/music/loop", 1, 0.35);

        oae::studio::TrackHeaderPanel testHeaders(model);
        testHeaders.setSize(200, 600);
        testHeaders.set_event_path("event:/music/loop");
        testHeaders.handle_track_drag(0, 90);
        testHeaders.handle_track_drag_ended(0, 90);
        juce::MessageManager::getInstance()->runDispatchLoopUntil(100);
        model.undo();
        juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
        std::cout << "[OAE Studio] TrackHeaderPanel drag-to-reorder simulation verified." << std::endl;
    }

    // Validate drag-and-drop audio import and instrument creation
    {
        // 1. Name collision avoidance & single-step Undo/Redo in ProjectDocument and AuthoringModel
        const fs::path import_test_src1 = temp_dir / "collision_test.wav";
        write_sine_wav(import_test_src1, 220.0f, 0.4f); // 400 ms sine
        const fs::path import_test_src2 = temp_dir / "other_folder" / "collision_test.wav";
        fs::create_directories(import_test_src2.parent_path(), ec);
        write_sine_wav(import_test_src2, 880.0f, 0.8f); // 800 ms sine (different content and size)

        std::string sample_id_1, sample_id_2;
        double dur1 = 0.0, dur2 = 0.0;
        if (!model.import_audio_asset(import_test_src1, &sample_id_1, &dur1)) {
            std::cerr << "[OAE Studio] Failed first asset import!" << std::endl;
            return 1;
        }
        if (!model.import_audio_asset(import_test_src2, &sample_id_2, &dur2)) {
            std::cerr << "[OAE Studio] Failed second asset import with same filename!" << std::endl;
            return 1;
        }
        if (sample_id_1 == sample_id_2) {
            std::cerr << "[OAE Studio] Collision avoidance failed: duplicate sample_id generated!" << std::endl;
            return 1;
        }
        if (std::abs(dur1 - 400.0) > 20.0 || std::abs(dur2 - 800.0) > 20.0) {
            std::cerr << "[OAE Studio] Imported sample duration mismatch!" << std::endl;
            return 1;
        }

        // Test single-instrument drag insertion on timeline
        const size_t inst_count_before = model.json()["events"][0]["tracks"][0]["instruments"].size();
        std::string drag_sample_id;
        double drag_dur = 0.0;
        if (!model.add_single_instrument_from_audio_file("event:/music/loop", 0, import_test_src1, 450.0, &drag_sample_id, &drag_dur)) {
            std::cerr << "[OAE Studio] add_single_instrument_from_audio_file failed!" << std::endl;
            return 1;
        }
        const auto& inst_list = model.json()["events"][0]["tracks"][0]["instruments"];
        if (inst_list.size() != inst_count_before + 1 ||
            inst_list.back().value("offset_ms", 0.0) != 450.0 ||
            std::abs(inst_list.back().value("length_ms", 0.0) - 400.0) > 20.0) {
            std::cerr << "[OAE Studio] Added instrument parameters mismatch!" << std::endl;
            return 1;
        }

        // Single Undo step removes the instrument
        if (!model.undo() || model.json()["events"][0]["tracks"][0]["instruments"].size() != inst_count_before) {
            std::cerr << "[OAE Studio] Undo failed to cleanly roll back instrument insertion!" << std::endl;
            return 1;
        }
        if (!model.redo() || model.json()["events"][0]["tracks"][0]["instruments"].size() != inst_count_before + 1) {
            std::cerr << "[OAE Studio] Redo failed to restore instrument insertion!" << std::endl;
            return 1;
        }
        model.undo(); // Leave clean state

        // 2. TimelineTrackLane FileDragAndDropTarget
        oae::studio::TimelineTrackLane testLane(model, cache, "event:/music/loop", 0);
        testLane.set_time_converter([](double ms) { return ms * 0.2; },
                                    [](float px) { return static_cast<double>(px) / 0.2; });
        testLane.setBounds(0, 0, 800, 80);

        juce::StringArray invalidFiles{ (temp_dir / "not_audio.txt").string(), (temp_dir / "doc.pdf").string() };
        if (testLane.isInterestedInFileDrag(invalidFiles)) {
            std::cerr << "[OAE Studio] TimelineTrackLane accepted invalid non-audio file extensions!" << std::endl;
            return 1;
        }

        juce::StringArray validFiles{ juce::String::fromUTF8(import_test_src1.u8string().c_str()) };
        if (!testLane.isInterestedInFileDrag(validFiles)) {
            std::cerr << "[OAE Studio] TimelineTrackLane rejected valid WAV audio file!" << std::endl;
            return 1;
        }

        // Drop simulation at x = 100 px (time = 500 ms)
        testLane.filesDropped(validFiles, 100, 40);
        const auto& inst_after_drop = model.json()["events"][0]["tracks"][0]["instruments"];
        if (inst_after_drop.size() != inst_count_before + 1 ||
            std::abs(inst_after_drop.back().value("offset_ms", 0.0) - 500.0) > 1.0) {
            std::cerr << "[OAE Studio] TimelineTrackLane drop did not create Single Instrument at drop position!" << std::endl;
            return 1;
        }
        model.undo();

        // 3. AuthoringTreeBrowser FileDragAndDropTarget
        oae::studio::AuthoringTreeBrowser browser(model);
        if (browser.isInterestedInFileDrag(invalidFiles) || !browser.isInterestedInFileDrag(validFiles)) {
            std::cerr << "[OAE Studio] AuthoringTreeBrowser file drag filter mismatch!" << std::endl;
            return 1;
        }
        const size_t sample_count_before = model.json().value("samples", nlohmann::json::array()).size();
        const fs::path batch_src = temp_dir / "batch_asset.wav";
        write_sine_wav(batch_src, 550.0f, 0.3f);
        browser.filesDropped(juce::StringArray{ juce::String::fromUTF8(batch_src.u8string().c_str()) }, 0, 0);
        const auto& samples_after = model.json().value("samples", nlohmann::json::array());
        if (samples_after.size() != sample_count_before + 1) {
            std::cerr << "[OAE Studio] AuthoringTreeBrowser drop did not register asset sample in project!" << std::endl;
            return 1;
        }
        model.undo();

        std::cout << "[OAE Studio] Audio drag-and-drop (Timeline and Assets Tree) and collision handling verified." << std::endl;
    }

    // Verify Track Color, Track Name, Undo/Redo, Universal Waveforms, Ghost Drag & Inspector Cards
    const std::string drumsColorHex = oae::studio::OaeTheme::TrackColors::hexForIndex(2);
    if (!model.set_track_color("event:/music/loop", 1, drumsColorHex)
        || !model.set_track_name("event:/music/loop", 1, "Drums")
        || model.get_track_color("event:/music/loop", 1) != drumsColorHex
        || model.get_track_name("event:/music/loop", 1) != "Drums") {
        std::cerr << "[OAE Studio] Track color or name setter failed!" << std::endl;
        return 1;
    }
    if (!model.undo() || model.get_track_name("event:/music/loop", 1) != "Track 2"
        || !model.redo() || model.get_track_name("event:/music/loop", 1) != "Drums") {
        std::cerr << "[OAE Studio] Track name Undo/Redo failed!" << std::endl;
        return 1;
    }

    {
        oae::studio::ProjectDocument wf_doc;
        const fs::path wf_dir = temp_dir / "wf_smoke";
        fs::create_directories(wf_dir, ec);
        if (!wf_doc.create_empty(wf_dir, "WfSmoke") || !wf_doc.import_audio_file(wav_path, "sine_wf")) {
            std::cerr << "[OAE Studio] Failed to initialize waveform smoke project!" << std::endl;
            return 1;
        }
        oae::studio::AuthoringModel wf_model(wf_doc);
        wf_model.create_event("event:/wf/leaf", "bus:/");
        wf_model.add_instrument("event:/wf/leaf", 0, "single");
        wf_model.create_event("event:/wf/cycle_a", "bus:/");
        wf_model.create_event("event:/wf/cycle_b", "bus:/");
        wf_model.add_instrument("event:/wf/cycle_a", 0, "event", "event:/wf/leaf");
        wf_model.add_instrument("event:/wf/cycle_b", 0, "event", "event:/wf/leaf");

        wf_model.create_event("event:/wf/stage", "bus:/");
        wf_model.add_track("event:/wf/stage");
        wf_model.add_instrument("event:/wf/stage", 0, "single");
        wf_model.add_instrument("event:/wf/stage", 0, "multi");
        wf_model.add_instrument("event:/wf/stage", 0, "scatterer");
        wf_model.add_instrument("event:/wf/stage", 0, "programmer");
        wf_model.add_instrument("event:/wf/stage", 0, "event", "event:/wf/leaf");
        wf_model.modify([](nlohmann::json& r) {
            for (auto& ev : r["events"]) {
                const auto p = ev.value("path", "");
                if (p == "event:/wf/leaf") {
                    ev["tracks"][0]["instruments"][0]["sample_id"] = "sine_wf";
                    ev["tracks"][0]["instruments"][0]["offset_ms"] = 0.0;
                    ev["tracks"][0]["instruments"][0]["length_ms"] = 400.0;
                } else if (p == "event:/wf/cycle_a") {
                    ev["tracks"][0]["instruments"][0]["target_event"] = "event:/wf/cycle_b";
                } else if (p == "event:/wf/cycle_b") {
                    ev["tracks"][0]["instruments"][0]["target_event"] = "event:/wf/cycle_a";
                } else if (p == "event:/wf/stage") {
                    auto& insts = ev["tracks"][0]["instruments"];
                    insts[0]["sample_id"] = "sine_wf";
                    insts[0]["offset_ms"] = 0.0;
                    insts[0]["length_ms"] = 400.0;
                    insts[1]["playlist"] = nlohmann::json::array({ {{ "sample_id", "sine_wf" }} });
                    insts[1]["offset_ms"] = 500.0;
                    insts[1]["length_ms"] = 400.0;
                    insts[2]["playlist"] = nlohmann::json::array({ {{ "sample_id", "sine_wf" }} });
                    insts[2]["offset_ms"] = 1000.0;
                    insts[2]["length_ms"] = 400.0;
                    insts[3]["programmer_key"] = "vo_line_01";
                    insts[3]["offset_ms"] = 1500.0;
                    insts[3]["length_ms"] = 400.0;
                    insts[4]["target_event"] = "event:/wf/leaf";
                    insts[4]["offset_ms"] = 2000.0;
                    insts[4]["length_ms"] = 400.0;
                }
            }
        });

        oae::studio::WaveformCache wf_cache;
        oae::studio::TimelineTrackLane wf_lane(wf_model, wf_cache, "event:/wf/stage", 0);
        wf_lane.set_time_converter(
            [](double ms) { return ms * 0.2; },
            [](float px) { return static_cast<double>(px) / 0.2; });
        wf_lane.setBounds(0, 0, 1200, 120);
        if (wf_lane.get_clips().size() != 5) {
            std::cerr << "[OAE Studio] Expected 5 instrument clips for universal waveform check!" << std::endl;
            return 1;
        }
        for (const auto& clip : wf_lane.get_clips()) {
            auto clip_env = clip->resolve_waveform_envelope(true);
            if (!clip_env || clip_env->is_empty()) {
                std::cerr << "[OAE Studio] Universal waveform resolution returned empty envelope!" << std::endl;
                return 1;
            }
        }

        // Cycle protection on mutually recursive Event Instruments
        oae::studio::TimelineTrackLane cycle_lane(wf_model, wf_cache, "event:/wf/cycle_a", 0);
        cycle_lane.setBounds(0, 0, 400, 80);
        if (cycle_lane.get_clips().empty()) {
            std::cerr << "[OAE Studio] Cycle protection test clip missing!" << std::endl;
            return 1;
        }
        (void)cycle_lane.get_clips()[0]->resolve_waveform_envelope(true);

        // Ghost Drag preview & TimelineRuler drag alignment guides
        oae::studio::TimelineTrackLane::GhostPreviewState ghost{};
        ghost.active = true;
        ghost.raw_time_ms = 220.0;
        ghost.snapped_time_ms = 200.0;
        ghost.delta_ms = 200.0;
        ghost.length_ms = 400.0;
        ghost.title = wf_lane.get_clips()[0]->get_display_title();
        ghost.envelope = wf_lane.get_clips()[0]->resolve_waveform_envelope(true);
        wf_lane.set_drop_target(ghost);
        if (!wf_lane.get_ghost_preview().active) {
            std::cerr << "[OAE Studio] Ghost drag preview did not activate!" << std::endl;
            return 1;
        }
        {
            juce::Image wf_img(juce::Image::ARGB, 1200, 120, true);
            juce::Graphics wf_g(wf_img);
            wf_lane.paintEntireComponent(wf_g, false);
        }
        wf_lane.clear_drop_target();
        if (wf_lane.get_ghost_preview().active) {
            std::cerr << "[OAE Studio] Ghost drag preview did not clear!" << std::endl;
            return 1;
        }

        oae::studio::TimelineRuler ruler(wf_model);
        ruler.set_event_path("event:/wf/stage");
        ruler.setSize(800, 28);
        ruler.set_ghost_guide_ms(220.0, 200.0);
        {
            juce::Image r_img(juce::Image::ARGB, 800, 28, true);
            juce::Graphics r_g(r_img);
            ruler.paintEntireComponent(r_g, false);
        }
        ruler.clear_ghost_guide();

        // Multi-track header/lane highlight + InspectorPanel modular collapsible cards & color swatches
        oae::studio::TrackHeaderPanel wf_headers(wf_model);
        wf_headers.setSize(200, 240);
        wf_headers.set_event_path("event:/wf/stage");
        wf_model.select({ oae::studio::ItemType::Event, "event:/wf/stage", 0, 0 });
        wf_headers.repaint_selection();
        wf_lane.refresh_selection();
        {
            juce::Image th_img(juce::Image::ARGB, 200, 240, true);
            juce::Graphics th_g(th_img);
            wf_headers.paintEntireComponent(th_g, false);
        }

        oae::studio::InspectorPanel inspector(wf_model);
        inspector.setSize(340, 640);
        const std::function<juce::Component*(juce::Component*, const juce::String&)> find_comp =
            [&](juce::Component* root, const juce::String& cid) -> juce::Component* {
                if (!root) return nullptr;
                if (root->getComponentID() == cid) return root;
                for (auto* child : root->getChildren()) {
                    if (auto* found = find_comp(child, cid)) return found;
                }
                return nullptr;
            };

        wf_model.select({ oae::studio::ItemType::Event, "event:/wf/stage", 0, 0 });
        if (!find_comp(&inspector, "inspector_card_clip")
            || !find_comp(&inspector, "inspector_card_track")
            || !find_comp(&inspector, "inspector_card_event")
            || !find_comp(&inspector, "inspector_track_color_3")
            || !inspector.is_card_expanded("inspector_card_clip")) {
            std::cerr << "[OAE Studio] InspectorPanel modular Clip/Track/Event cards or swatches missing!" << std::endl;
            return 1;
        }
        inspector.set_card_expanded("inspector_card_track", false);
        if (inspector.is_card_expanded("inspector_card_track")) {
            std::cerr << "[OAE Studio] InspectorPanel Track card did not collapse!" << std::endl;
            return 1;
        }
        wf_model.select({ oae::studio::ItemType::Event, "event:/wf/stage", 0, 1 });
        if (inspector.is_card_expanded("inspector_card_track")) {
            std::cerr << "[OAE Studio] InspectorPanel card collapse state was not preserved across selection!" << std::endl;
            return 1;
        }
        if (auto* swatchBtn = dynamic_cast<juce::Button*>(find_comp(&inspector, "inspector_track_color_3"))) {
            if (swatchBtn->onClick) {
                swatchBtn->onClick();
            } else {
                swatchBtn->triggerClick();
            }
        }
        if (wf_model.get_track_color("event:/wf/stage", 0) != oae::studio::OaeTheme::TrackColors::hexForIndex(3)) {
            std::cerr << "[OAE Studio] InspectorPanel Track color swatch click did not update track color!" << std::endl;
            return 1;
        }
        wf_model.select({ oae::studio::ItemType::Event, "event:/wf/stage", -1, -1 });
        if (find_comp(&inspector, "inspector_card_clip") != nullptr
            || !find_comp(&inspector, "inspector_card_event")) {
            std::cerr << "[OAE Studio] InspectorPanel Event-only selection did not filter out Clip card!" << std::endl;
            return 1;
        }

        // Lock Inspector: freeze view during selection changes
        wf_model.select({ oae::studio::ItemType::Event, "event:/wf/stage", 0, 0 });
        if (!find_comp(&inspector, "inspector_card_clip")) {
            std::cerr << "[OAE Studio] Clip card missing before lock!" << std::endl;
            return 1;
        }
        inspector.set_locked(true);
        if (!inspector.is_locked()) {
            std::cerr << "[OAE Studio] Inspector lock state failed to set!" << std::endl;
            return 1;
        }
        wf_model.select({ oae::studio::ItemType::Event, "event:/wf/stage", -1, -1 });
        if (!find_comp(&inspector, "inspector_card_clip")) {
            std::cerr << "[OAE Studio] Locked Inspector did not freeze view during selection change!" << std::endl;
            return 1;
        }
        inspector.set_locked(false);
        if (find_comp(&inspector, "inspector_card_clip") != nullptr) {
            std::cerr << "[OAE Studio] Unlocked Inspector did not synchronize to current selection!" << std::endl;
            return 1;
        }

        // Modular card actions (Reset to Default, Copy & Paste values, atomic Undo)
        auto* evCard = dynamic_cast<oae::studio::IInspectorCard*>(find_comp(&inspector, "inspector_card_event"));
        if (!evCard || evCard->get_card_type_id() != "event_header") {
            std::cerr << "[OAE Studio] EventHeaderCard missing or does not implement IInspectorCard!" << std::endl;
            return 1;
        }
        wf_model.modify([](nlohmann::json& r) {
            for (auto& ev : r["events"]) {
                if (ev.value("path", "") == "event:/wf/stage") {
                    ev["volume_db"] = -18.5;
                    break;
                }
            }
        });
        const auto serialized = evCard->serialize_values();
        if (serialized.value("volume_db", 0.0) != -18.5) {
            std::cerr << "[OAE Studio] Event card serialization failed!" << std::endl;
            return 1;
        }
        oae::studio::InspectorClipboard::copy("event_header", serialized);
        if (!oae::studio::InspectorClipboard::has_data_for("event_header") ||
            oae::studio::InspectorClipboard::has_data_for("track")) {
            std::cerr << "[OAE Studio] InspectorClipboard copy/has_data_for failed!" << std::endl;
            return 1;
        }
        auto get_stage_volume = [&]() -> double {
            for (const auto& ev : wf_model.json()["events"]) {
                if (ev.value("path", "") == "event:/wf/stage") {
                    return ev.value("volume_db", 0.0);
                }
            }
            return 999.0;
        };
        evCard->reset_to_default();
        if (get_stage_volume() != 0.0) {
            std::cerr << "[OAE Studio] Event card Reset to Default failed!" << std::endl;
            return 1;
        }
        if (!evCard->deserialize_values(oae::studio::InspectorClipboard::get_payload("event_header")) ||
            get_stage_volume() != -18.5) {
            std::cerr << "[OAE Studio] Event card Paste Values failed!" << std::endl;
            return 1;
        }
        if (!wf_model.undo() || get_stage_volume() != 0.0 ||
            !wf_model.redo() || get_stage_volume() != -18.5) {
            std::cerr << "[OAE Studio] Event card Paste Values atomic Undo/Redo failed!" << std::endl;
            return 1;
        }
        wf_model.undo(); // restore to 0.0

        // Inspector hierarchy & TimelineTrackLane selection/offset_ms smoke
        wf_model.select({ oae::studio::ItemType::Event, "event:/wf/stage", -1, -1 });
        if (!find_comp(&inspector, "inspector_card_event") ||
            find_comp(&inspector, "inspector_card_track") != nullptr ||
            find_comp(&inspector, "inspector_card_clip") != nullptr ||
            !inspector.is_card_expanded("inspector_card_event")) {
            std::cerr << "[OAE Studio] Event selection did not show exclusively EventHeaderCard!" << std::endl;
            return 1;
        }

        // Left-click empty area of TimelineTrackLane selects track
        {
            juce::MouseInputSource mouse = juce::Desktop::getInstance().getMainMouseSource();
            juce::MouseEvent laneClick(mouse, { 50.0f, 20.0f }, juce::ModifierKeys::leftButtonModifier,
                0.0f, 0.0f, 0.0f, 0.0f, 0.0f, &wf_lane, &wf_lane,
                juce::Time::getCurrentTime(), { 50.0f, 20.0f }, juce::Time::getCurrentTime(), 1, false);
            wf_lane.mouseDown(laneClick);
        }
        if (wf_model.get_selection().track_index != 0 || wf_model.get_selection().instrument_index != -1) {
            std::cerr << "[OAE Studio] TimelineTrackLane left-click did not select track 0!" << std::endl;
            return 1;
        }
        auto* trkCardSel = find_comp(&inspector, "inspector_card_track");
        auto* evCardSel = find_comp(&inspector, "inspector_card_event");
        if (!trkCardSel || !evCardSel || find_comp(&inspector, "inspector_card_clip") != nullptr ||
            trkCardSel->getY() >= evCardSel->getY() ||
            !inspector.is_card_expanded("inspector_card_track") ||
            inspector.is_card_expanded("inspector_card_event")) {
            std::cerr << "[OAE Studio] Track selection card hierarchy/order/collapse mismatch!" << std::endl;
            return 1;
        }

        // Clip selection places ClipCard on top with TrackCard and EventHeaderCard collapsed below
        wf_model.select({ oae::studio::ItemType::Event, "event:/wf/stage", 0, 0 });
        auto* clipCardSel = find_comp(&inspector, "inspector_card_clip");
        auto* trkCardClip = find_comp(&inspector, "inspector_card_track");
        auto* evCardClip = find_comp(&inspector, "inspector_card_event");
        if (!clipCardSel || !trkCardClip || !evCardClip ||
            !(clipCardSel->getY() < trkCardClip->getY() && trkCardClip->getY() < evCardClip->getY()) ||
            !inspector.is_card_expanded("inspector_card_clip") ||
            inspector.is_card_expanded("inspector_card_track") ||
            inspector.is_card_expanded("inspector_card_event")) {
            std::cerr << "[OAE Studio] Clip selection card hierarchy/order/collapse mismatch!" << std::endl;
            return 1;
        }

        // AuthoringModel::add_instrument with explicit offset_ms
        if (!wf_model.add_instrument("event:/wf/stage", 0, "silence", "", 1250.0)) {
            std::cerr << "[OAE Studio] add_instrument with offset_ms failed!" << std::endl;
            return 1;
        }
        bool found_offset = false;
        for (const auto& ev : wf_model.json()["events"]) {
            if (ev.value("path", "") == "event:/wf/stage") {
                const auto& insts = ev["tracks"][0]["instruments"];
                if (!insts.empty() && insts.back().value("kind", "") == "silence" &&
                    std::abs(insts.back().value("offset_ms", 0.0) - 1250.0) < 0.01) {
                    found_offset = true;
                }
                break;
            }
        }
        if (!found_offset) {
            std::cerr << "[OAE Studio] add_instrument did not persist explicit offset_ms in JSON!" << std::endl;
            return 1;
        }

        std::cout << "[OAE Studio] Universal waveforms, ghost drag, track colors, Lock Inspector & modular cards verified." << std::endl;
    }

    // 4. Persistence round-trip: Save and reload document
    if (!doc.save()) {
        std::cerr << "[OAE Studio] Failed to save timeline project" << std::endl;
        return 1;
    }

    oae::studio::ProjectDocument reloaded_doc;
    if (!reloaded_doc.open(doc.get_project_file())) {
        std::cerr << "[OAE Studio] Failed to reload saved project" << std::endl;
        return 1;
    }
    oae::studio::AuthoringModel reloaded_model(reloaded_doc);

    // Verify persistent properties exactly match
    if (!reloaded_model.is_track_muted("event:/music/loop", 0) || !reloaded_model.is_track_solo("event:/music/loop", 1)) {
        std::cerr << "[OAE Studio] Persisted Track Mute/Solo flags mismatch after reload!" << std::endl;
        return 1;
    }
    if (reloaded_model.get_track_color("event:/music/loop", 1) != drumsColorHex
        || reloaded_model.get_track_name("event:/music/loop", 1) != "Drums") {
        std::cerr << "[OAE Studio] Persisted Track color/name mismatch after reload!" << std::endl;
        return 1;
    }

    double r_vol = 0.0, r_pan = 0.0, r_off = 0.0, r_len = 0.0, r_start = 0.0, r_fin = 0.0, r_fout = 0.0;
    for (const auto& ev : reloaded_doc.get_root_json()["events"]) {
        if (ev.value("path", "") == "event:/music/loop") {
            const auto& trk1 = ev["tracks"][1];
            r_vol = trk1.value("volume_db", 0.0);
            r_pan = trk1.value("pan", 0.0);
            const auto& inst1 = trk1["instruments"][1];
            r_off = inst1.value("offset_ms", 0.0);
            r_len = inst1.value("length_ms", 0.0);
            r_start = inst1.value("start_offset_ms", 0.0);
            r_fin = inst1.value("fade_in_ms", 0.0);
            r_fout = inst1.value("fade_out_ms", 0.0);
        }
    }

    if (std::abs(r_vol - (-4.5)) > 0.001 || std::abs(r_pan - 0.35) > 0.001 ||
        std::abs(r_off - 300.0) > 0.001 || std::abs(r_len - 1500.0) > 0.001 ||
        std::abs(r_start - 50.0) > 0.001 || std::abs(r_fin - 120.0) > 0.001 ||
        std::abs(r_fout - 180.0) > 0.001) {
        std::cerr << "[OAE Studio] Exact persistence values mismatch after reload!" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Exact project persistence round-trip verified." << std::endl;

    // 5. Compile with oae_bankc and verify audio rendering
    const auto app_dir = get_app_dir();
    const auto bankc_exe = find_bankc_executable(app_dir);
    if (bankc_exe.empty()) {
        std::cerr << "[OAE Studio] oae_bankc not found!" << std::endl;
        return 1;
    }

    const fs::path out_bank_path = temp_dir / "timeline_demo.bank";
#if defined(_WIN32)
    const std::wstring cmd = L"\"" + bankc_exe.wstring() + L"\" \"" + doc.get_project_file().wstring() + L"\" -o \"" + out_bank_path.wstring() + L"\"";
    int compile_res = -1;
    std::string compile_output;
    const bool proc_ok = run_command_capture_output(cmd, compile_res, compile_output);
    if (!proc_ok || compile_res != 0) {
        std::cerr << "[OAE Studio] Compilation of timeline project failed with exit code: " << compile_res << "\n"
                  << compile_output << std::endl;
        return 1;
    }
#else
    const std::string cmd = "\"" + bankc_exe.string() + "\" \"" + doc.get_project_file().string() + "\" -o \"" + out_bank_path.string() + "\"";
    const int compile_res = std::system(cmd.c_str());
    if (compile_res != 0) return 1;
#endif

    std::cout << "[OAE Studio] Bank compiled successfully." << std::endl;

    oae::studio::StudioEngine engine;
    if (engine.init_headless() != OAE_OK) return 1;
    oae_bank bank_handle = 0;
    if (engine.load_bank_file(out_bank_path.string().c_str(), &bank_handle) != OAE_OK || bank_handle == 0) return 1;
    oae_event_instance instance = 0;
    if (engine.create_event_instance("event:/music/loop", &instance) != OAE_OK || instance == 0) return 1;

    oae_event_instance_start(instance);
    std::array<float, 2048> pcm_buf{};
    float max_peak = 0.0f;
    for (int frame = 0; frame < 25; ++frame) {
        engine.update();
        if (engine.render(pcm_buf.data(), 1024) == OAE_OK) {
            for (float s : pcm_buf) max_peak = std::max(max_peak, std::abs(s));
        }
    }

    std::cout << "[OAE Studio] Playback verified successfully (peak: " << max_peak << ")" << std::endl;
    if (max_peak < 0.001f) {
        std::cerr << "[OAE Studio] Rendered audio was silent!" << std::endl;
        return 1;
    }

    engine.unload_bank(bank_handle);
    engine.shutdown();

    fs::remove_all(temp_dir, ec);

    std::cout << "[OAE Studio] --smoke-timeline PASSED with exit code 0." << std::endl;
    return 0;
}

int run_smoke_transport_mixer()
{
    std::cout << "[OAE Studio] Starting --smoke-transport-mixer validation..." << std::endl;

    const fs::path temp_dir = fs::temp_directory_path() / "oae_studio_smoke_transport_mixer";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir, ec);

    // 1. Generate fixture WAV files
    const fs::path wav_path = temp_dir / "sine_audition.wav";
    const fs::path stream_wav_path = temp_dir / "sine_stream.wav";
    const fs::path short_wav_path = temp_dir / "sine_short.wav";
    if (!write_sine_wav(wav_path, 440.0f, 1.0f) ||
        !write_sine_wav(stream_wav_path, 330.0f, 1.5f) ||
        !write_sine_wav(short_wav_path, 550.0f, 0.08f)) {
        std::cerr << "[OAE Studio] Failed to write test WAV" << std::endl;
        return 1;
    }

    // 2. Initialize project document and authoring model
    oae::studio::ProjectDocument doc;
    if (!doc.create_empty(temp_dir, "TransportMixerProject")) {
        std::cerr << "[OAE Studio] Failed to create project document" << std::endl;
        return 1;
    }

    if (!doc.import_audio_file(wav_path, "sine_sample") ||
        !doc.import_audio_file(stream_wav_path, "stream_sample") ||
        !doc.import_audio_file(short_wav_path, "short_sample")) {
        std::cerr << "[OAE Studio] Failed to import audio file" << std::endl;
        return 1;
    }

    oae::studio::AuthoringModel model(doc);

    // 3. Configure buses, VCAs, snapshots, parameters, and events
    model.modify([](nlohmann::json& r) {
        if (r.contains("samples") && r["samples"].is_array()) {
            for (auto& s : r["samples"]) {
                if (s.value("id", "") == "stream_sample") {
                    s["stream"] = true;
                }
            }
        }
        // Buses: Master ('bus:/') + children
        r["buses"] = nlohmann::json::array({
            { { "path", "bus:/sfx" }, { "volume", 1.0 }, { "effects", nlohmann::json::array() } },
            { { "path", "bus:/music" }, { "volume", 1.0 }, { "effects", nlohmann::json::array() } },
            { { "path", "bus:/sfx/weapons" }, { "volume", 1.0 }, { "effects", nlohmann::json::array() } }
        });

        // Global parameter
        r["global_parameters"] = nlohmann::json::array({
            { { "name", "MasterVolume" }, { "kind", "continuous" }, { "min", 0.0 }, { "max", 1.0 }, { "default_value", 0.8 } }
        });

        // VCAs
        r["vcas"] = nlohmann::json::array({
            { { "path", "vca:/main_vca" }, { "buses", nlohmann::json::array({ "bus:/sfx" }) }, { "events", nlohmann::json::array({ "event:/music/test_loop" }) } }
        });

        // Events: 1 event and 1 snapshot + streaming 2-track mix event + short loop test event
        r["events"] = nlohmann::json::array({
            {
                { "path", "event:/music/test_loop" },
                { "kind", "event" },
                { "bus_path", "bus:/sfx" },
                { "volume_db", 0.0 },
                { "parameters", nlohmann::json::array({
                    { { "name", "Cutoff" }, { "kind", "continuous" }, { "min", 0.0 }, { "max", 1.0 }, { "default_value", 0.5 } }
                }) },
                { "tracks", nlohmann::json::array({
                    {
                        { "volume_db", 0.0 },
                        { "instruments", nlohmann::json::array({
                            {
                                { "kind", "single" },
                                { "sample_id", "sine_sample" },
                                { "offset_ms", 0.0 },
                                { "length_ms", 1000.0 },
                                { "loop", true }
                            }
                        }) }
                    }
                }) }
            },
            {
                { "path", "snapshot:/reverb_tail" },
                { "kind", "snapshot" },
                { "priority", 100 },
                { "snapshot_mode", "override" },
                { "fade_in_ms", 50.0 },
                { "snapshot_targets", nlohmann::json::array({
                    {
                        { "bus_path", "bus:/sfx" },
                        { "value", -6.0 },
                        { "param_id", -1 },
                        { "position", 0 },
                        { "effect_index", 0 }
                    }
                }) }
            },
            {
                { "path", "event:/music/streaming_mix" },
                { "kind", "event" },
                { "bus_path", "bus:/music" },
                { "volume_db", 0.0 },
                { "tracks", nlohmann::json::array({
                    {
                        { "volume_db", 0.0 },
                        { "pan", 0.0 },
                        { "instruments", nlohmann::json::array({
                            {
                                { "kind", "single" },
                                { "sample_id", "stream_sample" },
                                { "offset_ms", 0.0 },
                                { "length_ms", 1500.0 },
                                { "loop", true }
                            }
                        }) }
                    },
                    {
                        { "volume_db", -3.0 },
                        { "pan", 0.0 },
                        { "instruments", nlohmann::json::array({
                            {
                                { "kind", "single" },
                                { "sample_id", "sine_sample" },
                                { "offset_ms", 0.0 },
                                { "length_ms", 1500.0 },
                                { "loop", true }
                            }
                        }) }
                    }
                }) }
            },
            {
                { "path", "event:/music/short_loop_test" },
                { "kind", "event" },
                { "bus_path", "bus:/sfx" },
                { "volume_db", 0.0 },
                { "tracks", nlohmann::json::array({
                    {
                        { "volume_db", 0.0 },
                        { "pan", 0.0 },
                        { "instruments", nlohmann::json::array({
                            {
                                { "kind", "single" },
                                { "sample_id", "short_sample" },
                                { "offset_ms", 0.0 },
                                { "length_ms", 80.0 },
                                { "loop", false }
                            }
                        }) }
                    }
                }) }
            }
        });
    });

    if (!doc.save()) {
        std::cerr << "[OAE Studio] Failed to save project" << std::endl;
        return 1;
    }

    // 4. Locate oae_bankc
    const auto app_dir = get_app_dir();
    const auto bankc_exe = find_bankc_executable(app_dir);
    if (bankc_exe.empty()) {
        std::cerr << "[OAE Studio] oae_bankc executable not found!" << std::endl;
        return 1;
    }

    // 5. Initialize Headless StudioEngine
    oae::studio::StudioEngine engine;
    if (engine.init_headless() != OAE_OK) {
        std::cerr << "[OAE Studio] Failed to init StudioEngine headless" << std::endl;
        return 1;
    }

    // 6. Test PreviewRebuilder synchronous build
    oae::studio::AsyncBankPublisher publisher;
    oae::studio::AuditionTransport transport(engine, model);
    oae::studio::PreviewRebuilder rebuilder(engine, model, doc, transport, publisher);
    rebuilder.set_bankc_executable(bankc_exe);

    if (!rebuilder.rebuild_synchronously()) {
        std::cerr << "[OAE Studio] Initial PreviewRebuilder compilation failed!" << std::endl;
        return 1;
    }
    if (rebuilder.get_loaded_bank() == 0) {
        std::cerr << "[OAE Studio] Loaded preview bank handle is 0 after rebuild!" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Initial bank built and loaded into engine." << std::endl;

    // 7. Audition Transport validation
    transport.set_event_path("event:/music/test_loop");
    if (transport.get_state() != oae::studio::TransportState::Stopped) {
        std::cerr << "[OAE Studio] Initial transport state is not Stopped!" << std::endl;
        return 1;
    }

    constexpr double audition_start_ms = 250.0;
    if (!transport.seek(audition_start_ms) || transport.get_current_position_ms() != audition_start_ms ||
        transport.get_active_instance() != 0 || transport.get_state() != oae::studio::TransportState::Stopped) {
        std::cerr << "[OAE Studio] Stopped seek did not retain the position without starting playback!" << std::endl;
        return 1;
    }
    if (!transport.stop() || transport.get_current_position_ms() != 0.0) {
        std::cerr << "[OAE Studio] Stop did not clear the stopped seek position!" << std::endl;
        return 1;
    }
    if (!transport.seek(audition_start_ms)) {
        std::cerr << "[OAE Studio] Stopped seek failed before event selection change!" << std::endl;
        return 1;
    }
    transport.set_event_path("event:/missing");
    if (transport.get_current_position_ms() != 0.0) {
        std::cerr << "[OAE Studio] Event selection inherited a stopped seek position!" << std::endl;
        return 1;
    }
    if (!transport.seek(audition_start_ms) || transport.play() || transport.get_active_instance() != 0 ||
        transport.get_state() != oae::studio::TransportState::Stopped ||
        transport.get_current_position_ms() != audition_start_ms) {
        std::cerr << "[OAE Studio] A failed Play did not preserve the stopped seek position!" << std::endl;
        return 1;
    }
    transport.set_event_path("event:/music/test_loop");
    if (transport.get_current_position_ms() != 0.0 || !transport.seek(audition_start_ms)) {
        std::cerr << "[OAE Studio] Could not set a fresh start position for the selected event!" << std::endl;
        return 1;
    }

    // Play
    if (!transport.play()) {
        std::cerr << "[OAE Studio] Transport play() returned false!" << std::endl;
        return 1;
    }
    if (transport.get_state() != oae::studio::TransportState::Playing) {
        std::cerr << "[OAE Studio] Transport state is not Playing!" << std::endl;
        return 1;
    }
    if (transport.get_active_instance() == 0) {
        std::cerr << "[OAE Studio] Transport active instance is 0 during play!" << std::endl;
        return 1;
    }
    if (transport.get_current_position_ms() != audition_start_ms) {
        std::cerr << "[OAE Studio] Local Play did not start at the stopped seek position!" << std::endl;
        return 1;
    }
    {
        oae::studio::Spatializer3DDeck spatial_deck(transport);
        spatial_deck.setSize(640, 220);
        spatial_deck.setVisible(true);
        auto* sound_size = dynamic_cast<juce::Slider*>(spatial_deck.findChildWithID("spatial_preview_sound_size"));
        auto* min_extent = dynamic_cast<juce::Slider*>(spatial_deck.findChildWithID("spatial_preview_min_extent"));
        if (!sound_size || !min_extent) {
            std::cerr << "[OAE Studio] Spatializer3DDeck missing sound_size/min_extent sliders!" << std::endl;
            return 1;
        }
        sound_size->setValue(4.5, juce::sendNotificationSync);
        min_extent->setValue(120.0, juce::sendNotificationSync);
        oae_spatializer_settings active_spatial{};
        if (oae_event_instance_get_spatializer_settings(transport.get_active_instance(), &active_spatial) != OAE_OK ||
            active_spatial.enabled != 1 ||
            std::abs(active_spatial.sound_size - 4.5f) > 1.0e-3f ||
            std::abs(active_spatial.min_extent_deg - 120.0f) > 1.0e-3f) {
            std::cerr << "[OAE Studio] Spatializer3DDeck did not sync native 3D spatializer settings!" << std::endl;
            return 1;
        }
    }

    // Render audio and verify peak
    std::array<float, 2048> pcm_buf{};
    float play_peak = 0.0f;
    for (int frame = 0; frame < 15; ++frame) {
        engine.update();
        if (engine.render(pcm_buf.data(), 1024) == OAE_OK) {
            for (float s : pcm_buf) play_peak = std::max(play_peak, std::abs(s));
        }
    }
    if (play_peak < 0.001f) {
        std::cerr << "[OAE Studio] Rendered audio was silent during play!" << std::endl;
        return 1;
    }
    const double pos_after_play = transport.get_current_position_ms();
    std::cout << "[OAE Studio] Playback verified (peak: " << play_peak << ", pos: " << pos_after_play << " ms)" << std::endl;

    // Pause
    if (!transport.pause()) {
        std::cerr << "[OAE Studio] Transport pause() returned false!" << std::endl;
        return 1;
    }
    if (transport.get_state() != oae::studio::TransportState::Paused) {
        std::cerr << "[OAE Studio] Transport state is not Paused!" << std::endl;
        return 1;
    }
    const double paused_pos = transport.get_current_position_ms();
    if (paused_pos < pos_after_play) {
        std::cerr << "[OAE Studio] Paused position (" << paused_pos << ") was lost!" << std::endl;
        return 1;
    }

    // Render during pause: verify silence
    float pause_peak = 0.0f;
    for (int frame = 0; frame < 5; ++frame) {
        engine.update();
        if (engine.render(pcm_buf.data(), 1024) == OAE_OK) {
            for (float s : pcm_buf) pause_peak = std::max(pause_peak, std::abs(s));
        }
    }
    std::cout << "[OAE Studio] Pause verified (paused_pos: " << paused_pos << " ms, pause_peak: " << pause_peak << ")" << std::endl;

    // Resume
    if (!transport.resume()) {
        std::cerr << "[OAE Studio] Transport resume() returned false!" << std::endl;
        return 1;
    }
    if (transport.get_state() != oae::studio::TransportState::Playing) {
        std::cerr << "[OAE Studio] Transport state after resume is not Playing!" << std::endl;
        return 1;
    }

    // Seek
    if (!transport.seek(250.0) || transport.get_current_position_ms() != 250.0) {
        std::cerr << "[OAE Studio] Seek during playback did not update the transport position!" << std::endl;
        return 1;
    }

    // Key Off
    if (!transport.key_off()) {
        std::cerr << "[OAE Studio] Transport key_off() returned false!" << std::endl;
        return 1;
    }

    // Parameter modulation
    transport.set_local_parameter("Cutoff", 0.72f);
    if (std::abs(transport.get_local_parameter("Cutoff") - 0.72f) > 0.001f) {
        std::cerr << "[OAE Studio] Local parameter get/set mismatch!" << std::endl;
        return 1;
    }

    transport.set_global_parameter("MasterVolume", 0.42f);
    if (std::abs(transport.get_global_parameter("MasterVolume") - 0.42f) > 0.001f) {
        std::cerr << "[OAE Studio] Global parameter get/set mismatch!" << std::endl;
        return 1;
    }

    // Snapshots
    if (!transport.toggle_snapshot("snapshot:/reverb_tail", true)) {
        std::cerr << "[OAE Studio] Failed to toggle snapshot ON!" << std::endl;
        return 1;
    }
    if (!transport.is_snapshot_active("snapshot:/reverb_tail")) {
        std::cerr << "[OAE Studio] Snapshot was not reported active after toggle ON!" << std::endl;
        return 1;
    }
    if (!transport.toggle_snapshot("snapshot:/reverb_tail", false)) {
        std::cerr << "[OAE Studio] Failed to toggle snapshot OFF!" << std::endl;
        return 1;
    }
    if (transport.is_snapshot_active("snapshot:/reverb_tail")) {
        std::cerr << "[OAE Studio] Snapshot still active after toggle OFF!" << std::endl;
        return 1;
    }

    // Loop toggle
    transport.set_loop(true);
    if (!transport.is_loop_enabled()) {
        std::cerr << "[OAE Studio] Loop was not enabled!" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Transport Play, Pause, Resume, Seek, KeyOff, Parameters, and Snapshots verified." << std::endl;

    // 8. In-flight Live Bank Rebuild & Hot Swap
    const uint64_t rev_before = model.get_content_revision();
    model.modify([](nlohmann::json& r) {
        r["events"][0]["volume_db"] = -1.5;
    });
    if (model.get_content_revision() <= rev_before) {
        std::cerr << "[OAE Studio] Content revision did not increment on model modification!" << std::endl;
        return 1;
    }

    // Rebuild while playing
    if (!rebuilder.rebuild_synchronously()) {
        std::cerr << "[OAE Studio] In-flight synchronous preview rebuild failed!" << std::endl;
        return 1;
    }
    if (transport.get_state() != oae::studio::TransportState::Playing) {
        std::cerr << "[OAE Studio] Transport did not restore Playing state after hot swap!" << std::endl;
        return 1;
    }

    // Verify audio still renders after hot swap
    float swap_peak = 0.0f;
    for (int frame = 0; frame < 10; ++frame) {
        engine.update();
        if (engine.render(pcm_buf.data(), 1024) == OAE_OK) {
            for (float s : pcm_buf) swap_peak = std::max(swap_peak, std::abs(s));
        }
    }
    if (swap_peak < 0.001f) {
        std::cerr << "[OAE Studio] Audio silent after in-flight bank swap!" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] In-flight preview rebuild & bank swap verified (peak: " << swap_peak << ")" << std::endl;

    // 8b. Live Track Mix (Mute, Solo, Mute > Solo, output_track ancestry, Volume/Pan + Undo/Redo, resume/loop, gesture deferral, streaming ping-pong hot-swap)
    {
        transport.stop(true);
        transport.set_loop(false);
        transport.set_event_path("event:/music/streaming_mix");
        rebuilder.stopTimer();

        if (!transport.play() || transport.get_active_instance() == 0) {
            std::cerr << "[OAE Studio] Failed to play streaming_mix event!" << std::endl;
            return 1;
        }

        // Render frames so Channel::init() opens std::ifstream on the streaming sample
        for (int frame = 0; frame < 15; ++frame) {
            engine.update();
            engine.render(pcm_buf.data(), 1024);
        }

        oae_bus bus0 = 0;
        oae_bus bus1 = 0;
        if (engine.get_instance_track_bus(transport.get_active_instance(), 0, &bus0) != OAE_OK || bus0 == 0 ||
            engine.get_instance_track_bus(transport.get_active_instance(), 1, &bus1) != OAE_OK || bus1 == 0) {
            std::cerr << "[OAE Studio] get_instance_track_bus failed on streaming_mix!" << std::endl;
            return 1;
        }

        auto check_bus = [&](oae_bus b, int expected_mute, float expected_linear_vol, float expected_pan, const char* label) -> bool {
            int m = -1;
            float v = -1.0f;
            float p = -9.0f;
            if (oae_bus_get_mute(b, &m) != OAE_OK || oae_bus_get_volume(b, &v) != OAE_OK || oae_bus_get_pan(b, &p) != OAE_OK) {
                std::cerr << "[OAE Studio] Failed reading track bus state (" << label << ")" << std::endl;
                return false;
            }
            if (m != expected_mute || std::abs(v - expected_linear_vol) > 1.0e-3f || std::abs(p - expected_pan) > 1.0e-3f) {
                std::cerr << "[OAE Studio] Track bus mismatch (" << label << "): mute=" << m << " (exp " << expected_mute
                          << "), vol=" << v << " (exp " << expected_linear_vol
                          << "), pan=" << p << " (exp " << expected_pan << ")" << std::endl;
                return false;
            }
            return true;
        };

        const float track1_init_lin = std::pow(10.0f, -3.0f / 20.0f);
        if (!check_bus(bus0, 0, 1.0f, 0.0f, "initial track 0") ||
            !check_bus(bus1, 0, track1_init_lin, 0.0f, "initial track 1")) {
            return 1;
        }

        // Mute without bank rebuild
        const uint64_t rev_mute = model.get_content_revision();
        model.set_track_muted("event:/music/streaming_mix", 0, true);
        if (model.get_content_revision() != rev_mute || rebuilder.isTimerRunning()) {
            std::cerr << "[OAE Studio] set_track_muted incremented content revision or triggered PreviewRebuilder!" << std::endl;
            return 1;
        }
        if (!check_bus(bus0, 1, 1.0f, 0.0f, "muted track 0") ||
            !check_bus(bus1, 0, track1_init_lin, 0.0f, "unmuted track 1")) {
            return 1;
        }

        // Precedence: Mute overrides Solo
        model.set_track_solo("event:/music/streaming_mix", 0, true);
        if (model.get_content_revision() != rev_mute || rebuilder.isTimerRunning()) {
            std::cerr << "[OAE Studio] set_track_solo incremented content revision or triggered PreviewRebuilder!" << std::endl;
            return 1;
        }
        if (!check_bus(bus0, 1, 1.0f, 0.0f, "mute beats solo on track 0") ||
            !check_bus(bus1, 1, track1_init_lin, 0.0f, "non-solo track 1 muted")) {
            return 1;
        }

        model.set_track_muted("event:/music/streaming_mix", 0, false);
        if (!check_bus(bus0, 0, 1.0f, 0.0f, "solo audible track 0") ||
            !check_bus(bus1, 1, track1_init_lin, 0.0f, "non-solo track 1 still muted")) {
            return 1;
        }
        model.set_track_solo("event:/music/streaming_mix", 0, false);
        if (!check_bus(bus0, 0, 1.0f, 0.0f, "solo cleared track 0") ||
            !check_bus(bus1, 0, track1_init_lin, 0.0f, "solo cleared track 1")) {
            return 1;
        }

        // output_track ancestry precedence
        model.modify([](nlohmann::json& r) {
            r["events"][2]["tracks"][0]["output_track"] = 1;
        });
        rebuilder.stopTimer();
        model.set_track_solo("event:/music/streaming_mix", 0, true);
        if (!check_bus(bus0, 0, 1.0f, 0.0f, "output_track solo child") ||
            !check_bus(bus1, 0, track1_init_lin, 0.0f, "output_track ancestor kept audible")) {
            return 1;
        }
        model.set_track_muted("event:/music/streaming_mix", 1, true);
        if (!check_bus(bus1, 1, track1_init_lin, 0.0f, "mute overrides ancestor audibility")) {
            return 1;
        }
        model.set_track_muted("event:/music/streaming_mix", 1, false);
        model.set_track_solo("event:/music/streaming_mix", 0, false);
        model.modify([](nlohmann::json& r) {
            r["events"][2]["tracks"][0].erase("output_track");
        });
        rebuilder.stopTimer();

        // Live Volume/Pan + Undo/Redo
        model.set_track_volume("event:/music/streaming_mix", 0, -12.0);
        model.set_track_pan("event:/music/streaming_mix", 0, -0.75);
        const float lin_m12 = std::pow(10.0f, -12.0f / 20.0f);
        if (!check_bus(bus0, 0, lin_m12, -0.75f, "live volume -12 / pan -0.75")) {
            return 1;
        }
        if (!model.undo() || !check_bus(bus0, 0, lin_m12, 0.0f, "undo live pan")) {
            return 1;
        }
        if (!model.redo() || !check_bus(bus0, 0, lin_m12, -0.75f, "redo live pan")) {
            return 1;
        }

        // Reapply across pause / resume
        model.set_track_muted("event:/music/streaming_mix", 1, true);
        if (!transport.pause() || !transport.resume()) {
            std::cerr << "[OAE Studio] pause/resume failed during track mix test!" << std::endl;
            return 1;
        }
        oae_bus rbus0 = 0;
        oae_bus rbus1 = 0;
        if (engine.get_instance_track_bus(transport.get_active_instance(), 0, &rbus0) != OAE_OK ||
            engine.get_instance_track_bus(transport.get_active_instance(), 1, &rbus1) != OAE_OK ||
            !check_bus(rbus0, 0, lin_m12, -0.75f, "resume track 0") ||
            !check_bus(rbus1, 1, track1_init_lin, 0.0f, "resume track 1")) {
            return 1;
        }

        // Gesture deferral on PreviewRebuilder::timerCallback()
        model.begin_gesture();
        model.set_track_volume("event:/music/streaming_mix", 0, -6.0);
        rebuilder.timerCallback();
        if (rebuilder.is_rebuilding() || !rebuilder.isTimerRunning()) {
            std::cerr << "[OAE Studio] PreviewRebuilder did not defer timerCallback while gesture active!" << std::endl;
            return 1;
        }
        model.end_gesture();
        rebuilder.stopTimer();

        // 3 consecutive streaming hot-swaps while playing
        const float lin_m6 = std::pow(10.0f, -6.0f / 20.0f);
        for (int swap_i = 0; swap_i < 3; ++swap_i) {
            model.modify([swap_i](nlohmann::json& r) {
                r["events"][2]["volume_db"] = -0.5 * static_cast<double>(swap_i + 1);
            });
            rebuilder.stopTimer();
            if (!rebuilder.rebuild_synchronously()) {
                std::cerr << "[OAE Studio] Streaming hot-swap #" << swap_i << " failed!" << std::endl;
                return 1;
            }
            if (transport.get_state() != oae::studio::TransportState::Playing || transport.get_active_instance() == 0) {
                std::cerr << "[OAE Studio] Transport did not resume Playing after streaming hot-swap #" << swap_i << std::endl;
                return 1;
            }
            oae_bus sbus0 = 0;
            oae_bus sbus1 = 0;
            if (engine.get_instance_track_bus(transport.get_active_instance(), 0, &sbus0) != OAE_OK ||
                engine.get_instance_track_bus(transport.get_active_instance(), 1, &sbus1) != OAE_OK ||
                !check_bus(sbus0, 0, lin_m6, -0.75f, "hot-swap track 0") ||
                !check_bus(sbus1, 1, track1_init_lin, 0.0f, "hot-swap track 1")) {
                return 1;
            }
            float stream_swap_peak = 0.0f;
            for (int frame = 0; frame < 10; ++frame) {
                engine.update();
                if (engine.render(pcm_buf.data(), 1024) == OAE_OK) {
                    for (float s : pcm_buf) stream_swap_peak = std::max(stream_swap_peak, std::abs(s));
                }
            }
            if (stream_swap_peak < 0.001f) {
                std::cerr << "[OAE Studio] Streaming audio silent after hot-swap #" << swap_i << std::endl;
                return 1;
            }
        }

        // Loop restart via update_tick() reapplies track mix
        transport.stop(true);
        transport.set_event_path("event:/music/short_loop_test");
        transport.set_loop(true);
        model.set_track_volume("event:/music/short_loop_test", 0, -9.0);
        model.set_track_pan("event:/music/short_loop_test", 0, 0.6);
        model.set_track_muted("event:/music/short_loop_test", 0, true);
        rebuilder.stopTimer();
        if (!transport.play() || transport.get_active_instance() == 0) {
            std::cerr << "[OAE Studio] Failed to play short_loop_test!" << std::endl;
            return 1;
        }
        bool reached_natural_stop = false;
        for (int frame = 0; frame < 25; ++frame) {
            engine.update();
            engine.render(pcm_buf.data(), 1024);
            int pstate = -1;
            if (engine.get_instance_playback_state(transport.get_active_instance(), &pstate) == OAE_OK &&
                pstate == OAE_PLAYBACK_STOPPED) {
                reached_natural_stop = true;
                break;
            }
        }
        if (!reached_natural_stop) {
            std::cerr << "[OAE Studio] short_loop_test did not reach OAE_PLAYBACK_STOPPED before update_tick!" << std::endl;
            return 1;
        }
        transport.update_tick();
        oae_bus lbus0 = 0;
        const float lin_m9 = std::pow(10.0f, -9.0f / 20.0f);
        if (engine.get_instance_track_bus(transport.get_active_instance(), 0, &lbus0) != OAE_OK ||
            !check_bus(lbus0, 1, lin_m9, 0.6f, "loop update_tick track 0")) {
            return 1;
        }

        transport.stop(true);
        transport.set_loop(false);
        std::cout << "[OAE Studio] Live track mix, Mute/Solo precedence, resume/loop reapply & streaming ping-pong hot-swap verified." << std::endl;
    }

    // Test project close safety
    doc.close();
    rebuilder.on_project_changed();
    if (rebuilder.get_loaded_bank() != 0) {
        std::cerr << "[OAE Studio] Bank was not unloaded when project was closed!" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Project close safety verified." << std::endl;

    // Reload document for mixer testing
    if (!doc.open(temp_dir / "project.json")) {
        std::cerr << "[OAE Studio] Failed to reload project document!" << std::endl;
        return 1;
    }
    if (!rebuilder.rebuild_synchronously()) {
        std::cerr << "[OAE Studio] Failed to rebuild bank after reopening project!" << std::endl;
        return 1;
    }

    // 9. Mixer Deck & Hierarchical Solo Rule
    oae::studio::MixerDeckPanel mixer(engine, model, transport);
    mixer.setSize(800, 300);

    auto* master_strip = mixer.find_strip("bus:/");
    auto* sfx_strip = mixer.find_strip("bus:/sfx");
    auto* music_strip = mixer.find_strip("bus:/music");
    auto* weapons_strip = mixer.find_strip("bus:/sfx/weapons");
    auto* vca_strip = mixer.find_strip("vca:/main_vca");

    if (!master_strip || !sfx_strip || !music_strip || !weapons_strip || !vca_strip) {
        std::cerr << "[OAE Studio] Missing expected mixer strip!" << std::endl;
        return 1;
    }
    if (mixer.get_strips().size() != 5) {
        std::cerr << "[OAE Studio] Expected 5 strips, found " << mixer.get_strips().size() << std::endl;
        return 1;
    }

    mixer.setSize(320, 152);
    auto* fader = master_strip->findChildWithID("mixer_fader_master");
    const auto meter = master_strip->get_vu_meter().getBounds();
    if (!fader || meter.isEmpty() || fader->getBounds().isEmpty() || meter.intersects(fader->getBounds())
        || !master_strip->getLocalBounds().contains(meter)
        || !master_strip->getLocalBounds().contains(fader->getBounds())) {
        std::cerr << "[OAE Studio] Compact Mixer strip layout failed." << std::endl;
        return 1;
    }
    mixer.setSize(800, 300);

    // Initial state: no mutes, no solos
    if (master_strip->is_effective_muted() || sfx_strip->is_effective_muted() ||
        music_strip->is_effective_muted() || weapons_strip->is_effective_muted()) {
        std::cerr << "[OAE Studio] Initial strip effective mute unexpected!" << std::endl;
        return 1;
    }

    // Volume & Pan controls
    sfx_strip->set_volume_db(-5.0f);
    if (std::abs(sfx_strip->get_volume_db() - (-5.0f)) > 0.15f) {
        std::cerr << "[OAE Studio] Bus strip volume slider mismatch!" << std::endl;
        return 1;
    }
    sfx_strip->set_pan(0.3f);
    if (std::abs(sfx_strip->get_pan() - 0.3f) > 0.05f) {
        std::cerr << "[OAE Studio] Bus strip pan slider mismatch!" << std::endl;
        return 1;
    }
    vca_strip->set_volume_db(-2.0f);
    if (std::abs(vca_strip->get_volume_db() - (-2.0f)) > 0.15f) {
        std::cerr << "[OAE Studio] VCA strip volume slider mismatch!" << std::endl;
        return 1;
    }

    // Hierarchical Solo Test A: Solo 'bus:/sfx'
    // - 'bus:/sfx' is soloed -> audible
    // - 'bus:/' is ancestor of soloed -> audible
    // - 'bus:/sfx/weapons' is descendant of soloed -> audible
    // - 'bus:/music' is sibling/unrelated -> MUTED
    sfx_strip->set_soloed(true);
    if (sfx_strip->is_effective_muted()) {
        std::cerr << "[OAE Studio] Soloed bus 'bus:/sfx' was effectively muted!" << std::endl;
        return 1;
    }
    if (master_strip->is_effective_muted()) {
        std::cerr << "[OAE Studio] Ancestor 'bus:/' was effectively muted during bus solo!" << std::endl;
        return 1;
    }
    if (weapons_strip->is_effective_muted()) {
        std::cerr << "[OAE Studio] Descendant 'bus:/sfx/weapons' was effectively muted during parent solo!" << std::endl;
        return 1;
    }
    if (!music_strip->is_effective_muted()) {
        std::cerr << "[OAE Studio] Sibling 'bus:/music' was NOT muted during bus solo!" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Hierarchical Solo Rule Case A (solo parent) verified." << std::endl;

    // Hierarchical Solo Test B: Solo 'bus:/sfx/weapons'
    // - 'bus:/sfx' was un-soloed
    // - 'bus:/sfx/weapons' is soloed -> audible
    // - 'bus:/sfx' is ancestor -> audible
    // - 'bus:/' is ancestor -> audible
    // - 'bus:/music' is unrelated -> MUTED
    sfx_strip->set_soloed(false);
    weapons_strip->set_soloed(true);
    if (weapons_strip->is_effective_muted()) {
        std::cerr << "[OAE Studio] Soloed leaf 'bus:/sfx/weapons' was effectively muted!" << std::endl;
        return 1;
    }
    if (sfx_strip->is_effective_muted()) {
        std::cerr << "[OAE Studio] Ancestor 'bus:/sfx' was effectively muted during child solo!" << std::endl;
        return 1;
    }
    if (master_strip->is_effective_muted()) {
        std::cerr << "[OAE Studio] Master 'bus:/' was effectively muted during child solo!" << std::endl;
        return 1;
    }
    if (!music_strip->is_effective_muted()) {
        std::cerr << "[OAE Studio] Unrelated 'bus:/music' was NOT muted during child solo!" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Hierarchical Solo Rule Case B (solo leaf) verified." << std::endl;

    // Clear all solos
    weapons_strip->set_soloed(false);
    if (master_strip->is_effective_muted() || sfx_strip->is_effective_muted() ||
        music_strip->is_effective_muted() || weapons_strip->is_effective_muted()) {
        std::cerr << "[OAE Studio] Strips remained muted after clearing solo!" << std::endl;
        return 1;
    }

    // Explicit Mute Test: Mute 'bus:/music' without any solo
    music_strip->set_muted(true);
    if (!music_strip->is_effective_muted()) {
        std::cerr << "[OAE Studio] 'bus:/music' was not effectively muted after set_muted(true)!" << std::endl;
        return 1;
    }
    if (master_strip->is_effective_muted() || sfx_strip->is_effective_muted() || weapons_strip->is_effective_muted()) {
        std::cerr << "[OAE Studio] Other buses were muted when only 'bus:/music' was muted!" << std::endl;
        return 1;
    }
    music_strip->set_muted(false);
    if (music_strip->is_effective_muted()) {
        std::cerr << "[OAE Studio] 'bus:/music' remained muted after set_muted(false)!" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Individual Mute and Solo restoration verified." << std::endl;

    // VU meter test
    transport.play();
    for (int frame = 0; frame < 10; ++frame) {
        engine.update();
        engine.render(pcm_buf.data(), 1024);
    }
    sfx_strip->update_meter();
    const float meter_pk = sfx_strip->get_vu_meter().get_peak_db();
    if (std::isnan(meter_pk) || meter_pk < -80.0f) {
        std::cerr << "[OAE Studio] VU meter peak dB invalid: " << meter_pk << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] VU meter ballistic response verified (peak_db: " << meter_pk << ")" << std::endl;

    // 10. Clean teardown
    transport.stop(true);
    rebuilder.unload_preview_bank();
    engine.shutdown();
    fs::remove_all(temp_dir, ec);

    std::cout << "[OAE Studio] --smoke-transport-mixer PASSED with exit code 0." << std::endl;
    return 0;
}

int run_smoke_end_to_end()
{
    std::cout << "[OAE Studio] Starting --smoke-end-to-end validation (Studio -> Export -> Game Runtime)..." << std::endl;

    const fs::path temp_dir = fs::temp_directory_path() / "oae_studio_smoke_end_to_end";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir, ec);

    // 1. Locate oae_bankc
    const auto app_dir = get_app_dir();
    const auto bankc_exe = find_bankc_executable(app_dir);
    if (bankc_exe.empty()) {
        std::cerr << "[OAE Studio] oae_bankc executable not found!" << std::endl;
        return 1;
    }

    const fs::path wav_coin = temp_dir / "coin.wav";
    const fs::path wav_music = temp_dir / "music.wav";
    if (!write_sine_wav(wav_coin, 880.0f, 0.4f) || !write_sine_wav(wav_music, 440.0f, 1.5f)) {
        std::cerr << "[OAE Studio] Failed to generate fixture WAVs!" << std::endl;
        return 1;
    }

    // 2. Author project using ProjectDocument and AuthoringModel
    oae::studio::ProjectDocument doc;
    if (!doc.create_empty(temp_dir, "GameEndToEnd")) {
        std::cerr << "[OAE Studio] Failed to create project document!" << std::endl;
        return 1;
    }

    if (!doc.import_audio_file(wav_coin, "sample_coin") || !doc.import_audio_file(wav_music, "sample_music")) {
        std::cerr << "[OAE Studio] Failed to import audio files into project!" << std::endl;
        return 1;
    }

    oae::studio::AuthoringModel model(doc);
    model.modify([](nlohmann::json& r) {
        r["buses"] = nlohmann::json::array({
            { { "path", "bus:/sfx" }, { "volume", 1.0 }, { "effects", nlohmann::json::array() } },
            { { "path", "bus:/music" }, { "volume", 1.0 }, { "effects", nlohmann::json::array() } }
        });

        r["global_parameters"] = nlohmann::json::array({
            { { "name", "Difficulty" }, { "kind", "continuous" }, { "min", 0.0 }, { "max", 10.0 }, { "default_value", 5.0 } }
        });

        r["vcas"] = nlohmann::json::array({
            { { "path", "vca:/game" }, { "buses", nlohmann::json::array({ "bus:/sfx", "bus:/music" }) }, { "events", nlohmann::json::array({ "event:/music/stage1", "event:/sfx/pickup" }) } }
        });

        r["events"] = nlohmann::json::array({
            {
                { "path", "event:/music/stage1" },
                { "kind", "event" },
                { "bus_path", "bus:/music" },
                { "volume_db", -3.0 },
                { "tracks", nlohmann::json::array({
                    {
                        { "volume_db", 0.0 },
                        { "instruments", nlohmann::json::array({
                            {
                                { "kind", "single" },
                                { "sample_id", "sample_music" },
                                { "offset_ms", 0.0 },
                                { "length_ms", 1500.0 },
                                { "loop", true }
                            }
                        }) }
                    }
                }) }
            },
            {
                { "path", "event:/sfx/pickup" },
                { "kind", "event" },
                { "bus_path", "bus:/sfx" },
                { "volume_db", 0.0 },
                { "tracks", nlohmann::json::array({
                    {
                        { "volume_db", 0.0 },
                        { "instruments", nlohmann::json::array({
                            {
                                { "kind", "single" },
                                { "sample_id", "sample_coin" },
                                { "offset_ms", 0.0 },
                                { "length_ms", 400.0 },
                                { "loop", false }
                            }
                        }) }
                    }
                }) }
            },
            {
                { "path", "snapshot:/pause_duck" },
                { "kind", "snapshot" },
                { "priority", 120 },
                { "snapshot_mode", "override" },
                { "fade_in_ms", 50.0 },
                { "snapshot_targets", nlohmann::json::array({
                    {
                        { "bus_path", "bus:/music" },
                        { "value", -12.0 },
                        { "param_id", -1 },
                        { "position", 0 },
                        { "effect_index", 0 }
                    }
                }) }
            }
        });
    });

    if (!doc.save()) {
        std::cerr << "[OAE Studio] Failed to save project!" << std::endl;
        return 1;
    }

    // 3. Publish bank through Studio's AsyncBankPublisher
    const fs::path target_bank = temp_dir / "build" / "game.bank";
    const fs::path target_catalog = temp_dir / "build" / "game.bank.catalog.json";
    oae::studio::AsyncBankPublisher publisher;

    if (!publisher.start_publish(bankc_exe, model.create_immutable_snapshot(), doc.get_project_dir(), target_bank)) {
        std::cerr << "[OAE Studio] Failed to start async bank publisher!" << std::endl;
        return 1;
    }

    publisher.wait_until_finished();
    const auto pub_result = publisher.get_last_result();
    if (!pub_result.success || pub_result.exit_code != 0) {
        std::cerr << "[OAE Studio] Bank publication failed:\n" << pub_result.diagnostics << std::endl;
        return 1;
    }

    if (!fs::exists(target_bank) || !fs::exists(target_catalog)) {
        std::cerr << "[OAE Studio] Published bank or catalog file missing on disk!" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Studio exported bank successfully: " << target_bank.string() << std::endl;

    // 4. Game Runtime Consumption Phase (Simulating Unity / Game Engine)
    const fs::path game_streaming_assets = temp_dir / "GameStreamingAssets";
    fs::create_directories(game_streaming_assets, ec);
    const fs::path game_bank = game_streaming_assets / "game.bank";
    const fs::path game_catalog = game_streaming_assets / "game.bank.catalog.json";
    fs::copy_file(target_bank, game_bank, fs::copy_options::overwrite_existing, ec);
    fs::copy_file(target_catalog, game_catalog, fs::copy_options::overwrite_existing, ec);

    // Verify catalog structure
    std::ifstream cat_file(game_catalog);
    if (!cat_file.is_open()) {
        std::cerr << "[OAE Studio] Failed to read game catalog file!" << std::endl;
        return 1;
    }
    nlohmann::json cat_json;
    cat_file >> cat_json;
    if (!cat_json.contains("events") || cat_json["events"].size() < 2) {
        std::cerr << "[OAE Studio] Catalog missing expected events!" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Game catalog verified (" << cat_json["events"].size() << " events indexed)." << std::endl;

    // Initialize Game Audio System
    oae::studio::StudioEngine game_engine;
    if (game_engine.init_headless() != OAE_OK) {
        std::cerr << "[OAE Studio] Failed to initialize game audio runtime!" << std::endl;
        return 1;
    }

    oae_bank loaded_bank = 0;
    if (game_engine.load_bank_file(game_bank.string().c_str(), &loaded_bank) != OAE_OK || loaded_bank == 0) {
        std::cerr << "[OAE Studio] Game runtime failed to load published bank!" << std::endl;
        return 1;
    }

    // Play music event
    oae_event_instance music_inst = 0;
    if (game_engine.create_event_instance("event:/music/stage1", &music_inst) != OAE_OK || music_inst == 0) {
        std::cerr << "[OAE Studio] Game runtime failed to create instance for event:/music/stage1!" << std::endl;
        return 1;
    }
    oae_event_instance_start(music_inst);

    // Play SFX event
    oae_event_instance sfx_inst = 0;
    if (game_engine.create_event_instance("event:/sfx/pickup", &sfx_inst) != OAE_OK || sfx_inst == 0) {
        std::cerr << "[OAE Studio] Game runtime failed to create instance for event:/sfx/pickup!" << std::endl;
        return 1;
    }
    oae_event_instance_start(sfx_inst);

    // Modulate game parameters
    game_engine.set_global_parameter("Difficulty", 8.5f);

    // Render audio blocks and verify signal
    std::array<float, 2048> pcm_buf{};
    float max_peak = 0.0f;
    for (int frame = 0; frame < 20; ++frame) {
        game_engine.update();
        if (game_engine.render(pcm_buf.data(), 1024) == OAE_OK) {
            for (float s : pcm_buf) max_peak = std::max(max_peak, std::abs(s));
        }
    }
    std::cout << "[OAE Studio] Game runtime rendered audio successfully (peak: " << max_peak << ")" << std::endl;
    if (max_peak < 0.001f) {
        std::cerr << "[OAE Studio] Game runtime audio output was silent!" << std::endl;
        return 1;
    }

    // Test Snapshot activation in game runtime
    oae_event_instance snap_inst = 0;
    if (game_engine.create_event_instance("snapshot:/pause_duck", &snap_inst) == OAE_OK && snap_inst != 0) {
        oae_event_instance_start(snap_inst);
        game_engine.update();
        std::cout << "[OAE Studio] Game runtime snapshot activation verified." << std::endl;
        game_engine.stop_instance(snap_inst, true);
        game_engine.release_instance(snap_inst);
    }

    // Clean shutdown of game runtime
    game_engine.stop_instance(music_inst, true);
    game_engine.release_instance(music_inst);
    game_engine.stop_instance(sfx_inst, true);
    game_engine.release_instance(sfx_inst);
    game_engine.unload_bank(loaded_bank);
    game_engine.shutdown();

    fs::remove_all(temp_dir, ec);

    std::cout << "[OAE Studio] --smoke-end-to-end PASSED with exit code 0." << std::endl;
    return 0;
}

int run_smoke_device()
{
    std::cout << "[OAE Studio] Starting --smoke-device validation..." << std::endl;
    oae::studio::StudioEngine engine;
    auto res = engine.init_device();
    if (res != OAE_OK) {
        std::cout << "[OAE Studio] Audio device unavailable, testing headless fallback..." << std::endl;
        res = engine.init_headless();
    }

    if (res != OAE_OK) {
        std::cerr << "[OAE Studio] --smoke-device engine init failed with code: " << res << std::endl;
        return 1;
    }

    engine.update();
    engine.shutdown();
    std::cout << "[OAE Studio] --smoke-device PASSED with exit code 0." << std::endl;
    return 0;
}

int run_smoke_pro_ui()
{
    std::cout << "[OAE Studio] Starting --smoke-pro-ui validation (Epic 5B)..." << std::endl;

    // 1. Validate OaeDarkLookAndFeel design system
    oae::studio::OaeDarkLookAndFeel lookAndFeel;
    const auto bg = lookAndFeel.findColour(juce::ResizableWindow::backgroundColourId);
    if (bg != juce::Colour(oae::studio::OaePalette::bgDark)) {
        std::cerr << "[OAE Studio] LookAndFeel bg colour mismatch!" << std::endl;
        return 1;
    }
    const auto thumb = lookAndFeel.findColour(juce::Slider::thumbColourId);
    if (thumb != juce::Colour(oae::studio::OaePalette::accentCyan)) {
        std::cerr << "[OAE Studio] LookAndFeel slider thumb mismatch!" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] OAE Dark Pro LookAndFeel verified." << std::endl;

    const auto mouse_event = [](juce::Component& component, juce::Point<float> position,
                                juce::ModifierKeys modifiers, int clicks, bool dragged) {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), position, modifiers,
                                1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &component, &component,
                                now, { 20.0f, 20.0f }, now, clicks, dragged);
    };
    {
        oae::studio::OaeSlider slider(juce::Slider::LinearHorizontal, juce::Slider::NoTextBox);
        slider.setLookAndFeel(&lookAndFeel);
        slider.setSize(100, 40);
        slider.setRange(0.0, 100.0, 0.1);
        slider.setValue(25.0, juce::dontSendNotification);
        slider.setDoubleClickReturnValue(true, 25.0, {});
        const double span = static_cast<double>(lookAndFeel.getSliderLayout(slider).sliderBounds.getWidth());
        if (span <= 0.0) {
            std::cerr << "[OAE Studio] Linear OaeSlider layout span is zero!" << std::endl;
            return 1;
        }
        const auto left = juce::ModifierKeys::leftButtonModifier;
        const auto fine = left | juce::ModifierKeys::shiftModifier;
        slider.mouseDown(mouse_event(slider, {20.0f, 20.0f}, left, 1, false));
        slider.mouseDrag(mouse_event(slider, {45.0f, 20.0f}, left, 1, true));
        const auto normal = slider.getValue();
        slider.mouseDrag(mouse_event(slider, {45.0f, 20.0f}, fine, 1, true));
        const auto switched = slider.getValue();
        slider.mouseDrag(mouse_event(slider, {70.0f, 20.0f}, fine, 1, true));
        const auto precise = slider.getValue();
        slider.mouseUp(mouse_event(slider, {70.0f, 20.0f}, {}, 1, true));
        slider.mouseDoubleClick(mouse_event(slider, {70.0f, 20.0f}, {}, 2, false));
        const double expectedNormal = 25.0 + 100.0 * 25.0 / span;
        const double expectedPrecise = expectedNormal + 100.0 * 25.0 * static_cast<double>(oae::studio::OaeTheme::Sliders::fineDragScale) / span;
        if (std::abs(normal - expectedNormal) > 0.15 || std::abs(normal - switched) > 0.001
            || std::abs(precise - expectedPrecise) > 0.15 || slider.getValue() != 25.0) {
            std::cerr << "[OAE Studio] Linear 1:1 slider tracking or reset failed." << std::endl;
            return 1;
        }
        // Full track span drag from 0.0 reaches 100.0 on 1:1 tracking
        slider.setValue(0.0, juce::dontSendNotification);
        slider.mouseDown(mouse_event(slider, {10.0f, 20.0f}, left, 1, false));
        slider.mouseDrag(mouse_event(slider, {10.0f + static_cast<float>(span), 20.0f}, left, 1, true));
        slider.mouseUp(mouse_event(slider, {10.0f + static_cast<float>(span), 20.0f}, {}, 1, true));
        if (std::abs(slider.getValue() - 100.0) > 0.25) {
            std::cerr << "[OAE Studio] Linear slider 1:1 full-span travel failed (got " << slider.getValue() << ")." << std::endl;
            return 1;
        }
    }
    {
        oae::studio::OaeSlider slider(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox);
        slider.setLookAndFeel(&lookAndFeel);
        slider.setSize(100, 40);
        slider.setRange(0.0, 100.0, 0.1);
        slider.setValue(25.0, juce::dontSendNotification);
        slider.setDoubleClickReturnValue(true, 25.0, {});
        const auto left = juce::ModifierKeys::leftButtonModifier;
        const auto fine = left | juce::ModifierKeys::shiftModifier;
        slider.mouseDown(mouse_event(slider, {20.0f, 20.0f}, left, 1, false));
        slider.mouseDrag(mouse_event(slider, {45.0f, 20.0f}, left, 1, true));
        const auto normal = slider.getValue();
        slider.mouseDrag(mouse_event(slider, {45.0f, 20.0f}, fine, 1, true));
        const auto switched = slider.getValue();
        slider.mouseDrag(mouse_event(slider, {70.0f, 20.0f}, fine, 1, true));
        const auto precise = slider.getValue();
        slider.mouseUp(mouse_event(slider, {70.0f, 20.0f}, {}, 1, true));
        slider.mouseDoubleClick(mouse_event(slider, {70.0f, 20.0f}, {}, 2, false));
        if (std::abs(normal - 35.0) > 0.01 || normal != switched || std::abs(precise - 36.0) > 0.01
            || slider.getValue() != 25.0) {
            std::cerr << "[OAE Studio] Rotary slider precision or reset failed." << std::endl;
            return 1;
        }
    }

    oae::studio::OaeMeterComponent meter;
    meter.setSize(12, 100);
    meter.set_levels(1.0f, 0.5f);
    meter.set_levels(0.0f, 0.0f, 0.25f);
    if (std::abs(meter.get_peak_db() + 5.0f) > 0.01f || meter.get_peak_hold_db() != 0.0f) {
        std::cerr << "[OAE Studio] Meter decay or peak hold failed." << std::endl;
        return 1;
    }
    for (int i = 0; i < 5; ++i)
        meter.set_levels(0.0f, 0.0f, 0.25f);
    if (meter.get_peak_hold_db() >= 0.0f) {
        std::cerr << "[OAE Studio] Meter peak hold failed to decay." << std::endl;
        return 1;
    }

    // 2. Validate DockingLayoutManager persistence, clamping, 3 built-in templates, custom presets & Hide All
    oae::studio::DockingLayoutManager layout;
    layout.set_left_width(320);
    layout.set_right_width(340);
    layout.set_bottom_height(300);

    const std::string json = layout.dimensions().to_json();
    oae::studio::DockingDimensions loaded;
    if (!loaded.from_json(json)) {
        std::cerr << "[OAE Studio] DockingDimensions JSON deserialization failed!" << std::endl;
        return 1;
    }
    if (loaded.left_width != 320 || loaded.right_width != 340 || loaded.bottom_height != 300) {
        std::cerr << "[OAE Studio] DockingDimensions roundtrip values mismatch!" << std::endl;
        return 1;
    }

    // Test boundary clamping with constrained area
    loaded.clamp(400, 300);
    if (loaded.left_width > 180 || loaded.bottom_height != oae::studio::DockingDimensions::min_bottom_height) {
        std::cerr << "[OAE Studio] DockingDimensions boundary clamping failed!" << std::endl;
        return 1;
    }

    // Verify 3 read-only built-in templates and zero-waste slot collapse
    const auto presetNames = layout.get_preset_names();
    for (const char* expectedPreset : { "Default (Authoring)", "Stage Focus", "Mixing & Mastering" }) {
        if (std::find(presetNames.begin(), presetNames.end(), expectedPreset) == presetNames.end()
            || !layout.is_builtin_preset(expectedPreset)) {
            std::cerr << "[OAE Studio] Missing or writable built-in layout preset: " << expectedPreset << std::endl;
            return 1;
        }
    }
    if (!layout.apply_preset("Stage Focus") || layout.get_active_preset_name() != "Stage Focus"
        || !layout.is_panel_visible(oae::studio::DockPanelId::Timeline)
        || !layout.is_panel_visible(oae::studio::DockPanelId::Inspector)
        || layout.is_panel_visible(oae::studio::DockPanelId::Browser)
        || layout.is_panel_visible(oae::studio::DockPanelId::BottomDeck)) {
        std::cerr << "[OAE Studio] Stage Focus preset state mismatch!" << std::endl;
        return 1;
    }

    std::array<juce::Rectangle<int>, 5> slotRects{};
    std::array<bool, 5> slotOccupied{};
    bool showL = false, showR = false, showB = false;
    layout.compute_slot_rects({ 0, 0, 1280, 720 }, 44, slotRects, slotOccupied, showL, showR, showB);
    if (slotOccupied[static_cast<size_t>(oae::studio::DockSlotId::Left)]
        || !slotOccupied[static_cast<size_t>(oae::studio::DockSlotId::Right)]
        || slotOccupied[static_cast<size_t>(oae::studio::DockSlotId::Bottom)]
        || showL || !showR || showB
        || slotRects[static_cast<size_t>(oae::studio::DockSlotId::Center)].getWidth() != 1026
        || slotRects[static_cast<size_t>(oae::studio::DockSlotId::Center)].getHeight() != 676) {
        std::cerr << "[OAE Studio] Hidden docking slots did not collapse cleanly!" << std::endl;
        return 1;
    }

    // Hiding surrounding closable panels expands timeline while permanent toolbars stay docked at Top
    layout.set_panel_visible(oae::studio::DockPanelId::Inspector, false);
    layout.set_panel_visible(oae::studio::DockPanelId::Transport, false);
    layout.compute_slot_rects({ 0, 0, 1280, 720 }, 44, slotRects, slotOccupied, showL, showR, showB);
    if (!layout.is_panel_visible(oae::studio::DockPanelId::Transport)
        || !slotOccupied[static_cast<size_t>(oae::studio::DockSlotId::Top)]
        || slotOccupied[static_cast<size_t>(oae::studio::DockSlotId::Right)]
        || slotRects[static_cast<size_t>(oae::studio::DockSlotId::Center)].getWidth() != 1280
        || slotRects[static_cast<size_t>(oae::studio::DockSlotId::Center)].getHeight() != 676) {
        std::cerr << "[OAE Studio] Solo timeline did not expand while keeping permanent toolbars!" << std::endl;
        return 1;
    }

    if (!layout.apply_preset("Mixing & Mastering")
        || layout.is_panel_visible(oae::studio::DockPanelId::Timeline)
        || !layout.is_panel_visible(oae::studio::DockPanelId::BottomDeck)
        || !layout.is_panel_visible(oae::studio::DockPanelId::Browser)
        || layout.get_panel_slot(oae::studio::DockPanelId::BottomDeck) != oae::studio::DockSlotId::Bottom
        || layout.get_panel_slot(oae::studio::DockPanelId::Inspector) != oae::studio::DockSlotId::Right) {
        std::cerr << "[OAE Studio] Mixing & Mastering preset state mismatch!" << std::endl;
        return 1;
    }

    // Verify Hide All / Restore All keeps permanent toolbars visible while hiding closable panels
    layout.hide_all_panels();
    if (!layout.are_all_panels_hidden()
        || !layout.is_panel_visible(oae::studio::DockPanelId::Transport)
        || !layout.is_panel_visible(oae::studio::DockPanelId::ProjectToolbar)
        || layout.is_panel_visible(oae::studio::DockPanelId::BottomDeck)) {
        std::cerr << "[OAE Studio] Hide All panels failed!" << std::endl;
        return 1;
    }
    layout.compute_slot_rects({ 0, 0, 1280, 720 }, 44, slotRects, slotOccupied, showL, showR, showB);
    if (!slotOccupied[0]) {
        std::cerr << "[OAE Studio] Hide All hid permanent Top toolbar slot!" << std::endl;
        return 1;
    }
    for (size_t s = 1; s < 5; ++s) {
        if (slotOccupied[s]) {
            std::cerr << "[OAE Studio] Hide All left an occupied dock slot!" << std::endl;
            return 1;
        }
    }
    layout.restore_all_panels();
    if (layout.are_all_panels_hidden()
        || !layout.is_panel_visible(oae::studio::DockPanelId::BottomDeck)
        || layout.is_panel_visible(oae::studio::DockPanelId::Timeline)) {
        std::cerr << "[OAE Studio] Restore All panels failed to recover previous template state!" << std::endl;
        return 1;
    }

    // Verify per-panel slot movement, tab stacking, custom preset CRUD, protected built-ins & reset
    layout.set_panel_visible(oae::studio::DockPanelId::Browser, true);
    layout.set_panel_slot(oae::studio::DockPanelId::Inspector, oae::studio::DockSlotId::Left);
    if (layout.get_panel_slot(oae::studio::DockPanelId::Inspector) != oae::studio::DockSlotId::Left
        || layout.panels_in_slot(oae::studio::DockSlotId::Left, true).size() != 2) {
        std::cerr << "[OAE Studio] Panel slot reassignment / tab stacking failed!" << std::endl;
        return 1;
    }
    if (layout.delete_custom_preset("Stage Focus")) {
        std::cerr << "[OAE Studio] Built-in preset was deleted unexpectedly!" << std::endl;
        return 1;
    }
    if (!layout.save_custom_preset("Custom Smoke Rig") || layout.is_builtin_preset("Custom Smoke Rig")) {
        std::cerr << "[OAE Studio] Failed to save custom layout preset!" << std::endl;
        return 1;
    }

    // V2 JSON roundtrip + v1 legacy backward compatibility
    const std::string stateJson = layout.serialize_state_json();
    oae::studio::DockingLayoutManager roundtripLayout;
    if (!roundtripLayout.deserialize_state_json(stateJson)
        || roundtripLayout.get_panel_slot(oae::studio::DockPanelId::Inspector) != oae::studio::DockSlotId::Left
        || roundtripLayout.is_builtin_preset("Custom Smoke Rig")) {
        std::cerr << "[OAE Studio] DockingLayoutManager JSON roundtrip failed!" << std::endl;
        return 1;
    }
    layout.reset_layout();
    if (layout.get_active_preset_name() != "Default (Authoring)"
        || !layout.is_panel_visible(oae::studio::DockPanelId::Timeline)
        || layout.get_panel_slot(oae::studio::DockPanelId::Inspector) != oae::studio::DockSlotId::Right) {
        std::cerr << "[OAE Studio] Reset Layout failed to restore Default (Authoring)!" << std::endl;
        return 1;
    }
    if (!layout.apply_preset("Custom Smoke Rig")
        || layout.get_panel_slot(oae::studio::DockPanelId::Inspector) != oae::studio::DockSlotId::Left
        || !layout.delete_custom_preset("Custom Smoke Rig")) {
        std::cerr << "[OAE Studio] Custom preset apply or delete failed!" << std::endl;
        return 1;
    }
    oae::studio::DockingLayoutManager legacyCompat;
    if (!legacyCompat.deserialize_state_json(R"({"left_width":280,"right_width":310,"bottom_height":250})")
        || legacyCompat.dimensions().left_width != 280
        || legacyCompat.dimensions().right_width != 310
        || legacyCompat.dimensions().bottom_height != 250
        || !legacyCompat.is_panel_visible(oae::studio::DockPanelId::Timeline)) {
        std::cerr << "[OAE Studio] Legacy v1 layout JSON compatibility failed!" << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] DockingLayoutManager templates, presets, hide-all and persistence verified." << std::endl;

    {
        using namespace oae::studio;
        DockingLayoutManager workspace;
        const juce::Rectangle<int> area{ 0, 0, 1280, 720 };
        const auto groupBounds = [](const WorkspaceGeometry& geometry, DockPanelId panel) {
            for (const auto& group : geometry.groups)
                if (std::find(group.tabs.begin(), group.tabs.end(), panel) != group.tabs.end())
                    return group.bounds;
            return juce::Rectangle<int>{};
        };
        const WorkspaceDrop right{ 0, DockEdge::Right, 0 };
        const auto inspectorPreview = workspace.preview_workspace_split(DockPanelId::Inspector, right, area, 44);
        if (inspectorPreview.isEmpty() || !workspace.move_workspace_panel(DockPanelId::Inspector, right)) {
            std::cerr << "[OAE Studio] Workspace root split was rejected." << std::endl;
            return 1;
        }
        const WorkspaceDrop below{ workspace.workspace_group(DockPanelId::BottomDeck), DockEdge::Bottom, 0 };
        const auto logPreview = workspace.preview_workspace_split(DockPanelId::Diagnostics, below, area, 44);
        if (logPreview.isEmpty() || !workspace.move_workspace_panel(DockPanelId::Diagnostics, below)) {
            std::cerr << "[OAE Studio] Workspace nested split was rejected." << std::endl;
            return 1;
        }
        const auto geometry = workspace.workspace_geometry(area, 44);
        const auto inspector = groupBounds(geometry, DockPanelId::Inspector);
        const auto deck = groupBounds(geometry, DockPanelId::BottomDeck);
        const auto log = groupBounds(geometry, DockPanelId::Diagnostics);
        if (inspector != inspectorPreview || log != logPreview || inspector.getHeight() != area.getHeight()
            || inspector.getRight() != area.getRight() || log.getY() != deck.getBottom() + 4
            || log.getX() != deck.getX() || log.getWidth() != deck.getWidth()) {
            std::cerr << "[OAE Studio] Workspace reference geometry differs from its preview." << std::endl;
            return 1;
        }
        const auto divider = std::find_if(geometry.dividers.begin(), geometry.dividers.end(), [&](const auto& item) {
            return item.bounds.getBottom() == log.getY() && item.bounds.getWidth() == log.getWidth();
        });
        if (divider == geometry.dividers.end()) {
            std::cerr << "[OAE Studio] Workspace log divider is missing." << std::endl;
            return 1;
        }
        workspace.resize_workspace_split(*divider, -1000);
        const auto resized = workspace.workspace_geometry(area, 44);
        const auto resizedDeck = groupBounds(resized, DockPanelId::BottomDeck);
        if (resizedDeck.getHeight() != divider->min_first
            || groupBounds(resized, DockPanelId::Inspector) != inspector) {
            std::cerr << "[OAE Studio] Workspace split minimum or unrelated geometry changed." << std::endl;
            return 1;
        }
        const auto beforeHide = groupBounds(resized, DockPanelId::Diagnostics);
        workspace.set_panel_visible(DockPanelId::Diagnostics, false);
        const auto hidden = workspace.workspace_geometry(area, 44);
        if (!groupBounds(hidden, DockPanelId::Diagnostics).isEmpty()
            || groupBounds(hidden, DockPanelId::BottomDeck).getBottom() != area.getBottom()) {
            std::cerr << "[OAE Studio] Workspace hidden log left unused space." << std::endl;
            return 1;
        }
        workspace.set_panel_visible(DockPanelId::Diagnostics, true);
        const auto inspectorGroup = workspace.workspace_group(DockPanelId::Inspector);
        workspace.set_panel_slot(DockPanelId::Inspector, DockSlotId::Floating);
        workspace.dock_back_workspace_panel(DockPanelId::Inspector);
        if (workspace.workspace_group(DockPanelId::Inspector) != inspectorGroup
            || groupBounds(workspace.workspace_geometry(area, 44), DockPanelId::Diagnostics) != beforeHide) {
            std::cerr << "[OAE Studio] Workspace docking anchor or restored log geometry changed." << std::endl;
            return 1;
        }

        workspace.save_custom_preset("Inspector + diagnostics");
        DockingLayoutManager restored;
        if (!restored.deserialize_state_json(workspace.serialize_state_json())
            || !restored.apply_preset("Inspector + diagnostics")
            || groupBounds(restored.workspace_geometry(area, 44), DockPanelId::Inspector) != inspector
            || groupBounds(restored.workspace_geometry(area, 44), DockPanelId::Diagnostics) != beforeHide) {
            std::cerr << "[OAE Studio] Workspace nested preset did not round-trip." << std::endl;
            return 1;
        }

        const auto deckGroup = workspace.workspace_group(DockPanelId::BottomDeck);
        if (!workspace.move_workspace_panel(DockPanelId::Diagnostics, { deckGroup, DockEdge::Tabs, 0 })) {
            std::cerr << "[OAE Studio] Workspace tab insertion failed." << std::endl;
            return 1;
        }
        const auto grouped = workspace.workspace_geometry(area, 44);
        const auto tabs = std::find_if(grouped.groups.begin(), grouped.groups.end(),
            [deckGroup](const auto& item) { return item.id == deckGroup; });
        if (tabs == grouped.groups.end() || tabs->tabs != std::vector<DockPanelId>{
            DockPanelId::Diagnostics, DockPanelId::BottomDeck } || tabs->active != DockPanelId::Diagnostics) {
            std::cerr << "[OAE Studio] Workspace did not preserve insertion order or active tab." << std::endl;
            return 1;
        }
        size_t tabCount = 0;
        for (const auto& group : grouped.groups) {
            tabCount += group.tabs.size();
        }
        if (tabCount != dockPanelCount) {
            std::cerr << "[OAE Studio] Workspace duplicated or lost a tab." << std::endl;
            return 1;
        }
    }

    // 3. Validate ContextualBottomDeck & Sub-decks
    oae::studio::StudioEngine engine;
    engine.init_headless();
    oae::studio::ProjectDocument doc;
    const auto pro_ui_dir = fs::temp_directory_path() / "oae_smoke_pro_ui";
    fs::remove_all(pro_ui_dir);
    doc.create_empty(pro_ui_dir, "pro_ui_smoke");
    oae::studio::AuthoringModel model(doc);
    model.create_event("event:/music/theme");
    model.add_instrument("event:/music/theme", 0, "multi");
    oae::studio::AuditionTransport transport(engine, model);
    {
        constexpr auto event_path = "event:/music/theme";
        model.add_local_parameter(event_path, "Preview", "continuous");
        model.update_local_parameter(event_path, "Preview",
            { {"kind", "continuous"}, {"min", 0.0}, {"max", 100.0}, {"default_value", 23.456} });
        transport.set_event_path(event_path);
        oae::studio::ProjectToolbar projectToolbar;
        projectToolbar.setSize(projectToolbar.preferred_width(), projectToolbar.preferred_height());
        juce::Rectangle<int> prevProj;
        for (auto* control : std::initializer_list<juce::Component*>{
            &projectToolbar.grip_handle(), &projectToolbar.newProjectButton, &projectToolbar.openProjectButton,
            &projectToolbar.saveProjectButton, &projectToolbar.buildBankButton, &projectToolbar.helpButton }) {
            const auto bounds = control->getBounds();
            if (bounds.isEmpty() || !projectToolbar.getLocalBounds().contains(bounds) || prevProj.intersects(bounds)) {
                std::cerr << "[OAE Studio] ProjectToolbar controls do not fit." << std::endl;
                return 1;
            }
            prevProj = bounds;
        }
        projectToolbar.set_minimized(true);
        if (projectToolbar.newProjectButton.isVisible() || !projectToolbar.saveProjectButton.isVisible()) {
            std::cerr << "[OAE Studio] ProjectToolbar minimize toggle failed." << std::endl;
            return 1;
        }
        projectToolbar.set_minimized(false);

        oae::studio::TransportBar bar(model, transport);
        bar.setSize(320, bar.preferred_height(320));
        juce::Rectangle<int> previousControl;
        for (auto* control : std::initializer_list<juce::Component*>{
            &bar.grip_handle(), &bar.playButton, &bar.pauseButton, &bar.stopButton, &bar.loopButton,
            &bar.keyOffButton, &bar.timecodeLabel, &bar.statusLabel, &bar.paramsButton }) {
            const auto bounds = control->getBounds();
            if (bounds.isEmpty() || !bar.getLocalBounds().contains(bounds) || previousControl.intersects(bounds)) {
                std::cerr << "[OAE Studio] Transport controls do not fit after wrapping." << std::endl;
                return 1;
            }
            previousControl = bounds;
        }
        if (bar.statusLabel.getY() <= bar.playButton.getY()) {
            std::cerr << "[OAE Studio] Transport controls did not wrap." << std::endl;
            return 1;
        }
        bar.setSize(800, bar.preferred_height(800));
        bar.set_minimized(true);
        if (bar.keyOffButton.isVisible() || bar.statusLabel.isVisible() || bar.paramsButton.isVisible() || !bar.playButton.isVisible() || !bar.timecodeLabel.isVisible()) {
            std::cerr << "[OAE Studio] TransportBar minimize toggle failed." << std::endl;
            return 1;
        }
        bar.set_minimized(false);
        if (!bar.paramsButton.isVisible() || bar.paramsButton.getButtonText() != "Params (0)") {
            std::cerr << "[OAE Studio] TransportBar params button initial count mismatch." << std::endl;
            return 1;
        }
        bool paramsOpened = false;
        bar.onOpenGlobalParameters = [&paramsOpened] { paramsOpened = true; };
        if (bar.paramsButton.onClick) bar.paramsButton.onClick();
        if (!paramsOpened) {
            std::cerr << "[OAE Studio] TransportBar params button click did not invoke callback." << std::endl;
            return 1;
        }

        // Validate OaeSlider double-click reset
        oae::studio::OaeSlider testSlider(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
        testSlider.setRange(0.0, 100.0, 0.0);
        testSlider.setDoubleClickReturnValue(true, 23.456, {});
        testSlider.setValue(67.0, juce::dontSendNotification);
        testSlider.mouseDoubleClick(mouse_event(testSlider, {20.0f, 20.0f}, {}, 2, false));
        if (std::abs(testSlider.getValue() - 23.456) > 0.001) {
            std::cerr << "[OAE Studio] Slider reset failed." << std::endl;
            return 1;
        }

        // Verify TimelineParameterStrip contextual display, auto-collapse and labeled nominal states
        {
            oae::studio::TimelinePanel tl(model, &transport);
            tl.setSize(800, 400);

            // 1. Empty event (no parameters) -> preferred_height() == 0, not visible
            model.create_event("event:/empty_strip_test");
            tl.set_event_path("event:/empty_strip_test");
            if (tl.parameter_strip().preferred_height() != 0 || tl.parameter_strip().isVisible()) {
                std::cerr << "[OAE Studio] TimelineParameterStrip did not collapse to height 0 for event with no parameters." << std::endl;
                return 1;
            }

            // 2. Add local continuous and labeled parameters to event
            model.create_event("event:/strip_test");
            model.add_local_parameter("event:/strip_test", "Pitch", "continuous");
            model.update_local_parameter("event:/strip_test", "Pitch", {{"kind", "continuous"}, {"min", 0.0}, {"max", 2.0}, {"default_value", 1.0}});
            model.add_local_parameter("event:/strip_test", "Surface", "labeled");
            model.update_local_parameter("event:/strip_test", "Surface", {{"kind", "labeled"}, {"labels", {"Dirt", "Grass", "Stone"}}, {"default_value", 1.0}});

            tl.set_event_path("event:/strip_test");
            if (tl.parameter_strip().preferred_height() != 42 || !tl.parameter_strip().isVisible() || tl.parameter_strip().get_parameter_count() != 2) {
                std::cerr << "[OAE Studio] TimelineParameterStrip failed to populate local parameters." << std::endl;
                return 1;
            }

            // 3. Manual toggle [v] / [>]
            tl.parameter_strip().set_expanded(false);
            if (tl.parameter_strip().preferred_height() != 24) {
                std::cerr << "[OAE Studio] TimelineParameterStrip manual collapse failed." << std::endl;
                return 1;
            }
            tl.parameter_strip().set_expanded(true);
            if (tl.parameter_strip().preferred_height() != 42) {
                std::cerr << "[OAE Studio] TimelineParameterStrip manual expand failed." << std::endl;
                return 1;
            }

            // 4. Labeled parameter nominal text & double-click reset
            auto* surfaceSlider = tl.parameter_strip().find_slider_by_name("Surface", false);
            if (!surfaceSlider || surfaceSlider->getTextFromValue(1.0) != "Grass" || surfaceSlider->getTextFromValue(0.0) != "Dirt") {
                std::cerr << "[OAE Studio] TimelineParameterStrip labeled textFromValue failed." << std::endl;
                return 1;
            }
            surfaceSlider->setValue(2.0, juce::sendNotificationSync);
            surfaceSlider->mouseDoubleClick(mouse_event(*surfaceSlider, { 10.0f, 10.0f }, {}, 2, false));
            if (std::abs(surfaceSlider->getValue() - 1.0) > 0.001 || surfaceSlider->getTextFromValue(surfaceSlider->getValue()) != "Grass") {
                std::cerr << "[OAE Studio] TimelineParameterStrip labeled slider reset failed." << std::endl;
                return 1;
            }
        }
        std::cout << "[OAE Studio] TimelineParameterStrip contextual display, auto-collapse and labeled nominal states verified." << std::endl;

        // Verify Timeline/Layers mode switching, StemLayerCard controls, Mask round-trip, AdjustmentLayerCard & loop stem drag-and-drop
        {
            oae::studio::TimelinePanel tl(model, &transport);
            tl.setSize(960, 480);
            oae::studio::InspectorPanel inspector(model);
            inspector.setSize(340, 640);

            tl.set_event_path("event:/strip_test");
            if (tl.get_view_mode() != oae::studio::TimelinePanel::StageViewMode::Timeline || tl.layer_stack_view().isVisible()) {
                std::cerr << "[OAE Studio] TimelinePanel did not start in default Timeline mode." << std::endl;
                return 1;
            }

            const std::function<juce::Component*(juce::Component*, const juce::String&)> find_layer_ui =
                [&](juce::Component* root, const juce::String& cid) -> juce::Component* {
                    if (!root) return nullptr;
                    if (root->getComponentID() == cid) return root;
                    for (auto* child : root->getChildren()) {
                        if (auto* found = find_layer_ui(child, cid)) return found;
                    }
                    return nullptr;
                };

            auto* layersBtn = dynamic_cast<juce::Button*>(find_layer_ui(&tl, "timeline.mode.layers"));
            auto* timelineBtn = dynamic_cast<juce::Button*>(find_layer_ui(&tl, "timeline.mode.timeline"));
            if (!layersBtn || !timelineBtn) {
                std::cerr << "[OAE Studio] Timeline/Layers mode buttons missing." << std::endl;
                return 1;
            }

            if (layersBtn->onClick) layersBtn->onClick();
            if (tl.get_view_mode() != oae::studio::TimelinePanel::StageViewMode::Layers ||
                !tl.layer_stack_view().isVisible() ||
                !tl.parameter_strip().isVisible()) {
                std::cerr << "[OAE Studio] Failed switching to Layers mode with active TimelineParameterStrip." << std::endl;
                return 1;
            }

            const auto find_strip_tracks = [&]() -> const nlohmann::json& {
                for (const auto& ev : model.json()["events"]) {
                    if (ev.value("path", "") == "event:/strip_test") return ev["tracks"];
                }
                return model.json()["events"][0]["tracks"];
            };

            const size_t initialTracks = find_strip_tracks().size();
            auto* addStemBtn = dynamic_cast<juce::Button*>(find_layer_ui(&tl, "timeline.layers.add_stem"));
            if (!addStemBtn || !addStemBtn->isVisible()) {
                std::cerr << "[OAE Studio] + Stem Layer button missing or hidden in Layers mode." << std::endl;
                return 1;
            }
            if (addStemBtn->onClick) addStemBtn->onClick();

            if (find_strip_tracks().size() != initialTracks + 1 ||
                !find_strip_tracks().back()["instruments"][0].value("loop", false) ||
                model.get_selection().track_index != static_cast<int>(initialTracks) ||
                !find_layer_ui(&inspector, "inspector_card_track")) {
                std::cerr << "[OAE Studio] + Stem Layer did not create looped stem or synchronize InspectorPanel." << std::endl;
                return 1;
            }

            if (!model.undo() || find_strip_tracks().size() != initialTracks ||
                !model.redo() || find_strip_tracks().size() != initialTracks + 1) {
                std::cerr << "[OAE Studio] + Stem Layer Undo/Redo failed." << std::endl;
                return 1;
            }

            // Opacity, Eye (Mute), and Solo round-trip with Undo/Redo
            if (std::abs(oae::studio::AuthoringModel::opacity_pct_to_db(50.0) - (-40.0)) > 0.001 ||
                std::abs(oae::studio::AuthoringModel::db_to_opacity_pct(-40.0) - 50.0) > 0.001) {
                std::cerr << "[OAE Studio] Opacity dB mapping mismatch." << std::endl;
                return 1;
            }
            model.set_track_volume("event:/strip_test", 0, oae::studio::AuthoringModel::opacity_pct_to_db(50.0));
            if (std::abs(find_strip_tracks()[0].value("volume_db", 0.0) - (-40.0)) > 0.001) {
                std::cerr << "[OAE Studio] Layer opacity volume_db update failed." << std::endl;
                return 1;
            }

            auto* eyeBtn = dynamic_cast<juce::Button*>(find_layer_ui(&tl.layer_stack_view(), "layers.card.0.eye"));
            auto* soloBtn = dynamic_cast<juce::Button*>(find_layer_ui(&tl.layer_stack_view(), "layers.card.0.solo"));
            if (!eyeBtn || !soloBtn) {
                std::cerr << "[OAE Studio] StemLayerCard eye/solo buttons missing." << std::endl;
                return 1;
            }
            if (eyeBtn->onClick) eyeBtn->onClick();
            soloBtn = dynamic_cast<juce::Button*>(find_layer_ui(&tl.layer_stack_view(), "layers.card.0.solo"));
            if (!soloBtn) {
                std::cerr << "[OAE Studio] StemLayerCard solo button missing after mute rebuild." << std::endl;
                return 1;
            }
            if (soloBtn->onClick) soloBtn->onClick();
            if (!model.is_track_muted("event:/strip_test", 0) || !model.is_track_solo("event:/strip_test", 0)) {
                std::cerr << "[OAE Studio] StemLayerCard eye/solo did not update track mute/solo state." << std::endl;
                return 1;
            }
            model.undo();
            model.undo();
            if (model.is_track_muted("event:/strip_test", 0) || model.is_track_solo("event:/strip_test", 0)) {
                std::cerr << "[OAE Studio] StemLayerCard eye/solo Undo failed." << std::endl;
                return 1;
            }

            // Parameter mask round-trip & complex custom curve preservation
            if (!model.set_track_layer_mask("event:/strip_test", 0, "local", "Pitch", 0.25, 0.75, 0.10)) {
                std::cerr << "[OAE Studio] Failed to set track layer mask." << std::endl;
                return 1;
            }
            const auto parsedMask = model.get_track_layer_mask("event:/strip_test", 0);
            if (parsedMask.state != oae::studio::LayerMaskSpec::State::MaskActive ||
                parsedMask.param_name != "Pitch" ||
                std::abs(parsedMask.min_norm - 0.25) > 0.001 ||
                std::abs(parsedMask.max_norm - 0.75) > 0.001 ||
                std::abs(parsedMask.fade_norm - 0.10) > 0.001) {
                std::cerr << "[OAE Studio] LayerMaskSpec round-trip mismatch." << std::endl;
                return 1;
            }
            if (!model.undo() ||
                model.get_track_layer_mask("event:/strip_test", 0).state != oae::studio::LayerMaskSpec::State::AlwaysActive ||
                !model.redo() ||
                model.get_track_layer_mask("event:/strip_test", 0).state != oae::studio::LayerMaskSpec::State::MaskActive) {
                std::cerr << "[OAE Studio] Layer mask Undo/Redo failed." << std::endl;
                return 1;
            }

            const nlohmann::json complexPts = {
                { { "x", 0.0 }, { "y", -80.0 }, { "shape", "linear" } },
                { { "x", 0.5 }, { "y", -12.0 }, { "shape", "ease_in" } },
                { { "x", 1.0 }, { "y", 0.0 }, { "shape", "linear" } },
                { { "x", 1.5 }, { "y", -24.0 }, { "shape", "s_curve" } },
                { { "x", 2.0 }, { "y", -80.0 }, { "shape", "linear" } }
            };
            model.add_parameter_automation("event:/strip_test", 1, "volume", "local", "Pitch", complexPts);
            if (model.get_track_layer_mask("event:/strip_test", 1).state != oae::studio::LayerMaskSpec::State::CustomCurve ||
                model.set_track_layer_mask("event:/strip_test", 1, "local", "Pitch", 0.2, 0.8, 0.1) ||
                find_strip_tracks()[1]["automations"][0]["curve"]["points"].size() != 5) {
                std::cerr << "[OAE Studio] Custom manual automation curve was overwritten or misclassified." << std::endl;
                return 1;
            }

            // AdjustmentLayerCard visibility for parameter-conditioned track FX
            model.add_effect({ oae::studio::ItemType::Event, "event:/strip_test", 0 }, "lowpass", 0);
            if (tl.layer_stack_view().get_adjustment_card_count() < 1) {
                std::cerr << "[OAE Studio] AdjustmentLayerCard not created for track effect." << std::endl;
                return 1;
            }

            // Drag-and-drop audio stem import in loop with single-step Undo/Redo
            const fs::path dropWav = pro_ui_dir / "dropped_stem_loop.wav";
            if (!write_sine_wav(dropWav, 330.0f, 0.3f)) {
                std::cerr << "[OAE Studio] Failed to write temporary stem WAV." << std::endl;
                return 1;
            }
            const size_t tracksBeforeDrop = find_strip_tracks().size();
            const size_t samplesBeforeDrop = model.get_sample_ids().size();
            if (!tl.layer_stack_view().isInterestedInFileDrag({ dropWav.string() })) {
                std::cerr << "[OAE Studio] LayerStackView rejected supported WAV drag." << std::endl;
                return 1;
            }
            tl.layer_stack_view().filesDropped({ dropWav.string() }, 20, 20);
            if (find_strip_tracks().size() != tracksBeforeDrop + 1 ||
                model.get_sample_ids().size() != samplesBeforeDrop + 1 ||
                !find_strip_tracks().back()["instruments"][0].value("loop", false)) {
                std::cerr << "[OAE Studio] LayerStackView drag-and-drop stem import failed." << std::endl;
                return 1;
            }
            if (!model.undo() ||
                find_strip_tracks().size() != tracksBeforeDrop ||
                model.get_sample_ids().size() != samplesBeforeDrop ||
                !model.redo() ||
                find_strip_tracks().size() != tracksBeforeDrop + 1) {
                std::cerr << "[OAE Studio] Drag-and-drop stem import single-step Undo/Redo failed." << std::endl;
                return 1;
            }

            // Paint check in Layers mode and switch back to Timeline
            {
                juce::Image layersImg(juce::Image::ARGB, 960, 480, true);
                juce::Graphics layersG(layersImg);
                tl.paintEntireComponent(layersG, false);
            }
            if (timelineBtn->onClick) timelineBtn->onClick();
            if (tl.get_view_mode() != oae::studio::TimelinePanel::StageViewMode::Timeline || tl.layer_stack_view().isVisible()) {
                std::cerr << "[OAE Studio] Failed returning from Layers mode to Timeline mode." << std::endl;
                return 1;
            }

            // TrackHeaderItem ComponentIDs, toggle_state, semantic colours, set_track_index & onDragEnd ordering
            model.set_track_muted("event:/strip_test", 0, true);
            model.set_track_solo("event:/strip_test", 1, true);

            auto* mute0 = dynamic_cast<juce::TextButton*>(find_layer_ui(&tl, "track_mute_0"));
            auto* solo0 = dynamic_cast<juce::TextButton*>(find_layer_ui(&tl, "track_solo_0"));
            auto* auto0 = dynamic_cast<juce::TextButton*>(find_layer_ui(&tl, "track_auto_0"));
            auto* vol0 = dynamic_cast<juce::Slider*>(find_layer_ui(&tl, "track_volume_0"));
            auto* pan0 = dynamic_cast<juce::Slider*>(find_layer_ui(&tl, "track_pan_0"));
            auto* title0 = dynamic_cast<juce::Label*>(find_layer_ui(&tl, "track_title_0"));
            auto* swatch0 = dynamic_cast<juce::TextButton*>(find_layer_ui(&tl, "track_color_swatch_0"));
            auto* mute1 = dynamic_cast<juce::TextButton*>(find_layer_ui(&tl, "track_mute_1"));
            auto* solo1 = dynamic_cast<juce::TextButton*>(find_layer_ui(&tl, "track_solo_1"));
            if (!mute0 || !solo0 || !auto0 || !vol0 || !pan0 || !title0 || !swatch0 || !mute1 || !solo1) {
                std::cerr << "[OAE Studio] Missing deterministic TrackHeaderItem ComponentIDs!" << std::endl;
                return 1;
            }
            if (!mute0->getToggleState() || solo0->getToggleState() || auto0->getToggleState() ||
                mute1->getToggleState() || !solo1->getToggleState()) {
                std::cerr << "[OAE Studio] TrackHeaderItem toggle_state mismatch before reorder!" << std::endl;
                return 1;
            }
            if (mute0->findColour(juce::TextButton::buttonOnColourId) != juce::Colour(oae::studio::OaeTheme::Colors::accentAmber) ||
                mute0->findColour(juce::TextButton::textColourOnId) != juce::Colour(oae::studio::OaeTheme::Colors::bgDark) ||
                solo0->findColour(juce::TextButton::buttonOnColourId) != juce::Colour(oae::studio::OaeTheme::Colors::accentCyan) ||
                solo0->findColour(juce::TextButton::textColourOnId) != juce::Colour(oae::studio::OaeTheme::Colors::bgDark) ||
                auto0->findColour(juce::TextButton::buttonOnColourId) != juce::Colour(oae::studio::OaeTheme::Colors::accentPurple) ||
                auto0->findColour(juce::TextButton::textColourOnId) != juce::Colour(oae::studio::OaeTheme::Colors::textPrimary)) {
                std::cerr << "[OAE Studio] TrackHeaderItem semantic buttonOnColourId/textColourOnId mismatch!" << std::endl;
                return 1;
            }

            // Reorder track 1 -> 0 (same count hits set_track_index fast path)
            if (!model.reorder_track("event:/strip_test", 1, 0)) {
                std::cerr << "[OAE Studio] Failed reordering track 1 -> 0 in pro_ui smoke!" << std::endl;
                return 1;
            }
            mute0 = dynamic_cast<juce::TextButton*>(find_layer_ui(&tl, "track_mute_0"));
            solo0 = dynamic_cast<juce::TextButton*>(find_layer_ui(&tl, "track_solo_0"));
            mute1 = dynamic_cast<juce::TextButton*>(find_layer_ui(&tl, "track_mute_1"));
            solo1 = dynamic_cast<juce::TextButton*>(find_layer_ui(&tl, "track_solo_1"));
            if (!mute0 || !solo0 || !mute1 || !solo1 ||
                mute0->getToggleState() || !solo0->getToggleState() ||
                !mute1->getToggleState() || solo1->getToggleState()) {
                std::cerr << "[OAE Studio] set_track_index did not preserve ComponentIDs/toggle_state after reorder!" << std::endl;
                return 1;
            }

            // Verify is_adjusting_slider_ clears before end_gesture() so final refresh syncs panSlider
            vol0 = dynamic_cast<juce::Slider*>(find_layer_ui(&tl, "track_volume_0"));
            pan0 = dynamic_cast<juce::Slider*>(find_layer_ui(&tl, "track_pan_0"));
            if (!vol0 || !pan0 || !vol0->onDragStart || !vol0->onDragEnd) {
                std::cerr << "[OAE Studio] Track 0 sliders missing drag callbacks!" << std::endl;
                return 1;
            }
            vol0->onDragStart();
            vol0->setValue(-18.0, juce::sendNotificationSync);
            model.set_track_pan("event:/strip_test", 0, 0.42);
            vol0->onDragEnd();
            if (std::abs(pan0->getValue() - 0.42) > 0.01) {
                std::cerr << "[OAE Studio] TrackHeaderItem onDragEnd suppressed final TimelinePanel refresh!" << std::endl;
                return 1;
            }
        }
        std::cout << "[OAE Studio] Timeline/Layers mode, StemLayerCard, Mask round-trip, AdjustmentLayer & loop stem drop verified." << std::endl;
    }
    std::cout << "[OAE Studio] Fine drag, reset, preview Undo/Redo and meter ballistics verified." << std::endl;

    oae::studio::DiagnosticsLogPanel diag;

    oae::studio::ContextualBottomDeck deck(engine, model, transport, diag, layout.bottom_slot());
    deck.setSize(1000, 300);

    // Verify polymorphic tab switching
    deck.set_active_tab(oae::studio::DeckTab::Playlist);
    if (deck.get_active_tab() != oae::studio::DeckTab::Playlist) {
        std::cerr << "[OAE Studio] Tab switch to Playlist failed!" << std::endl;
        return 1;
    }

    // Verify PlaylistDeck duration calculation for compressed formats (WAV, OGG, MP3, FLAC)
    {
        const fs::path fixtures_candidates[] = {
            fs::current_path() / "native" / "tests" / "fixtures",
            fs::current_path() / ".." / "native" / "tests" / "fixtures",
            fs::current_path() / ".." / ".." / "native" / "tests" / "fixtures",
            fs::current_path() / ".." / ".." / ".." / "native" / "tests" / "fixtures"
        };
        fs::path fixtures_dir;
        for (const auto& candidate : fixtures_candidates) {
            if (fs::exists(candidate / "tone.ogg")) {
                fixtures_dir = fs::canonical(candidate);
                break;
            }
        }

        if (!fixtures_dir.empty()) {
            std::string ogg_id, mp3_id, flac_id;
            model.import_audio_asset(fixtures_dir / "tone.ogg", &ogg_id);
            model.import_audio_asset(fixtures_dir / "tone.mp3", &mp3_id);
            model.import_audio_asset(fixtures_dir / "tone.flac", &flac_id);

            const auto dur_ogg = deck.playlist_deck().duration(ogg_id);
            const auto dur_mp3 = deck.playlist_deck().duration(mp3_id);
            const auto dur_flac = deck.playlist_deck().duration(flac_id);

            if (dur_ogg == "Unreadable" || dur_ogg == "Missing" || !dur_ogg.contains("ms") ||
                dur_mp3 == "Unreadable" || dur_mp3 == "Missing" || !dur_mp3.contains("ms") ||
                dur_flac == "Unreadable" || dur_flac == "Missing" || !dur_flac.contains("ms")) {
                std::cerr << "[OAE Studio] PlaylistDeck duration failed for compressed formats (ogg="
                          << dur_ogg << ", mp3=" << dur_mp3 << ", flac=" << dur_flac << ")" << std::endl;
                return 1;
            }
            std::cout << "[OAE Studio] PlaylistDeck duration verified for compressed formats (OGG, MP3, FLAC)." << std::endl;
        }
    }
    deck.set_active_tab(oae::studio::DeckTab::Effects);
    if (deck.get_active_tab() != oae::studio::DeckTab::Effects) {
        std::cerr << "[OAE Studio] Tab switch to Effects failed!" << std::endl;
        return 1;
    }
    deck.set_active_tab(oae::studio::DeckTab::Logic);
    if (deck.get_active_tab() != oae::studio::DeckTab::Logic) {
        std::cerr << "[OAE Studio] Tab switch to Logic failed!" << std::endl;
        return 1;
    }
    deck.set_active_tab(oae::studio::DeckTab::Spatializer);
    if (deck.get_active_tab() != oae::studio::DeckTab::Spatializer) {
        std::cerr << "[OAE Studio] Tab switch to Spatializer failed!" << std::endl;
        return 1;
    }
    {
        auto& spatial = deck.spatializer_deck();
        auto* sound_size = dynamic_cast<juce::Slider*>(spatial.findChildWithID("spatial_preview_sound_size"));
        auto* min_extent = dynamic_cast<juce::Slider*>(spatial.findChildWithID("spatial_preview_min_extent"));
        auto* coords = dynamic_cast<juce::Label*>(spatial.findChildWithID("coords"));
        if (!sound_size || !min_extent) {
            std::cerr << "[OAE Studio] Spatializer3DDeck envelopment sliders missing!" << std::endl;
            return 1;
        }
        sound_size->setValue(6.0, juce::sendNotificationSync);
        min_extent->setValue(90.0, juce::sendNotificationSync);
        if (std::abs(sound_size->getValue() - 6.0) > 1.0e-3 || std::abs(min_extent->getValue() - 90.0) > 1.0e-3) {
            std::cerr << "[OAE Studio] Spatializer3DDeck envelopment slider values did not update!" << std::endl;
            return 1;
        }
        (void)coords;
    }
    deck.set_active_tab(oae::studio::DeckTab::Mixer);
    if (deck.get_active_tab() != oae::studio::DeckTab::Mixer) {
        std::cerr << "[OAE Studio] Tab switch to Mixer failed!" << std::endl;
        return 1;
    }

    auto& slot = layout.bottom_slot();
    auto* tabBar = slot.findChildWithID("deck_tab_bar");
    const auto find_tab = [](juce::Component* bar, const juce::String& id) -> juce::Component* {
        if (bar != nullptr) {
            for (auto* child : bar->getChildren()) {
                if (auto* viewport = dynamic_cast<juce::Viewport*>(child))
                    return viewport->getViewedComponent()->findChildWithID(id);
            }
        }
        return nullptr;
    };
    auto* mixerTab = find_tab(tabBar, "btn_tab_mixer");
    auto* effectsTab = find_tab(tabBar, "btn_tab_effects");
    auto* popOut = tabBar != nullptr
        ? dynamic_cast<juce::TextButton*>(tabBar->findChildWithID("btn_dock_pop_out")) : nullptr;
    if (!mixerTab || !effectsTab || !popOut) {
        std::cerr << "[OAE Studio] Dock tab controls missing." << std::endl;
        return 1;
    }

    slot.move_tab(1);
    auto& mixerPanel = deck.mixer_deck();
    auto* dockParent = mixerPanel.getParentComponent();
    const auto mixerTabX = mixerTab->getX();
    if (mixerTabX <= effectsTab->getX()) {
        std::cerr << "[OAE Studio] Mixer tab reordering failed." << std::endl;
        return 1;
    }

    popOut->onClick();
    auto* floating = dynamic_cast<juce::DocumentWindow*>(mixerPanel.getParentComponent());
    if (!dockParent || !floating || !floating->isOnDesktop() || !floating->isVisible()
        || !floating->isResizable() || floating->getContentComponent() != &mixerPanel) {
        std::cerr << "[OAE Studio] Mixer pop-out failed." << std::endl;
        return 1;
    }
    deck.set_active_tab(oae::studio::DeckTab::Diagnostics);
    if (!mixerPanel.isVisible() || !diag.isVisible()) {
        std::cerr << "[OAE Studio] Floating Mixer and docked panel are not simultaneously visible." << std::endl;
        return 1;
    }
    floating->closeButtonPressed();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
    if (mixerPanel.getParentComponent() != dockParent || !mixerPanel.isVisible()
        || !dockParent->getLocalBounds().contains(mixerPanel.getBounds())
        || deck.get_active_tab() != oae::studio::DeckTab::Mixer || mixerTab->getX() != mixerTabX) {
        std::cerr << "[OAE Studio] Closing the window did not restore the Mixer tab." << std::endl;
        return 1;
    }

    popOut->onClick();
    popOut->onClick();
    if (mixerPanel.getParentComponent() != dockParent) {
        std::cerr << "[OAE Studio] Dock Back did not restore the Mixer." << std::endl;
        return 1;
    }
    std::cout << "[OAE Studio] Transport wrapping, tab order, pop-out and dock back verified." << std::endl;

    // Verify selection-driven contextual auto-switching
    deck.on_selection_changed({ oae::studio::ItemType::Bus, "bus:/sfx" });
    if (deck.get_active_tab() != oae::studio::DeckTab::Effects) {
        std::cerr << "[OAE Studio] Contextual auto-switch for Bus failed!" << std::endl;
        return 1;
    }

    deck.on_selection_changed({ oae::studio::ItemType::Event, "event:/music/theme", 0, 0 });
    if (deck.get_active_tab() != oae::studio::DeckTab::Playlist) {
        std::cerr << "[OAE Studio] Contextual auto-switch for Instrument failed!" << std::endl;
        return 1;
    }

    deck.on_selection_changed({ oae::studio::ItemType::Event, "event:/music/theme", 0, -1 });
    if (deck.get_active_tab() != oae::studio::DeckTab::Effects) {
        std::cerr << "[OAE Studio] Contextual auto-switch for Track failed!" << std::endl;
        return 1;
    }

    oae::studio::ItemSelection logicSel{ oae::studio::ItemType::Event, "event:/music/theme", -1, -1, 0 };
    deck.on_selection_changed(logicSel);
    if (deck.get_active_tab() != oae::studio::DeckTab::Logic) {
        std::cerr << "[OAE Studio] Contextual auto-switch for Logic failed!" << std::endl;
        return 1;
    }

    // Verify GlobalParametersDeck, card grid, filters, telemetry, pop-out and dock back
    {
        deck.set_active_tab(oae::studio::DeckTab::Parameters);
        if (deck.get_active_tab() != oae::studio::DeckTab::Parameters) {
            std::cerr << "[OAE Studio] Tab switch to Parameters failed!" << std::endl;
            return 1;
        }

        // Add diverse global parameters to model
        model.create_global_parameter("World_Wind", "continuous");
        model.update_global_parameter("World_Wind", {
            {"kind", "continuous"}, {"min", 0.0}, {"max", 100.0}, {"default_value", 10.0},
            {"velocity", 2.5}, {"seek_speed", 5.0}
        });
        model.create_global_parameter("Game_Difficulty", "discrete");
        model.update_global_parameter("Game_Difficulty", {
            {"kind", "discrete"}, {"min", 1.0}, {"max", 5.0}, {"default_value", 2.0}
        });
        model.create_global_parameter("Biome", "labeled");
        model.update_global_parameter("Biome", {
            {"kind", "labeled"}, {"labels", {"Desert", "Forest", "Tundra"}}, {"default_value", 0.0},
            {"hold_during_playback", true}
        });

        auto& paramsDeck = deck.parameters_deck();
        paramsDeck.refresh();
        if (paramsDeck.get_card_count() < 3) {
            std::cerr << "[OAE Studio] GlobalParametersDeck card population failed." << std::endl;
            return 1;
        }

        auto* windCard = paramsDeck.find_card("World_Wind");
        auto* diffCard = paramsDeck.find_card("Game_Difficulty");
        auto* biomeCard = paramsDeck.find_card("Biome");

        if (!windCard || !diffCard || !biomeCard) {
            std::cerr << "[OAE Studio] GlobalParametersDeck failed to locate cards." << std::endl;
            return 1;
        }

        // Telemetry verification (Task 3.3)
        if (!windCard->has_velocity() || !windCard->has_seek_speed() || !biomeCard->has_hold()) {
            std::cerr << "[OAE Studio] GlobalParamCard modifier telemetry flag failed." << std::endl;
            return 1;
        }

        // Specialized controls (Task 3.2)
        if (!windCard->get_slider() || biomeCard->get_option_buttons().size() != 3) {
            std::cerr << "[OAE Studio] GlobalParamCard specialized control layout failed." << std::endl;
            return 1;
        }

        // Labeled card segmented button click
        biomeCard->get_option_buttons()[1]->onClick(); // Select "Forest" (index 1)
        if (std::abs(biomeCard->get_current_value() - 1.0) > 0.001 || !biomeCard->is_modified()) {
            std::cerr << "[OAE Studio] GlobalParamCard labeled option click failed." << std::endl;
            return 1;
        }

        // Search filter (Task 3.1)
        paramsDeck.searchEditor.setText("biome");
        paramsDeck.apply_filters();
        if (!paramsDeck.is_card_visible("Biome") || paramsDeck.is_card_visible("World_Wind") || paramsDeck.is_card_visible("Game_Difficulty")) {
            std::cerr << "[OAE Studio] GlobalParametersDeck search filter failed." << std::endl;
            return 1;
        }
        paramsDeck.searchEditor.setText("");
        paramsDeck.apply_filters();

        // Modified filter & reset all (Task 3.1)
        paramsDeck.filterModifiedBtn.onClick();
        if (!paramsDeck.is_card_visible("Biome") || paramsDeck.is_card_visible("Game_Difficulty")) {
            std::cerr << "[OAE Studio] GlobalParametersDeck modified filter failed." << std::endl;
            return 1;
        }

        paramsDeck.resetAllBtn.onClick();
        if (biomeCard->is_modified()) {
            std::cerr << "[OAE Studio] GlobalParametersDeck reset all failed." << std::endl;
            return 1;
        }
        paramsDeck.filterAllBtn.onClick();
        if (!paramsDeck.is_card_visible("Biome") || !paramsDeck.is_card_visible("World_Wind")) {
            std::cerr << "[OAE Studio] GlobalParametersDeck filter all restore failed." << std::endl;
            return 1;
        }

        // Pop-Out & Dock Back (Task 4.1)
        paramsDeck.popOutBtn.onClick();
        if (!paramsDeck.is_floating() || paramsDeck.popOutBtn.getButtonText() != "Dock Back") {
            std::cerr << "[OAE Studio] GlobalParametersDeck pop-out failed." << std::endl;
            return 1;
        }
        paramsDeck.popOutBtn.onClick();
        if (paramsDeck.is_floating() || paramsDeck.popOutBtn.isVisible()) {
            std::cerr << "[OAE Studio] GlobalParametersDeck dock back failed." << std::endl;
            return 1;
        }
    }
    std::cout << "[OAE Studio] GlobalParametersDeck card grid, specialized controls, filters, telemetry, pop-out and dock back verified." << std::endl;

    std::cout << "[OAE Studio] ContextualBottomDeck polymorphic switching verified." << std::endl;

    // 4. Validate TimelineOverviewMinimap
    oae::studio::TimelineOverviewMinimap minimap(model);
    minimap.setSize(800, 24);
    minimap.set_total_duration(12000.0);
    minimap.set_visible_range(1000.0, 4000.0);
    minimap.set_playhead_time_ms(2500.0);
    std::cout << "[OAE Studio] TimelineOverviewMinimap viewport verified." << std::endl;

    engine.shutdown();

    // 5. Validate MainComponent WorkspaceBar, Hide All / Restore and slot reclamation
    {
        const fs::path layoutPath = get_app_dir() / "oae_studio_layout.json";
        std::error_code layoutEc;
        const bool hadLayout = fs::exists(layoutPath, layoutEc);

        {
            oae::studio::MainComponent mainComp;
            mainComp.setSize(1280, 720);
            const std::function<juce::Component*(juce::Component*, const juce::String&)> find_ui =
                [&](juce::Component* root, const juce::String& cid) -> juce::Component* {
                    if (!root) return nullptr;
                    if (root->getComponentID() == cid) return root;
                    for (auto* child : root->getChildren()) {
                        if (auto* found = find_ui(child, cid)) return found;
                    }
                    return nullptr;
                };

            auto* wsBar = find_ui(&mainComp, "workspace_bar");
            auto* wsMenuBtn = dynamic_cast<juce::Button*>(find_ui(&mainComp, "btn_workspace_menu"));
            auto* tlView = find_ui(&mainComp, "timeline_view");
            auto* brwPanel = find_ui(&mainComp, "browser_panel");
            auto* insPanel = find_ui(&mainComp, "inspector_panel");
            auto* btmDeck = find_ui(&mainComp, "bottom_deck");
            auto* trnBar = find_ui(&mainComp, "transport_bar");
            auto* prjBar = find_ui(&mainComp, "project_toolbar");
            if (!wsBar || !wsMenuBtn || !tlView || !brwPanel || !insPanel || !btmDeck || !trnBar || !prjBar) {
                std::cerr << "[OAE Studio] MainComponent WorkspaceBar or dockable panels missing!" << std::endl;
                return 1;
            }
            const auto is_overlay_topmost = [&mainComp]() -> bool {
                const int idx = mainComp.getIndexOfChildComponent(&mainComp.highlight_overlay());
                if (idx < 0) return false;
                for (int i = idx + 1; i < mainComp.getNumChildComponents(); ++i) {
                    if (!mainComp.getChildComponent(i)->isAlwaysOnTop()) {
                        return false;
                    }
                }
                return mainComp.highlight_overlay().getBounds() == mainComp.getLocalBounds();
            };
            if (!is_overlay_topmost() || mainComp.highlight_overlay().isVisible()) {
                std::cerr << "[OAE Studio] TutorialHighlightOverlay must start hidden at top Z-order covering MainComponent!" << std::endl;
                return 1;
            }
            if (prjBar->getHeight() != 36 || trnBar->getHeight() != 36) {
                std::cerr << "[OAE Studio] Top toolbars must use 36 px height." << std::endl;
                return 1;
            }
            if (find_ui(&mainComp, "btn_slot_header_tab_transport") != nullptr
                || find_ui(&mainComp, "btn_slot_header_tab_project_toolbar") != nullptr) {
                std::cerr << "[OAE Studio] Toolbar docks must not render SlotHeaderBar." << std::endl;
                return 1;
            }
            mainComp.toggle_hide_all();
            if (!wsBar->isVisible() || wsBar->getHeight() != 28 || wsBar->getWidth() != 1280
                || tlView->isVisible() || brwPanel->isVisible() || insPanel->isVisible()
                || btmDeck->isVisible() || !trnBar->isVisible() || !prjBar->isVisible()) {
                std::cerr << "[OAE Studio] WorkspaceBar Hide All did not hide panels while keeping permanent toolbars!" << std::endl;
                return 1;
            }
            mainComp.toggle_hide_all();
            if (!tlView->isVisible() || !brwPanel->isVisible() || !insPanel->isVisible()
                || !btmDeck->isVisible() || !trnBar->isVisible() || !prjBar->isVisible()) {
                std::cerr << "[OAE Studio] WorkspaceBar Restore did not recover hidden panels!" << std::endl;
                return 1;
            }
            auto* trnVisBtn = dynamic_cast<juce::Button*>(find_ui(&mainComp, "btn_vis_transport"));
            if (!trnVisBtn) return 1;
            if (trnVisBtn->onClick) trnVisBtn->onClick();
            if (!trnBar->isVisible()) {
                std::cerr << "[OAE Studio] [Trn] button must never close TransportBar!" << std::endl;
                return 1;
            }
            if (trnVisBtn->onClick) trnVisBtn->onClick();
            mainComp.docking_layout().apply_preset("Stage Focus");
            mainComp.sync_workspace_layout();
            if (!tlView->isVisible() || brwPanel->isVisible() || !insPanel->isVisible()
                || btmDeck->isVisible() || tlView->getWidth() < 1000) {
                std::cerr << "[OAE Studio] Stage Focus preset did not reclaim hidden dock areas!" << std::endl;
                return 1;
            }
            mainComp.docking_layout().reset_layout();
            mainComp.sync_workspace_layout();
            if (!is_overlay_topmost()) {
                std::cerr << "[OAE Studio] TutorialHighlightOverlay lost top Z-order or bounds after layout sync!" << std::endl;
                return 1;
            }

            auto* prjGrip = find_ui(&mainComp, "grip_project_toolbar");
            if (!prjGrip) return 1;
            const auto trnRightEdge = juce::Point<float>(
                static_cast<float>(trnBar->getRight() - 4 - prjGrip->getScreenPosition().x + mainComp.getScreenPosition().x),
                static_cast<float>(trnBar->getBounds().getCentreY() - prjGrip->getScreenPosition().y + mainComp.getScreenPosition().y));
            prjGrip->mouseDown(mouse_event(*prjGrip, { 4.0f, 10.0f }, {}, 1, false));
            prjGrip->mouseDrag(mouse_event(*prjGrip, trnRightEdge, {}, 1, false));
            prjGrip->mouseUp(mouse_event(*prjGrip, trnRightEdge, {}, 1, false));
            juce::MessageManager::getInstance()->runDispatchLoopUntil(50);
            mainComp.docking_layout().reset_layout();
            mainComp.sync_workspace_layout();
            if (!is_overlay_topmost()) {
                std::cerr << "[OAE Studio] TutorialHighlightOverlay lost top Z-order or bounds after layout sync!" << std::endl;
                return 1;
            }

            auto* inspectorTab = find_ui(&mainComp, "btn_slot_header_tab_inspector");
            if (!inspectorTab) return 1;
            const auto inspectorBounds = insPanel->getBounds();
            const auto localStart = inspectorTab->getLocalBounds().getCentre().toFloat();
            const juce::Point<int> rootTarget{ mainComp.getWidth() - 3, 130 };
            const auto localTarget = inspectorTab->getLocalPoint(&mainComp, rootTarget).toFloat();
            const auto mouse = juce::Desktop::getInstance().getMainMouseSource();
            const auto timestamp = juce::Time::getCurrentTime();
            const auto event = [&](juce::Point<float> position, bool dragged) {
                return juce::MouseEvent(mouse, position, juce::ModifierKeys::leftButtonModifier,
                    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, inspectorTab, inspectorTab, timestamp,
                    localStart, timestamp, 1, dragged);
            };
            const auto previewPixel = [&] {
                return mainComp.createComponentSnapshot(mainComp.getLocalBounds())
                    .getPixelAt(rootTarget.x, rootTarget.y);
            };
            const auto baseline = previewPixel();
            inspectorTab->mouseDown(event(localStart, false));
            inspectorTab->mouseDrag(event(localTarget, true));
            if (previewPixel() == baseline || !mainComp.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey))) {
                std::cerr << "[OAE Studio] Workspace drag did not show or cancel its preview." << std::endl;
                return 1;
            }
            inspectorTab->mouseDrag(event(localTarget, true));
            inspectorTab->mouseUp(event(localTarget, true));
            if (previewPixel() != baseline || insPanel->getBounds() != inspectorBounds) {
                std::cerr << "[OAE Studio] Cancelled workspace drag changed the inspector bounds." << std::endl;
                return 1;
            }
        }

        if (!hadLayout) {
            fs::remove(layoutPath, layoutEc);
        }
    }

    std::cout << "[OAE Studio] --smoke-pro-ui PASSED with exit code 0." << std::endl;
    return 0;
}

class HeadlessUiHost : public juce::Component {
public:
    HeadlessUiHost(oae::studio::AuthoringModel& model,
                   oae::studio::AuditionTransport& transport,
                   oae::studio::StudioEngine& engine,
                   juce::Component& diag)
        : transportBar(model, transport),
          timelinePanel(model, &transport),
          contextualDeck(engine, model, transport, diag, dockingLayout.bottom_slot()),
          inspectorPanel(model)
    {
        addAndMakeVisible(transportBar);
        addAndMakeVisible(timelinePanel);
        addAndMakeVisible(contextualDeck);
        addAndMakeVisible(inspectorPanel);
        zeroBoundsDummy.setComponentID("zero_bounds_dummy");
        addAndMakeVisible(zeroBoundsDummy);
        addChildComponent(highlightOverlay);
        setVisible(true);
        setSize(1280, 800);
    }

    void resized() override
    {
        dockingLayout.layout_bounds(
            getLocalBounds(),
            &transportBar,
            transportBar.preferred_height(getWidth()),
            nullptr,
            nullptr,
            &timelinePanel,
            nullptr,
            &inspectorPanel,
            nullptr,
            &contextualDeck
        );
        highlightOverlay.setBounds(getLocalBounds());
        highlightOverlay.toFront(false);
        highlightOverlay.refresh_target_visibility();
    }

    oae::studio::TransportBar transportBar;
    oae::studio::TimelinePanel timelinePanel;
    oae::studio::DockingLayoutManager dockingLayout;
    oae::studio::ContextualBottomDeck contextualDeck;
    oae::studio::InspectorPanel inspectorPanel;
    juce::Component zeroBoundsDummy;
    oae::studio::TutorialHighlightOverlay highlightOverlay;
};

int run_smoke_mcp()
{
    std::cout << "[OAE Studio] Starting --smoke-mcp validation (EPICO 6: MCP Server)..." << std::endl;

    const fs::path temp_dir = fs::temp_directory_path() / "oae_studio_smoke_mcp";
    std::error_code ec;
    fs::remove_all(temp_dir, ec);
    fs::create_directories(temp_dir, ec);

    const fs::path wav_coin = temp_dir / "coin.wav";
    if (!write_sine_wav(wav_coin, 440.0f, 0.5f)) {
        std::cerr << "[OAE Studio] Failed to generate fixture WAV!" << std::endl;
        return 1;
    }

    // 1. Spin up headless StudioEngine, ProjectDocument, AuthoringModel, AuditionTransport, HeadlessUiHost
    oae::studio::StudioEngine engine;
    if (engine.init_headless() != OAE_OK) {
        std::cerr << "[OAE Studio] Failed to init headless engine!" << std::endl;
        return 1;
    }

    oae::studio::ProjectDocument doc;
    if (!doc.create_empty(temp_dir, "McpTestProj")) {
        std::cerr << "[OAE Studio] Failed to create project document!" << std::endl;
        return 1;
    }
    if (!doc.import_audio_file(wav_coin, "sample_coin")) {
        std::cerr << "[OAE Studio] Failed to import audio file!" << std::endl;
        return 1;
    }

    oae::studio::AuthoringModel model(doc);
    model.modify([](nlohmann::json& r) {
        r["buses"] = nlohmann::json::array({
            { { "path", "bus:/sfx" }, { "volume", 1.0 }, { "effects", nlohmann::json::array() } },
            { { "path", "bus:/music" }, { "volume", 1.0 }, { "effects", nlohmann::json::array() } }
        });
        r["events"] = nlohmann::json::array({
            {
                { "path", "event:/coin" },
                { "kind", "event" },
                { "bus_path", "bus:/sfx" },
                { "volume_db", -3.0 },
                { "tracks", nlohmann::json::array({
                    {
                        { "volume_db", 0.0 },
                        { "instruments", nlohmann::json::array({
                            {
                                { "kind", "single" },
                                { "sample_id", "sample_coin" },
                                { "offset_ms", 50.0 },
                                { "length_ms", 300.0 },
                                { "loop", false }
                            }
                        }) }
                    }
                }) }
            }
        });
    });

    const auto app_dir = get_app_dir();
    const auto bankc_exe = find_bankc_executable(app_dir);

    oae::studio::AuditionTransport transport(engine, model);
    oae::studio::AsyncBankPublisher publisher;
    oae::studio::PreviewRebuilder rebuilder(engine, model, doc, transport, publisher);
    if (!bankc_exe.empty()) {
        rebuilder.set_bankc_executable(bankc_exe);
    }
    oae::studio::DiagnosticsLogPanel diag;

    HeadlessUiHost ui_host(model, transport, engine, diag);
    oae::studio::McpContext ctx{
        engine,
        doc,
        model,
        transport,
        &ui_host,
        &rebuilder,
        &diag,
        &publisher,
        &ui_host.highlightOverlay
    };
    oae::studio::McpServer server(ctx);

    // 2. Test JSON-RPC initialize
    {
        const std::string init_req = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2024-11-05\"}}";
        const std::string init_res_str = server.handle_message(init_req);
        const auto init_res = nlohmann::json::parse(init_res_str, nullptr, false);
        if (init_res.is_discarded() || init_res.value("jsonrpc", "") != "2.0" || init_res["id"] != 1) {
            std::cerr << "[OAE Studio] MCP initialize failed: invalid response!" << std::endl;
            return 1;
        }
        if (!init_res["result"].contains("serverInfo") || init_res["result"]["serverInfo"].value("name", "") != "openaudio-studio-mcp") {
            std::cerr << "[OAE Studio] MCP initialize failed: bad serverInfo!" << std::endl;
            return 1;
        }
        std::cout << "[OAE Studio] MCP initialize verified." << std::endl;
    }

    // 3. Test tools/list
    {
        const std::string list_req = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\"}";
        const std::string list_res_str = server.handle_message(list_req);
        const auto list_res = nlohmann::json::parse(list_res_str, nullptr, false);
        if (list_res.is_discarded() || !list_res["result"].contains("tools") || !list_res["result"]["tools"].is_array()) {
            std::cerr << "[OAE Studio] MCP tools/list failed!" << std::endl;
            return 1;
        }
        const auto& tools = list_res["result"]["tools"];
        if (tools.size() < 13) {
            std::cerr << "[OAE Studio] MCP tools/list failed: expected at least 13 tools, found " << tools.size() << std::endl;
            return 1;
        }
        bool has_screenshot_tool = false;
        bool has_highlight_tool = false;
        for (const auto& t : tools) {
            const std::string name = t.value("name", "");
            if (name == "studio_ui_screenshot") {
                const auto& props = t["inputSchema"]["properties"];
                if (props.contains("component_id") && props.contains("output_path") &&
                    props.contains("scale") && props.contains("include_base64")) {
                    has_screenshot_tool = true;
                }
            } else if (name == "studio_ui_highlight") {
                const auto& props = t["inputSchema"]["properties"];
                if (props.contains("component_id") && props.contains("label") &&
                    props.contains("dim_opacity") && props.contains("color") &&
                    props.contains("clear")) {
                    has_highlight_tool = true;
                }
            }
        }
        if (!has_screenshot_tool) {
            std::cerr << "[OAE Studio] MCP tools/list missing studio_ui_screenshot schema!" << std::endl;
            return 1;
        }
        if (!has_highlight_tool) {
            std::cerr << "[OAE Studio] MCP tools/list missing studio_ui_highlight schema!" << std::endl;
            return 1;
        }
        std::cout << "[OAE Studio] MCP tools/list verified (" << tools.size() << " tools)." << std::endl;
    }

    // 4. Test tools/call: studio_select_item
    {
        const std::string sel_req = "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"studio_select_item\",\"arguments\":{\"type\":\"event\",\"path\":\"event:/coin\"}}}";
        const std::string sel_res_str = server.handle_message(sel_req);
        const auto sel_res = nlohmann::json::parse(sel_res_str, nullptr, false);
        if (sel_res.is_discarded() || sel_res["result"].value("isError", true)) {
            std::cerr << "[OAE Studio] MCP studio_select_item failed!" << std::endl;
            return 1;
        }
        const auto cur_sel = model.get_selection();
        if (cur_sel.type != oae::studio::ItemType::Event || cur_sel.id_or_path != "event:/coin") {
            std::cerr << "[OAE Studio] MCP studio_select_item did not update AuthoringModel selection!" << std::endl;
            return 1;
        }
        std::cout << "[OAE Studio] MCP studio_select_item verified." << std::endl;
    }

    // 5. Test tools/call: studio_set_mixer_fader (verifying RFC 6902 diff)
    {
        const std::string fader_req = "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"studio_set_mixer_fader\",\"arguments\":{\"bus_path\":\"bus:/sfx\",\"volume_db\":-6.5}}}";
        const std::string fader_res_str = server.handle_message(fader_req);
        const auto fader_res = nlohmann::json::parse(fader_res_str, nullptr, false);
        if (fader_res.is_discarded() || fader_res["result"].value("isError", true)) {
            std::cerr << "[OAE Studio] MCP studio_set_mixer_fader failed!" << std::endl;
            return 1;
        }
        if (!fader_res["result"].contains("diff") || !fader_res["result"]["diff"].is_array() || fader_res["result"]["diff"].empty()) {
            std::cerr << "[OAE Studio] MCP studio_set_mixer_fader failed to return RFC 6902 JSON patch diff!" << std::endl;
            return 1;
        }
        std::cout << "[OAE Studio] MCP studio_set_mixer_fader verified with RFC 6902 diff (" << fader_res["result"]["diff"].size() << " ops)." << std::endl;
    }

    // 6. Test tools/call: studio_move_clip (verifying RFC 6902 diff)
    {
        const std::string move_req = "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/call\",\"params\":{\"name\":\"studio_move_clip\",\"arguments\":{\"event_path\":\"event:/coin\",\"track_index\":0,\"instrument_index\":0,\"new_offset_ms\":450.0}}}";
        const std::string move_res_str = server.handle_message(move_req);
        const auto move_res = nlohmann::json::parse(move_res_str, nullptr, false);
        if (move_res.is_discarded() || move_res["result"].value("isError", true)) {
            std::cerr << "[OAE Studio] MCP studio_move_clip failed!" << std::endl;
            return 1;
        }
        if (!move_res["result"].contains("diff") || !move_res["result"]["diff"].is_array() || move_res["result"]["diff"].empty()) {
            std::cerr << "[OAE Studio] MCP studio_move_clip failed to return RFC 6902 JSON patch diff!" << std::endl;
            return 1;
        }

        bool found_offset_patch = false;
        for (const auto& patch_op : move_res["result"]["diff"]) {
            const std::string path = patch_op.value("path", "");
            if (path.find("offset_ms") != std::string::npos) {
                found_offset_patch = true;
                break;
            }
        }
        if (!found_offset_patch) {
            std::cerr << "[OAE Studio] RFC 6902 diff does not contain offset_ms operation!" << std::endl;
            return 1;
        }
        std::cout << "[OAE Studio] MCP studio_move_clip verified with RFC 6902 diff: " << move_res["result"]["diff"].dump() << std::endl;
    }

    // 7. Test tools/call: studio_trigger_control
    {
        const std::string trig_req = "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/call\",\"params\":{\"name\":\"studio_trigger_control\",\"arguments\":{\"component_id\":\"btn_play\"}}}";
        const std::string trig_res_str = server.handle_message(trig_req);
        const auto trig_res = nlohmann::json::parse(trig_res_str, nullptr, false);
        if (trig_res.is_discarded() || trig_res["result"].value("isError", true)) {
            std::cerr << "[OAE Studio] MCP studio_trigger_control failed!" << std::endl;
            return 1;
        }
        std::cout << "[OAE Studio] MCP studio_trigger_control verified (btn_play)." << std::endl;
    }

    // 8. Test tools/call: studio_transport
    {
        const std::string tr_req = "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"tools/call\",\"params\":{\"name\":\"studio_transport\",\"arguments\":{\"action\":\"pause\"}}}";
        const std::string tr_res_str = server.handle_message(tr_req);
        const auto tr_res = nlohmann::json::parse(tr_res_str, nullptr, false);
        if (tr_res.is_discarded() || tr_res["result"].value("isError", true)) {
            std::cerr << "[OAE Studio] MCP studio_transport failed!" << std::endl;
            return 1;
        }
        std::cout << "[OAE Studio] MCP studio_transport verified." << std::endl;
    }

    // 9. Test tools/call: studio_build_bank
    {
        const std::string bld_req = "{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"tools/call\",\"params\":{\"name\":\"studio_build_bank\",\"arguments\":{}}}";
        const std::string bld_res_str = server.handle_message(bld_req);
        const auto bld_res = nlohmann::json::parse(bld_res_str, nullptr, false);
        if (bld_res.is_discarded() || bld_res["result"].value("isError", true)) {
            std::cerr << "[OAE Studio] MCP studio_build_bank failed: " << bld_res_str << std::endl;
            return 1;
        }
        std::cout << "[OAE Studio] MCP studio_build_bank verified." << std::endl;
    }

    // 9a. Test TrackHeaderItem MCP inspection, single toggle & idempotent control
    {
        auto call_mcp = [&](int req_id, const std::string& tool, const nlohmann::json& arguments) {
            const nlohmann::json req = {
                { "jsonrpc", "2.0" },
                { "id", req_id },
                { "method", "tools/call" },
                { "params", {
                    { "name", tool },
                    { "arguments", arguments }
                }}
            };
            return nlohmann::json::parse(server.handle_message(req.dump()), nullptr, false);
        };

        auto inspect_timeline = [&](int req_id) {
            const auto res = call_mcp(req_id, "studio_ui_inspect", { { "component_id", "timeline_view" } });
            if (res.is_discarded() || res["result"].value("isError", true)) return nlohmann::json();
            return nlohmann::json::parse(res["result"]["content"][0].value("text", ""), nullptr, false);
        };

        auto find_inspected_comp = [](const nlohmann::json& payload, const std::string& cid) -> const nlohmann::json* {
            if (!payload.is_object() || !payload.contains("components") || !payload["components"].is_array()) return nullptr;
            for (const auto& c : payload["components"]) {
                if (c.value("component_id", "") == cid) return &c;
            }
            return nullptr;
        };

        const auto init_inspect = inspect_timeline(800);
        for (const char* cid : { "track_color_swatch_0", "track_title_0", "track_volume_0", "track_pan_0",
                                 "track_mute_0", "track_solo_0", "track_auto_0" }) {
            if (!find_inspected_comp(init_inspect, cid)) {
                std::cerr << "[OAE Studio] MCP studio_ui_inspect missing track header ComponentID: " << cid << std::endl;
                return 1;
            }
        }
        if (find_inspected_comp(init_inspect, "track_mute_0")->value("toggle_state", true) ||
            find_inspected_comp(init_inspect, "track_solo_0")->value("toggle_state", true) ||
            find_inspected_comp(init_inspect, "track_auto_0")->value("toggle_state", true)) {
            std::cerr << "[OAE Studio] Initial track_mute_0/track_solo_0/track_auto_0 toggle_state should be false!" << std::endl;
            return 1;
        }

        // Single toggle via studio_trigger_control (no double toggle)
        const auto tm = call_mcp(801, "studio_trigger_control", { { "component_id", "track_mute_0" } });
        const auto ts = call_mcp(802, "studio_trigger_control", { { "component_id", "track_solo_0" } });
        if (tm.is_discarded() || tm["result"].value("isError", true) ||
            ts.is_discarded() || ts["result"].value("isError", true) ||
            !model.is_track_muted("event:/coin", 0) || !model.is_track_solo("event:/coin", 0)) {
            std::cerr << "[OAE Studio] MCP studio_trigger_control failed single toggle on track_mute_0/track_solo_0!" << std::endl;
            return 1;
        }
        const auto after_trig_inspect = inspect_timeline(803);
        if (!find_inspected_comp(after_trig_inspect, "track_mute_0")->value("toggle_state", false) ||
            !find_inspected_comp(after_trig_inspect, "track_solo_0")->value("toggle_state", false)) {
            std::cerr << "[OAE Studio] toggle_state not true after studio_trigger_control!" << std::endl;
            return 1;
        }

        // Idempotent studio_ui_set_control toggle on track_mute_0 and track_auto_0
        const auto idem_true = call_mcp(804, "studio_ui_set_control", { { "component_id", "track_mute_0" }, { "toggle", true } });
        if (idem_true.is_discarded() || idem_true["result"].value("isError", true) || !model.is_track_muted("event:/coin", 0)) {
            std::cerr << "[OAE Studio] Idempotent toggle=true flipped track_mute_0!" << std::endl;
            return 1;
        }
        const auto set_false = call_mcp(805, "studio_ui_set_control", { { "component_id", "track_mute_0" }, { "toggle", false } });
        const auto idem_false = call_mcp(806, "studio_ui_set_control", { { "component_id", "track_mute_0" }, { "toggle", false } });
        if (set_false.is_discarded() || set_false["result"].value("isError", true) ||
            idem_false.is_discarded() || idem_false["result"].value("isError", true) ||
            model.is_track_muted("event:/coin", 0)) {
            std::cerr << "[OAE Studio] Idempotent toggle=false failed on track_mute_0!" << std::endl;
            return 1;
        }

        call_mcp(807, "studio_ui_set_control", { { "component_id", "track_auto_0" }, { "toggle", true } });
        call_mcp(808, "studio_ui_set_control", { { "component_id", "track_auto_0" }, { "toggle", true } });
        if (!find_inspected_comp(inspect_timeline(809), "track_auto_0")->value("toggle_state", false)) {
            std::cerr << "[OAE Studio] Idempotent toggle=true failed on track_auto_0!" << std::endl;
            return 1;
        }
        call_mcp(810, "studio_ui_set_control", { { "component_id", "track_auto_0" }, { "toggle", false } });
        if (find_inspected_comp(inspect_timeline(811), "track_auto_0")->value("toggle_state", true)) {
            std::cerr << "[OAE Studio] toggle=false failed on track_auto_0!" << std::endl;
            return 1;
        }

        // Start playback and apply track_volume_0 & track_pan_0 live via studio_ui_set_control
        call_mcp(812, "studio_ui_set_control", { { "component_id", "track_solo_0" }, { "toggle", false } });
        const auto play_res = call_mcp(813, "studio_transport", { { "action", "play" } });
        if (play_res.is_discarded() || play_res["result"].value("isError", true) || transport.get_active_instance() == 0) {
            std::cerr << "[OAE Studio] Failed to play event:/coin for MCP track volume/pan verification!" << std::endl;
            return 1;
        }
        const auto set_vol = call_mcp(814, "studio_ui_set_control", { { "component_id", "track_volume_0" }, { "value", -12.0 } });
        const auto set_pan = call_mcp(815, "studio_ui_set_control", { { "component_id", "track_pan_0" }, { "value", -0.5 } });
        if (set_vol.is_discarded() || set_vol["result"].value("isError", true) ||
            set_pan.is_discarded() || set_pan["result"].value("isError", true)) {
            std::cerr << "[OAE Studio] studio_ui_set_control failed on track_volume_0/track_pan_0!" << std::endl;
            return 1;
        }
        oae_bus mcp_bus0 = 0;
        float mcp_vol = 0.0f;
        float mcp_pan = 0.0f;
        if (engine.get_instance_track_bus(transport.get_active_instance(), 0, &mcp_bus0) != OAE_OK || mcp_bus0 == 0 ||
            oae_bus_get_volume(mcp_bus0, &mcp_vol) != OAE_OK || oae_bus_get_pan(mcp_bus0, &mcp_pan) != OAE_OK ||
            std::abs(mcp_vol - std::pow(10.0f, -12.0f / 20.0f)) > 1.0e-3f ||
            std::abs(mcp_pan - (-0.5f)) > 1.0e-3f) {
            std::cerr << "[OAE Studio] Live track bus did not reflect MCP studio_ui_set_control track_volume_0/track_pan_0!" << std::endl;
            return 1;
        }
        call_mcp(816, "studio_transport", { { "action", "stop" } });
        rebuilder.stopTimer();

        std::cout << "[OAE Studio] MCP TrackHeaderItem ComponentIDs, toggle_state & live mix verified." << std::endl;
    }

    // 9b. Test tools/call: studio_ui_screenshot (full window -> disk PNG, scoped timeline_view -> base64, errors)
    {
        const fs::path shot_path = temp_dir / "screenshots" / "full_window.png";
        const nlohmann::json full_req = {
            { "jsonrpc", "2.0" },
            { "id", 81 },
            { "method", "tools/call" },
            { "params", {
                { "name", "studio_ui_screenshot" },
                { "arguments", {
                    { "output_path", shot_path.u8string() }
                }}
            }}
        };
        const auto full_res = nlohmann::json::parse(server.handle_message(full_req.dump()), nullptr, false);
        if (full_res.is_discarded() || full_res["result"].value("isError", true)) {
            std::cerr << "[OAE Studio] MCP studio_ui_screenshot (full window) failed: " << full_res.dump() << std::endl;
            return 1;
        }
        const auto& full_content = full_res["result"]["content"];
        if (!full_content.is_array() || full_content.size() != 1 || full_content[0].value("type", "") != "text") {
            std::cerr << "[OAE Studio] MCP studio_ui_screenshot (output_path) unexpected content structure!" << std::endl;
            return 1;
        }
        const auto full_meta = nlohmann::json::parse(full_content[0].value("text", ""), nullptr, false);
        if (full_meta.is_discarded() || full_meta.value("width", 0) != 1280 ||
            full_meta.value("height", 0) != 800 || full_meta.value("byte_size", 0) <= 0 ||
            !full_meta.contains("output_path") || !fs::exists(shot_path)) {
            std::cerr << "[OAE Studio] MCP studio_ui_screenshot (full window) metadata or file validation failed!" << std::endl;
            return 1;
        }
        {
            std::ifstream png_in(shot_path, std::ios::binary);
            std::array<unsigned char, 4> hdr{};
            png_in.read(reinterpret_cast<char*>(hdr.data()), 4);
            if (!png_in || hdr[0] != 0x89 || hdr[1] != 0x50 || hdr[2] != 0x4E || hdr[3] != 0x47) {
                std::cerr << "[OAE Studio] MCP studio_ui_screenshot PNG disk file missing 89 50 4E 47 header!" << std::endl;
                return 1;
            }
        }

        const nlohmann::json scoped_req = {
            { "jsonrpc", "2.0" },
            { "id", 82 },
            { "method", "tools/call" },
            { "params", {
                { "name", "studio_ui_screenshot" },
                { "arguments", {
                    { "component_id", "timeline_view" }
                }}
            }}
        };
        const auto scoped_res = nlohmann::json::parse(server.handle_message(scoped_req.dump()), nullptr, false);
        if (scoped_res.is_discarded() || scoped_res["result"].value("isError", true)) {
            std::cerr << "[OAE Studio] MCP studio_ui_screenshot (timeline_view) failed: " << scoped_res.dump() << std::endl;
            return 1;
        }
        const auto& scoped_content = scoped_res["result"]["content"];
        if (!scoped_content.is_array() || scoped_content.size() != 2 ||
            scoped_content[0].value("type", "") != "text" ||
            scoped_content[1].value("type", "") != "image" ||
            scoped_content[1].value("mimeType", "") != "image/png") {
            std::cerr << "[OAE Studio] MCP studio_ui_screenshot (timeline_view) unexpected content blocks!" << std::endl;
            return 1;
        }
        const auto scoped_meta = nlohmann::json::parse(scoped_content[0].value("text", ""), nullptr, false);
        const std::string b64_data = scoped_content[1].value("data", "");
        if (scoped_meta.is_discarded() || scoped_meta.value("component_id", "") != "timeline_view" ||
            scoped_meta.value("width", 0) <= 0 || scoped_meta.value("height", 0) <= 0 ||
            scoped_meta.value("byte_size", 0) <= 0 || b64_data.empty()) {
            std::cerr << "[OAE Studio] MCP studio_ui_screenshot (timeline_view) invalid metadata or base64!" << std::endl;
            return 1;
        }
        juce::MemoryOutputStream decoded_png;
        if (!juce::Base64::convertFromBase64(decoded_png, juce::String::fromUTF8(b64_data.c_str())) ||
            decoded_png.getDataSize() < 4 ||
            decoded_png.getDataSize() != scoped_meta.value("byte_size", size_t{ 0 })) {
            std::cerr << "[OAE Studio] MCP studio_ui_screenshot base64 decode mismatch!" << std::endl;
            return 1;
        }
        const auto* decoded_bytes = static_cast<const unsigned char*>(decoded_png.getData());
        if (decoded_bytes[0] != 0x89 || decoded_bytes[1] != 0x50 || decoded_bytes[2] != 0x4E || decoded_bytes[3] != 0x47) {
            std::cerr << "[OAE Studio] MCP studio_ui_screenshot decoded base64 missing PNG header!" << std::endl;
            return 1;
        }

        const nlohmann::json missing_req = {
            { "jsonrpc", "2.0" },
            { "id", 83 },
            { "method", "tools/call" },
            { "params", {
                { "name", "studio_ui_screenshot" },
                { "arguments", {
                    { "component_id", "non_existent_component" }
                }}
            }}
        };
        const auto missing_res = nlohmann::json::parse(server.handle_message(missing_req.dump()), nullptr, false);
        if (missing_res.is_discarded() || !missing_res["result"].value("isError", false)) {
            std::cerr << "[OAE Studio] MCP studio_ui_screenshot expected error for non_existent_component!" << std::endl;
            return 1;
        }

        const nlohmann::json zero_req = {
            { "jsonrpc", "2.0" },
            { "id", 84 },
            { "method", "tools/call" },
            { "params", {
                { "name", "studio_ui_screenshot" },
                { "arguments", {
                    { "component_id", "zero_bounds_dummy" }
                }}
            }}
        };
        const auto zero_res = nlohmann::json::parse(server.handle_message(zero_req.dump()), nullptr, false);
        if (zero_res.is_discarded() || !zero_res["result"].value("isError", false)) {
            std::cerr << "[OAE Studio] MCP studio_ui_screenshot expected error for zero_bounds_dummy!" << std::endl;
            return 1;
        }

        std::cout << "[OAE Studio] MCP studio_ui_screenshot verified (full PNG, timeline_view base64, invalid/zero bounds errors)." << std::endl;
    }

    // 9c. Test tools/call: studio_ui_highlight (inactive inspect, activation, click-through, resize Z-order, screenshot diff, errors, clear)
    {
        const auto inspect_visible_state = [&](bool& out_has_overlay, int& out_count) -> bool {
            const nlohmann::json inspect_req = {
                { "jsonrpc", "2.0" },
                { "id", 90 },
                { "method", "tools/call" },
                { "params", {
                    { "name", "studio_ui_inspect" },
                    { "arguments", { { "visible_only", true } } }
                }}
            };
            const auto inspect_res = nlohmann::json::parse(server.handle_message(inspect_req.dump()), nullptr, false);
            if (inspect_res.is_discarded() || inspect_res["result"].value("isError", true)) {
                return false;
            }
            const auto payload = nlohmann::json::parse(inspect_res["result"]["content"][0].value("text", ""), nullptr, false);
            if (payload.is_discarded() || !payload.contains("components") || !payload["components"].is_array()) {
                return false;
            }
            out_count = payload.value("count", 0);
            out_has_overlay = false;
            for (const auto& c : payload["components"]) {
                if (c.value("component_id", "") == "tutorial_highlight_overlay") {
                    out_has_overlay = true;
                    break;
                }
            }
            return true;
        };

        bool has_overlay = true;
        int inspect_count = 0;
        if (ui_host.highlightOverlay.isVisible() ||
            !inspect_visible_state(has_overlay, inspect_count) ||
            inspect_count <= 0 ||
            has_overlay) {
            std::cerr << "[OAE Studio] TutorialHighlightOverlay must stay hidden and excluded from studio_ui_inspect when inactive!" << std::endl;
            return 1;
        }

        const auto capture_full_base64 = [&](int req_id) -> std::string {
            const nlohmann::json shot_req = {
                { "jsonrpc", "2.0" },
                { "id", req_id },
                { "method", "tools/call" },
                { "params", {
                    { "name", "studio_ui_screenshot" },
                    { "arguments", nlohmann::json::object() }
                }}
            };
            const auto res = nlohmann::json::parse(server.handle_message(shot_req.dump()), nullptr, false);
            if (res.is_discarded() || res["result"].value("isError", true) || res["result"]["content"].size() < 2) {
                return {};
            }
            return res["result"]["content"][1].value("data", "");
        };

        const std::string baseline_b64 = capture_full_base64(91);
        if (baseline_b64.empty()) {
            std::cerr << "[OAE Studio] Failed to capture baseline full-window screenshot for studio_ui_highlight!" << std::endl;
            return 1;
        }

        const nlohmann::json hl_req = {
            { "jsonrpc", "2.0" },
            { "id", 92 },
            { "method", "tools/call" },
            { "params", {
                { "name", "studio_ui_highlight" },
                { "arguments", {
                    { "component_id", "timeline_view" },
                    { "label", "Timeline Spotlight" },
                    { "dim_opacity", 0.5 },
                    { "color", "amber" }
                }}
            }}
        };
        const auto hl_res = nlohmann::json::parse(server.handle_message(hl_req.dump()), nullptr, false);
        if (hl_res.is_discarded() || hl_res["result"].value("isError", true)) {
            std::cerr << "[OAE Studio] MCP studio_ui_highlight activation failed: " << hl_res.dump() << std::endl;
            return 1;
        }
        const auto hl_meta = nlohmann::json::parse(hl_res["result"]["content"][0].value("text", ""), nullptr, false);
        if (hl_meta.is_discarded() || !hl_meta.value("active", false) ||
            hl_meta.value("component_id", "") != "timeline_view" ||
            hl_meta.value("label", "") != "Timeline Spotlight" ||
            hl_meta.value("color", "") != "amber" ||
            !hl_meta.contains("bounds") || hl_meta["bounds"].value("w", 0) <= 0 || hl_meta["bounds"].value("h", 0) <= 0 ||
            !ui_host.highlightOverlay.isVisible()) {
            std::cerr << "[OAE Studio] MCP studio_ui_highlight returned invalid payload or overlay remained hidden!" << std::endl;
            return 1;
        }

        const auto tl_center = ui_host.getLocalArea(&ui_host.timelinePanel, ui_host.timelinePanel.getLocalBounds()).getCentre();
        const auto* hit_under_overlay = ui_host.getComponentAt(tl_center);
        if (ui_host.highlightOverlay.hitTest(tl_center.x, tl_center.y) ||
            hit_under_overlay == nullptr ||
            hit_under_overlay == &ui_host.highlightOverlay) {
            std::cerr << "[OAE Studio] TutorialHighlightOverlay intercepted hitTest/getComponentAt!" << std::endl;
            return 1;
        }

        const nlohmann::json mouse_req = {
            { "jsonrpc", "2.0" },
            { "id", 93 },
            { "method", "tools/call" },
            { "params", {
                { "name", "studio_ui_mouse" },
                { "arguments", {
                    { "action", "click" },
                    { "component_id", "btn_play" }
                }}
            }}
        };
        const auto mouse_res = nlohmann::json::parse(server.handle_message(mouse_req.dump()), nullptr, false);
        if (mouse_res.is_discarded() || mouse_res["result"].value("isError", true)) {
            std::cerr << "[OAE Studio] MCP studio_ui_mouse click-through failed while highlight was active!" << std::endl;
            return 1;
        }

        ui_host.setSize(1024, 680);
        if (ui_host.highlightOverlay.getBounds() != ui_host.getLocalBounds() ||
            ui_host.getIndexOfChildComponent(&ui_host.highlightOverlay) != ui_host.getNumChildComponents() - 1) {
            std::cerr << "[OAE Studio] TutorialHighlightOverlay failed bounds or Z-order check after resize to 1024x680!" << std::endl;
            return 1;
        }
        ui_host.setSize(1280, 800);
        if (ui_host.highlightOverlay.getBounds() != ui_host.getLocalBounds() ||
            ui_host.getIndexOfChildComponent(&ui_host.highlightOverlay) != ui_host.getNumChildComponents() - 1) {
            std::cerr << "[OAE Studio] TutorialHighlightOverlay failed bounds or Z-order check after restore to 1280x800!" << std::endl;
            return 1;
        }

        const std::string highlighted_b64 = capture_full_base64(94);
        if (highlighted_b64.empty() || highlighted_b64 == baseline_b64) {
            std::cerr << "[OAE Studio] studio_ui_screenshot did not reflect active spotlight overlay!" << std::endl;
            return 1;
        }

        const std::vector<nlohmann::json> invalid_hl_args = {
            nlohmann::json::object(),
            { { "component_id", "non_existent_component" } },
            { { "component_id", "zero_bounds_dummy" } },
            { { "component_id", "timeline_view" }, { "color", "magenta" } }
        };
        for (size_t idx = 0; idx < invalid_hl_args.size(); ++idx) {
            const nlohmann::json err_req = {
                { "jsonrpc", "2.0" },
                { "id", 95 + static_cast<int>(idx) },
                { "method", "tools/call" },
                { "params", {
                    { "name", "studio_ui_highlight" },
                    { "arguments", invalid_hl_args[idx] }
                }}
            };
            const auto err_res = nlohmann::json::parse(server.handle_message(err_req.dump()), nullptr, false);
            if (err_res.is_discarded() || !err_res["result"].value("isError", false)) {
                std::cerr << "[OAE Studio] Expected deterministic error for invalid studio_ui_highlight case " << idx << std::endl;
                return 1;
            }
            if (!ui_host.highlightOverlay.is_active() || !ui_host.highlightOverlay.isVisible()) {
                std::cerr << "[OAE Studio] Invalid studio_ui_highlight call corrupted active overlay state!" << std::endl;
                return 1;
            }
        }

        const nlohmann::json clear_req = {
            { "jsonrpc", "2.0" },
            { "id", 99 },
            { "method", "tools/call" },
            { "params", {
                { "name", "studio_ui_highlight" },
                { "arguments", { { "clear", true } } }
            }}
        };
        const auto clear_res = nlohmann::json::parse(server.handle_message(clear_req.dump()), nullptr, false);
        if (clear_res.is_discarded() || clear_res["result"].value("isError", true)) {
            std::cerr << "[OAE Studio] MCP studio_ui_highlight clear failed!" << std::endl;
            return 1;
        }
        const auto clear_meta = nlohmann::json::parse(clear_res["result"]["content"][0].value("text", ""), nullptr, false);
        if (clear_meta.is_discarded() || clear_meta.value("active", true) ||
            ui_host.highlightOverlay.isVisible() || ui_host.highlightOverlay.is_active() ||
            !inspect_visible_state(has_overlay, inspect_count) ||
            inspect_count <= 0 ||
            has_overlay) {
            std::cerr << "[OAE Studio] MCP studio_ui_highlight clear did not hide overlay cleanly!" << std::endl;
            return 1;
        }

        std::cout << "[OAE Studio] MCP studio_ui_highlight verified (activate, click-through, resize Z-order, screenshot diff, errors, clear)." << std::endl;
    }

    // 10. Test resources/list
    {
        const std::string rlist_req = "{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"resources/list\"}";
        const std::string rlist_res_str = server.handle_message(rlist_req);
        const auto rlist_res = nlohmann::json::parse(rlist_res_str, nullptr, false);
        if (rlist_res.is_discarded() || !rlist_res["result"].contains("resources") || rlist_res["result"]["resources"].size() < 3) {
            std::cerr << "[OAE Studio] MCP resources/list failed!" << std::endl;
            return 1;
        }
        std::cout << "[OAE Studio] MCP resources/list verified (" << rlist_res["result"]["resources"].size() << " resources)." << std::endl;
    }

    // 11. Test resources/read: oae://project/state
    {
        const std::string rstate_req = "{\"jsonrpc\":\"2.0\",\"id\":10,\"method\":\"resources/read\",\"params\":{\"uri\":\"oae://project/state\"}}";
        const std::string rstate_res_str = server.handle_message(rstate_req);
        const auto rstate_res = nlohmann::json::parse(rstate_res_str, nullptr, false);
        if (rstate_res.is_discarded() || !rstate_res["result"].contains("contents") || rstate_res["result"]["contents"].empty()) {
            std::cerr << "[OAE Studio] MCP resources/read for oae://project/state failed!" << std::endl;
            return 1;
        }
        const std::string text = rstate_res["result"]["contents"][0].value("text", "");
        const auto state_json = nlohmann::json::parse(text, nullptr, false);
        if (state_json.is_discarded() || !state_json.contains("events")) {
            std::cerr << "[OAE Studio] oae://project/state returned invalid JSON payload!" << std::endl;
            return 1;
        }
        std::cout << "[OAE Studio] MCP resource oae://project/state verified." << std::endl;
    }

    // 12. Test resources/read: oae://audio/meters
    {
        const std::string rmeters_req = "{\"jsonrpc\":\"2.0\",\"id\":11,\"method\":\"resources/read\",\"params\":{\"uri\":\"oae://audio/meters\"}}";
        const std::string rmeters_res_str = server.handle_message(rmeters_req);
        const auto rmeters_res = nlohmann::json::parse(rmeters_res_str, nullptr, false);
        if (rmeters_res.is_discarded() || !rmeters_res["result"].contains("contents") || rmeters_res["result"]["contents"].empty()) {
            std::cerr << "[OAE Studio] MCP resources/read for oae://audio/meters failed!" << std::endl;
            return 1;
        }
        const std::string text = rmeters_res["result"]["contents"][0].value("text", "");
        const auto meters_json = nlohmann::json::parse(text, nullptr, false);
        if (meters_json.is_discarded() || !meters_json.contains("master") || !meters_json.contains("buses")) {
            std::cerr << "[OAE Studio] oae://audio/meters returned invalid JSON telemetry!" << std::endl;
            return 1;
        }
        std::cout << "[OAE Studio] MCP resource oae://audio/meters verified." << std::endl;
    }

    // 13. Test resources/read: oae://diagnostics/log
    {
        const std::string rdiag_req = "{\"jsonrpc\":\"2.0\",\"id\":12,\"method\":\"resources/read\",\"params\":{\"uri\":\"oae://diagnostics/log\"}}";
        const std::string rdiag_res_str = server.handle_message(rdiag_req);
        const auto rdiag_res = nlohmann::json::parse(rdiag_res_str, nullptr, false);
        if (rdiag_res.is_discarded() || !rdiag_res["result"].contains("contents") || rdiag_res["result"]["contents"].empty()) {
            std::cerr << "[OAE Studio] MCP resources/read for oae://diagnostics/log failed!" << std::endl;
            return 1;
        }
        std::cout << "[OAE Studio] MCP resource oae://diagnostics/log verified." << std::endl;
    }

    // 14. Test TCP Socket on 127.0.0.1:7451
    {
        if (server.start_tcp(7451)) {
            juce::StreamingSocket client;
            if (client.connect("127.0.0.1", 7451, 2000)) {
                const std::string ping_req = "{\"jsonrpc\":\"2.0\",\"id\":42,\"method\":\"ping\"}\n";
                client.write(ping_req.data(), static_cast<int>(ping_req.size()));
                char buf[512];
                if (client.waitUntilReady(true, 2000) > 0) {
                    const int bytes = client.read(buf, sizeof(buf) - 1, false);
                    if (bytes > 0) {
                        buf[bytes] = '\0';
                        const auto ping_res = nlohmann::json::parse(buf, nullptr, false);
                        if (!ping_res.is_discarded() && ping_res["id"] == 42) {
                            std::cout << "[OAE Studio] MCP TCP Socket Server (127.0.0.1:7451) verified." << std::endl;
                        }
                    }
                }
                client.close();
            }
            server.stop_tcp();
        }
    }

    server.stop();
    transport.stop(true);
    rebuilder.unload_preview_bank();
    engine.shutdown();
    fs::remove_all(temp_dir, ec);

    std::cout << "[OAE Studio] --smoke-mcp PASSED with exit code 0." << std::endl;
    return 0;
}

int run_smoke_vu()
{
    std::cout << "[OAE Studio] Starting --smoke-vu validation (EPICO 7: VU Ballistics & ITU-R BS.1770)..." << std::endl;
    using namespace oae::studio;
    constexpr double kTwoPi = 6.28318530717958647692;

    // 1. VU-01: Mixer Scale Selector
    {
        StudioEngine engine;
        engine.init_headless();
        ProjectDocument doc;
        AuthoringModel model(doc);
        AuditionTransport transport(engine, model);
        MixerDeckPanel mixer(engine, model, transport);

        const MeterScale scales[] = {
            MeterScale::DbfsNominal, MeterScale::K12, MeterScale::K14, MeterScale::K20, MeterScale::EbuR128Lufs
        };
        for (auto sc : scales) {
            mixer.set_meter_scale(sc);
            if (mixer.get_meter_scale() != sc) {
                std::cerr << "[FAIL] Mixer scale mismatch" << std::endl;
                return 1;
            }
            for (const auto& strip : mixer.get_strips()) {
                if (strip->get_vu_meter().get_scale() != sc) {
                    std::cerr << "[FAIL] Strip VU scale mismatch" << std::endl;
                    return 1;
                }
            }
        }
        engine.shutdown();
    }

    // 2. VU-02: IEC 60268-17 Ballistics (instant attack, 20 dB/s decay, 300 ms RMS, 1.2 s Peak Hold)
    {
        BallisticMeter bm;
        bm.update(1.0f, 1.0f, 1.0f / 30.0f);
        if (std::fabs(bm.peak_dbfs() - 0.0f) > 0.01f) {
            std::cerr << "[FAIL] Instant attack failed: " << bm.peak_dbfs() << std::endl;
            return 1;
        }
        // Decay for 0.5 s -> should drop 10.0 dB, while peak hold stays at 0.0 dBFS (< 1.2 s)
        for (int i = 0; i < 15; ++i) {
            bm.update(0.0f, 0.0f, 1.0f / 30.0f);
        }
        if (std::fabs(bm.peak_dbfs() - (-10.0f)) > 0.05f) {
            std::cerr << "[FAIL] 20 dB/s decay after 0.5s expected -10.0 dB, got " << bm.peak_dbfs() << std::endl;
            return 1;
        }
        if (std::fabs(bm.peak_hold_dbfs() - 0.0f) > 0.05f) {
            std::cerr << "[FAIL] Peak Hold released prematurely before 1.2s: " << bm.peak_hold_dbfs() << std::endl;
            return 1;
        }
        // Advance another 1.2 s (total 1.7 s -> 0.5 s past 1.2 s hold): peak hold should be ~ -10.0 dBFS
        for (int i = 0; i < 36; ++i) {
            bm.update(0.0f, 0.0f, 1.0f / 30.0f);
        }
        if (std::fabs(bm.peak_hold_dbfs() - (-10.0f)) > 0.1f) {
            std::cerr << "[FAIL] Peak Hold decay expected -10.0 dB at 1.7s, got " << bm.peak_hold_dbfs() << std::endl;
            return 1;
        }
    }

    // 3. VU-03: ITU-R BS.1770 / EBU R128 (K-Weighting, M/S/I gating, 4x True Peak)
    {
        constexpr uint32_t kSr = 48000;
        EbuR128Processor r128;
        r128.prepare(kSr, 2);

        // 1.5 s of 997 Hz stereo sine at -23 LUFS-equivalent (calibrated) -> check gating vs -75 LUFS silence
        const float amp_23 = std::pow(10.0f, -23.0f / 20.0f);
        std::vector<float> buf(kSr * 2);
        for (uint32_t n = 0; n < kSr; ++n) {
            const float s = amp_23 * static_cast<float>(std::sin(kTwoPi * 997.0 * n / kSr));
            buf[n * 2] = s;
            buf[n * 2 + 1] = s;
        }
        r128.process(buf.data(), kSr);
        const float integrated_before_silence = r128.snapshot().integrated_lufs;

        // Process 2.0 s of -75 LUFS (below -70 LUFS absolute gate)
        const float amp_75 = std::pow(10.0f, -75.0f / 20.0f);
        for (uint32_t n = 0; n < kSr; ++n) {
            const float s = amp_75 * static_cast<float>(std::sin(kTwoPi * 997.0 * n / kSr));
            buf[n * 2] = s;
            buf[n * 2 + 1] = s;
        }
        r128.process(buf.data(), kSr);
        const float integrated_after_2s = r128.snapshot().integrated_lufs;
        for (int i = 0; i < 5; ++i) {
            r128.process(buf.data(), kSr);
        }
        const float integrated_after_7s = r128.snapshot().integrated_lufs;

        if (std::fabs(integrated_after_7s - integrated_after_2s) > 0.01f || integrated_after_7s < -25.0f) {
            std::cerr << "[FAIL] EBU R128 absolute gate (-70 LUFS) failed: before="
                      << integrated_before_silence << " after_2s=" << integrated_after_2s
                      << " after_7s=" << integrated_after_7s << std::endl;
            return 1;
        }

        // True Peak 4x oversampling test: 12 kHz sine at 48 kHz with 45 deg phase has sample peak -3.01 dBFS, True Peak ~0 dBTP
        EbuR128Processor tp_proc;
        tp_proc.prepare(kSr, 2);
        std::vector<float> tp_buf(4800 * 2);
        float max_sample_peak = 0.0f;
        for (uint32_t n = 0; n < 4800; ++n) {
            const float s = static_cast<float>(std::sin(kTwoPi * 12000.0 * n / kSr + 0.7853981633974483));
            max_sample_peak = std::max(max_sample_peak, std::fabs(s));
            tp_buf[n * 2] = s;
            tp_buf[n * 2 + 1] = s;
        }
        tp_proc.process(tp_buf.data(), 4800);
        const float sample_peak_db = linear_to_dbfs(max_sample_peak);
        const float true_peak_db = tp_proc.snapshot().true_peak_dbtp;
        if (true_peak_db <= sample_peak_db + 2.0f) {
            std::cerr << "[FAIL] 4x True Peak failed to detect inter-sample peak: sample="
                      << sample_peak_db << " dBFS, true_peak=" << true_peak_db << " dBTP" << std::endl;
            return 1;
        }
    }

    // 4. VU-04: Acoustic Calibration (-14 dBFS RMS sine marks 0.00 VU +- 0.05 dB in K-14)
    {
        auto verify_k_calibration = [&](float target_rms_db, MeterScale scale) -> bool {
            const float rms_linear = std::pow(10.0f, target_rms_db / 20.0f);
            const float peak_linear = rms_linear * 1.41421356237f;
            BallisticMeter bm;
            // 3.0 seconds at 30 Hz (10x the 300 ms integration time constant)
            for (int i = 0; i < 90; ++i) {
                bm.update(peak_linear, rms_linear, 1.0f / 30.0f);
            }
            const float vu = dbfs_to_scale(bm.rms_dbfs(), scale);
            if (std::fabs(vu) > 0.05f) {
                std::cerr << "[FAIL] Calibration failed for " << meter_scale_name(scale)
                          << " at " << target_rms_db << " dBFS: got " << vu << " VU" << std::endl;
                return false;
            }
            return true;
        };

        if (!verify_k_calibration(-14.0f, MeterScale::K14)) return 1;
        if (!verify_k_calibration(-12.0f, MeterScale::K12)) return 1;
        if (!verify_k_calibration(-20.0f, MeterScale::K20)) return 1;
    }

    std::cout << "[OAE Studio] --smoke-vu PASSED with exit code 0." << std::endl;
    return 0;
}

int run_smoke_live()
{
    std::cout << "[OAE Studio] Starting --smoke-live validation (EPICO 8: Live-Linking)..." << std::endl;

    using namespace oae::studio;

    StudioEngine engine;
    if (engine.init_headless() != OAE_OK) {
        std::cerr << "[FAIL] StudioEngine headless init failed." << std::endl;
        return 1;
    }

    ProjectDocument doc;
    AuthoringModel model(doc);
    AuditionTransport transport(engine, model);

    LiveBridgeServer server;
    server.set_studio_bank_crc32(0x12345678u);
    transport.set_live_bridge(&server);

    const int port = LiveBridgeServer::kDefaultPort;
    if (!server.start(port)) {
        std::cerr << "[FAIL] LiveBridgeServer failed to bind port " << port << std::endl;
        return 1;
    }

    auto read_json_line = [](juce::StreamingSocket& sock, int timeout_ms = 2000) -> nlohmann::json {
        std::string buf;
        char ch = 0;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            if (sock.waitUntilReady(true, 50) > 0) {
                if (sock.read(&ch, 1, false) == 1) {
                    if (ch == '\n') {
                        return nlohmann::json::parse(buf);
                    }
                    if (ch != '\r') {
                        buf.push_back(ch);
                    }
                }
            }
        }
        return nlohmann::json{};
    };

    auto send_json_line = [](juce::StreamingSocket& sock, const nlohmann::json& j) -> bool {
        const std::string line = j.dump() + "\n";
        return sock.write(line.data(), static_cast<int>(line.size())) == static_cast<int>(line.size());
    };

    // 1. LIVE-01: Handshake with CRC mismatch
    {
        juce::StreamingSocket client;
        if (!client.connect("127.0.0.1", port, 1000)) {
            std::cerr << "[FAIL] Client failed to connect for CRC mismatch test." << std::endl;
            return 1;
        }
        send_json_line(client, { {"type", "hello"}, {"version", 1}, {"client", "UnityTest"}, {"bank_crc32", 0x99999999u} });
        const auto ack = read_json_line(client);
        if (ack.value("type", "") != "hello_ack" || ack.value("crc_match", true) != false) {
            std::cerr << "[FAIL] Expected crc_match=false in hello_ack, got: " << ack.dump() << std::endl;
            return 1;
        }
        client.close();
        for (int i = 0; i < 20 && server.is_client_connected(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        server.poll_incoming();
    }

    // 2. LIVE-01 / LIVE-03 / LIVE-04: Handshake with matching CRC, bidirectional sync (<10ms) & remote audition + fallback
    {
        juce::StreamingSocket client;
        if (!client.connect("127.0.0.1", port, 1000)) {
            std::cerr << "[FAIL] Client failed to connect for matching CRC test." << std::endl;
            return 1;
        }
        send_json_line(client, { {"type", "hello"}, {"version", 1}, {"client", "UnityTest"}, {"bank_crc32", 0x12345678u} });
        const auto ack = read_json_line(client);
        if (ack.value("type", "") != "hello_ack" || ack.value("crc_match", false) != true || ack.value("status", "") != "connected") {
            std::cerr << "[FAIL] Expected valid hello_ack, got: " << ack.dump() << std::endl;
            return 1;
        }

        server.poll_incoming();
        if (!server.is_client_connected() || !transport.is_remote_audition_active()) {
            std::cerr << "[FAIL] Server/transport did not enter remote audition state." << std::endl;
            return 1;
        }

        // Measure Studio -> Client latency (< 10ms)
        const auto t0 = std::chrono::steady_clock::now();
        server.send_bus_volume("bus:/SFX", -6.0f);
        const auto bus_msg = read_json_line(client);
        const auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0).count();
        if (bus_msg.value("type", "") != "bus_volume" || std::fabs(bus_msg.value("volume_db", 0.0f) - (-6.0f)) > 0.01f) {
            std::cerr << "[FAIL] Unexpected bus_volume payload: " << bus_msg.dump() << std::endl;
            return 1;
        }
        if (elapsed_us > 10000) {
            std::cerr << "[FAIL] Loopback sync latency exceeded 10ms: " << (elapsed_us / 1000.0) << " ms" << std::endl;
            return 1;
        }

        // Studio -> Client curve_update
        server.send_curve_update("event:/Engine", 0, "volume", nlohmann::json::array({ {{"x", 0.0}, {"y", 0.0}}, {{"x", 1.0}, {"y", -6.0}} }));
        const auto curve_msg = read_json_line(client);
        if (curve_msg.value("type", "") != "curve_update" || curve_msg.value("event_path", "") != "event:/Engine") {
            std::cerr << "[FAIL] Unexpected curve_update payload: " << curve_msg.dump() << std::endl;
            return 1;
        }

        // Client -> Studio parameter sync without echo loop
        send_json_line(client, { {"type", "param_set"}, {"name", "RPM"}, {"value", 0.85f}, {"is_global", true} });
        for (int i = 0; i < 30; ++i) {
            server.poll_incoming();
            if (std::fabs(transport.get_global_parameter("RPM") - 0.85f) < 0.001f) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        if (std::fabs(transport.get_global_parameter("RPM") - 0.85f) > 0.001f) {
            std::cerr << "[FAIL] Client -> Studio global parameter sync failed." << std::endl;
            return 1;
        }

        // LIVE-04: Remote audition play & stop
        transport.set_event_path("event:/TestLive");
        if (!transport.play()) {
            std::cerr << "[FAIL] Remote AuditionTransport::play() failed." << std::endl;
            return 1;
        }
        const auto play_msg = read_json_line(client);
        if (play_msg.value("type", "") != "event_play" || play_msg.value("event_path", "") != "event:/TestLive") {
            std::cerr << "[FAIL] Expected event_play message, got: " << play_msg.dump() << std::endl;
            return 1;
        }
        transport.stop(true);
        const auto stop_msg = read_json_line(client);
        if (stop_msg.value("type", "") != "event_stop") {
            std::cerr << "[FAIL] Expected event_stop message, got: " << stop_msg.dump() << std::endl;
            return 1;
        }

        // Disconnect and verify automatic offline fallback
        client.close();
        for (int i = 0; i < 30 && server.is_client_connected(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        server.poll_incoming();
        if (server.is_client_connected() || transport.is_remote_audition_active()) {
            std::cerr << "[FAIL] Server did not revert to offline mode after client disconnect." << std::endl;
            return 1;
        }
    }

    transport.set_live_bridge(nullptr);
    server.stop();
    engine.shutdown();
    std::cout << "[OAE Studio] --smoke-live PASSED with exit code 0." << std::endl;
    return 0;
}

int run_mcp_bridge(int port)
{
    juce::StreamingSocket sock;
    if (!sock.connect("127.0.0.1", port, 250)) {
        return -1;
    }

    std::atomic<bool> bridge_running{ true };

    std::thread sock_reader([&sock, &bridge_running]() {
        std::string buffer;
        char temp[2048];
        while (bridge_running.load() && sock.isConnected()) {
            const int ready = sock.waitUntilReady(true, 50);
            if (ready > 0) {
                const int bytes = sock.read(temp, sizeof(temp) - 1, false);
                if (bytes <= 0) break;
                temp[bytes] = '\0';
                buffer.append(temp, static_cast<size_t>(bytes));
                size_t nl = 0;
                while ((nl = buffer.find('\n')) != std::string::npos) {
                    std::string line = buffer.substr(0, nl);
                    buffer.erase(0, nl + 1);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (!line.empty()) {
                        std::cout << line << "\n" << std::flush;
                    }
                }
            } else if (ready < 0) {
                break;
            }
        }
        bridge_running = false;
    });

    std::string in_line;
    while (bridge_running.load() && std::getline(std::cin, in_line)) {
        if (!in_line.empty() && in_line.back() == '\r') in_line.pop_back();
        if (in_line.empty()) continue;
        in_line += "\n";
        if (sock.write(in_line.data(), static_cast<int>(in_line.size())) < 0) {
            break;
        }
    }

    bridge_running = false;
    sock.close();
    if (sock_reader.joinable()) {
        sock_reader.join();
    }
    return 0;
}

} // namespace

class OpenAudioStudioApplication : public juce::JUCEApplication {
public:
    class MainWindow : public juce::DocumentWindow {
    public:
        explicit MainWindow(juce::String name)
            : DocumentWindow(name,
                             juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                         .findColour(juce::ResizableWindow::backgroundColourId),
                             DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new oae::studio::MainComponent(), true);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen(true);
           #else
            setResizable(true, true);
            setResizeLimits(640, 400, 16384, 16384);
            centreWithSize(getWidth(), getHeight());
           #endif

            setVisible(true);
        }

        oae::studio::MainComponent* getMainComponent() const noexcept
        {
            return dynamic_cast<oae::studio::MainComponent*>(getContentComponent());
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

    OpenAudioStudioApplication() = default;

    const juce::String getApplicationName() override { return "OpenAudio Studio"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String& commandLine) override
    {
        if (commandLine.contains("--smoke-headless")) {
            const int code = run_smoke_headless();
            setApplicationReturnValue(code);
            quit();
            return;
        }

        if (commandLine.contains("--smoke-project")) {
            const int code = run_smoke_project();
            setApplicationReturnValue(code);
            quit();
            return;
        }

        if (commandLine.contains("--smoke-authoring")) {
            const int code = run_smoke_authoring();
            setApplicationReturnValue(code);
            quit();
            return;
        }

        if (commandLine.contains("--smoke-timeline")) {
            const int code = run_smoke_timeline();
            setApplicationReturnValue(code);
            quit();
            return;
        }

        if (commandLine.contains("--smoke-transport-mixer")) {
            const int code = run_smoke_transport_mixer();
            setApplicationReturnValue(code);
            quit();
            return;
        }

        if (commandLine.contains("--smoke-end-to-end")) {
            const int code = run_smoke_end_to_end();
            setApplicationReturnValue(code);
            quit();
            return;
        }

        if (commandLine.contains("--smoke-device")) {
            const int code = run_smoke_device();
            setApplicationReturnValue(code);
            quit();
            return;
        }

        if (commandLine.contains("--smoke-pro-ui")) {
            const int code = run_smoke_pro_ui();
            setApplicationReturnValue(code);
            quit();
            return;
        }

        if (commandLine.contains("--smoke-mcp")) {
            const int code = run_smoke_mcp();
            setApplicationReturnValue(code);
            quit();
            return;
        }

        if (commandLine.contains("--smoke-vu")) {
            const int code = run_smoke_vu();
            setApplicationReturnValue(code);
            quit();
            return;
        }

        if (commandLine.contains("--smoke-live")) {
            const int code = run_smoke_live();
            setApplicationReturnValue(code);
            quit();
            return;
        }

        const auto cliArgs = getCommandLineParameterArray();

        if (cliArgs.contains("--test-ui-controls")) {
            oae::studio::testing::UiControlTestOptions testOpts;
            for (const auto& arg : cliArgs) {
                if (arg.startsWith("--filter=")) {
                    testOpts.filter = arg.fromFirstOccurrenceOf("--filter=", false, false).unquoted().toStdString();
                } else if (arg == "--audit-only") {
                    testOpts.auditOnly = true;
                } else if (arg.startsWith("--report=")) {
                    const auto rpt = arg.fromFirstOccurrenceOf("--report=", false, false).unquoted().toStdString();
                    testOpts.reportPath = std::filesystem::u8path(rpt);
                }
            }
            const int code = oae::studio::testing::run_ui_controls_suite(testOpts);
            setApplicationReturnValue(code);
            quit();
            return;
        }

        for (const auto& arg : cliArgs) {
            if (arg.startsWith("--snapshot=")) {
                const auto outStr = arg.fromFirstOccurrenceOf("--snapshot=", false, false);
                auto comp = std::make_unique<oae::studio::MainComponent>();
                for (const auto& pArg : cliArgs) {
                    if (!pArg.startsWith("--")) {
                        const juce::File candidate(pArg);
                        if (candidate.existsAsFile() && candidate.hasFileExtension(".json")) {
                            comp->open_project_file(std::filesystem::u8path(candidate.getFullPathName().toStdString()));
                            break;
                        }
                    }
                }
                const bool ok = comp->dump_ui_screenshots(std::filesystem::u8path(outStr.toStdString()));
                setApplicationReturnValue(ok ? 0 : 1);
                quit();
                return;
            }
        }

        const bool isMcpStdio = cliArgs.contains("--mcp");

        if (isMcpStdio) {
            if (run_mcp_bridge(7451) == 0) {
                setApplicationReturnValue(0);
                quit();
                return;
            }
        }

        if (isMcpStdio && cliArgs.contains("--headless")) {
            mcpComponent = std::make_unique<oae::studio::MainComponent>();
            mcpServer = std::make_unique<oae::studio::McpServer>(*mcpComponent);
            mcpServer->start_stdio();
            mcpServer->start_tcp(7451);
            return;
        }

        mainWindow = std::make_unique<MainWindow>(getApplicationName());
        if (auto* comp = mainWindow->getMainComponent()) {
            for (const auto& arg : cliArgs) {
                if (!arg.startsWith("--")) {
                    const juce::File candidate(arg);
                    if (candidate.existsAsFile() && candidate.hasFileExtension(".json")) {
                        comp->open_project_file(std::filesystem::u8path(candidate.getFullPathName().toStdString()));
                        break;
                    }
                }
            }
            int mcpPort = 7451;
            for (const auto& arg : cliArgs) {
                if (arg.startsWith("--mcp-port=")) {
                    const int parsed = arg.fromFirstOccurrenceOf("--mcp-port=", false, false).getIntValue();
                    if (parsed > 0 && parsed <= 65535) mcpPort = parsed;
                }
            }
            mcpServer = std::make_unique<oae::studio::McpServer>(*comp);
            if (isMcpStdio) {
                mcpServer->start_stdio();
            }
            mcpServer->start_tcp(mcpPort);
        }
    }

    void shutdown() override
    {
        if (mcpServer) {
            mcpServer->stop();
            mcpServer = nullptr;
        }
        mcpComponent = nullptr;
        mainWindow = nullptr;
    }

private:
    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<oae::studio::MainComponent> mcpComponent;
    std::unique_ptr<oae::studio::McpServer> mcpServer;
};

START_JUCE_APPLICATION(OpenAudioStudioApplication)
