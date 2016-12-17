import re
import struct
import os.path
import argparse
import xml.etree.ElementTree as ElementTree

SVG_ARGUMENT_ROUNDING_MULTIPLIER = 1
SVG_XML_NAMESPACE = "http://www.w3.org/2000/svg"

SVG_PATH_COMMAND_SIZE = {
    "M": 2,
    "H": 1,
    "V": 1,
    "L": 2,
    "Z": 0,
    "Q": 4,
    "T": 2,
    "C": 6,
    "S": 4,
    "A": 7,
}

SVG_COMMANDS = "".join(SVG_PATH_COMMAND_SIZE.keys())
SVG_COMMANDS += SVG_COMMANDS.lower()
SVG_COMMANDS_RE = re.compile("([%s])" % (SVG_COMMANDS,))
SVG_ARGUMENT_RE = re.compile("[-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?")

C_ARRAY_FORMAT = """/* draw size: %s bytes */
static const uint8_t %s[] PROGMEM = {
\t%s
};
"""

# SVG_FILE_DATA = """<?xml version="1.0" encoding="UTF-8" standalone="no"?>
# <svg
#    xmlns:dc="http://purl.org/dc/elements/1.1/"
#    xmlns:cc="http://creativecommons.org/ns#"
#    xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
#    xmlns:svg="http://www.w3.org/2000/svg"
#    xmlns="http://www.w3.org/2000/svg"
#    version="1.1"
#    id="svg2">
#   <metadata
#      id="metadata10">
#     <rdf:RDF>
#       <cc:Work
#          rdf:about="">
#         <dc:format>image/svg+xml</dc:format>
#         <dc:type
#            rdf:resource="http://purl.org/dc/dcmitype/StillImage" />
#         <dc:title></dc:title>
#       </cc:Work>
#     </rdf:RDF>
#   </metadata>
#   <defs
#      id="defs8" />
#   <path
#      id="path4"
#      d="%s"
#      style="fill:none;stroke:#000000" />
# </svg>
# """


def absolute_arguments_points(holder, arguments):
    return [j + holder.current_point[i % 2] for i, j in enumerate(arguments)]


def draw_command(holder, arguments):
    print "%s: %s" % (holder.path_command, arguments)
    # holder.svg_path += holder.path_command
    # if arguments:
    #     holder.svg_path += " ".join(str(i) for i in arguments)
    holder.draw_data += holder.path_command
    holder.draw_data += struct.pack("<%s" % ("h" * len(arguments),), *arguments)


def path_command_to_draw_data_m(holder, arguments, is_relative=False):
    if is_relative:
        arguments = absolute_arguments_points(holder, arguments)
    draw_command(holder, arguments)
    holder.start_point = arguments
    holder.current_point = arguments


def path_command_to_draw_data_h(holder, arguments, is_relative=False):
    if is_relative:
        arguments.append(0)
        arguments = absolute_arguments_points(holder, arguments)
        arguments.pop(1)
    draw_command(holder, arguments)
    holder.current_point[0] = arguments[0]


def path_command_to_draw_data_v(holder, arguments, is_relative=False):
    if is_relative:
        arguments.insert(0, 0)
        arguments = absolute_arguments_points(holder, arguments)
        arguments.pop(0)
    draw_command(holder, arguments)
    holder.current_point[0] = arguments[1]


def path_command_to_draw_data_l(holder, arguments, is_relative=False):
    if is_relative:
        arguments = absolute_arguments_points(holder, arguments)
    draw_command(holder, arguments)
    holder.current_point = arguments


def path_command_to_draw_data_z(holder, arguments, is_relative=False):
    if holder.current_point != holder.start_point:
        holder.current_point = holder.start_point
        draw_command(holder, arguments)


def path_command_to_draw_data_q(holder, arguments, is_relative=False):
    if is_relative:
        arguments = absolute_arguments_points(holder, arguments)
    draw_command(holder, arguments)
    holder.current_point = arguments[2:]


def path_command_to_draw_data_t(holder, arguments, is_relative=False):
    if is_relative:
        arguments = absolute_arguments_points(holder, arguments)
    draw_command(holder, arguments)
    holder.current_point = arguments


def path_command_to_draw_data_c(holder, arguments, is_relative=False):
    if is_relative:
        arguments = absolute_arguments_points(holder, arguments)
    draw_command(holder, arguments)
    holder.current_point = arguments[4:]


def path_command_to_draw_data_s(holder, arguments, is_relative=False):
    if is_relative:
        arguments = absolute_arguments_points(holder, arguments)
    draw_command(holder, arguments)
    holder.current_point = arguments[2:]


