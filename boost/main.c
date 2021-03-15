/*
 * attiny406.c
 *
 * Created: 9/22/2020 5:06:26 PM
 * Author : jtperrotta
 */ 

#define F_CPU 20000000UL // ???? 
#include <avr/io.h>
#include <stdbool.h>
#include <stdio.h>

#include <util/delay.h>
#include <avr/interrupt.h>

#define BEST_TH 0.000033//
/* max eff is the solution to this for 1mH its that  (3 (1 - e^(-(x R)/L)) - 1)/R = 0*/
#define VIN 5ULL
#define maxV 200 // voltage range 
#define PERC (100/(BEST_TH*40)) // best percentage b4 it starts dropping 
#define PIDPVAL 3 // adgust the P in PID 
int32_t times = 1;

void initADC(){
	ADC0.CTRLB |= 0x3; // mult by 4
	ADC0.CTRLC = ADC_REFSEL_VDDREF_gc| ADC_SAMPCAP_bm | ADC_PRESC_DIV16_gc;
	ADC0.MUXPOS = 1;
	ADC0.CTRLA = ADC_ENABLE_bm;
}
uint16_t ReadADC(uint8_t pos){
	ADC0.MUXPOS = pos&0x1f;
	ADC0.CTRLA = ADC_ENABLE_bm;
	ADC0.COMMAND = ADC_STCONV_bm;
	while(!(ADC0.INTFLAGS & ADC_RESRDY_bm))
		;
	uint16_t tmp =  ADC0.RES;
	ADC0.INTFLAGS = ADC_RESRDY_bm;
	return tmp;
}

void init_timer(){
	TCA0.SINGLE.CTRLA = TCA_SINGLE_ENABLE_bm;
	TCA0.SINGLE.CTRLB = TCA_SINGLE_CMP2EN_bm | TCA_SINGLE_WGMODE1_bm | TCA_SINGLE_WGMODE0_bm;
	TCA0.SINGLE.CTRLC = TCA_SINGLE_CMP2OV_bm;

	TCA0.SINGLE.CMP2 = 128;
	TCA0.SINGLE.PER = 0xFF; // set top to 255
}


void setDiv(uint8_t val){
	TCA0.SINGLE.CTRLA = 0;
	TCA0.SINGLE.CTRLA = ((val & 7) << 1);
	TCA0.SINGLE.CTRLESET = (1 << 2); //reset timmer
	TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;
}
void setFreq(uint16_t val){
	TCA0.SINGLE.PER = val;
	TCA0.SINGLE.CTRLESET = (1 << 2); //reset timmer
}

void setPWM(uint16_t val){
	TCA0.SINGLE.CMP2 = val;
	TCA0.SINGLE.CTRLESET = (1 << 2); //reset timmer
}

void RRINT(){
	CPUINT.CTRLA |= CPUINT_LVL0RR_bm;
	CPUINT.LVL0PRI = PORTB_PORT_vect_num;
	sei();
}

/* Time low in Hz */
void SetPwm(uint32_t TimeLow){
	uint32_t Th = BEST_TH*F_CPU;
	uint32_t Tl = F_CPU/TimeLow;
	uint32_t Tt = Th + Tl;
	uint8_t i = 0;
	while(Tt > UINT16_MAX){
		Tt >>= 1;
		Th >>= 1;
		i++;
		if(i == 0x5){
			Tt >>= 1; // top skips 32 and more
			Th >>= 1;
		}
					
	}
	setDiv(i);
	setFreq(Tt);
	setPWM(Th);
}
void setV(uint32_t V){
		uint32_t target = (V*1024ULL*8ULL*11ULL)/(VIN*511ULL); // scale based on 22k 1M R div
		uint32_t adc = ReadADC(1);
		int32_t diff = ((int32_t)target-(int32_t) adc);
		diff /= PIDPVAL;
		if ((times + diff) > 1){
			if ((times + diff) < F_CPU/10){ // wont trigger mosfet on high freq
					if ((times + diff) <  PERC) {
						times += diff;
						SetPwm(times);
					}	
			} else {
				SetPwm(PERC);
			}

		} else {
			SetPwm(1);

		}

}
int main(void)
{
	CPU_CCP = 0xD8;
	CLKCTRL_MCLKCTRLA = CLKCTRL_CLKSEL_OSC20M_gc;
	CPU_CCP = 0xD8;
	CLKCTRL_MCLKCTRLB = 0;
	PORTB.DIR = 1 << 4 | 1 << 2;
	PORTB.OUT = 1 << 2;
	RRINT();
	init_timer();
	initADC();
	uint32_t target = 20;
	while (1) 
    {
		setV(target);
		target = (maxV*((uint32_t)ReadADC(2)))/(1024ULL*8ULL);// scale vin to a vout target
	}
}
