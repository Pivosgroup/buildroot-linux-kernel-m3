#include "register.h"
#include "clock.h"

typedef struct {
    int clock;
    int setting;
} clock_t;

const clock_t pll_table[] = {
    {900, 0x00100220},
    {175, 0x00110430},
    {0, 0}
};

static clock_t *find_pll(int clock)
{
    clock_t *p = pll_table;

    while (p->clock) {
        if (p->clock == clock) {
            return p;
        }

        p++;
    }

    return NULL;
}

void set_system_clock(int clock)
{
    clock_t *p = find_pll(clock);

    if (!p) {
        return;
    }

    Wr(HHI_SYS_PLL_CNTL, p->setting);   // system PLL set to 900Mhz

    Wr(HHI_A9_CLK_CNTL,                 // A9 clk set to system clock/2
       (0 << 10) |                     // 0 - sys_pll_clk, 1 - audio_pll_clk
       (0 << 0) |                      // 0 - sys/audio pll clk, 1 - XTAL
       (1 << 2) |                      // div2
       (1 << 7);                       // Connect A9 to the PLL divider output
}

   void set_mpeg_clock(int clock)
{
    clock_t *p = find_pll(clock);

    if (!p) {
        return;
    }

    Wr(HHI_OTHER_PLL_CNTL, p->setting); // other PLL set to 175Mhz

    Wr(HHI_MPEG_CLK_CNTL,               // MPEG clk81 set to other/2
       (0 << 12) |                     // select other PLL
       (1 << 0) |                      // div2
       (1 << 7) |                      // cntl_hi_mpeg_div_en, enable gating
       (1 << 8);                       // Connect clk81 to the PLL divider output

       Wr(HHI_MALI_CLK_CNTL,                // GPU clock set to other/2
          (1 << 9)  |                     // select "other" PLL
          (1 << 8)  |                     // Enable gated clock
          (1 << 0));                      // Divide the "other" PLL output by 2

       Wr(HHI_HDMI_CLK_CNTL,                // HDMI clock set to other/4
          (1 << 9)  |                     // select "other" PLL
          (1 << 8)  |                     // Enable gated clock
          (3 << 0));                      // Divide the "other" PLL output by 4
}

