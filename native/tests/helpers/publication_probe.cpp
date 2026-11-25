#include "../../tools/bankc/publication.h"
#include "../../src/studio/bank_writer.h"
#include "../../src/studio/bank_format.h"
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>

#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char** argv)
#endif
{
    if (argc != 4) return 99;
#ifdef _WIN32
    const std::string mode = std::filesystem::path(argv[1]).u8string();
    const auto destination = std::filesystem::path(argv[2]);
    const std::string stop_phase = std::filesystem::path(argv[3]).u8string();
#else
    const std::string mode = argv[1];
    const auto destination = std::filesystem::u8path(argv[2]);
    const std::string stop_phase = argv[3];
#endif
    oae::bankc::PublicationSession session(destination, std::cerr,
        [&](const std::string& phase, const std::filesystem::path&) {
            if (phase == stop_phase) std::_Exit(71);
        });
    const auto opened = session.open();
    if (opened != 0) return opened;
    if (mode == "hold") {
        std::ofstream marker(destination.parent_path() / "ready.flag");
        marker << "ready";
        marker.close();
        std::this_thread::sleep_for(std::chrono::seconds(30));
        return 0;
    }
    oae::ModelSetBuilder builder;
    builder.add_event(oae::EventBuilder("event:/publication_new"));
    oae::BankWriter writer;
    writer.set_model_set(builder.build());
    const auto bank = writer.write();
    const auto catalog = nlohmann::json{{"version", 1}, {"bank_size", bank.size()},
        {"bank_crc32", oae::calc_crc32(bank.data(), bank.size())}, {"events", nlohmann::json::array()}}.dump(2);
    return session.publish(bank, catalog);
}
