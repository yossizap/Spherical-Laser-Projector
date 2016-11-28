/*********************************** LICENCE **********************************\
| Laser Projector - arduino based blue laser projector software.               |
|                                                                              |
| Hardware:                                                                    |
|     * Two uno-polar step motors for X and Y axes,                            |
|       connected via NPNs to pins 4-7 and A0-A3.                              |
|     * Blue laser and fan, connected via MOSFATS to pins 8,9.                 |
|                                                                              |
| Functions:                                                                   |
|     * relative/absolute steps/point (x, y)                                   |
|     * set lasers mask (lasers mask)                                          |
|     * set speed (speed)                                                      |
|     * set distance (distance)                                                |
|     * set home                                                               |
|     * draw polygon (x, y, cornners, [size, start angle])                     |
|     * draw text (x, y, text, [size])                                         |
|                                                                              |
| Protocol:                                                                    |
|     * use textual single line protocol ending with \n and binary protocol!   |
|     * binary starts with barker and then like textual...                     |
|     * max length 60 bytes                                                    |
|     * text: no spaces - for two words need two commands                      |
|     * listen for serial in loop,                                             |
|     * when full command recived - execut                                     |
|     * while executing, check buffer size, if not 0, return                   |
|                                                                              |
| By A.E.TEC (Arad Eizen) 2016.                                                |
|	                                                                           |
\******************************************************************************/
#include <math.h>					// asin
#include "paths/cloud.h"
// #include "paths/skull.h"


#define CORNER_DEG					(1010)
#define X_AXIS_LIMIT_MIN			(-CORNER_DEG)
#define Y_AXIS_LIMIT_MIN			(-1500)
#define X_AXIS_LIMIT_MAX			(CORNER_DEG * 2)
#define Y_AXIS_LIMIT_MAX			(1500)

#define SERIAL_BAUDRATE				(115200)
#define SERIAL_BUFFER_SIZE			(60)
#define STEPS_DELAY_MS				(4)
#define BEZIER_SEGMENTS				(30)
#define STEPS_PER_RADIAN			(648.68)
#define Z_SQARE						(10000) // 19600

#define X_AXIS_A_PIN				(4)
#define X_AXIS_B_PIN				(5)
#define X_AXIS_C_PIN				(6)
#define X_AXIS_D_PIN				(7)
#define LASER_PIN					(8)
#define BUTTON_PIN					(12)
#define Y_AXIS_A_PIN				(A0)
#define Y_AXIS_B_PIN				(A1)
#define Y_AXIS_C_PIN				(A2)
#define Y_AXIS_D_PIN				(A3)

#define LASER_MASK					(0x01)

// const uint8_t STEPS_MASKS[] = {0b0001, 0b0011, 0b0010, 0b0110, 0b0100, 0b1100, 0b1000, 0b1001};
const uint8_t STEPS_MASKS[] = {0b1001, 0b1000, 0b1100, 0b0100, 0b0110, 0b0010, 0b0011, 0b0001};
const uint8_t PATH_COMMAND_ARGUMENTS[] = {
	'm', 2,
	'h', 1,
	'v', 1,
	'l', 2,
	'z', 0,
	'q', 4,
	't', 2,
	'c', 6,
	's', 4,
	'a', 7,
	'M', 2,
	'H', 1,
	'V', 1,
	'L', 2,
	'Z', 0,
	'Q', 4,
	'T', 2,
	'C', 6,
	'S', 4,
	'A', 7,
	'e', 0,
};

int16_t current_position_x;
int16_t current_position_y;
int16_t draw_last_x;
int16_t draw_last_y;
// bool text_right_diraction = false;
// uint8_t text_char_sep = 10;
int16_t draw_x = 0;
int16_t draw_y = 0;
double draw_scale = 2;


/* turn laser on or off (pull mosfet gate down to turn the laser on) */
void set_laser(bool is_on) {
	if (is_on)
		PORTB |= LASER_MASK;
	else
		PORTB &= ~LASER_MASK;
}

/* cuts the power to the motors (they will hold thier positions) 
	must call after done steping the motors with "step_to_current_position" */
void disable_axes() {
	PORTC &= 0xF0;
	PORTD &= 0x0F;
}

/* sets the motors positions to match "current_position" global
    must call with each change of 1 step in "current_position" global */
void step_to_current_position() {
	PORTC = (PORTC & 0xF0) | STEPS_MASKS[current_position_y & 7];
	PORTD = (PORTD & 0x0F) | (STEPS_MASKS[current_position_x & 7] << 4);
	delay(STEPS_DELAY_MS);
}

