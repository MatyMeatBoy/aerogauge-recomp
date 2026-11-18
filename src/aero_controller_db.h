#ifndef AERO_CONTROLLER_DB_H
#define AERO_CONTROLLER_DB_H

#include <filesystem>
#include <string>
#include <vector>

#include "aero_paths.h"

namespace aero {
namespace controller_db {

inline constexpr const char* kDatabaseFilename = "gamecontrollerdb.txt";

// Returns candidate user-supplied SDL mapping database paths in load order:
// executable directory first (if non-empty), then per-user app_data_dir.
// Later entries override earlier ones in SDL_GameControllerAddMappingsFromFile;
// duplicate paths (for example, portable mode in the executable directory) are
// collapsed to a single entry.
inline std::vector<std::filesystem::path> candidate_paths(
    const std::filesystem::path& app_data_dir,
    const std::filesystem::path& exe_dir) {
    std::vector<std::filesystem::path> paths;
    auto append_unique = [&paths](const std::filesystem::path& dir) {
        if (dir.empty()) return;
        std::error_code ec;
        const std::filesystem::path candidate = dir / kDatabaseFilename;
        const std::filesystem::path norm = std::filesystem::weakly_canonical(candidate, ec);
        const std::filesystem::path& key = ec ? candidate : norm;
        for (const auto& existing : paths) {
            std::error_code existing_ec;
            const std::filesystem::path existing_norm =
                std::filesystem::weakly_canonical(existing, existing_ec);
            const std::filesystem::path& existing_key = existing_ec ? existing : existing_norm;
            if (key == existing_key) return;
        }
        paths.push_back(candidate);
    };

    append_unique(exe_dir);
    append_unique(app_data_dir);
    return paths;
}

#if defined(SDL_h_)
// Loads optional user-supplied SDL gamecontrollerdb.txt files on the SDL main
// thread. Must run after SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) and before
// replaying SDL_CONTROLLERDEVICEADDED for already-connected controllers.
inline void load_user_mappings(const std::filesystem::path& app_data_dir) {
    std::filesystem::path exe_dir;
#if !defined(__ANDROID__)
    if (char* base_path = SDL_GetBasePath()) {
        const auto parsed = aero::paths::from_utf8(base_path);
        SDL_free(base_path);
        if (parsed) {
            exe_dir = *parsed;
        }
    }
#endif
    for (const auto& path : candidate_paths(app_data_dir, exe_dir)) {
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec)) {
            continue;
        }
        const auto utf8 = path.u8string();
        const std::string path_str(reinterpret_cast<const char*>(utf8.data()), utf8.size());
        const int added = SDL_GameControllerAddMappingsFromFile(path_str.c_str());
        if (added < 0) {
            AERO_LOG_WARN("input", "Failed to load controller mappings from %s: %s\n",
                          path_str.c_str(), SDL_GetError());
        } else {
            AERO_LOG_INFO("input", "Loaded %d controller mapping(s) from %s\n",
                          added, path_str.c_str());
        }
    }
}
#endif

} // namespace controller_db
} // namespace aero

#endif
