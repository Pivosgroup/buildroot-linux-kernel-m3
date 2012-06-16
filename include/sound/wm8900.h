/**
 * struct wm8900_platform_data - platform-specific WM8900 data
 * @is_hp_unpluged:          HP Detect
 */

#ifndef _WM8900_H_
#define _WM8900_H_

struct wm8900_platform_data {
    int (*is_hp_pluged)(void);
};

#endif
