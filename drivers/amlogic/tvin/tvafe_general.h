#ifndef _TVAFE_GENERAL_H
#define _TVAFE_GENERAL_H

// ***************************************************************************
// *** enum definitions *********************************************
// ***************************************************************************
typedef enum tvafe_adc_ch_e {
    TVAFE_ADC_CH_NULL = 0,
    TVAFE_ADC_CH_PGA,
    TVAFE_ADC_CH_A,
    TVAFE_ADC_CH_B,
    TVAFE_ADC_CH_C,
} tvafe_adc_ch_t;

typedef enum tvafe_state_e {
    TVAFE_STATE_NOSIG = 0,
    TVAFE_STATE_UNSTABLE,
    TVAFE_STATE_STABLE,
} tvafe_state_t;

// ***************************************************************************
// *** macro definitions *********************************************
// ***************************************************************************

#define TVAFE_SURE_NOSIG    8 // Stay in TVAFE_STATE_NOSIG    for some cycles (10mS per each) => be sure TVIN_SIG_STATUS_NOSIG
#define TVAFE_SURE_UNSTABLE 8 // Stay in TVAFE_STATE_UNSTABLE for some cycles (10mS per each) => be sure TVIN_SIG_STATUS_UNSTABLE
#define TVAFE_EXIT_NOSIG    1 // Have signal                  for some cycles (10mS per each) => exit    TVAFE_STATE_NOSIG
#define TVAFE_BACK_NOSIG    8 // No signal                    for some cycles (10mS per each) => back to TVAFE_STATE_NOSIG
#define TVAFE_EXIT_STABLE   1 // Signal unstable              for some cycles (10mS per each) => exit    TVAFE_STATE_STABLE
#define TVAFE_BACK_STABLE   8 // Signal stable                for some cycles (10mS per each) => back to TVAFE_STATE_STABLE

// ***************************************************************************
// *** global parameters **********
// ***************************************************************************
extern enum tvafe_adc_pin_e         tvafe_default_cvbs_out;

// *****************************************************************************
// ******** function claims ********
// *****************************************************************************
enum tvafe_adc_pin_e tvafe_get_free_pga_pin(struct tvafe_pin_mux_s *pinmux);
int tvafe_source_muxing(struct tvafe_info_s *info);
void tvafe_vga_set_edid(struct tvafe_vga_edid_s *edid);
void tvafe_vga_get_edid(struct tvafe_vga_edid_s *edid);
void tvafe_set_fmt(struct tvafe_info_s *info);
void tvafe_set_cal_value(struct tvafe_adc_cal_s *para);
void tvafe_get_cal_value(struct tvafe_adc_cal_s *para);
void tvafe_get_wss_data(struct tvafe_comp_wss_s *para);
enum tvin_scan_mode_e tvafe_top_get_scan_mode(void);
void tvafe_check_cvbs_3d_comb(void);
void tvafe_init_state_handler(void);
void tvafe_run_state_handler(struct tvafe_info_s *info);
void tvafe_stop_module(struct tvafe_info_s *info);
void tvafe_reset_module(enum tvin_port_e port);


#endif  // _TVAFE_GENERAL_H