void print_point(int16_t x, int16_t y) {
	Serial.print("(");
	Serial.print(x);
	Serial.print(", ");
	Serial.print(y);
	Serial.print(")\n");
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
	print_point(current_position_x, current_position_y);
}

/* go to the given axes position from current motors position */
void absolute_steps(int16_t x, int16_t y) {
	if (x < X_AXIS_LIMIT_MIN) return;
	if (x > X_AXIS_LIMIT_MAX) return;
	if (y < Y_AXIS_LIMIT_MIN) return;
	if (y > Y_AXIS_LIMIT_MAX) return;
	relative_steps(x - current_position_x, y - current_position_y);
}

/* convert absolute point to absolute axes steps */
void absolute_point(int32_t x, int32_t y) {
	double n = sqrt(x * x + y * y + Z_SQARE);
	double a = asin(y / n);
	absolute_steps(round(STEPS_PER_RADIAN * asin(x / n / cos(a))), round(STEPS_PER_RADIAN * a));
}

/* set the current laser position to (0, 0) for trigo to work */
void set_home() {
	current_position_x = 0;
	current_position_y = 0;
	step_to_current_position();
	disable_axes();
}

/* turn laser off and put projector in home position */
void go_home() {
	set_laser(false);
	absolute_steps(0, 0);
}

void go_to(int16_t x, int16_t y) {
	absolute_steps(x * draw_scale + draw_x, y * draw_scale + draw_y);
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
	int16_t pts[BEZIER_SEGMENTS + 1][2];
	for (uint8_t i = 0; i <= BEZIER_SEGMENTS; ++i) {
		double t = (double)i / (double)BEZIER_SEGMENTS;
		double a = pow((1.0 - t), 2.0);
		double b = 2.0 * t * (1.0 - t);
		double c = pow(t, 2.0);
		double x = a * x0 + b * x1 + c * x2;
		double y = a * y0 + b * y1 + c * y2;
		pts[i][0] = x;
		pts[i][1] = y;
	}

	/* draw segments */
	go_to(x0, y0);
	set_laser(true);
	for (uint8_t i = 0; i < BEZIER_SEGMENTS; ++i) {
		draw_line(pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1]);
	}
}

void draw_cubic_bezier(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3) {
	int16_t pts[BEZIER_SEGMENTS + 1][2];
	for (uint8_t i = 0; i <= BEZIER_SEGMENTS; ++i) {
		double t = (double)i / (double)BEZIER_SEGMENTS;
		double a = pow((1.0 - t), 3.0);
		double b = 3.0 * t * pow((1.0 - t), 2.0);
		double c = 3.0 * pow(t, 2.0) * (1.0 - t);
		double d = pow(t, 3.0);
		double x = a * x0 + b * x1 + c * x2 + d * x3;
		double y = a * y0 + b * y1 + c * y2 + d * y3;
		pts[i][0] = x;
		pts[i][1] = y;
	}

	/* draw segments */
	go_to(x0, y0);
	set_laser(true);
	for (uint8_t i = 0; i < BEZIER_SEGMENTS; ++i) {
		draw_line(pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1]);
	}
}

