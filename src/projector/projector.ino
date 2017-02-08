/*
 * MIT License
 * Copyright Arad Eizen and Yossi Zapesochini 2016.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
 
/* 
 * Arduino implementation of the Spherical Laser Projector.
 */
 
/* Uncomment the following line to compile the code for the 2D version of the projector */
// #define USE_2D_PROJECTOR_PINOUTS

#include <EEPROM.h>
#include "gen/svgs.h"

#ifdef USE_2D_PROJECTOR_PINOUTS
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

#define LAMP_FADE_MS				(50)

#define EEPROM_RECORD_ADDRESS		(0)
#define EEPROM_STATUS_VALID			(0x55)

#define X_AXIS_A_PIN				(4)
#define X_AXIS_B_PIN				(5)
#define X_AXIS_C_PIN				(6)
#define X_AXIS_D_PIN				(7)
#ifdef USE_2D_PROJECTOR_PINOUTS
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
#ifdef USE_2D_PROJECTOR_PINOUTS
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


/* Default positions for the drawings */
#define DEFAULT_X_POS (-100)
#define DEFAULT_Y_POS (600)
#define DEFAULT_SCALE (1.0)

const draw_image_t g_drawings[] = {
	{MUSEUM_LOGO_1_DRAW, 0, 0, 1.1},
	{MUSEUM_LOGO_2_DRAW, 440, 0, 1.1},
	{MUSEUM_LOGO_3_DRAW, 220, 330, 1.1},
	{ANDROID_1_DRAW, 0, 0, 1.1},
	{EAGLE_1_DRAW, 0, 0, 1.1},
	{BUTTERFLY_1_DRAW, 0, 0, 1.1},
	{FLOWER_1_DRAW, 0, 0, 1.2},
	{APPLE_1_DRAW, 0, 0, 1.1},
	{BALL_1_DRAW, 0, 0, 1},
	{BICYCLE_1_DRAW, 0, 0, 1},
	{BRAIN_1_DRAW, 0, 0, 1.1},
	{CHESS_1_DRAW, 0, 0, 1.1},
	{MENORAH_1_DRAW, 0, 0, 1.1},
	{PATTERN_1_DRAW, 0, 0, 1.2},
	{PRESENT_1_DRAW, 0, 0, 0.9},
	{RECYCLE_1_DRAW, 0, 0, 1.1},
	{TEXT_1_DRAW, 0, 0, 1.1},
	{TRISKELITON_1_DRAW, 0, 0, 1.1},
	{YIN_YANG_1_DRAW, 0, 0, 1.1},
};

int16_t g_current_position_x = 0;
int16_t g_current_position_y = 0;
int16_t g_draw_x = 0;
int16_t g_draw_y = 0;
double g_drawing_scale = 2.0;

uint8_t g_lamp_fade_index = 0;
uint32_t g_lamp_fade_ms = 0;


/* Log the given position as point to the serial interface */
void print_point(int16_t x, int16_t y) {
	Serial.print('(');
	Serial.print(x);
	Serial.print(", ");
	Serial.print(y);
	Serial.println(')');
}

/* Set the button LED to the given brightness */
void set_lamp(uint8_t brightness) {
	analogWrite(LIGHT_PIN, brightness);
}

#ifdef USE_2D_PROJECTOR_PINOUTS
/* Turn lasers on or off (pull MOSFET source down to turn the laser on) */
void set_lasers(uint8_t mask) {
	mask &= LASERS_MASK;
	DDRB &= ~LASERS_MASK ^ mask;
	DDRB |= mask;
}

/* Turn the blue laser on or off */
void set_laser(bool is_on) {
	set_lasers(is_on ? LASER_BLUE_MASK : 0);
	Serial.print(F("set laser: "));
	Serial.println(is_on);
}
#else
/* Turn laser on or off (pull NPN base up to turn the laser on) */
void set_laser(bool is_on) {
	if (is_on)
		PORTB |= LASER_MASK;
	else
		PORTB &= ~LASER_MASK;
}
#endif

/* Write the motors' current position to the EEPROM and serial log */
void write_current_position_to_eeprom() {
	eeprom_record_t eeprom_record;
    
	// EEPROM.write(EEPROM_RECORD_ADDRESS, EEPROM_STATUS_INVALID);
	Serial.print(F("set record to: "));
	print_point(g_current_position_x, g_current_position_y);
	eeprom_record.x = g_current_position_x;
	eeprom_record.y = g_current_position_y;
	eeprom_record.status = EEPROM_STATUS_VALID;
	EEPROM.put(EEPROM_RECORD_ADDRESS, eeprom_record);
}

