#include <math.h>
#include "gen/svgs.h"


#define X_AXIS_LIMIT_MIN			(-1200)
#define Y_AXIS_LIMIT_MIN			(-800)
#define X_AXIS_LIMIT_MAX			(1200)
#define Y_AXIS_LIMIT_MAX			(600)

#define SERIAL_BAUDRATE				(115200)
#define SERIAL_BUFFER_SIZE			(60)
#define STEPS_DELAY_MS				(4)
#define BEZIER_SEGMENTS				(30)
// #define STEPS_PER_RADIAN			(648.68)
// #define Z_SQARE						(10000) // 19700

#define X_AXIS_A_PIN				(4)
#define X_AXIS_B_PIN				(5)
#define X_AXIS_C_PIN				(6)
#define X_AXIS_D_PIN				(7)
#define LASER_RED_PIN				(8)
#define LASER_BLUE_PIN				(9)
#define Y_AXIS_A_PIN				(A0)
#define Y_AXIS_B_PIN				(A1)
#define Y_AXIS_C_PIN				(A2)
#define Y_AXIS_D_PIN				(A3)

#define _PINB_MASK(pin)				(1 << (pin - 8))
#define LASER_BLUE_MASK				(_PINB_MASK(LASER_BLUE_PIN))
#define LASER_RED_MASK				(_PINB_MASK(LASER_RED_PIN))
#define LASERS_MASK					(LASER_BLUE_MASK | LASER_RED_MASK)


// const uint8_t STEPS_MASKS[] = {0b0001, 0b0011, 0b0010, 0b0110, 0b0100, 0b1100, 0b1000, 0b1001};
const uint8_t STEPS_MASKS[] = {0b1001, 0b1000, 0b1100, 0b0100, 0b0110, 0b0010, 0b0011, 0b0001};
// const uint8_t PATH_COMMAND_ARGUMENTS[] = {2,1,1,2,0,4,2,6,4,7,2,1,1,2,0,4,2,6,4,7,0};
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

int16_t current_position_x = 0;
int16_t current_position_y = 0;
// uint8_t steps_delay_ms = STEPS_DELAY_MS;
// bool text_right_diraction = false;
// uint8_t text_char_sep = 10;
int16_t draw_x = 0;
int16_t draw_y = 0;
double draw_scale = 2.0;


void print_point(int16_t x, int16_t y) {
	Serial.print('(');
	Serial.print(x);
	Serial.print(", ");
	Serial.print(y);
	Serial.println(')');
}

/* turn lasers on or off (pull mosfets sorce down to turn the laser on) */
void set_lasers(uint8_t mask) {
	mask &= LASERS_MASK;
	DDRB &= ~LASERS_MASK ^ mask;
	DDRB |= mask;
}

/* turn laser on or off (pull npn base up to turn the laser on) */
void set_laser(bool is_on) {
	set_lasers(is_on ? LASER_BLUE_MASK : 0);
}

/* sets the motors positions to match "current_position" global
    must call with each change of 1 step in "current_position" global */
void step_to_current_position() {
	Serial.print(F("current position: "));
	print_point(current_position_x, current_position_y);
	PORTC = (PORTC & 0xF0) | STEPS_MASKS[current_position_x & 7];
	PORTD = (PORTD & 0x0F) | (STEPS_MASKS[current_position_y & 7] << 4);
	delay(STEPS_DELAY_MS);
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

/*bool absolute_point(int16_t x, int16_t y) {
	int16_t x_steps = math.sqrt(x * x + y * y);
	int16_t y_steps = math.sqrt(x * x + y * y);
	return absolute_steps();
}*/

/* set the current laser position to (0, 0) for trigo to work */
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

void go_to(int16_t x, int16_t y) {
	if (absolute_steps(round(x * draw_scale) + draw_x, round(y * draw_scale) + draw_y)) {
		Serial.println(F("point out of range!"));
		set_laser(false);
	}
	// absolute_point(x * draw_scale + draw_x, y * draw_scale + draw_y);
}

void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
	go_to(x0, y0);
	set_laser(true);
	go_to(x1, y1);
	set_laser(false);
}

void draw_arc(int16_t x0, int16_t y0, int16_t radius, int16_t rotation, int16_t arc, int16_t sweep, int16_t x1, int16_t y1) {
	// TODO
}

void draw_quadratic_bezier(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
	go_to(x0, y0);
	set_laser(true);
	for (uint8_t i = 0; i <= BEZIER_SEGMENTS; i++) {
		double t = (double)i / (double)BEZIER_SEGMENTS;
		double a = pow((1.0 - t), 2.0);
		double b = 2.0 * t * (1.0 - t);
		double c = pow(t, 2.0);
		double x = a * x0 + b * x1 + c * x2;
		double y = a * y0 + b * y1 + c * y2;
		go_to(x, y);
	}
	set_laser(false);
}

void draw_cubic_bezier(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3) {
	go_to(x0, y0);
	set_laser(true);
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
	set_laser(false);
}

void draw_path(PGM_P path, int16_t x, int16_t y, double scale) {
	uint8_t i;
	uint8_t command;
	uint8_t arguments_count;
	int16_t current_x, current_y;
	int16_t control_x, control_y;
	int16_t start_x, start_y;
	int16_t arguments[6];

	draw_x = x;
	draw_y = y;
	draw_scale = scale;

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
			return;
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
			break;
			case 'H':
				set_laser(true);
				go_to(arguments[0], current_y);
				current_x = arguments[0];
			break;
			case 'V':
				set_laser(true);
				go_to(current_x, arguments[0]);
				current_y = arguments[0];
			break;
			case 'L':
				set_laser(true);
				go_to(arguments[0], arguments[1]);
				current_x = arguments[0];
				current_y = arguments[1];
			break;
			case 'Z':
				set_laser(true);
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
				set_laser(true);
				go_to(arguments[3], arguments[4]);
			break;
			case 'E':
				set_laser(false);
				Serial.println(F("path end!"));
				return;
			break;
		}
	}
}

