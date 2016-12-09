'''
This script generates C code for the arduino and openGL simulator from a given SVG file.
'''
import xml.etree.ElementTree as ElementTree
from optparse import OptionParser
import svg.path
import os
import struct

SVG_XML_NAMESPACE = "http://www.w3.org/2000/svg"

ROUNDING_MULTIPLIER = 1

# C function names.
CUBIC_BEZIER_FUNC_NAME = "draw_cubic_bezier"
LINE_FUNC_NAME = "draw_line"
QUADRATIC_BEZIER_FUNC_NAME = "draw_quadratic_bezier"
ARC_FUNC_NAME = "draw_arc"
SET_LASER_FUNC_NAME = "set_laser"
DRAW_START_FUNC_NAME = "draw_start"

# svg instruction names
ARDUINO_END_COMMAND = hex(ord("e"))
CUBIC_BEZIER_INSTRUCTION =  hex(ord("c"))
LINE_INSTRUCTION =  hex(ord("l"))
QUADRATIC_BEZIER_INSTRUCTION =  hex(ord("q"))
ARC_INSTRUCTION =  hex(ord("a"))
DRAW_START_INSTRUCTION =  hex(ord("m"))

# Format string for array and function arguments generation
LINE_FORMAT = "{0}, {1}," # {XEND}, {YEND}
QUADRATIC_BEZIER_FORMAT = "{0}, {1}, {2}, {3}," # {XCNT}, {YCNT}, {XEND}, {YEND}
ARC_FORMAT = "{0}, {1}, {2}, {3}, {4}, {5}, {6}," # {XRADIUS}, {YRADIUS}, {ROTATION}, {ARC}, {SWEEP}, {XEND}, {YEND}
CUBIC_BEZIER_FORMAT = "{0}, {1}, {2}, {3}, {4}, {5}," # {XCNT1}, {YCNT1}, {XCNT2}, {YCNT2}, {XEND}, {YEND}
DRAW_START_FORMAT = "{0}, {1}," # {XSTART, YSTART}

# function externs
g_externs = "extern void %s(int16_t x1, int16_t y1);\n" % DRAW_START_FUNC_NAME
g_externs += "extern void %s(bool is_on);\n" % SET_LASER_FUNC_NAME
g_externs += "extern void %s(int16_t x1, int16_t y1);\n" % LINE_FUNC_NAME
g_externs += "extern void %s(int16_t x1, int16_t y1, int16_t x2, int16_t y2);\n" % QUADRATIC_BEZIER_FUNC_NAME
g_externs += "extern void %s(int16_t x1, int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3);\n" % CUBIC_BEZIER_FUNC_NAME
g_externs += "extern void %s(int16_t x0, int16_t y0, int16_t radius, int16_t rotation, int16_t arc, int16_t sweep, int16_t x1, int16_t y1);\n" % ARC_FUNC_NAME

def generate_code(paths, close=True, gen_funcs=False):
    '''
    Return a string that contains a generated code from the given paths. An array is created if 'gen_funcs' isn't True,
    otherwise function calls are generated.
    '''
    gen_code = ""
    instructions_counter = 0
    start_x = None
    start_y = None
    end_x = None
    end_y = None
    
    # Go over all the paths in the paths list, parse them as svg.path classes and convert them to
    # function calls
    for path in paths:
        for instruction in svg.path.parse_path(path):
            # Log the first instruction's location to return to it when "close" is True.
            if True == close:
                instructions_counter += 1
                if 1 == instructions_counter:
                    start_x = instruction.start.real
                    start_y = instruction.start.imag
            
            # Only move the cursor to the start point if the start x or y are different from the previous end points
            if end_x != round_float(instruction.start.real) or end_y != round_float(instruction.start.imag):
                gen_code += create_path_command(gen_funcs,
                    DRAW_START_FUNC_NAME,
                    DRAW_START_FORMAT,
                    DRAW_START_INSTRUCTION,
                    instruction.start.real, 
                    instruction.start.imag)
                    
            # Put the end points aside for the next iteration
            end_x = round_float(instruction.end.real)
            end_y = round_float(instruction.end.imag)
                
            if isinstance(instruction, svg.path.QuadraticBezier):
                gen_code += create_path_command(gen_funcs,
                    QUADRATIC_BEZIER_FUNC_NAME,
                    QUADRATIC_BEZIER_FORMAT,
                    QUADRATIC_BEZIER_INSTRUCTION,
                    instruction.control.real, 
                    instruction.control.imag, 
                    instruction.end.real, 
                    instruction.end.imag,)
                    
            elif isinstance(instruction, svg.path.Line):
                gen_code += create_path_command(gen_funcs,
                    LINE_FUNC_NAME, 
                    LINE_FORMAT,
                    LINE_INSTRUCTION,
                    instruction.end.real, 
                    instruction.end.imag,)
                    
            elif isinstance(instruction, svg.path.CubicBezier):
                gen_code += create_path_command(gen_funcs,
                    CUBIC_BEZIER_FUNC_NAME,
                    CUBIC_BEZIER_FORMAT,
                    CUBIC_BEZIER_INSTRUCTION,
                    instruction.control1.real, 
                    instruction.control1.imag, 
                    instruction.control2.real, 
                    instruction.control2.imag, 
                    instruction.end.real, 
                    instruction.end.imag,)
                    
            elif isinstance(instruction, svg.path.Arc):
                gen_code += create_path_command(gen_funcs,
                    ARC_FUNC_NAME, 
                    ARC_FORMAT,
                    ARC_INSTRUCTION,
                    instruction.radius.real, 
                    instruction.radius.imag,
                    instruction.rotation,
                    str(instruction.arc).lower(), 
                    str(instruction.arc).lower(),
                    instruction.end.real, 
                    instruction.end.imag,)
                        
        # If close is enabled we have to draw a line back to the starting position from this instruction's end position
        # in case the 'z' instruction wasn't in the path.
        if True == close and False == path.closed:
            gen_code += create_path_command(gen_funcs,
                LINE_FUNC_NAME,
                LINE_FORMAT,
                start_x, 
                start_y,)
            instructions_counter = 0
            
    # Turn off the laser and return to 0,0 once all the paths were drawn
    if True == gen_funcs:
        gen_code += create_function_call(SET_LASER_FUNC_NAME, "false")
    else:
        gen_code += ARDUINO_END_COMMAND
        
    return gen_code
    
    
