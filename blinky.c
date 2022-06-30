#define F_CPU 3330000UL

#include <avr/io.h>
#include <util/delay.h>
	
#define LED_PIN (1 << 0x01)


void turn_led_off() {
	PORTB.OUT &= ~PIN1_bm;
}


void turn_led_on() {
	PORTB.OUT |= PIN1_bm;
}


int main(void) {
	PORTB.DIR |= PIN1_bm;
	
	PORTB.OUT |= PIN1_bm;
	
	while (1) {
		_delay_ms(5000);
		turn_led_off();
		
		_delay_ms(5000);
		turn_led_on();
	}
}

