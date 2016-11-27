#!/usr/bin/python

import sys
import xml.etree.ElementTree as ElementTree
from optparse import OptionParser
import svg.path
import os

g_output_buffer = ""
SVG_XML_NAMESPACE = "http://www.w3.org/2000/svg"

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

def parse_paths_from_svg(filename):
  '''
  Returns a list of paths from a given path file
  '''
  paths = []

  tree = ElementTree.ElementTree()
  tree.parse(filename)
  # Append the "d" field(path instructions) to te paths list in all of the nodes named "path"
  for node in tree.findall('.//{%s}path' % SVG_XML_NAMESPACE):
    paths.append(node.attrib['d'])

  return paths


def create_function_wrapper(func_name, func_content):
  '''
  Creates a function that wraps a given code block with the given name
  '''
  indent = "\t"
  indented_code = ''.join(indent+line for line in func_content.splitlines(True))
  wrapper = "extern void set_laser(bool is_on);\n"
  wrapper += "extern void draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1);\n"
  wrapper += "extern void draw_quadratic_bezier(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);\n"
  wrapper += "extern void draw_cubic_bezier(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3);\n"
  wrapper += "void Drawing::draw_%s(Display &display) {\n  draw(%s_program, display);\n};\n" % (func_name, func_name)
  wrapper += "PROGMEM static const int32_t %s_program[] = {\n%s\n};\n" % (func_name, indented_code)
  return wrapper
  
x = 0.0
y = 0.0
start_x = None
start_y = None
def main(instr_string, output_file):
  global x, y, g_output_buffer
  stream = Stream(instr_string)
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
    global x, y, start_x, start_y, g_output_buffer
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
    g_output_buffer += "  %s(%s, %s, %s)," % (op, format_float(x), format_float(y), ", ".join(map(process_coord, args)))
    (dx, dy) = parse_coord(args[-1])
    if is_rel:
      x += dx
      y += dy
    else:
      x = dx
      y = dy
  def emit_straight(is_rel, is_vertical, op, arg):
    global x, y, g_output_buffer
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
    g_output_buffer += "  %s(%s, %s, %s, %s)\n" % (op, format_float(x), format_float(y), format_float(nx), format_float(ny))
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
    elif instr == "H":
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
      print "failed to process %s" % instr
  g_output_buffer += "  end()"
  
  with open(output_file, "wb") as f:
    f.write(create_function_wrapper("draw_" +
      os.path.splitext(os.path.basename(options.output_file))[0], g_output_buffer))

def seperate_chars(string, chars):
  formatted_str = string
  for char in chars:
    formatted_str = formatted_str.replace(char, " " + char + " ")
    
  return formatted_str
        
if __name__ == "__main__":
  parser = OptionParser()
  parser.add_option("-i", "--input_file", dest="input_file", help="SVG input file")
  parser.add_option("-o", "--output_file", dest="output_file", help="Generated code output file path")

  (options, args) = parser.parse_args()
  
  paths = parse_paths_from_svg(options.input_file)
  full_instructions_string = ""
  for path in paths:
    full_instructions_string += path + " "

  # Remove things that this script can't parse
  #full_instructions_string = full_instructions_string.replace(",", " ")
  full_instructions_string = seperate_chars(full_instructions_string, ("M", "m", "L", "l", "v", "V", "H", "h", "c", "C", "z"))
    
  main(full_instructions_string, options.output_file)