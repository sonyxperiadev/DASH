/*
 * Author: Aleksej Makarov <aleksej.makarov@sonymobile.com>
 */
#include <hardware/lights.h>
/*
 * Helper function for opening a connection to the illumination service
 */
int illumination_connect(void);
int sysals_activate(void);
void sysals_deactivate(void);
int illumination_set_brightness(const char *light_id, unsigned char brightness);
int illumination_set_thermal_limit(unsigned char brightness_limit);
int illumination_set_thermal_mitigation(unsigned char brightness,
	unsigned char mode);
int illumination_set_light_state(int illumination_soc_fd, const char *light_id,
	struct light_state_t const *state);
int send_light_cmd(const struct light_state_t *state, const char *led_id);
#define SYS_ALS_DEV_NAME "system_als"
