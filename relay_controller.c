#include <time.h>
#include <stdarg.h>
#include <stdbool.h>

#include <avr/io.h>


// Configurable Constants
const double RELAY_POWER_TIME = 0.15;
const double GROUND_FAULT_DELAY_TIME = 15.0;
const double PROBATION_TIME = 10.0;
const int FAULT_LIMIT = 20;

// Pins
// NOTE: THESE ARE TEMPORARY VALUES
const uint8_t OPENWRT_PIN = (1 < 0x01); // Pin 11 (PA1)
const uint8_t EBIKE_PIN = (1 < 0x02); // Pin 12 (PA2)
const uint8_t RELAY_ON_PIN = (1 < 0x03); // Pin 13 (PA3)
const uint8_t RELAY_OFF_PIN = (1 < 0x04); // Pin 2 (PA4)
const uint8_t GFI_PIN = (1 < 0x05); // Pin 3 (PA5)
const uint8_t OPENWRT_STATUS_PIN = (1 < 0x06); // Pin 4 (PA6)
const uint8_t EBIKE_STATUS_PIN = (1 < 0x07); // Pin 5 (PA7)

volatile uint8_t fault_count;

enum State {
	START,
	RELAY_OFF,
	RELAY_ENABLE,
	RELAY_ON,
	RELAY_DISABLE,
	SOFT_GROUND_FAULT,
	HARD_GROUND_FAULT,
	PROBATIONARY_RELAY_ON
};


/************************************************************************/
/*                            PIN FUNCTIONS                             */
/************************************************************************/


/**
 * Checks if one or multiple pins are all HIGH
 */
bool is_high(int num, ...) {
	va_list valist;
	bool output;
	int i;
	
	// Initializes list for num number of args
	va_start(valist, num);
	
	output = true;
	
	// Sets output to false if any pins aren't HIGH
	for (i = 0; i < num; i++) {
		if (PORTA.IN & va_arg(valist, uint8_t)) {
			output = false;
		} 
	}
	
	// Cleans memory reserved for valist
	va_end(valist);
	
	return output;
}


/**
 * Must be a pin configured for inputs
 */
void start_signal(uint8_t pin) {
	if (pin == RELAY_ON_PIN) {
		PORTA.OUT &= ~RELAY_ON_PIN;
	} else if (pin == RELAY_OFF_PIN) {
		PORTA.OUT &= ~RELAY_OFF_PIN;
	}
}


/**
 * Must be a pin configured for inputs
 */
void stop_signal(uint8_t pin) {
	if (pin == RELAY_ON_PIN) {
		PORTA.OUT |= RELAY_ON_PIN;
		} else if (pin == RELAY_OFF_PIN) {
		PORTA.OUT |= RELAY_OFF_PIN;
	}
}


/**
 * Status pins are read by OpenWRT and EBIKE. This function should be called
 * whenever any of these pins changes
 */
void update_status_pins() {
	if (PORTA.IN & OPENWRT_PIN) {
		PORTA.OUT |= OPENWRT_STATUS_PIN;
	} else {
		PORTA.OUT &= ~OPENWRT_STATUS_PIN;
	}
	
	if (PORTA.IN & EBIKE_PIN) {
		PORTA.OUT |= EBIKE_STATUS_PIN;
	} else {
		PORTA.OUT &= ~EBIKE_STATUS_PIN;
	}
}


/************************************************************************/
/*                           HELPER FUNCTIONS                           */
/************************************************************************/


/**
 * Returns the difference in terms of seconds, but decimal points hold
 * millisecond values
 */
double get_time_diff(clock_t tic, clock_t toc) {
	return (double)(toc - tic) / CLOCKS_PER_SEC;
}


/************************************************************************/
/*                           STATE FUNCTIONS                            */
/************************************************************************/


/**
 * Configures variables and pins
 * 
 * Possible Return States:
 *     - RELAY_OFF
 */
enum State start(void) {
	fault_count = 0;
	
	// Configure input pins
	PORTA.DIR |= PIN1_bm;
	PORTA.DIR |= PIN2_bm;
	PORTA.DIR |= PIN5_bm;
	
	// Configure output pins
	PORTA.DIR &= ~PIN3_bm;
	PORTA.DIR &= ~PIN4_bm;
	PORTA.DIR &= ~PIN6_bm;
	PORTA.DIR &= ~PIN7_bm;
	
	// Enable pull-up config for output pins
	PORTA.PIN3CTRL = PORT_PULLUPEN_bm;
	PORTA.PIN4CTRL = PORT_PULLUPEN_bm;
	PORTA.PIN6CTRL = PORT_PULLUPEN_bm;
	PORTA.PIN7CTRL = PORT_PULLUPEN_bm;
		
	return RELAY_OFF;
}


/**
 * The default "OFF" position
 * 
 * Possible Return States:
 *     - RELAY_ENABLE
 */
enum State relay_off(void) {
	while (1) {
		if (is_high(OPENWRT_PIN, EBIKE_PIN)) {			
			return RELAY_ENABLE;
		}
	}
}


/**
 * Configures relay pins to enable relay while checking for ground faults.
 * PROBATIONARY_RELAY_ON occurs when fault_count > 0
 * 
 * Possible Return States:
 *     - SOFT_GROUND_FAULT
 *     - RELAY_ON
 *     - PROBATIONARY_RELAY_ON
 */
enum State relay_enable(void) {
	start_signal(RELAY_OFF_PIN);
	
