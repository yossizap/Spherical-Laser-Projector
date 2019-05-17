/*********************************** LICENCE **********************************\
| Spherical Laser Projector - arduino based blue laser projector software.     |
|                                                                              |
| Hardware:                                                                    |
|     * Two uno-polar step motors for X and Y axes,                            |
|       connected via NPNs to pins 4-7 and A0-A3.                              |
|     * Blue laser connected via NPN to pins 8.                                |
|     * Normally open button connected via GND and pin 12.                     |
|     * LED inside the button connected to GND and pin 11 via 1K resistor.     |
|     * Input Powrer goes to pin 13, and to arduino 5V via diod with capacitor.|
|                                                                              |
| Software:                                                                    |
|     * Draws formated paths of SVG path commands.                             |
|     * Saves and restors motors positions from EEPROM.                        |
|     * Goes to next draw starting point for the best UI experience.           |
|                                                                              |
| Arad Eizen and Yossi Zapesochini 2016.                                       |
|	                                                                           |
\******************************************************************************/
// #define USE_OLD_PROJECTOR_PINOUTS


// #include <math.h>
#include <EEPROM.h>
#include "gen/svgs.h"

#ifdef USE_OLD_PROJECTOR_PINOUTS
	#define X_AXIS_LIMIT_MIN		(-1200)
	#define Y_AXIS_LIMIT_MIN		(-800)
	#define X_AXIS_LIMIT_MAX		(1200)
	#define Y_AXIS_LIMIT_MAX		(600)
#else
	#define CORNER_DEG				(1010)
	#define X_AXIS_LIMIT_MIN		(-CORNER_DEG)
	#define Y_AXIS_LIMIT_MIN		(-2400)
	#define X_AXIS_LIMIT_MAX		(CORNER_DEG * 2)
	#define Y_AXIS_LIMIT_MAX		(2400)
#endif
#define SERIAL_BAUDRATE				(115200)
#define SERIAL_BUFFER_SIZE			(60)
#define STEPS_DELAY_MS				(4)
#define BEZIER_SEGMENTS				(30)

#define LAMP_FADE_MS				(150)
#define EEPROM_RECORD_ADDRESS		(0)
#define EEPROM_STATUS_VALID			(0x55)

#define X_AXIS_A_PIN				(4)
#define X_AXIS_B_PIN				(5)
#define X_AXIS_C_PIN				(6)
#define X_AXIS_D_PIN				(7)
#ifdef USE_OLD_PROJECTOR_PINOUTS
	#define LASER_RED_PIN			(8)
	#define LASER_BLUE_PIN			(9)
#else
	#define LASER_PIN				(8)
#endif
#define LIGHT_PIN					(11)
#define BUTTON_PIN					(12)
#define POWER_PIN					(13)
#define Y_AXIS_A_PIN				(A0)
#define Y_AXIS_B_PIN				(A1)
#define Y_AXIS_C_PIN				(A2)
#define Y_AXIS_D_PIN				(A3)

#define _PINB_MASK(pin)				(1 << (pin - 8))
#ifdef USE_OLD_PROJECTOR_PINOUTS
	#define LASER_BLUE_MASK			(_PINB_MASK(LASER_BLUE_PIN))
	#define LASER_RED_MASK			(_PINB_MASK(LASER_RED_PIN))
	#define LASERS_MASK				(LASER_BLUE_MASK | LASER_RED_MASK)
#else
	#define LASER_MASK				(_PINB_MASK(LASER_PIN))
#endif
#define LIGHT_MASK					(_PINB_MASK(LIGHT_PIN))
#define BUTTON_MASK					(_PINB_MASK(BUTTON_PIN))
#define POWER_MASK					(_PINB_MASK(POWER_PIN))


const uint8_t STEPS_MASKS[] = {0b1001, 0b1000, 0b1100, 0b0100, 0b0110, 0b0010, 0b0011, 0b0001};
const uint8_t LAMP_FADE_PWM[] = {5, 10, 20, 40, 80, 130, 180, 220, 255, 200, 100, 40, 10};
const uint8_t PATH_COMMAND_ARGUMENTS[] = {
	'M', 2,
	'H', 1,
	'V', 1,
	'L', 2,
	'Z', 0,
	'Q', 4,
	'T', 2,
	'C', 6,
	'S', 4,
	'A', 5,
	'E', 0,
};

typedef struct eeprom_record_t {
	uint8_t status;
	int16_t x;
	int16_t y;
};

typedef struct draw_image_t {
	PGM_P path;
	int16_t x;
	int16_t y;
	double scale;
};

