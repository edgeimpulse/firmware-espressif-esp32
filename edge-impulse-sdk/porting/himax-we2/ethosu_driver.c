#include "../ei_classifier_porting.h"
#if EI_PORTING_HIMAX_WE2 == 1
#include "WE2_core.h"

// Impelements weak functions from edge-impulse-sdk/porting/ethos-core-driver/src/ethosu_driver.c
void ethosu_flush_dcache(uint32_t *p, size_t bytes)
{
    hx_CleanDCache_by_Addr((volatile void *)p, bytes);
}

void ethosu_invalidate_dcache(uint32_t *p, size_t bytes)
{
    hx_InvalidateDCache_by_Addr((volatile void *)p, bytes);
}

#endif // #if EI_PORTING_HIMAX_WE2 == 1