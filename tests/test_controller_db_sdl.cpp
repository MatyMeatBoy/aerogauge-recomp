#undef NDEBUG
// Real SDL2 regression for user-supplied gamecontrollerdb.txt loading:
// verifies that an already-connected, previously unmapped virtual joystick is
// recognized after load_user_mappings_from(), that untagged lines without a
// matching platform:<OS>, field are ignored by SDL, and that app_data_dir
// overrides exe_dir for equivalent mapping keys.
#include <SDL.h>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "aero_controller_db.h"

namespace {

const char* current_sdl_platform() {
#if defined(_WIN32)
    return "Windows";
#elif defined(__APPLE__)
    return "Mac OS X";
#elif defined(__ANDROID__)
    return "Android";
#else
    return "Linux";
#endif
}

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    assert(out && "failed to write test gamecontrollerdb.txt");
    out << content;
}

} // namespace

int main() {
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    assert(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) == 0);

    const int virt_index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, 4, 12, 1);
    assert(virt_index >= 0);

    // Virtual joysticks start unmapped (not recognized as SDL_GameController).
    assert(SDL_IsGameController(virt_index) == SDL_FALSE);

    char guid_str[64]{};
    SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(virt_index), guid_str, sizeof(guid_str));

    const auto tmp = std::filesystem::temp_directory_path() /
                     ("aero-controller-db-sdl-" + std::to_string(
                          std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto exe_dir = tmp / "exe";
    const auto data_dir = tmp / "data";
    std::filesystem::create_directories(exe_dir);
    std::filesystem::create_directories(data_dir);

    const auto exe_db = exe_dir / aero::controller_db::kDatabaseFilename;
    const auto data_db = data_dir / aero::controller_db::kDatabaseFilename;

    // 1. A mapping line missing the comma-terminated platform:<OS>, field is
    //    ignored by SDL_GameControllerAddMappingsFromFile.
    write_file(exe_db, std::string(guid_str) + ",Virtual Test Pad,a:b5,b:b4,\n");
    aero::controller_db::load_user_mappings_from({}, exe_dir);
    assert(SDL_IsGameController(virt_index) == SDL_FALSE);

    // 2. A properly platform-tagged mapping in exe_dir makes the already-attached
    //    virtual joystick recognizable, and an equivalent entry in app_data_dir
    //    overrides it.
    const std::string plat = std::string("platform:") + current_sdl_platform() + ",";
    write_file(exe_db, std::string(guid_str) + ",Virtual Test Pad,a:b5,b:b4," + plat + "\n");
    write_file(data_db, std::string(guid_str) + ",Virtual Test Pad,a:b4,b:b5," + plat + "\n");

    aero::controller_db::load_user_mappings_from(data_dir, exe_dir);
    assert(SDL_IsGameController(virt_index) == SDL_TRUE);

    SDL_GameController* gc = SDL_GameControllerOpen(virt_index);
    assert(gc != nullptr);
    const SDL_GameControllerButtonBind bind_a =
        SDL_GameControllerGetBindForButton(gc, SDL_CONTROLLER_BUTTON_A);
    const SDL_GameControllerButtonBind bind_b =
        SDL_GameControllerGetBindForButton(gc, SDL_CONTROLLER_BUTTON_B);
    assert(bind_a.bindType == SDL_CONTROLLER_BINDTYPE_BUTTON && bind_a.value.button == 4);
    assert(bind_b.bindType == SDL_CONTROLLER_BINDTYPE_BUTTON && bind_b.value.button == 5);

    SDL_GameControllerClose(gc);
    SDL_JoystickDetachVirtual(virt_index);
    SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);

    std::error_code ec;
    std::filesystem::remove_all(tmp, ec);
    return 0;
}