const draw_image_t draws[] = {
	{ANDROID_1_DRAW, -400, -300, 1.0},
	{EAGLE_1_DRAW, -400, -300, 1.0},
	{BUTTERFLY_1_DRAW, -400, -300, 1.0},
	{FLOWER_1_DRAW, -400, -300, 1.0},
	{APPLE_1_DRAW, -400, -300, 1.0},
	{BALL_1_DRAW, -400, -300, 1.0},
	{BICYCLE_1_DRAW, -400, -300, 1.0},
	{BRAIN_1_DRAW, -400, -300, 1.0},
	{CHESS_1_DRAW, -400, -300, 1.0},
	{HAND_1_DRAW, -400, -300, 1.0},
	{MENORAH_1_DRAW, -400, -300, 1.0},
	{MUSEUM_LOGO_1_DRAW, -400, -300, 1.0},
	{MUSEUM_LOGO_2_DRAW, -400, -300, 1.0},
	{MUSEUM_LOGO_3_DRAW, -400, -300, 1.0},
	{PATTERN_1_DRAW, -400, -300, 1.0},
	{PRESENT_1_DRAW, -400, -300, 1.0},
	{RECYCLE_1_DRAW, -400, -300, 1.0},
	{TEXT_1_DRAW, -400, -300, 1.0},
	{TRISKELITON_1_DRAW, -400, -300, 1.0},
	{YIN_YANG_1_DRAW, -400, -300, 1.0},
};

int16_t current_position_x = 0;
int16_t current_position_y = 0;
int16_t draw_x = 0;
int16_t draw_y = 0;
double draw_scale = 2.0;

uint8_t lamp_fade_index = 0;
uint32_t lamp_fade_ms = 0;


/* logs prints the given position as point */
void print_point(int16_t x, int16_t y) {
	Serial.print('(');
	Serial.print(x);
	Serial.print(", ");
	Serial.print(y);
	Serial.println(')');
}

/* set the button LED to the given brightness */
void set_lamp(uint8_t brightness) {
	analogWrite(LIGHT_PIN, brightness);
}

#ifdef USE_OLD_PROJECTOR_PINOUTS
/* turn lasers on or off (pull mosfets sorce down to turn the laser on) */
void set_lasers(uint8_t mask) {
	mask &= LASERS_MASK;
	DDRB &= ~LASERS_MASK ^ mask;
	DDRB |= mask;
}

/* turn blue laser on or off */
void set_laser(bool is_on) {
	set_lasers(is_on ? LASER_BLUE_MASK : 0);
	Serial.print(F("set laser: "));
	Serial.println(is_on);
}
#else
/* turn laser on or off (pull npn base up to turn the laser on) */
void set_laser(bool is_on) {
	if (is_on)
		PORTB |= LASER_MASK;
	else
		PORTB &= ~LASER_MASK;
}
#endif

/* writes current motors position to the eeprom and log */
void write_current_position_to_eeprom() {
	eeprom_record_t eeprom_record;
	// EEPROM.write(EEPROM_RECORD_ADDRESS, EEPROM_STATUS_INVALID);
	Serial.print(F("set record to: "));
	print_point(current_position_x, current_position_y);
	eeprom_record.x = current_position_x;
	eeprom_record.y = current_position_y;
	eeprom_record.status = EEPROM_STATUS_VALID;
	EEPROM.put(EEPROM_RECORD_ADDRESS, eeprom_record);
}

/* the position save macanisem- must be called repeatedly */
void test_power() {
	if (!(PINB & POWER_MASK)) {
		set_lamp(0);
		Serial.println(F("no power, write current position to eeprom!"));
		write_current_position_to_eeprom();
		disable_axes();
		set_laser(0);
		set_lamp(255);
		Serial.println(F("wait for power up..."));
		while (!(PINB & POWER_MASK));
		Serial.println(F("power returned, calling to setup"));
		setup();
	}
}

/* delay that keeps the button LED fade and position safe */
void busy_delay(uint32_t ms) {
	lamp_fade_ms += ms;
	ms += millis();
	if (lamp_fade_ms > LAMP_FADE_MS) {
		lamp_fade_ms = 0;
		if (++lamp_fade_index >= sizeof(LAMP_FADE_PWM))
			lamp_fade_index = 0;
		set_lamp(LAMP_FADE_PWM[lamp_fade_index]);
	}
	do {
		test_power();
	} while (millis() < ms);
}

/* sets the motors positions to match "current_position" global
    must call with each change of 1 step in "current_position" global */
void step_to_current_position() {
	Serial.print(F("current position: "));
	print_point(current_position_x, current_position_y);
	PORTC = (PORTC & 0xF0) | STEPS_MASKS[current_position_y & 7];
	PORTD = (PORTD & 0x0F) | (STEPS_MASKS[current_position_x & 7] << 4);
	busy_delay(STEPS_DELAY_MS);
}

