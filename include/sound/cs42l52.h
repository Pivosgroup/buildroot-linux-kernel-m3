/**
 * struct cs42l52_platform_data - platform-specific cs42l52 data
 * @is_hp_unpluged:          HP Detect
 */

#ifndef __CS42L52_PDATA_H__
#define __CS42L52_PDATA_H__

struct cs42l52_platform_data {
    int (*cs42l52_pwr_rst)(void);
};

#endif

