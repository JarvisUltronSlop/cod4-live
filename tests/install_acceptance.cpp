// Developer-only acceptance probe for the real embedded CAB and owned retail data.
// Invoke explicitly; never registered with CTest and never deletes the installation.
#include "install_package.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <array>

namespace fs = std::filesystem;
static std::string read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}
static void write(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary); file << content;
    if (!file) throw std::runtime_error("Cannot write acceptance fixture");
}
static void require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}
int wmain(int argc, wchar_t** argv) {
    if (argc != 3) {
        std::cerr << "install_acceptance <owned Steam CoD4 folder> <new empty destination>\n";
        return 2;
    }
    try {
        fs::path source = fs::weakly_canonical(argv[1]), destination = fs::weakly_canonical(argv[2]);
        require(!fs::exists(destination) || fs::is_empty(destination), "Acceptance destination must be new/empty");
        const std::array sourceFiles{source / "iw3mp.exe", source / "binkw32.dll", source / "localization.txt"};
        std::array<std::string, 3> sourceHashes;
        for (size_t i = 0; i < sourceFiles.size(); ++i) {
            sourceHashes[i] = live::setup::HashFile(sourceFiles[i]);
            require(!sourceHashes[i].empty(), "Cannot hash retail source");
        }
        live::setup::Plan plan;
        std::string error;
        require(live::setup::Inspect(source, destination, plan, error), error);
        require(!plan.repair, "Acceptance must begin with a clean installation");
        std::atomic_bool cancelled{false};
        unsigned last = 101;
        const auto progress = [&](unsigned percent, const std::string& text) {
            if (percent != last) { std::cout << percent << "% " << text << std::endl; last = percent; }
        };
        require(live::setup::Install(plan, cancelled, progress, error), error);
        for (const auto& asset : plan.assets)
            require(fs::file_size(destination / asset.relative) == asset.bytes, "Installed asset size mismatch");
        for (auto name : {"cod4-live.exe", "cod4-live_server.exe", "steam_api.dll", "D3DX9_43.dll"}) {
            std::ifstream binary(destination / name, std::ios::binary);
            char mz[2]{}; binary.read(mz, 2);
            require(binary && mz[0] == 'M' && mz[1] == 'Z', "Acceptance requires real PE payload, not fixtures");
        }
        const auto originalClientHash = live::setup::HashFile(destination / "cod4-live.exe");
        const auto originalRuntimeHash = live::setup::HashFile(destination / "D3DX9_43.dll");
        const std::array contentFiles{
            destination / "main/maps/mp/bots/_bot.gsc",
            destination / "main/maps/mp/bots/_cod4live.gsc",
            destination / "main/images/live_controller/button_a.iwi",
            destination / "main/images/live_controller/button_ps3_circle.iwi"};
        std::array<std::string, std::tuple_size_v<decltype(contentFiles)>> contentHashes;
        for (size_t i = 0; i < contentFiles.size(); ++i) {
            contentHashes[i] = live::setup::HashFile(contentFiles[i]);
            require(!contentHashes[i].empty(), "Controller/Bot Warfare payload missing");
        }
        const auto profile = destination / "players/profiles/acceptance/config_mp.cfg";
        const auto server = destination / "main/server.cfg";
        const std::string profileText = "// acceptance profile preservation\nseta name acceptance\n";
        const std::string serverText = "// acceptance server configuration preservation\nset sv_hostname acceptance\n";
        write(profile, profileText); write(server, serverText);
        write(destination / "cod4-live.exe", "acceptance repair fixture");
        fs::remove(destination / "D3DX9_43.dll");
        write(contentFiles[0], "acceptance bot-script repair fixture");
        fs::remove(contentFiles[2]);
        require(live::setup::Inspect(source, destination, plan, error) && plan.repair, error);
        require(live::setup::Install(plan, cancelled, progress, error), error);
        require(live::setup::HashFile(destination / "cod4-live.exe") == originalClientHash, "Client repair mismatch");
        require(live::setup::HashFile(destination / "D3DX9_43.dll") == originalRuntimeHash, "Runtime repair mismatch");
        for (size_t i = 0; i < contentFiles.size(); ++i)
            require(live::setup::HashFile(contentFiles[i]) == contentHashes[i], "Controller/Bot Warfare content repair mismatch");
        require(read(profile) == profileText && read(server) == serverText, "Repair changed user configuration");
        for (size_t i = 0; i < sourceFiles.size(); ++i)
            require(live::setup::HashFile(sourceFiles[i]) == sourceHashes[i], "Retail source changed");
        std::cout << "PASS: real payload clean install, asset sizes, binary/content repair, profile/server preservation, retail source hashes\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