void draw_path(int16_t x, int16_t y, double scale) {
	uint8_t command;
	uint8_t arguments_count;
	uint16_t start_x, start_y, a_x, a_y, b_x, b_y;
	uint16_t m_x = 0, m_y = 0;
	uint16_t arguments[7];

	draw_x = x;
	draw_y = y;
	draw_scale = scale;

	PGM_P path = CLOUD_PATH;

	for (;;) {
		command = pgm_read_byte(path++);
		Serial.print("command: ");
		Serial.println(command);
		arguments_count = -1;
		for (uint8_t i = 0; i < sizeof(PATH_COMMAND_ARGUMENTS); i += 2) {
			Serial.println(PATH_COMMAND_ARGUMENTS[i]);
			if (command == PATH_COMMAND_ARGUMENTS[i]) {
				arguments_count = PATH_COMMAND_ARGUMENTS[i + 1];
				break;
			}
		}
		arguments_count *= sizeof(*arguments);
		Serial.print("arguments_count: ");
		Serial.println(arguments_count);
		if (arguments_count > sizeof(arguments) * sizeof(*arguments)) {
			Serial.println("Command error!");
			return;
		}
		
		memcpy_P(arguments, path, arguments_count);
		path += arguments_count;

		switch (command) {
			case 'm':
			case 'M':
				set_laser(false);
				m_x = arguments[0];
				m_y = arguments[1];
				start_x = m_x;
				start_y = m_y;
			break;
			case 'h':
				arguments[0] += start_x;
			case 'H':
				draw_line(start_x, start_y, arguments[0], start_y);
				start_x = arguments[0];
			break;
			case 'v':
				arguments[0] += start_y;
			case 'V':
				draw_line(start_x, start_y, start_x, arguments[0]);
				start_y = arguments[0];
			break;
			case 'l':
				arguments[0] += start_x;
				arguments[1] += start_y;
			case 'L':
				draw_line(start_x, start_y, arguments[0], arguments[1]);
				start_x = arguments[0];
				start_y = arguments[1];
			break;
			case 'z':
			case 'Z':
				draw_line(start_x, start_y, m_x, m_y);
				start_x = m_x;
				start_y = m_y;
			break;
			case 'q':
				arguments[0] += start_x;
				arguments[1] += start_y;
				arguments[2] += start_x;
				arguments[3] += start_y;
			case 'Q':
				draw_quadratic_bezier(start_x, start_y, arguments[0], arguments[1], arguments[2], arguments[3]);
				a_x = arguments[0];
				a_y = arguments[1];
				start_x = arguments[2];
				start_y = arguments[3];
			break;
			case 't':
				arguments[0] += start_x;
				arguments[1] += start_y;
			case 'T':
				a_x += a_x - start_x;
				a_y += a_y - start_y;
				draw_quadratic_bezier(start_x, start_y, a_x, a_y, arguments[0], arguments[1]);
				start_x = arguments[0];
				start_y = arguments[1];
			break;
			case 'c':
				arguments[0] += start_x;
				arguments[1] += start_y;
				arguments[2] += start_x;
				arguments[3] += start_y;
				arguments[4] += start_x;
				arguments[5] += start_y;
			case 'C':
				draw_cubic_bezier(start_x, start_y, arguments[0], arguments[1], arguments[2], arguments[3], arguments[4], arguments[5]);
				a_x = arguments[2];
				a_y = arguments[3];
				start_x = arguments[4];
				start_y = arguments[5];
			break;
			case 's':
				arguments[0] += start_x;
				arguments[1] += start_y;
				arguments[2] += start_x;
				arguments[3] += start_y;
			case 'S':
				a_x += a_x - start_x;
				a_y += a_y - start_y;
				draw_cubic_bezier(start_x, start_y, a_x, a_y, arguments[0], arguments[1], arguments[2], arguments[3]);
				start_x = arguments[2];
				start_y = arguments[3];
			break;
			case 'a':
			case 'A':
				Serial.println("Arc!");
			break;
			case 'e':
			case 'E':
				return;
			break;
			default:
				Serial.println("Path error!");
				return;
			break;
		}
	}
}

void test() {
	/*for (uint16_t i = 0; i < 1010*2; i++)
		relative_steps(1, 0);
	delay(3000);
	go_home();
	for (uint16_t i = 0; i < 1010; i++)
		relative_steps(-1, 0);
	delay(3000);
	go_home();*/
	/*
	for (uint16_t i = 0; i < 1500; i++)
		relative_steps(0, 1);
	delay(3000);
	go_home();
	for (uint16_t i = 0; i < 1500; i++)
		relative_steps(0, -1);
	delay(3000);
	go_home();*/
}

/* called once at power-on */
void setup() {
	Serial.begin(SERIAL_BAUDRATE);
	DDRD |= 0xF0;				// (4 - 7) OUTPUTS
	DDRC |= 0x0F;				// (A0 - A3) OUTPUTS
	DDRB |= LASER_MASK;			// (8) OUTPUT
	set_laser(false);			// turn off laser
	pinMode(BUTTON_PIN, INPUT_PULLUP);
	set_home();
	// -X is right
	// -Y is left
	// relative_steps(20, 0); set_home();
	// set_laser(true); delay(100); set_laser(false);
	
	// draw_path(0, -180, 3);
	// draw_path(-20, 50, 0.5);
	// draw_cloud();
	// test();
	// draw_skull();
	go_home();
}

/* called repeatedly after "setup" */
void loop() {
	static uint8_t index = 0;
	while(digitalRead(BUTTON_PIN));
	switch (index++) {
		case 0:
			draw_path(0, 400, 3);
		break;
		case 1:
			// draw_path(400, 300, 1.5);
		break;
		default:
			index = 0;
		break;
	}
	go_home();
	delay(1000);
	while(!digitalRead(BUTTON_PIN));
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
