#include <linux/types.h>
#include <hardware/lights.h>

void sysals_deactivate(void){}

int illumination_connect(void)
{
    return 0;
}

int sysals_activate(void)
{
    return 0;
}

int illumination_set_brightness(const char *light_id, unsigned char brightness)
{
    return 0;
}

int illumination_set_thermal_limit(unsigned char brightness_limit)
{
    return 0;
}

int illumination_set_thermal_mitigation(unsigned char brightness, unsigned char mode)
{
    return 0;
}

int illumination_set_light_state(int illumination_soc_fd, const char *light_id, struct light_state_t const *state)
{
    return 0;
}

int send_light_cmd(const struct light_state_t *state, const char *led_id)
{
    return 0;
}