/* Position save mechanism. Must be called repeatedly */
void test_power() {
	if (!(PINB & POWER_MASK)) {
		set_lamp(0);
		Serial.println(F("No power. Saving the current position in EEPROM!"));
		write_current_position_to_eeprom();
		disable_axes();
		set_laser(0);
		set_lamp(255);
        
		Serial.println(F("Waiting for power up..."));
		while (!(PINB & POWER_MASK));
        
		Serial.println(F("Power returned, calling setup"));
		setup();
	}
}

/* Delay that keeps the button LED fade and position safe */
void busy_delay(uint32_t ms) {
	ms += millis();
    
	do {
		test_power();
	} while (millis() < ms);
}

/* Lamp fading effect */
void lamp_fade() {
	if ((millis() - g_lamp_fade_ms) > LAMP_FADE_MS) {
		g_lamp_fade_ms = millis();
        
		if (++g_lamp_fade_index >= sizeof(LAMP_FADE_PWM))
			g_lamp_fade_index = 0;
            
		set_lamp(LAMP_FADE_PWM[g_lamp_fade_index]);
	}
}

/* Sets the motors' positions to match g_current_position.
    The function must be called with each step of g_current_position */
void step_to_current_position() {
	Serial.print(F("Current position: "));
	print_point(g_current_position_x, g_current_position_y);
	PORTC = (PORTC & 0xF0) | STEPS_MASKS[g_current_position_y & 7];
	PORTD = (PORTD & 0x0F) | (STEPS_MASKS[g_current_position_x & 7] << 4);
	busy_delay(STEPS_DELAY_MS);
}

/* Cuts the power from the motors(they will hold their positions).
	Must be called after done moving the motors with "step_to_current_position" */
void disable_axes() {
	PORTC &= 0xF0;
	PORTD &= 0x0F;
}

/* Increase/decrease the motors' current position by the given steps */
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
			g_current_position_x += step_x;
		}
		if (e < delta_y) {
			err += delta_x;
			g_current_position_y += step_y;
		}
        
		step_to_current_position();
	}
	disable_axes();
}

/* Go to the given axes position from current motors position */
bool absolute_steps(int16_t x, int16_t y) {
	if (x < X_AXIS_LIMIT_MIN) return true;
	if (x > X_AXIS_LIMIT_MAX) return true;
	if (y < Y_AXIS_LIMIT_MIN) return true;
	if (y > Y_AXIS_LIMIT_MAX) return true;
    
	relative_steps(x - g_current_position_x, y - g_current_position_y);
    
	return false;
}

/* Set the current laser position to (0, 0) */
void set_home() {
	g_current_position_x = 0;
	g_current_position_y = 0;
    
	step_to_current_position();
    
	disable_axes();
}

/* Turn the laser off and put the projector in the home position */
void go_home() {
	Serial.println(F("Returning to the home position"));
	set_laser(false);
	absolute_steps(0, 0);
}

/* Moves the motors to the given position */
void go_to(int16_t x, int16_t y) {
	if (absolute_steps(round(x * g_drawing_scale) + g_draw_x, round(y * g_drawing_scale) + g_draw_y)) {
		Serial.println(F("Point out of range!"));
		set_laser(false);
	}
}

/* Moves the motors in a linear path */
void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
	go_to(x0, y0);
	go_to(x1, y1);
}

/* Moves the motors in an arced path */
void draw_arc(int16_t x0, int16_t y0, int16_t radius, int16_t rotation, int16_t arc, int16_t sweep, int16_t x1, int16_t y1) {
	// TODO
}

/* Moves the motors in a bezier path */
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

/* Moves the motors in a bezier path */
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