def serialize_float(number):
    '''
    Serialize a given float as int16_t
    '''
    return ", ".join(["0x%02X" % (ord(i),) for i in struct.pack("<h", round_float(number))])

    
def round_float(number):
    return int(round(number * ROUNDING_MULTIPLIER))

    
def create_path_command(gen_funcs, func_name, format, instruction, *args):
    '''
    Creates a path command
    '''
    func_variables = []
    array_variables = []
    for arg in args:
        if isinstance(arg, float):
            func_variables.append(round_float(arg))
            array_variables.append(serialize_float(arg))
        else:
            func_variables.append(arg)
            array_variables.append(arg)
            
    function_call = create_function_call(func_name, format.format(*func_variables))
    if True == gen_funcs:
        return function_call 
    
    array_instruction = instruction + ', ' + format.format(*array_variables) + ' // ' + function_call
    return array_instruction
    
    
def create_function_call(func_name, arguments, ):
    '''
    Creates a C function call string from a function name and arguments
    '''
    return "{FUNC}({ARGS});\n".format(FUNC=func_name, ARGS=arguments)


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
    wrapper = "void %s()\n{\n%s}" % (func_name, indent_lines("\t", func_content))
    return wrapper
    
    
def indent_lines(indent, lines):
    '''
    Adds line indent with the given indent string to the given lines
    '''
    return ''.join(indent+line for line in lines.splitlines(True))

    
def create_array_wrapper(array_name, array_content):
    '''
    Creates an array that wraps a given code block with the given name
    '''
    wrapper = "static const uint8_t %s[] PROGMEM = {\n%s\n};" % (array_name, indent_lines("\t", array_content))
    return wrapper
    
    
def create_doc(text):
    '''
    Creates a documentation block with the given text
    '''
    return "/*\n" + text + "*/\n"
    
def generate_code_from_file(file, close, generate_funcs):
    '''
    Returns generated code from a given svg file
    '''
    paths = parse_paths_from_svg(file)
    
    code = generate_code(paths, close, generate_funcs)
    if False == options.generate_funcs:
        code = create_array_wrapper(os.path.splitext(os.path.basename(file))[0].upper() + "_PATH", code)
    else:
        code = create_function_wrapper("draw_" + os.path.splitext(os.path.basename(file))[0], code)
    
    return code
    
if "__main__" == __name__:
    parser = OptionParser()
    parser.add_option("-i", "--input_file", dest="input_file", help="SVG input file")
    parser.add_option("-d", "--input_dir", dest="input_dir", help="SVG input directory")
    parser.add_option("-o", "--output_file", dest="output_file", help="Generated code output file path")
    parser.add_option("-c", "--close_svg", dest="close", action="store_true", 
        default=False, help="Whether the script should add a line to the starting point when reaching the 'z' instruction(Default=False)")
    parser.add_option("-f", "--generate_funcs", dest="generate_funcs", action="store_true", 
        default=False, help="Whether the script should generate the code as function calls instead of an array(Default=False)")
    
    (options, args) = parser.parse_args()
    
    code = ""
    if None != options.input_dir:
        for file in os.listdir(options.input_dir):
            if True == file.endswith(".svg"):
                code += generate_code_from_file(os.path.join(options.input_dir, file), options.close, options.generate_funcs)
                code += '\n\n'
    else:
        code += generate_code_from_file(options.input_file, options.close, options.generate_funcs)

    with open(("%s.h" % options.output_file), "wb") as f:
        f.write(create_doc("WARNING: Automatically generated code, do not modify.\n"))
        
        if False == options.generate_funcs:
            f.write(create_doc(indent_lines("\t", g_externs)))
        else:
            f.write(g_externs)
            
        f.write(code)
