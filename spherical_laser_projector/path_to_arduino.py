import os.path
import struct

# TODO: get from argparse, support multy path too
SVG_PATH = "m95.57318,4.09856c-0.31894,0.01009 -0.62716,0.03697 -0.94742,0.06886c-5.12415,0.51039 -9.64676,3.76233 -11.9443,8.60793l-1.21812,3.66698c0.2628,-1.27078 0.66632,-2.50322 1.21812,-3.66698c-4.09736,-4.2452 -9.84842,-6.20462 -15.53098,-5.28526c-5.68257,0.91935 -10.61763,4.60634 -13.31468,9.95076c-7.61382,-4.68933 -17.1046,-4.40532 -24.44691,0.74029c-7.34232,5.1456 -11.25576,14.25631 -10.06638,23.44799l0.77824,3.73584c-0.35168,-1.2239 -0.61432,-2.46899 -0.77824,-3.73584l-0.11843,0.34432c-6.38239,0.69554 -11.61673,5.64758 -12.97632,12.27491c-1.35959,6.62733 1.4675,13.42742 7.00417,16.85432l8.64524,2.10034c-3.01869,0.24848 -6.03247,-0.48317 -8.64524,-2.10034c-4.27063,4.78383 -5.25336,11.86617 -2.43623,17.71512c2.81713,5.84895 8.83349,9.21679 15.04035,8.45298l3.77278,-0.99852c-1.21391,0.5081 -2.47463,0.83877 -3.77278,0.99852c3.52358,6.4866 9.36614,11.21836 16.22462,13.10126c6.85848,1.8829 14.14336,0.77009 20.21734,-3.08163c4.95516,7.81436 13.74987,11.91876 22.56899,10.53609c8.81911,-1.38267 16.09401,-8.01294 18.67778,-17.00926l0.89667,-5.02703c-0.13426,1.70784 -0.4256,3.38687 -0.89667,5.02703c6.0659,3.97794 13.69611,4.20211 19.96357,0.58534c6.26746,-3.61677 10.17819,-10.5135 10.23556,-18.025l-2.24693,-11.22325l-8.85147,-7.59368c6.8444,4.28745 11.15946,10.81978 11.09839,18.81693c8.13835,0.08529 15.0513,-6.88829 18.15331,-14.89171c3.10201,-8.00343 1.92858,-17.12778 -3.07914,-23.98169c2.07711,-5.10303 1.95388,-10.90768 -0.33835,-15.90745c-2.29225,-4.99977 -6.52047,-8.71583 -11.60595,-10.17457c-1.13806,-6.62657 -5.82918,-11.95694 -12.02889,-13.70382c-6.19971,-1.74688 -12.80128,0.40718 -16.95211,5.54351l-2.52082,4.25232c0.64413,-1.53774 1.48538,-2.97106 2.52082,-4.25232c-2.92602,-4.03677 -7.51537,-6.31459 -12.29959,-6.16328l0.00001,-0.00002z"
PATH_NAME = "cloud"


ROUNDING_MULTIPLIER = 1
ARDUINO_END_COMMAND = "e"
SVG_POINT_SEPARATOR = ","
SVG_PATH_COMMANDS = {
    "m": 2,
    "h": 1,
    "v": 1,
    "l": 2,
    "z": 0,
    "q": 4,
    "t": 2,
    "c": 6,
    "a": 7,
}

FILE_FORMAT = \
"""/*
Auto genereted file, do not modify
*/

static const uint8_t %s[] PROGMEM = {
	%s
};
"""


def parse_svg_number_to_data(number):
    return struct.pack("<h", int(round(number * ROUNDING_MULTIPLIER)))

def parse_svg_path(path):
    path = path.replace(SVG_POINT_SEPARATOR, " ")
    commands = "".join(SVG_PATH_COMMANDS.keys())
    commands += commands.upper()

    for command in commands:
        path = path.replace(command, " %s " % (command,))

    return "".join([i if i in commands else parse_svg_number_to_data(float(i)) for i in path.strip().split(" ")])

def main():
    data = parse_svg_path(SVG_PATH)
    data += ARDUINO_END_COMMAND

    with open(os.path.join(os.path.dirname(os.path.sys.argv[0]), "%s.h" % (PATH_NAME,)), "wb") as f:
        f.write(FILE_FORMAT % (
            "%s_PATH" % (PATH_NAME.upper(),),
            ",".join(["0x%02X" % (ord(i),) for i in data])))

if __name__ == "__main__":
    main()