/* cuts the power to the motors (they will hold thier positions) 
	must call after done steping the motors with "step_to_current_position" */
void disable_axes() {
	PORTC &= 0xF0;
	PORTD &= 0x0F;
}

/* add/inc the motors current position by the given steps */
void relative_steps(int16_t x, int16_t y) {
	int32_t steps, err, e;
	int16_t delta_x = abs(x);
	int16_t delta_y = abs(y);
	int16_t step_x = (x > 0 ? 1 : (x < 0 ? -1 : 0));
	int16_t step_y = (y > 0 ? 1 : (y < 0 ? -1 : 0));

	test_power();

	steps = max(delta_x, delta_y);
	err = (delta_x > delta_y ? delta_x : -delta_y) / 2;
	while (steps--) {
		e = err;
		if (e >= -delta_x) {
			err -= delta_y;
			current_position_x += step_x;
		}
		if (e < delta_y) {
			err += delta_x;
			current_position_y += step_y;
		}
		step_to_current_position();
	}
	disable_axes();
}

/* go to the given axes position from current motors position */
bool absolute_steps(int16_t x, int16_t y) {
	if (x < X_AXIS_LIMIT_MIN) return true;
	if (x > X_AXIS_LIMIT_MAX) return true;
	if (y < Y_AXIS_LIMIT_MIN) return true;
	if (y > Y_AXIS_LIMIT_MAX) return true;
	relative_steps(x - current_position_x, y - current_position_y);
	return false;
}

/* set the current laser position to (0, 0) */
void set_home() {
	current_position_x = 0;
	current_position_y = 0;
	step_to_current_position();
	disable_axes();
}

/* turn laser off and put projector in home position */
void go_home() {
	Serial.println(F("going home"));
	set_laser(false);
	absolute_steps(0, 0);
}

/* moves motors to the given position */
void go_to(int16_t x, int16_t y) {
	if (absolute_steps(round(x * draw_scale) + draw_x, round(y * draw_scale) + draw_y)) {
		Serial.println(F("point out of range!"));
		set_laser(false);
	}
}

/* moves motors in linear path */
void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
	go_to(x0, y0);
	go_to(x1, y1);
}

/* moves motors in arc path */
void draw_arc(int16_t x0, int16_t y0, int16_t radius, int16_t rotation, int16_t arc, int16_t sweep, int16_t x1, int16_t y1) {
	// TODO
}

/* moves motors in bezier path */
void draw_quadratic_bezier(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
	go_to(x0, y0);
	for (uint8_t i = 0; i <= BEZIER_SEGMENTS; i++) {
		double t = (double)i / (double)BEZIER_SEGMENTS;
		double a = pow((1.0 - t), 2.0);
		double b = 2.0 * t * (1.0 - t);
		double c = pow(t, 2.0);
		double x = a * x0 + b * x1 + c * x2;
		double y = a * y0 + b * y1 + c * y2;
		go_to(x, y);
	}
}

/* moves motors in bezier path */
void draw_cubic_bezier(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3) {
	go_to(x0, y0);
	for (uint8_t i = 0; i <= BEZIER_SEGMENTS; i++) {
		double t = (double)i / (double)BEZIER_SEGMENTS;
		double a = pow((1.0 - t), 3.0);
		double b = 3.0 * t * pow((1.0 - t), 2.0);
		double c = 3.0 * pow(t, 2.0) * (1.0 - t);
		double d = pow(t, 3.0);
		double x = a * x0 + b * x1 + c * x2 + d * x3;
		double y = a * y0 + b * y1 + c * y2 + d * y3;
		go_to(x, y);
	}
}

/* default positions for the drawings */
#define DEFAULT_X_POS (-100)
#define DEFAULT_Y_POS (600)
#define DEFAULT_SCALE (1.0)

