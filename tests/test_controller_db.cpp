#undef NDEBUG
// User-supplied gamecontrollerdb.txt discovery and load-order contract.
// Compile from the repo root:
//   g++ -std=c++20 -o test_controller_db tests/test_controller_db.cpp -I src
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "aero_controller_db.h"

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    assert(input && "repo source must be readable via __FILE__");
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

} // namespace

int main() {
    using aero::controller_db::candidate_paths;
    using aero::controller_db::kDatabaseFilename;

    assert(std::string(kDatabaseFilename) == "gamecontrollerdb.txt");

    // Empty inputs yield no paths.
    assert(candidate_paths({}, {}).empty());

    const auto tmp = std::filesystem::temp_directory_path() /
                     ("aero-controller-db-" + std::to_string(
                          std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto exe_dir = tmp / "exe";
    const auto data_dir = tmp / "data";
    std::filesystem::create_directories(exe_dir);
    std::filesystem::create_directories(data_dir);

    // Android-style (no exe_dir) returns only app_data_dir.
    {
        const auto paths = candidate_paths(data_dir, {});
        assert(paths.size() == 1);
        assert(paths[0] == data_dir / kDatabaseFilename);
    }

    // Desktop with separate exe_dir and app_data_dir loads exe_dir first so
    // per-user app_data_dir entries override exe-adjacent entries.
    {
        const auto paths = candidate_paths(data_dir, exe_dir);
        assert(paths.size() == 2);
        assert(paths[0] == exe_dir / kDatabaseFilename);
        assert(paths[1] == data_dir / kDatabaseFilename);
    }

    // Portable mode / identical directories (even with trailing separator)
    // collapse to one path so the same file is not loaded twice.
    {
        const auto exe_with_slash = exe_dir / "";
        const auto paths = candidate_paths(exe_dir, exe_with_slash);
        assert(paths.size() == 1);
    }

    std::error_code ec;
    std::filesystem::remove_all(tmp, ec);

    // Verify src/main.cpp loads user mappings after SDL_InitSubSystem and before
    // replaying SDL_CONTROLLERDEVICEADDED for already-connected joysticks.
    const auto repo = std::filesystem::path(__FILE__).parent_path().parent_path();
    const std::string main_cpp = read_file(repo / "src" / "main.cpp");
    const auto init_pos = main_cpp.find("SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER)");
    const auto load_pos = main_cpp.find("aero::controller_db::load_user_mappings(");
    const auto enum_pos = main_cpp.find("SDL_NumJoysticks()");
    assert(init_pos != std::string::npos);
    assert(load_pos != std::string::npos);
    assert(enum_pos != std::string::npos);
    assert(init_pos < load_pos && load_pos < enum_pos);

    return 0;
}
