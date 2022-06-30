#define F_CPU 3330000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>


volatile uint8_t x;

const uint8_t LED_PIN = (1 << 0x06); // PA6, Pin 4
const uint8_t SWITCH_PIN = (1 << 0x02); // PA2, Pin 12


int main(void) {
	// LED Setup
	
	// Set LED pin to output
	PORTA.DIRSET = LED_PIN;
	
	// Switch Setup
	
	// Set button pin to input
	PORTA.DIRCLR = SWITCH_PIN;
	
	// Enable pull up register
	PORTA.PIN2CTRL = PORT_ISC_FALLING_gc | PORT_PULLUPEN_bm;
	
	// Enable interrupts globally
	sei();
	
    while (1) {
		if (x == 1) {
			//Toggles pin
			PORTA.OUTTGL = LED_PIN;
			
			x = 0;
		}
    }
}


ISR(PORTA_PORT_vect) {
	x = 1;
	
	// De-bounce
	_delay_ms(50);
	
	// Clear interrupt flag
	PORTA.INTFLAGS |= SWITCH_PIN;
}
