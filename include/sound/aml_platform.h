/*
 * struct aml_platform_data - platform-specific amlogic data
 * @is_hp_unpluged:          HP Detect
 */

struct aml_audio_platform {
	int (*is_hp_pluged)(void);
	void (*mute_spk)(void *, int);
	void (*mute_headphone)(void *, int);
	void (*audio_pre_start)(void);	
	void (*audio_post_stop)(void);
};

