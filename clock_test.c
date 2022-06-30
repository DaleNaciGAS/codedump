#define F_CPU 3330000UL

#include <avr/io.h>
#include <stdbool.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define LED_PIN (1 << 0x01)

bool led_on = false;
volatile uint8_t x = 0;


void timer_config() {
	sei();  // Globally enable interrupts
	
	TCA0.SINGLE.PER = 0x3E7;  // Sets TOP value to period register
	
	TCA0.SINGLE.CTRLA |= (1 << 0x01);  // Enables peripheral
	
	TCA0.SINGLE.CTRLD &= ~(1 << 0x01);  // Disables split mode
	
	TCA0.SINGLE.CTRLESET &= ~(1 << 0x01);  // Sets counter to be incrementing
	
	TCA0.SINGLE.INTCTRL |= (1 << 0x01);  // Enables overflow interrupt flag

	TCA0.SINGLE.EVCTRL &= ~(1 << 0x01);  // Disables counting on event input
	
	// Sets mode of operation to normal
	TCA0.SINGLE.CTRLB &= ~(1 << 0x01);
	TCA0.SINGLE.CTRLB &= ~(1 << 0x02);
	TCA0.SINGLE.CTRLB &= ~(1 << 0x03);
	
	CLKCTRL.MCLKCTRLB |= (1 << 0x01);  // Enables prescaling
	
	// Set frequency to 20 MHz
	FUSE.OSCCFG &= ~(1 << 0x01);
	FUSE.OSCCFG &= (1 << 0x02);
	
	// Enables 10 prescaler
	CLKCTRL.MCLKCTRLB |= (1 << 0x02);
	CLKCTRL.MCLKCTRLB &= ~(1 << 0x03);
	CLKCTRL.MCLKCTRLB &= ~(1 << 0x04);
	CLKCTRL.MCLKCTRLB |= (1 << 0x05);
}


void led_config() {
	PORTB.DIR |= LED_PIN;
	PORTB.OUT |= LED_PIN;
}


void toggle_led() {
	if (led_on) {
		PORTB.OUT &= ~LED_PIN;
	} else {
		PORTB.OUT |= LED_PIN;
	}
	
	led_on = !led_on;
}


int main(void) {
	timer_config();
	led_config();
	
	while (1) {
		if (x >= 1000) {
			toggle_led();
			x = 0;
		}
	}
	
	return 0;
}


ISR(TCA0_OVF_vect) {
	x++;
	
	TCA0.SINGLE.INTFLAGS |= (1 << 0x01);  // Clear overflow flag
}