/* Draw the given drawing from PROGMEM in the given x, y, and scale */
void draw_path(PGM_P path, int16_t x, int16_t y, double scale, bool only_start_point) {
	uint8_t i;
	uint8_t command;
	uint8_t arguments_count;
	int16_t current_x, current_y;
	int16_t control_x, control_y;
	int16_t start_x, start_y;
	int16_t arguments[6];

	g_draw_x = x + DEFAULT_X_POS;
	g_draw_y = y + DEFAULT_Y_POS;
	g_drawing_scale = scale * DEFAULT_SCALE;

     /* Parse and draw the SVG instructions until 'E' is reached */
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

/* Draw the path at the given index from the g_drawings array */
bool draw_next_path(uint8_t index, bool only_start_point) {
	if (index >= (sizeof(g_drawings) / sizeof(*g_drawings)))
		return true;
        
	draw_image_t draw_image = g_drawings[index];
	draw_path(draw_image.path, draw_image.x, draw_image.y, draw_image.scale, only_start_point);
    
	return false;
}

void manual_mode() {
	char buffer[SERIAL_BUFFER_SIZE] = {0};
	char text[SERIAL_BUFFER_SIZE] = {0};
	char * ptr = buffer + 2;
	int16_t i, x, y;
	
	while (true) {
		uint8_t len = Serial.readBytesUntil('\n', buffer, SERIAL_BUFFER_SIZE - 1);
		if (0 == len) return;
		buffer[len] = 0;
		Serial.flush();
	
		switch (buffer[0]) {
			case '?':
				Serial.println(F(
					"Enter command:\n"
					"set home:            h\n"
					"set laser (ie: l 1): l <is_on>\n"
					"go (ie: g 10 10):    g <x> <y>\n"
					"draw (ie: d 2):      d <index>\n"
					"print this help:     ?\n"
				));
			break;
			case 'h':
				set_home();
				Serial.println(F("Home"));
			break;
			case 'l':
				sscanf(ptr, "%d", &i);
				set_laser(i);
			break;
			case 'g':
				x = y = 0;
				sscanf(ptr, "%d %d", &x, &y);
				Serial.print(F("Go: ("));
				Serial.print(x);
				Serial.print(F(", "));
				Serial.print(y);
				Serial.print(F(")\n"));
				relative_steps(x, y);
			case 'd':
				sscanf(ptr, "%d", &i);
				draw_next_path(i, false);
			break;
		}
	}
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
    
    /* Turn the lamp on to indicate that the setup is done */
	set_lamp(255); delay(1000);
	
	/* Manual mode (never returns) if the button is pressed for 3 seconds at startup */
	for (uint8_t i = 0; !(PINB & BUTTON_MASK); i++) {
		set_lamp(0); delay(500);
		set_lamp(255); delay(500);
		if (i > 2)
			manual_mode();
	}

    /* Uncomment to adjust the projector's starting position on reset(used for calibration) */
	//relative_steps(10, 0); set_home(); while(true);

	/* Calibrate the projector's position(if it's needed) */
	EEPROM.get(EEPROM_RECORD_ADDRESS, eeprom_record);
	switch (eeprom_record.status) {
		case EEPROM_STATUS_VALID:
			Serial.print(F("Valid eeprom record: "));
			print_point(eeprom_record.x, eeprom_record.y);
			g_current_position_x = eeprom_record.x;
			g_current_position_y = eeprom_record.y;
			go_home();
			write_current_position_to_eeprom();
		break;
		default:
			Serial.println(F("First time EEPROM read!"));
			write_current_position_to_eeprom();
		break;
	}
	
	/* Move to the first path's starting coordinates */
	draw_next_path(0, true);
}

/* The program's main loop - called after setup. */
void loop() {
	static uint8_t index = 0;
    
	/* Wait for a button press, turn on the LED's flickering meanwhile */
	while(PINB & BUTTON_MASK)
        lamp_fade();
    
    /* Turn the lamp off to indicate that the drawing has started */
    set_lamp(0);
    
	/* Draw the path and increment the drawing's index */
	draw_next_path(index++, false);
    
	/* Move to the next drawing's starting point */
	while (draw_next_path(index, true)) index = 0;
    
	/* Save the current position so that the arduino will be be able to be reprogrammed/reset without re-calibrating */
	write_current_position_to_eeprom();
    
	/* Wait for a button release */
	while(!(PINB & BUTTON_MASK));
}

/*
משולשים ישרי זווית - כמו מסור
void draw_razor(uint16_t len, uint8_t max_segments, uint8_t max_height)
{
    uint8_t segments = rand() % max_segments;
    uint8_t start_y = g_starting_y;
    for(uint16_t i = 0; i < segments; ++i)
    {
        draw_line(sqrt(pow(len/segments)/2), rand() % max_height);
        draw_line(0, start_y);
    }
}
 
הרבה משולשים שווי צלעות - כמו דשא
void draw_grass(uint16_t len, uint8_t max_segments, uint8_t max_height)
{
    uint8_t segments = rand() % max_segments;
    uint8_t start_y = g_starting_y;
    for(uint16_t i = 0; i < segments; ++i)
    {
        draw_line((len/segments)/2, rand() % max_height);
        draw_line((len/segments)/2, start_y);
    }
}
 
מוניטור לב
void draw_heartmonitor(uint16_t len, uint8_t max_segments, uint8_t max_height)
{
    uint8_t segments = rand() % max_segments;
    uint8_t start_y = g_starting_y;
    for(uint16_t i = 0; i < segments; ++i)
    {
        ratio = rand() % 10;
        draw_line((len/segments)/ratio, rand() % max_height);
        draw_line((len/segments) - ratio, start_y);
    }
}
*/
