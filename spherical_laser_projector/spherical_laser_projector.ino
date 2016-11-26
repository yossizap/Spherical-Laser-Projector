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
#include "paths/skull.h"

#define X_AXIS_LIMIT_MIN			(-1800)
#define Y_AXIS_LIMIT_MIN			(-1400)
#define X_AXIS_LIMIT_MAX			(700)
#define Y_AXIS_LIMIT_MAX			(1800)

#define SERIAL_BAUDRATE				(115200)
#define SERIAL_BUFFER_SIZE			(60)
#define STEPS_DELAY_MS				(5)
#define BEZIER_SEGMENTS				(30)
#define STEPS_PER_RADIAN			(648.68)
#define Z_SQARE						(10000) // 19600

#define X_AXIS_A_PIN				(4)
#define X_AXIS_B_PIN				(5)
#define X_AXIS_C_PIN				(6)
#define X_AXIS_D_PIN				(7)
#define LASER_PIN					(8)
#define Y_AXIS_A_PIN				(A0)
#define Y_AXIS_B_PIN				(A1)
#define Y_AXIS_C_PIN				(A2)
#define Y_AXIS_D_PIN				(A3)

#define LASER_MASK					(0x01)

// const uint8_t STEPS_MASKS[] = {0b0001, 0b0011, 0b0010, 0b0110, 0b0100, 0b1100, 0b1000, 0b1001};
const uint8_t STEPS_MASKS[] = {0b1001, 0b1000, 0b1100, 0b0100, 0b0110, 0b0010, 0b0011, 0b0001};

int16_t current_position_x;
int16_t current_position_y;
// bool text_right_diraction = false;
// uint8_t text_char_sep = 10;
int16_t draw_x = 0;
int16_t draw_y = 0;
double draw_scale = 2;


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

/* turn laser on or off (pull mosfet gate down to turn the laser on) */
void set_laser(bool is_on) {
	if (is_on)
		PORTB |= LASER_MASK;
	else
		PORTB &= ~LASER_MASK;
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
	// absolute_steps(x * draw_scale + draw_x, y * draw_scale + draw_y);
	absolute_point(x * draw_scale + draw_x, y * draw_scale + draw_y);
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
	for (uint8_t i = 0; i < BEZIER_SEGMENTS; ++i) {
		draw_line(pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1]);
	}
}

void draw_cubic_bezier(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3) {
	go_to(x0, y0);
	set_laser(true);
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
	for (uint8_t i = 0; i < BEZIER_SEGMENTS; ++i) {
		draw_line(pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1]);
	}
}

void draw_path(int16_t x, int16_t y, double scale) {
	draw_x = x;
	draw_y = y;
	draw_scale = scale;
}

/* called once at power-on */
void setup() {
	Serial.begin(SERIAL_BAUDRATE);
	DDRD |= 0xF0;				// (4 - 7) OUTPUTS
	DDRC |= 0x0F;				// (A0 - A3) OUTPUTS
	DDRB |= LASER_MASK;			// (8) OUTPUT
	set_laser(false);			// turn off laser
	set_home();
	// -X is right
	// -Y is left
	// relative_steps(000,-5000); set_home();
	// set_laser(true); delay(100); set_laser(false);
	
	// draw_path(0, -180, 3);
	draw_path(-20, 50, 0.5);
	draw_cloud();
	// draw_skull();
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
