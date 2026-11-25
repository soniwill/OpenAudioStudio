#include <oae_audio.h>

int main(void)
{
    oae_init_settings settings;
    if (oae_init_settings_default(&settings) != OAE_OK || oae_get_version() == 0) {
        return 1;
    }
    oae_system system = 0;
    if (oae_system_create(&system) != OAE_OK) {
        return 1;
    }
    settings.flags = OAE_INIT_NO_DEVICE;
    const oae_result init = oae_system_init(system, &settings);
    const oae_result release = oae_system_release(system);
    return init == OAE_OK && release == OAE_OK ? 0 : 1;
}
