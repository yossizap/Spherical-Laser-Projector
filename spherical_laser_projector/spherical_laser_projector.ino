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
// #include "arial_font.h"

#define X_AXIS_LIMIT_MIN			(-200)
#define Y_AXIS_LIMIT_MIN			(-100)
#define X_AXIS_LIMIT_MAX			(200)
#define Y_AXIS_LIMIT_MAX			(300)

#define SERIAL_BAUDRATE				(115200)
#define SERIAL_BUFFER_SIZE			(60)

#define STEPS_PER_RADIAN			(648.68)
#define STEP_DELAY_MS				(4)

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

const uint8_t STEPS_MASKS[] = {0b0001, 0b0011, 0b0010, 0b0110, 0b0100, 0b1100, 0b1000, 0b1001};

typedef struct xy_point {
	int16_t x;
	int16_t y;
};

uint8_t steps_delay_ms = STEP_DELAY_MS;
xy_point current_position;
bool text_right_diraction = false;
uint8_t text_char_sep = 10;
uint8_t draw_scale = 10;


/* cuts the power to the motors (they will hold thier positions) 
	must call after done steping the motors with "step_to_current_position" */
void disable_axes() {
	PORTC &= 0xF0;
	PORTD &= 0x0F;
}

/* sets the motors positions to match "current_position" global
    must call with each change of 1 step in "current_position" global */
void step_to_current_position() {
	PORTC = (PORTC & 0xF0) | STEPS_MASKS[current_position.x & 7];
	PORTD = (PORTD & 0x0F) | (STEPS_MASKS[current_position.y & 7] << 4);
	delay(steps_delay_ms);
}

/* turn laser on or off (pull mosfet gate down to turn the laser on) */
void set_laser(bool is_on) {
	if (is_on)
		PORTB |= LASER_MASK;
	else
		PORTB &= ~LASER_MASK;
}

/* go to the given axes position from current motors position */
void absolute_steps(xy_point * p) {
	xy_point steps = {p->x - current_position.x, p->y - current_position.y};
	relative_steps(&steps);
}

/* add/inc the motors current position by the given steps */
void relative_steps(xy_point * p) {
	int32_t steps, err, e;
	xy_point delta = {abs(p->x), abs(p->y)};
	xy_point step = {(p->x > 0 ? 1 : (p->x < 0 ? -1 : 0)), (p->y > 0 ? 1 : (p->y < 0 ? -1 : 0))};

	steps = max(delta.x, delta.y);
	err = (delta.x > delta.y ? delta.x : -delta.y) / 2;
	while (steps--) {
		e = err;
		if (e >= -delta.x) {
			err -= delta.y;
			current_position.x += step.x;
		}
		if (e < delta.y) {
			err += delta.x;
			current_position.y += step.y;
		}
		step_to_current_position();
	}
}

/* set the current laser position to (0, 0) for trigo to work */
void set_home() {
	current_position.x = 0;
	current_position.y = 0;
	step_to_current_position();
	disable_axes();
}

/* turn laser off and put projector in home position */
void go_home() {
	xy_point p = {0, 0};
	set_laser(false);
	absolute_steps(&p);
}

/* draw regular polygon x, y are the center point with absolute_steps */
void draw_polygon(xy_point * p, uint8_t corners, int16_t size, int16_t start_angle) {
	xy_point tmp;
	double current_angle = PI * start_angle / 180.0;
	double step_angle = PI * 2.0 / corners;
	corners++;
	set_laser(true);
	while (corners) {
		tmp.x = p->x + round(size * cos(current_angle));
		tmp.y = p->y + round(size * sin(current_angle));
		absolute_steps(&tmp);
		current_angle += step_angle;
		corners--;
	}
	set_laser(false);
	disable_axes();
}

/*void get_path_size(char * path, xy_point * p) {
	
}

void get_text_size(char * text, xy_point * p) {
	xy_point tmp;
	p->x = (*text ? -text_char_sep : 0);
	p->y = 0;
	while (*text) {
		get_path_size(char_to_path(*text++), &tmp);
		p->x += tmp->x + text_char_sep;
		if (p->y < tmp->y)
			p->y = tmp->y;
	}
}

void draw_path(char * path, xy_point * p) {
	
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

/* called once at power-on */
void setup() {
	Serial.begin(SERIAL_BAUDRATE);
	DDRD |= 0xF0;				// (4 - 7) OUTPUTS
	DDRC |= 0x0F;				// (A0 - A3) OUTPUTS
	DDRB |= LASER_MASK;			// (8) OUTPUT
	set_laser(false);			// turn off laser
	set_home();
}

/* called repeatedly after "setup" */
void loop() {
	xy_point tmp = {0, 0};
	draw_polygon(&tmp, 4, 100, 0);
}
