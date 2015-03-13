//
// Command table for BREWise controller
//
#include <stdio.h>
#include <stdlib.h>
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_memmap.h"
#include "pin.h"
#include "adc.h"
#include "hw_adc.h"
#include "rom.h"
#include "rom_map.h"

#define ADC_DATA_MASK		0x3FFC

void adcInit(void)
{
#ifdef CC3200_ES_1_2_1
    //
    // Enable ADC clocks.###IMPORTANT###Need to be removed for PG 1.32
    //
    HWREG(GPRCM_BASE + GPRCM_O_ADC_CLK_CONFIG) = 0x00000043;
    HWREG(ADC_BASE + ADC_O_ADC_CTRL) = 0x00000004;
    HWREG(ADC_BASE + ADC_O_ADC_SPARE0) = 0x00000100;
    HWREG(ADC_BASE + ADC_O_ADC_SP                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   ARE1) = 0x0355AA00;
#endif

    //
    // Enable ADC channel
    //
    MAP_ADCChannelEnable(ADC_BASE, ADC_CH_1);

    //
    // Configure ADC timer which is used to timestamp the ADC data samples
    //
    MAP_ADCTimerConfig(ADC_BASE,2^17);

    //
    // Enable ADC timer which is used to timestamp the ADC data samples
    //
    MAP_ADCTimerEnable(ADC_BASE);

    //
    // Enable ADC module
    //
    MAP_ADCEnable(ADC_BASE);



	return;
}

int sampleAdc (void)
{
	unsigned long longSample;
	unsigned int sample;

	if(MAP_ADCFIFOLvlGet(ADC_BASE, ADC_CH_1))
	{
		longSample = MAP_ADCFIFORead(ADC_BASE, ADC_CH_1);
	}

	sample = (int)((longSample & ADC_DATA_MASK)>>2);

	return sample;
}
