'''
This script generates C code for the arduino and openGL simulator from a given SVG file.
'''
import xml.etree.ElementTree as ElementTree
from optparse import OptionParser
import svg.path
import os

SVG_XML_NAMESPACE = "http://www.w3.org/2000/svg"

ROUNDING_MULTIPLIER = 1

# C function names
CUBIC_BEZIER_FUNC_NAME = "cubic_bezier"
LINE_FUNC_NAME = "line"
QUADRATIC_BEZIER_FUNC_NAME = "quadratic_bezier"
ARC_FUNC_NAME = "arc"
TURN_OFF_LASER_FUNC_NAME = "set_laser"


def generate_code(paths):
    '''
    Return a string that contains the generated C code from the given paths list
    '''
    gen_code = ""
    # Go over all the paths in the paths list, parse them as svg.path classes and convert them to
    # function calls
    for path in paths:
        for instruction in svg.path.parse_path(path):
            if isinstance(instruction, svg.path.QuadraticBezier):
                gen_code += create_function_call(QUADRATIC_BEZIER_FUNC_NAME,
                        "{XSTART}, {YSTART}, {XCNT}, {YCNT}, {XEND}, {YEND}".format(
                            XSTART=round_float(instruction.start.real), 
                            YSTART=round_float(instruction.start.imag),
                            XCNT=round_float(instruction.control1.real), 
                            YCNT=round_float(instruction.control1.imag), 
                            XEND=round_float(instruction.end.real), 
                            YEND=round_float(instruction.end.imag),)
                        )
            if isinstance(instruction, svg.path.Line):
                gen_code += create_function_call(LINE_FUNC_NAME, 
                        "{XSTART}, {YSTART}, {XEND}, {YEND}".format(
                            XSTART=round_float(instruction.start.real), 
                            YSTART=round_float(instruction.start.imag),
                            XEND=round_float(instruction.end.real), 
                            YEND=round_float(instruction.end.imag),)
                        )
            if isinstance(instruction, svg.path.CubicBezier):
                gen_code += create_function_call(CUBIC_BEZIER_FUNC_NAME,
                        "{XSTART}, {YSTART}, {XCNT1}, {YCNT1}, {XCNT2}, {YCNT2}, {XEND}, {YEND}".format(
                            XSTART=round_float(instruction.start.real), 
                            YSTART=round_float(instruction.start.imag),
                            XCNT1=round_float(instruction.control1.real), 
                            YCNT1=round_float(instruction.control1.imag), 
                            XCNT2=round_float(instruction.control2.real), 
                            YCNT2=round_float(instruction.control2.imag), 
                            XEND=round_float(instruction.end.real), 
                            YEND=round_float(instruction.end.imag),)
                        )
            if isinstance(instruction, svg.path.Arc):
                gen_code += create_function_call(ARC_FUNC_NAME, 
                    "{XSTART}, {YSTART}, {XRADIUS}, {YRADIUS}, {ROTATION}, {ARC}, {SWEEP}, {XEND}, {YEND}".format(
                            XSTART=round_float(instruction.start.real), 
                            YSTART=round_float(instruction.start.imag),
                            XRADIUS=round_float(instruction.radius.real), 
                            YRADIUS=round_float(instruction.radius.imag),
                            ROTATION=round_float(instruction.rotation),
                            ARC=str(instruction.arc).lower(), 
                            SWEEP=str(instruction.arc).lower(),
                            XEND=round_float(instruction.end.real), 
                            YEND=round_float(instruction.end.imag),)
                        )

        # Turn off the laser once a path is finished
        gen_code += create_function_call(TURN_OFF_LASER_FUNC_NAME, "false")

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


def create_function_wrapper(func_name, func_content):
    '''
    Creates a function that wraps a given code block with the given name
    '''
    indent = "\t"
    indented_code = ''.join(indent+line for line in func_content.splitlines(True))
    return "void %s()\n{\n%s}" % (func_name, indented_code)


if "__main__" == __name__:
    parser = OptionParser()
    parser.add_option("-i", "--input_file", dest="input_file", help="SVG input file")
    parser.add_option("-o", "--output_file", dest="output_file", help="Generated code output file path")

    (options, args) = parser.parse_args()

    paths = parse_paths_from_svg(options.input_file)
    code = generate_code(paths)

    with open(options.output_file, "wb") as f:
        f.write(create_function_wrapper("draw_" +
            os.path.splitext(os.path.basename(options.output_file))[0], code))