/* called once at power-on */
void setup() {
	Serial.begin(SERIAL_BAUDRATE);
	Serial.println(F("\nstart!"));

	DDRD |= 0xF0;				// (4 - 7) OUTPUTS
	DDRC |= 0x0F;				// (A0 - A3) OUTPUTS
	PORTB &= ~LASERS_MASK;		// (8, 9) LOW for laser outputs
	set_lasers(0);				// turn off lasers
	// delay(1000);
	// relative_steps(-100, -100); set_home();
	// set_laser(true); delay(100); set_laser(false);

	draw_path(APPLE_1_DRAW, -400, -300, 1.0);
	draw_path(BALL_1_DRAW, -400, -300, 1.0);
	draw_path(BICYCLE_1_DRAW, -400, -300, 1.0);
	draw_path(BRAIN_1_DRAW, -400, -300, 1.0);
	draw_path(CHESS_1_DRAW, -400, -300, 1.0);
	draw_path(HAND_1_DRAW, -400, -300, 1.0);
	draw_path(MENORAH_1_DRAW, -400, -300, 1.0);
	draw_path(MUSEUM_LOGO_1_DRAW, -400, -300, 1.0);
	draw_path(MUSEUM_LOGO_2_DRAW, -400, -300, 1.0);
	draw_path(MUSEUM_LOGO_3_DRAW, -400, -300, 1.0);
	draw_path(PATTERN_1_DRAW, -400, -300, 1.0);
	draw_path(PRESENT_1_DRAW, -400, -300, 1.0);
	draw_path(RECYCLE_1_DRAW, -400, -300, 1.0);
	draw_path(TEXT_1_DRAW, -400, -300, 1.0);
	draw_path(TRISKELITON_1_DRAW, -400, -300, 1.0);
	draw_path(YIN_YANG_1_DRAW, -400, -300, 1.0);
	go_home();
}

/* called repeatedly after "setup" */
void loop() {

}

/* draw regular polygon x, y are the center point with absolute_steps */
/*void draw_polygon(xy_point * p, uint8_t corners, int16_t size, int16_t start_angle) {
	xy_point tmp;
	double current_angle = PI * start_angle / 180.0;
	double step_angle = PI * 2.0 / corners;
	corners++;
	set_laser(true);
	while (corners) {
		tmp.x = x + round(size * cos(current_angle));
		tmp.y = y + round(size * sin(current_angle));
		absolute_steps(&tmp);
		current_angle += step_angle;
		corners--;
	}
	set_laser(false);
	disable_axes();
}*/

/*void get_path_size(char * path, xy_point * p) {
	
}*/

/*void get_text_size(char * text, xy_point * p) {
	xy_point tmp;
	x = (*text ? -text_char_sep : 0);
	y = 0;
	while (*text) {
		get_path_size(char_to_path(*text++), &tmp);
		x += tmx + text_char_sep;
		if (y < tmy)
			y = tmy;
	}
}*/

/*void draw_path(char * path, xy_point * p) {
	
}*/

/* draw a NULL terminated string */
/*void draw_text(char * text, xy_point * p) {
	xy_point tmp;
	
	if (text_right_diraction) {
		get_text_size(text, &tmp);
	}
	
	draw_path
	while (*text)
		x += draw_char(x, y, *text++, scale) + 1;
}*/

/*
void draw_start(int16_t x0, int16_t y0) {
	go_to(x0, y0);
	draw_last_x = x0;
	draw_last_y = y0;
}

void draw_line(int16_t x1, int16_t y1) {
	draw_line(draw_last_x, draw_last_y, x1, y1);
	draw_last_x = x1;
	draw_last_y = y1;
}

void draw_quadratic_bezier(int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
	draw_quadratic_bezier(draw_last_x, draw_last_y,  x1,  y1,  x2,  y2);
	draw_last_x = x2;
	draw_last_y = y2;
}

void draw_cubic_bezier(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3) {
	draw_cubic_bezier(draw_last_x, draw_last_y,  x1,  y1,  x2,  y2,  x3,  y3);
	draw_last_x = x3;
	draw_last_y = y3;
}
*/

/*void test() {
	for (uint16_t i = 0; i < 1010*2; i++)
		relative_steps(1, 0);
	delay(3000);
	go_home();
	for (uint16_t i = 0; i < 1010; i++)
		relative_steps(-1, 0);
	delay(3000);
	go_home();

	for (uint16_t i = 0; i < 1700; i++)
		relative_steps(0, 1);
	delay(3000);
	go_home();
	for (uint16_t i = 0; i < 1700; i++)
		relative_steps(0, -1);
	delay(3000);
	go_home();
}*/

/* convert absolute point to absolute axes steps */
/*void absolute_point(int32_t x, int32_t y) {
	double n = sqrt(x * x + y * y + Z_SQARE);
	double a = asin(y / n);
	absolute_steps(round(STEPS_PER_RADIAN * asin(x / n / cos(a))), round(STEPS_PER_RADIAN * a));
}*/
