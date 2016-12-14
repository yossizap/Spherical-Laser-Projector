import struct
import os.path
import argparse
import xml.etree.ElementTree as ElementTree


ROUNDING_MULTIPLIER = 1
SVG_POINT_SEPARATOR = ","
SVG_PATH_COMMANDS = "mhvlzqtcsaMHVLZQTCSA"
SVG_XML_NAMESPACE = "http://www.w3.org/2000/svg"
PATH_END_COMMAND = "e"

FILE_FORMAT = \
"""static const uint8_t %s[] PROGMEM = {
	%s
};

"""


def parse_svg_number_to_data(number):
    return struct.pack("<h", int(round(number * ROUNDING_MULTIPLIER)))

def parse_svg_path(path):
    path = path.replace(SVG_POINT_SEPARATOR, " ").replace("-", " -").replace("  ", " ")

    for command in SVG_PATH_COMMANDS:
        path = path.replace(command, " %s " % (command,))

    return "".join([i if i in SVG_PATH_COMMANDS else parse_svg_number_to_data(float(i))
        for i in path.split(" ") if i])

def convert_image_to_bitmap(image_path, bitmap_path, bitmap_name, append=False):
    paths = []

    svg_tree = ElementTree.ElementTree()
    svg_tree.parse(image_path)
    # Append the "d" field(path instructions) to te paths list in all of the nodes named "path"
    for node in svg_tree.findall(".//{%s}path" % SVG_XML_NAMESPACE):
        paths.append(node.attrib["d"])

    data = parse_svg_path("".join(paths))
    data += PATH_END_COMMAND

    with open(bitmap_path, "a+" if append else "wb") as f:
        f.write(FILE_FORMAT % (
            "%s_PATH" % (bitmap_name.upper(),),
            ",".join(["0x%02X" % (ord(i),) for i in data])))

def main(args):
    if os.path.isfile(args.image_path):
        convert_image_to_bitmap(
            image_path=args.image_path,
            bitmap_path=args.bitmap_path,
            bitmap_name=args.bitmap_name)
    elif os.path.isdir(args.image_path):
        image_path, _, image_names = os.walk(args.image_path).next()
        for image_name in image_names:
            convert_image_to_bitmap(
                image_path=os.path.join(image_path, image_name),
                bitmap_path=args.bitmap_path,
                bitmap_name="_".join((args.bitmap_name, os.path.splitext(image_name)[0])),
                append=True)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="convert image file(s) to VGA bitmap file(s)")
    parser.add_argument(
        "-i",
        metavar="<image path>",
        dest="image_path",
        type=str,
        required=True,
        help="the image file path or image files directory")
    parser.add_argument(
        "-n",
        metavar="<bitmap name>",
        dest="bitmap_name",
        type=str,
        help="the bitmap name (default is the image file name)")
    parser.add_argument(
        "-o",
        metavar="<bitmap path>",
        dest="bitmap_path",
        type=str,
        help="the bitmap file path (default is the image file path)")
    args = parser.parse_args()

    args.image_path = os.path.abspath(args.image_path)

    if args.bitmap_name is None:
        args.bitmap_name = os.path.split(os.path.splitext(args.image_path)[0])[1]
    if args.bitmap_path is None:
        args.bitmap_path = os.path.join(os.path.split(args.image_path)[0], "%s.h" % args.bitmap_name)
    args.bitmap_path = os.path.abspath(args.bitmap_path)
    main(args)
