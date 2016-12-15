import re
import struct
import os.path
import argparse
import xml.etree.ElementTree as ElementTree


SVG_ARGUMENT_ROUNDING_MULTIPLIER = 1
SVG_XML_NAMESPACE = "http://www.w3.org/2000/svg"

SVG_COMMANDS_ARGUMENTS_COUNT = {
    "m": 2,
    "h": 1,
    "v": 1,
    "l": 2,
    "z": 0,
    "q": 4,
    "t": 2,
    "c": 6,
    "s": 4,
    "a": 7,
}

SVG_COMMANDS = "".join(SVG_COMMANDS_ARGUMENTS_COUNT.keys())
SVG_COMMANDS += SVG_COMMANDS.upper()
SVG_COMMANDS_RE = re.compile("([%s])" % (SVG_COMMANDS,))
SVG_ARGUMENT_RE = re.compile("[-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?")

C_ARRAY_FORMAT = \
"""/* draw size: %s bytes */
static const uint8_t %s[] PROGMEM = {
	%s
};
"""


def get_file_or_folder_name(path):
    return os.path.splitext(os.path.basename(path))[0]


def parse_svg_argument(number):
    return struct.pack("<h", int(round(number * SVG_ARGUMENT_ROUNDING_MULTIPLIER)))


def tokenize_svg_path(path):
    for command in SVG_COMMANDS_RE.split(path):
        if command in SVG_COMMANDS:
            yield command
        for argument in SVG_ARGUMENT_RE.findall(command):
            yield parse_svg_argument(argument)


def read_svg_file_paths(svg_file):
    svg_tree = ElementTree.ElementTree()
    svg_tree.parse(svg_file)
    # return the "d" field(s) (path instructions)
    return [i.attrib["d"] for i in svg_tree.findall(".//{%s}path" % SVG_XML_NAMESPACE)]


def parse_svg_path_to_draw(path):
    # TODO: finish...
    # tokens = tokenize_svg_path(path)
    # start_point = None
    # while True:
        # token = tokens.next()
        # if token in SVG_COMMANDS:

    # TEST ONLY:
    return "a"


def parse_svg_file_to_draw(svg_file):
    return "".join(parse_svg_path_to_draw(path) for path in read_svg_file_paths(svg_file))


def parse_draw_data_to_c_array(draw_name, draw_data):
    return C_ARRAY_FORMAT % (
        len(draw_data),
        "%s_DRAW" % (draw_name.upper(),),
        ",".join(["0x%02X" % (ord(i),) for i in draw_data]))


def parse_svg_file_to_c_array(svg_file):
    return parse_draw_data_to_c_array(
        get_file_or_folder_name(svg_file),
        parse_svg_file_to_draw(svg_file))


def main(args):
    with open(args.output_path, "wb") as f:
        f.write("\n".join(parse_svg_file_to_c_array(i) for i in args.svg_files))


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