	clock_t tic = clock();
	clock_t toc = clock();
	
	while (get_time_diff(tic, toc) < RELAY_POWER_TIME) {		
		if (!is_high(GFI_PIN)) {
			stop_signal(RELAY_OFF_PIN);
			
			return SOFT_GROUND_FAULT;
		}
		
		toc = clock();
	}
	
	stop_signal(RELAY_OFF_PIN);
	
	start_signal(RELAY_ON_PIN);
	
	tic = clock();
	toc = clock();
	
	while (get_time_diff(tic, toc) < RELAY_POWER_TIME) {
		if (!is_high(GFI_PIN)) {
			stop_signal(RELAY_ON_PIN);
			
			return SOFT_GROUND_FAULT;
		}
		
		toc = clock();
	}
	
	stop_signal(RELAY_ON_PIN);
	
	if (fault_count == 0) {
		return RELAY_ON;
	} else {
		return PROBATIONARY_RELAY_ON;
	}
}


/**
 * The default "ON" position
 * 
 * Possible Return States:
 *     - SOFT_GROUND_FAULT
 *     - RELAY_DISABLE
 */
enum State relay_on(void) {
	while (is_high(OPENWRT_PIN, EBIKE_PIN)) {
		if (is_high(GFI_PIN)) {
			return SOFT_GROUND_FAULT;
		}
	}
		
	return RELAY_DISABLE;
}


/**
 * Configures relay pins to disable relay while checking for ground faults
 * 
 * Possible Return States:
 *     - SOFT_GROUND_FAULT
 *     - RELAY_OFF
 */
enum State relay_disable(void) {
	start_signal(RELAY_ON_PIN);
	
	clock_t tic = clock();
	clock_t toc = clock();
	
	while (get_time_diff(tic, toc) < RELAY_POWER_TIME) {		
		if (!is_high(GFI_PIN)) {
			stop_signal(RELAY_ON_PIN);
			
			return SOFT_GROUND_FAULT;
		}
		
		toc = clock();
	}
	
	stop_signal(RELAY_ON_PIN);
	
	start_signal(RELAY_OFF_PIN);
	
	tic = clock();
	toc = clock();
	
	while (get_time_diff(tic, toc) < RELAY_POWER_TIME) {
		if (!is_high(GFI_PIN)) {
			stop_signal(RELAY_OFF_PIN);
			
			return SOFT_GROUND_FAULT;
		}
		
		toc = clock();
	}
	
	stop_signal(RELAY_OFF_PIN);
	
	return RELAY_OFF;
}


/**
 * Returns HARD_GROUND_FAULT state if FAULT_LIMIT consecutive ground fault
 * have occurred. Otherwise, it waits 15 seconds before enabling the relay.
 * 
 * Possible Return States:
 *     - HARD_GROUND_FAULT
 *     - RELAY_ENABLE
 */
enum State soft_ground_fault(void) {
	// TODO
	
	fault_count++;
	
	if (fault_count >= FAULT_LIMIT) {
		return HARD_GROUND_FAULT;
	} else {
		clock_t tic = clock();
		clock_t toc = clock();
		
		while (get_time_diff(tic, toc) < GROUND_FAULT_DELAY_TIME) {			
			toc = clock();
		}
		
		return RELAY_ENABLE;
	}
}


/**
 * Waits 10 seconds before going to a regular relay. If another ground fault
 * occurs while inside this function, then it counts as a consecutive ground
 * fault
 * 
 * Possible Return States:
 *     - RELAY_DISABLE
 *     - SOFT_GROUND_FAULT
 *     - RELAY_ON
 */
enum State probationary_relay_on(void) {
	clock_t tic = clock();
	clock_t toc = clock();
	
	while (get_time_diff(tic, toc) < PROBATION_TIME) {
		if (!is_high(OPENWRT_PIN, EBIKE_PIN)) {			
			fault_count = 0;
			
			return RELAY_DISABLE;
		}
		
		if (!is_high(GFI_PIN)) {
			return SOFT_GROUND_FAULT;
		}
	}
	
	fault_count = 0;
	
	return RELAY_ON;
}


/**
 * Once a hard ground fault occurs, only a power cycle can get the system out
 * of it
 *
 * Possible Return States:
 *     - NONE
 */
void hard_ground_fault(void) {
	while (1) {
		;
	}
}


/**
 * Calls different states to run and controls switches between them.
 */
void state_machine(void) {
	enum State current_state = START;
	
	while (1) {
		if (current_state == START) {
			current_state = start();
		} else if (current_state == RELAY_OFF) {
			current_state = relay_off();
		} else if (current_state == RELAY_ENABLE) {
			current_state = relay_enable();
		} else if (current_state == RELAY_ON) {
			current_state = relay_on();
		} else if (current_state == RELAY_DISABLE) {
			current_state = relay_disable();
		} else if (current_state == SOFT_GROUND_FAULT) {
			current_state = soft_ground_fault();
		} else if (current_state == HARD_GROUND_FAULT) {
			hard_ground_fault();
		} else if (current_state == PROBATIONARY_RELAY_ON) {
			current_state = probationary_relay_on();
		} else {
			return;
		}
	}
}


/************************************************************************/
/*                             MAIN FUNCTION                            */
/************************************************************************/


int main(void) {
	state_machine();
	
	printf("UNEXPECTED ERROR - INVALID STATE.");
	
	return 0;
}