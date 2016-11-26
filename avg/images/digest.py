#!/usr/bin/python

import sys

class Stream(object):

  def __init__(self, str):
    self.cursor = 0
    self.parts = str.split(" ")
  
  def has_more(self):
    return self.cursor < len(self.parts)
  
  def next(self):
    result = self.peek()
    self.cursor += 1
    return result
  
  def peek(self):
    return self.parts[self.cursor]

x = 0.0
y = 0.0
start_x = None
start_y = None
def main(args):
  global x, y
  assert len(args) == 1
  stream = Stream(open(args[0], "rt").read())
  def format_float(val):
    return "e(%s)" % val
  def parse_coord(coord):
    [x_str, y_str] = coord.split(",")
    return (float(x_str), float(y_str))
  def update_start():
    global start_x, start_y
    if start_x is None:
      start_x = x
      start_y = y
  def emit(is_rel, draws, op, *args):
    global x, y, start_x, start_y
    if draws:
      update_start()
    else:
      start_x = start_y = None
    def process_coord(str):
      (vx, vy) = parse_coord(str)
      if is_rel:
        vx += x
        vy += y
      return "%s, %s" % (format_float(vx), format_float(vy))
    print "  %s(%s, %s, %s)," % (op, format_float(x), format_float(y), ", ".join(map(process_coord, args)))
    (dx, dy) = parse_coord(args[-1])
    if is_rel:
      x += dx
      y += dy
    else:
      x = dx
      y = dy
  def emit_straight(is_rel, is_vertical, op, arg):
    global x, y
    update_start()
    d = float(arg)
    nx = x
    ny = y
    if is_rel:
      if is_vertical:
        ny += d
      else:
        nx += d
    else:
      if is_vertical:
        ny = d
      else:
        nx = d
    print "  %s(%s, %s, %s, %s)," % (op, format_float(x), format_float(y), format_float(nx), format_float(ny))
    x = nx
    y = ny
  while stream.has_more():
    instr = stream.next()
    if instr == "m":
      emit(True, False, "move_to", stream.next())
      while stream.has_more() and len(stream.peek()) > 1:
        emit(True, True, "line_to", stream.next())
    elif instr == "M":
      emit(False, False, "move_to", stream.next())
      while stream.has_more() and len(stream.peek()) > 1:
        emit(False, True, "line_to", stream.next())
    elif instr == "L":
      while stream.has_more() and len(stream.peek()) > 1:
        emit(False, True, "line_to", stream.next())
    elif instr == "l":
      while stream.has_more() and len(stream.peek()) > 1:
        emit(True, True, "line_to", stream.next())
    elif instr == "v":
      while stream.has_more() and len(stream.peek()) > 1:
        emit_straight(True, True, "line_to", stream.next())
    elif instr == "V":
      while stream.has_more() and len(stream.peek()) > 1:
        emit_straight(False, True, "line_to", stream.next())
    elif instr == "h":
      while stream.has_more() and len(stream.peek()) > 1:
        emit_straight(True, False, "line_to", stream.next())
    elif instr == "h":
      while stream.has_more() and len(stream.peek()) > 1:
        emit_straight(False, False, "line_to", stream.next())
    elif instr == "c":
      while stream.has_more() and len(stream.peek()) > 1:
        emit(True, True, "curve_to", stream.next(), stream.next(), stream.next())
    elif instr == "C":
      while stream.has_more() and len(stream.peek()) > 1:
        emit(False, True, "curve_to", stream.next(), stream.next(), stream.next())
    elif instr == "z":
      emit(False, False, "line_to", "%s,%s" % (start_x, start_y))
    else:
      print instr
  print "  end()"

if __name__ == "__main__":
  main(sys.argv[1:])
