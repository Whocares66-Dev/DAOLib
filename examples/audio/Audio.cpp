// Example: the game's sound events starting (dao/Audio.h).
//
// Logs each FMOD event the game starts, with its path and, for a placed one, where it is. What it
// sees goes to example_audio.log. Footsteps, blows and ambience come by the hundred, so it logs
// the first few hundred and then counts.
#include "dao/Audio.h"
#include "dao/Log.h"
#include "dao/Plugin.h"

#include <spdlog/spdlog.h>

#include <atomic>

extern "C" __declspec(dllexport) bool __cdecl DAOLib_Load(const dao::LoaderInfo *info)
{
    dao::OpenLog("example_audio");
    if (info == nullptr || info->apiVersion != dao::kPluginApiVersion || !info->knownExecutable)
    {
        spdlog::error("not the loader or the game this was built for");
        return false;
    }
    dao::audio::OnEventStart([](const dao::audio::EventStart &sound) {
        static std::atomic<int> seen{0};
        const int n = ++seen;
        if (n <= 300)
        {
            if (sound.positional)
            {
                spdlog::info("{} at {:.1f} {:.1f} {:.1f}", sound.path, sound.position[0], sound.position[1],
                             sound.position[2]);
            }
            else
            {
                spdlog::info("{} (not placed)", sound.path);
            }
        }
        else if (n % 500 == 0)
        {
            spdlog::info("{} sound events so far", n);
        }
    });
    return true;
}
