#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "rp.h"
#include "rp_hw-profiles.h"
#include "rp_hw-calib.h"

#define LINE_LENGTH 0x400


int main(){
rp_calib_params_t calib_params;
rp_HPeModels_t model;
//profiles_t *profile;
int state;
rp_CalibInit();
//profile = getProfile(&state);
state=rp_HPPrint();
rp_HPGetModel(&model);
calib_params = rp_GetCalibrationSettings();
printf("State = %d\n",state);

printf("Model = %d\n",model);
printf("Calibration parameters:\n ID = %d,  adc count 1V  = %d, adc count 20V  = %d, dac count = %d\n",calib_params.dataStructureId,calib_params.fast_adc_count_1_1,calib_params.fast_adc_count_1_20,calib_params.fast_dac_count_x1);

for (int i = 0; i<calib_params.fast_adc_count_1_1;i++) 
	{
		printf("IN%d 1V scale = %f, value = %d, offset = %d, gain = %f\n",i,calib_params.fast_adc_1_1[i].baseScale,calib_params.fast_adc_1_1[i].calibValue,calib_params.fast_adc_1_1[i].offset,calib_params.fast_adc_1_1[i].gainCalc);
	}

for (int i = 0; i<calib_params.fast_adc_count_1_20;i++) 
	{
		printf("IN%d 20V scale = %f, value = %d, offset = %d, gain = %f\n",i,calib_params.fast_adc_1_20[i].baseScale,calib_params.fast_adc_1_20[i].calibValue,calib_params.fast_adc_1_20[i].offset,calib_params.fast_adc_1_20[i].gainCalc);
	}

for (int i = 0; i<calib_params.fast_dac_count_x1;i++) 
	{
		printf("OUT%d 1V scale = %f, value = %d, offset = %d, gain = %f\n",i,calib_params.fast_dac_x1[i].baseScale,calib_params.fast_dac_x1[i].calibValue,calib_params.fast_dac_x1[i].offset,calib_params.fast_dac_x1[i].gainCalc);
	}
}