def path_command_to_draw_data_a(holder, arguments, is_relative=False):
    if is_relative:
        arguments = arguments[:5].extend(absolute_arguments_points(holder, arguments[5:]))
    arguments[0] = abs(arguments[0])
    arguments[1] = abs(arguments[1])
    arguments[2] %= 360
    if arguments.pop(3):
        arguments[2] |= 0x200
    if arguments.pop(3):
        arguments[2] |= 0x400
    draw_command(holder, arguments)
    holder.current_point = arguments[3:]


def get_file_or_folder_name(path):
    return os.path.splitext(os.path.basename(path))[0]


def read_path_from_svg_file(svg_file):
    svg_tree = ElementTree.ElementTree()
    svg_tree.parse(svg_file)
    # return the "d" field(s) (path instructions)
    paths = [i.attrib["d"] for i in svg_tree.findall(".//{%s}path" % SVG_XML_NAMESPACE)]
    # join path and make sure they starts with M command
    return "".join(i[0].upper() + i[1:] for i in paths if i[0] in "mM")


def parse_svg_path_to_commands(path):
    # split path to list of (command, argument, command, argument...) strings
    tokens = SVG_COMMANDS_RE.split(path)
    tokens.append(tokens.pop(0))
    # return list of ((command, argument), (command, argument)...) where argument is a list of flots
    return zip(
        tokens[0::2],
        [[int(round(float(i) * SVG_ARGUMENT_ROUNDING_MULTIPLIER))
          for i in SVG_ARGUMENT_RE.findall(j)]
         for j in tokens[1::2]])


def parse_path_commands_to_draw_data(path_commands):
    class Holder():
        pass
    holder = Holder()
    holder.start_point = (0, 0)
    holder.current_point = (0, 0)
    holder.path_command = ""
    holder.draw_data = ""
    holder.svg_path = ""

    for command, arguments in path_commands:
        holder.path_command = command.upper()
        arguments_count = SVG_PATH_COMMAND_SIZE[holder.path_command]
        function = globals()["path_command_to_draw_data_%s" % (holder.path_command.lower(),)]
        for i in xrange((len(arguments) / arguments_count) if arguments_count else 1):
            if i and holder.path_command == "M":
                holder.path_command = "L"
                function = path_command_to_draw_data_l
            function(holder, arguments[i * arguments_count: (i + 1) * arguments_count], command.islower())
    # with open(r"C:\Users\arad-lab\Documents\GitHub\Spherical-Laser-Projector\svgs\test\test.svg", "wb") as f:
    #     f.write(SVG_FILE_DATA % (holder.svg_path,))
    return holder.draw_data


def parse_draw_data_to_c_array(draw_name, draw_data):
    return C_ARRAY_FORMAT % (
        len(draw_data),
        "%s_DRAW" % (draw_name.upper(),),
        ",".join(["0x%02X" % (ord(i),) for i in draw_data]))


def parse_svg_file_to_c_array(svg_file):
    draw_name = get_file_or_folder_name(svg_file)
    print draw_name
    svg_path = read_path_from_svg_file(svg_file)
    path_commands = parse_svg_path_to_commands(svg_path)
    draw_data = parse_path_commands_to_draw_data(path_commands) + "E"
    c_array = parse_draw_data_to_c_array(draw_name, draw_data)
    return c_array


def main(arguments):
    with open(arguments.output_path, "wb") as f:
        f.write("\n".join(parse_svg_file_to_c_array(i) for i in arguments.svg_files))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="convert svg file(s) to arduino draw arrays h file")
    parser.add_argument(
        "-i",
        metavar="<input path>",
        dest="input_path",
        type=str,
        required=True,
        help="the input svg file path or svg files directory")
    parser.add_argument(
        "-o",
        metavar="<output path>",
        dest="output_path",
        type=str,
        required=True,
        help="the output h file path (default is the image input path)")
    args = parser.parse_args()

    args.input_path = os.path.abspath(args.input_path)
    args.output_path = os.path.abspath(args.output_path)
    if os.path.isdir(args.output_path):
        args.output_path = os.path.join(args.output_path, "%s.h" % (get_file_or_folder_name(args.input_path),))

    if os.path.isfile(args.input_path):
        args.svg_files = [args.input_path]
    elif os.path.isdir(args.input_path):
        args.svg_files = [os.path.join(args.input_path, i) for i in os.walk(args.input_path).next()[2]]
    else:
        print "the input is not a valid file or directory"
        exit(1)
    main(args)
