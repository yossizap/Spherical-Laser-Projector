#define X_AXIS_LIMIT_MIN			(-200)
#define Y_AXIS_LIMIT_MIN			(-100)
#define X_AXIS_LIMIT_MAX			(200)
#define Y_AXIS_LIMIT_MAX			(300)

#define SERIAL_BAUDRATE				(115200)
#define SERIAL_BUFFER_SIZE			(60)

#define STEPS_PER_RADIAN			(648.68)
#define STEPS_DELAY_MS				(5)
#define DRAW_SCALE					(4)

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

/* cuts the power to the motors (they will hold thier positions) 
	must call after done steping the motors with "step_to_current_position" */
void Display::disable_axes() {
	PORTC &= 0xF0;
	PORTD &= 0x0F;
}

/* sets the motors positions to match "current_position" global
    must call with each change of 1 step in "current_position" global */
void Display::step_to_current_position() {
	PORTC = (PORTC & 0xF0) | STEPS_MASKS[current_position_x & 7];
	PORTD = (PORTD & 0x0F) | (STEPS_MASKS[current_position_y & 7] << 4);
	delay(STEPS_DELAY_MS);
}

/* turn laser on or off (pull mosfet gate down to turn the laser on) */
void Display::set_laser(bool is_on) {
	if (is_on)
		PORTB |= LASER_MASK;
	else
		PORTB &= ~LASER_MASK;
}

/* go to the given axes position from current motors position */
void Display::absolute_steps(int16_t x, int16_t y) {
	relative_steps(x - current_position_x, y - current_position_y);
}

/* add/inc the motors current position by the given steps */
void Display::relative_steps(int16_t x, int16_t y) {
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

/* set the current laser position to (0, 0) for trigo to work */
void Display::set_home() {
	current_position_x = 0;
	current_position_y = 0;
	step_to_current_position();
	disable_axes();
}

/* turn laser off and put projector in home position */
void Display::go_home() {
	set_laser(false);
	absolute_steps(0, 0);
}

void Display::go_to(int16_t x, int16_t y) {
	// absolute_steps(x / DRAW_SCALE, y / DRAW_SCALE);
	absolute_steps(x * DRAW_SCALE+100 * DRAW_SCALE, y * DRAW_SCALE+100 * DRAW_SCALE);
}

void Display::draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
	go_to(x0, y0);
	set_laser(true);
	go_to(x1, y1);
	set_laser(false);
}

void Display::initialize() {
	DDRD |= 0xF0;				// (4 - 7) OUTPUTS
	DDRC |= 0x0F;				// (A0 - A3) OUTPUTS
	DDRB |= LASER_MASK;			// (8) OUTPUT

	set_laser(true); delay(100); set_laser(false);
	set_home();
	
	// relative_steps(-500,0); set_home();
	// set_laser(true); delay(100); set_laser(false);
}

Display::Display() { 
	// initialize();
}

void Display::transform_line(Point<fixed> p0, Point<fixed> p1) {
  p0 = transform().transform(p0);
  p1 = transform().transform(p1);
  draw_line(p0.x.to_int16(), p0.y.to_int16(), p1.x.to_int16(), p1.y.to_int16());
}

void Display::draw_cubic_bezier_plain(Point<double> *p) {
  int16_t last_x = static_cast<int16_t>(p[0].x);
  int16_t last_y = static_cast<int16_t>(p[0].y);
  for (uint8_t i = 1; i <= kN; i++) {
    double t = ((double) i) / kN;
    Point<double> n = (p[0] * pow(1 - t, 3)) + (p[1] * 3 * t * pow(1 - t, 2)) + (p[2] * 3 * pow(t, 2) * (1 - t)) + (p[3] * pow(t, 3));
    int16_t x = static_cast<int16_t>(n.x);
    int16_t y = static_cast<int16_t>(n.y);
    draw_line(last_x, last_y, x, y);
    last_x = x;
    last_y = y;
  }
}

void Display::draw_cubic_bezier_faster_diffs(Point<double> *p) {
  static const double d = 1.0 / kN;

  // Calculate the four ks.  
  Point<double> k0 = p[0];
  Point<double> k1 = (p[1] - p[0]) * 3;
  Point<double> k2 = (p[2] - p[1] * 2 + p[0]) * 3;
  Point<double> k3 = p[3] - p[2] * 3 + p[1] * 3 - p[0];
  
  // Caldulate the four ds.
  Point<double> d0 = k0;
  Point<double> d1 = (((k3 * d) + k2) * d + k1) * d;
  Point<double> d2 = ((k3 * (3 * d)) + k2) * (2 * d * d);
  Point<double> d3 = k3 * (6 * d * d * d);
  
  // Plot
  int16_t last_x = static_cast<int16_t>(p[0].x);
  int16_t last_y = static_cast<int16_t>(p[0].y);
  for (uint8_t i = 1; i <= kN; i++) {
    d0 = d0 + d1;
    d1 = d1 + d2;
    d2 = d2 + d3;
    int16_t x = static_cast<int16_t>(d0.x);
    int16_t y = static_cast<int16_t>(d0.y);
    draw_line(last_x, last_y, x, y);
    last_x = x;
    last_y = y;
  }
}