/* draw the given draw from PROGMEM in the given x, y, and scale */
void draw_path(PGM_P path, int16_t x, int16_t y, double scale, bool only_start_point) {
	uint8_t i;
	uint8_t command;
	uint8_t arguments_count;
	int16_t current_x, current_y;
	int16_t control_x, control_y;
	int16_t start_x, start_y;
	int16_t arguments[6];

	draw_x = x + DEFAULT_X_POS;
	draw_y = y + DEFAULT_Y_POS;
	draw_scale = scale * DEFAULT_SCALE;

	while (true) {
		command = pgm_read_byte(path++);
		for (i = 0; i < sizeof(PATH_COMMAND_ARGUMENTS); i += 2) {
			if (command == PATH_COMMAND_ARGUMENTS[i]) {
				arguments_count = PATH_COMMAND_ARGUMENTS[i + 1];
				break;
			}
		}
		if (i == sizeof(PATH_COMMAND_ARGUMENTS)) {
			Serial.println(F("bad command!"));
			goto finally;
		}
		arguments_count *= sizeof(*arguments);
		memcpy_P(arguments, path, arguments_count);
		path += arguments_count;

		switch (command) {
			case 'M':
				set_laser(false);
				start_x = arguments[0];
				start_y = arguments[1];
				current_x = start_x;
				current_y = start_y;
				go_to(current_x, current_y);
				if (only_start_point)
					return;
				set_laser(true);
			break;
			case 'H':
				go_to(arguments[0], current_y);
				current_x = arguments[0];
			break;
			case 'V':
				go_to(current_x, arguments[0]);
				current_y = arguments[0];
			break;
			case 'L':
				go_to(arguments[0], arguments[1]);
				current_x = arguments[0];
				current_y = arguments[1];
			break;
			case 'Z':
				go_to(start_x, start_y);
				current_x = start_x;
				current_y = start_y;
			break;
			case 'Q':
				draw_quadratic_bezier(current_x, current_y, arguments[0], arguments[1], arguments[2], arguments[3]);
				control_x = arguments[0];
				control_y = arguments[1];
				current_x = arguments[2];
				current_y = arguments[3];
			break;
			case 'T':
				control_x += control_x - current_x;
				control_y += control_y - current_y;
				draw_quadratic_bezier(current_x, current_y, control_x, control_y, arguments[0], arguments[1]);
				current_x = arguments[0];
				current_y = arguments[1];
			break;
			case 'C':
				draw_cubic_bezier(current_x, current_y, arguments[0], arguments[1], arguments[2], arguments[3], arguments[4], arguments[5]);
				control_x = arguments[2];
				control_y = arguments[3];
				current_x = arguments[4];
				current_y = arguments[5];
			break;
			case 'S':
				control_x += control_x - current_x;
				control_y += control_y - current_y;
				draw_cubic_bezier(current_x, current_y, control_x, control_y, arguments[0], arguments[1], arguments[2], arguments[3]);
				current_x = arguments[2];
				current_y = arguments[3];
			break;
			case 'A':
				//TODO: implement arc!
				go_to(arguments[3], arguments[4]);
			break;
			case 'E':
				Serial.println(F("path end!"));
				goto finally;
			break;
		}
	}
	
finally:
	set_laser(false);
}

/* draw the path at the given index from the global "draws" array */
bool draw_next_path(uint8_t index, bool only_start_point) {
	if (index >= (sizeof(draws) / sizeof(*draws)))
		return true;
	draw_image_t draw_image = draws[index];
	draw_path(draw_image.path, draw_image.x, draw_image.y, draw_image.scale, only_start_point);
	return false;
}

/* called once at power-on */
void setup() {
	eeprom_record_t eeprom_record;
	Serial.begin(SERIAL_BAUDRATE);
	Serial.println(F("\nstart!"));

	DDRD |= 0xF0;						// (4 - 7) OUTPUTS
	DDRC |= 0x0F;						// (A0 - A3) OUTPUTS
	DDRB |= LASER_MASK | LIGHT_MASK;	// (8) OUTPUT
	pinMode(BUTTON_PIN, INPUT_PULLUP);

	set_laser(false);
	/* set lamp on to indicate code started */
	set_lamp(255); delay(1000);

	/* calibrate position if needed */
	EEPROM.get(EEPROM_RECORD_ADDRESS, eeprom_record);
	switch (eeprom_record.status) {
		case EEPROM_STATUS_VALID:
			Serial.print(F("valid eeprom record: "));
			print_point(eeprom_record.x, eeprom_record.y);
			current_position_x = eeprom_record.x;
			current_position_y = eeprom_record.y;
			go_home();
			write_current_position_to_eeprom();
		break;
		default:
			Serial.println(F("first time eeprom read!"));
			write_current_position_to_eeprom();
		break;
	}
	
	/* Move to the first path's starting coordinates */
	draw_next_path(0, true);

	/* uncommant for calibration */
	// relative_steps(40, 0); set_home();
}

/* called repeatedly after "setup" */
void loop() {
	static uint8_t index = 0;

	set_lamp(0);
	/* wait for button press */
	while(PINB & BUTTON_MASK);
	/* draw path and increment index */
	draw_next_path(index++, false);
	/* go to next draw starting point */
	while (draw_next_path(index, true)) index = 0;
	/* save current position so arduino can be reprogramed/reset without re-calibrating */
	write_current_position_to_eeprom();
	/* set lamp on to indicate movement completed */
	set_lamp(255); delay(1000);
	/* wait for button release */
	while(!(PINB & BUTTON_MASK));
}
