'''
This script generates C code for the arduino and openGL simulator from a given SVG file.
'''
import xml.etree.ElementTree as ElementTree
from optparse import OptionParser
import svg.path
import os

SVG_XML_NAMESPACE = "http://www.w3.org/2000/svg"

ROUNDING_MULTIPLIER = 1

# C function names.
CUBIC_BEZIER_FUNC_NAME = "draw_cubic_bezier"
LINE_FUNC_NAME = "draw_line"
QUADRATIC_BEZIER_FUNC_NAME = "draw_quadratic_bezier"
ARC_FUNC_NAME = "draw_arc"
SET_LASER_FUNC_NAME = "set_laser"
DRAW_START_FUNC_NAME = "draw_start"

def generate_code(paths, close=True):
    '''
    Return a string that contains the generated C code from the given paths list
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
            # NOTE: It might wiser to place this for the first path and do the closing on the last path. Requires testing.
            instructions_counter += 1
            if 1 == instructions_counter:
                start_x = instruction.start.real
                start_y = instruction.start.imag
            
            # Only move the cursor to the start point if the start x or y are different from the previous end points
            if end_x != round_float(instruction.start.real) or end_y != round_float(instruction.start.imag):
                gen_code += create_function_call(DRAW_START_FUNC_NAME,
                    "{XSTART}, {YSTART}".format(
                        XSTART=round_float(instruction.control1.real), 
                        YSTART=round_float(instruction.control1.imag),)
                    )
                    
            # Put the end points aside for the next iteration
            end_x = round_float(instruction.end.real)
            end_y = round_float(instruction.end.imag)
                
            if isinstance(instruction, svg.path.QuadraticBezier):
                gen_code += create_function_call(QUADRATIC_BEZIER_FUNC_NAME,
                    "{XCNT}, {YCNT}, {XEND}, {YEND}".format(
                        XCNT=round_float(instruction.control1.real), 
                        YCNT=round_float(instruction.control1.imag), 
                        XEND=round_float(instruction.end.real), 
                        YEND=round_float(instruction.end.imag),),
                    )
                    
            elif isinstance(instruction, svg.path.Line):
                gen_code += create_function_call(LINE_FUNC_NAME, 
                    "{XEND}, {YEND}".format(
                        XEND=round_float(instruction.end.real), 
                        YEND=round_float(instruction.end.imag),),
                    )
                    
            elif isinstance(instruction, svg.path.CubicBezier):
                gen_code += create_function_call(CUBIC_BEZIER_FUNC_NAME,
                    "{XCNT1}, {YCNT1}, {XCNT2}, {YCNT2}, {XEND}, {YEND}".format(
                        XCNT1=round_float(instruction.control1.real), 
                        YCNT1=round_float(instruction.control1.imag), 
                        XCNT2=round_float(instruction.control2.real), 
                        YCNT2=round_float(instruction.control2.imag), 
                        XEND=round_float(instruction.end.real), 
                        YEND=round_float(instruction.end.imag),),
                    )
                    
            elif isinstance(instruction, svg.path.Arc):
                gen_code += create_function_call(ARC_FUNC_NAME, 
                    "{XRADIUS}, {YRADIUS}, {ROTATION}, {ARC}, {SWEEP}, {XEND}, {YEND}".format(
                        XRADIUS=round_float(instruction.radius.real), 
                        YRADIUS=round_float(instruction.radius.imag),
                        ROTATION=round_float(instruction.rotation),
                        ARC=str(instruction.arc).lower(), 
                        SWEEP=str(instruction.arc).lower(),
                        XEND=round_float(instruction.end.real), 
                        YEND=round_float(instruction.end.imag),),
                    )
                        
        # If close is enabled we have to draw a line back to the starting position from this instruction's end position
        if True == close:
            gen_code += create_function_call(LINE_FUNC_NAME,
                "{XEND}, {YEND}".format(
                    XEND=round_float(start_x), 
                    YEND=round_float(start_y),),
                )
            
        # Turn off the laser once a path is finished
        gen_code += create_function_call(SET_LASER_FUNC_NAME, "false")
        
    return gen_code


def round_float(number):
    return int(round(number * ROUNDING_MULTIPLIER))


def create_function_call(func_name, arguments):
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


def create_function_wrapper(func_name, func_content, optimize=False):
    '''
    Creates a function that wraps a given code block with the given name
    '''
    indent = "\t"
    indented_code = ''.join(indent+line for line in func_content.splitlines(True))
    
    wrapper = "extern void %s(uint16_t x1, uint16_t y1);\n)" % DRAW_START_FUNC_NAME
    wrapper += "extern void %s(bool is_on);\n" % SET_LASER_FUNC_NAME
    wrapper += "extern void %s(uint16_t x1, uint16_t y1);\n" % LINE_FUNC_NAME
    wrapper += "extern void %s(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);\n" % QUADRATIC_BEZIER_FUNC_NAME
    wrapper += "extern void %s(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t x3, uint16_t y3);\n" % CUBIC_BEZIER_FUNC_NAME
    wrapper += "void %s()\n{\n%s}" % (func_name, indented_code)
    return wrapper


if "__main__" == __name__:
    parser = OptionParser()
    parser.add_option("-i", "--input_file", dest="input_file", help="SVG input file")
    parser.add_option("-o", "--output_file", dest="output_file", help="Generated code output file path")
    parser.add_option("-c", "--close_svg", dest="close", action="store_false", default=True, help="Whether the script should add a line to the starting point when reaching the 'z' instruction(Default=True)")
    
    (options, args) = parser.parse_args()
        
    paths = parse_paths_from_svg(options.input_file)
    code = generate_code(paths, options.close)
    
    with open(options.output_file, "wb") as f:
        f.write(create_function_wrapper("draw_" + 
            os.path.splitext(os.path.basename(options.output_file))[0], code))