static inline Point<fixed> mult_3(const Point<fixed> &p) {
  return (p << 1) + p;
}

void Display::draw_cubic_bezier_fixed_diffs(Point<fixed> *p) {
  // Calculate the four ks.  
  Point<fixed> k0 = p[0];
  Point<fixed> k1 = mult_3(p[1] - p[0]);
  Point<fixed> k2 = mult_3(p[2] - (p[1] << 1) + p[0]);
  Point<fixed> k3 = p[3] + mult_3(p[1] - p[2]) - p[0];
  
  // Caldulate the four ds.
  Point<fixed> d0 = k0;
  Point<fixed> d1 = ((((k3 >> kLogN) + k2) >> kLogN) + k1) >> kLogN;
  Point<fixed> d2 = ((mult_3(k3) >> kLogN) + k2) >> (kLogN * 2 - 1);
  Point<fixed> d3 = mult_3(k3) >> (3 * kLogN - 1);
  
  // Plot
  int16_t last_x = p[0].x.to_int16();
  int16_t last_y = p[0].y.to_int16();
  for (uint8_t i = 1; i <= kN; i++) {
    d0 = d0 + d1;
    d1 = d1 + d2;
    d2 = d2 + d3;
    int16_t x = d0.x.to_int16();
    int16_t y = d0.y.to_int16();
    draw_line(last_x, last_y, x, y);
    last_x = x;
    last_y = y;
  }
}

void Display::draw_cubic_bezier_fixed_diffs_precomputed(Point<fixed> *d) {
  // Caldulate the four ds.
  Point<fixed> d0 = d[0];
  Point<fixed> d1 = d[1];
  Point<fixed> d2 = d[2] >> kLogN;
  Point<fixed> d3 = d[3] >> (2 * kLogN);
  
  // Plot
  int16_t last_x = d[0].x.to_int16();
  int16_t last_y = d[0].y.to_int16();
  for (uint8_t i = 1; i <= kN; i++) {
    d0 = d0 + d1;
    d1 = d1 + d2;
    d2 = d2 + d3;
    int16_t x = d0.x.to_int16();
    int16_t y = d0.y.to_int16();
    draw_line(last_x, last_y, x, y);
    last_x = x;
    last_y = y;
  }
}

void Display::draw_cubic_bezier_raw_diffs(Point<double> *p) {
  static const double t0 = 0.0 / kN;
  static const double t1 = 1.0 / kN;
  static const double t2 = 2.0 / kN;
  static const double t3 = 3.0 / kN;

  // Calculate the four ks.  
  Point<double> d0 = (p[0] * pow(1 - t0, 3)) + (p[1] * 3 * t0 * pow(1 - t0, 2)) + (p[2] * 3 * pow(t0, 2) * (1 - t0)) + (p[3] * pow(t0, 3));
  Point<double> d1 = (p[0] * pow(1 - t1, 3)) + (p[1] * 3 * t1 * pow(1 - t1, 2)) + (p[2] * 3 * pow(t1, 2) * (1 - t1)) + (p[3] * pow(t1, 3));
  Point<double> d2 = (p[0] * pow(1 - t2, 3)) + (p[1] * 3 * t2 * pow(1 - t2, 2)) + (p[2] * 3 * pow(t2, 2) * (1 - t2)) + (p[3] * pow(t2, 3));
  Point<double> d3 = (p[0] * pow(1 - t3, 3)) + (p[1] * 3 * t3 * pow(1 - t3, 2)) + (p[2] * 3 * pow(t3, 2) * (1 - t3)) + (p[3] * pow(t3, 3));
  
  d3 = d3 - d2;
  d2 = d2 - d1;
  d1 = d1 - d0;
  d3 = d3 - d2;
  d2 = d2 - d1;
  d3 = d3 - d2;
  
  // Plot
  int16_t last_x = static_cast<int16_t>(p[0].x);
  int16_t last_y = static_cast<int16_t>(p[0].y);
  for (uint8_t i = 1; i <= kN; i++) {
    d0 = d0 + d1;
    d1 = d1 + d2;
    d2 = d2 + d3;
    int16_t x = static_cast<int16_t>(d0.x);
    int16_t y = static_cast<int16_t>(d0.y);
    draw_line(last_x, last_y, x, y);
    last_x = x;
    last_y = y;
  }
}

void Display::transform_cubic_bezier(Point<fixed> *p) {
#ifdef PRECOMPUTED
  p[0] = transform().transform(p[0]);
  p[1] = transform().transform_no_translate(p[1]);
  p[2] = transform().transform_no_translate(p[2]);
  p[3] = transform().transform_no_translate(p[3]);
  draw_cubic_bezier_fixed_diffs_precomputed(p);
#else
  p[0] = transform().transform(p[0]);
  p[1] = transform().transform(p[1]);
  p[2] = transform().transform(p[2]);
  p[3] = transform().transform(p[3]);
  draw_cubic_bezier_fixed_diffs(p);
#endif
}
