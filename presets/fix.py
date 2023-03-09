"""thanks chatgpt"""
import sys
import os

# Check if filename is provided in the command line arguments
if len(sys.argv) < 2:
    print("Usage: python program.py filename")
    sys.exit()

filename = sys.argv[1]
output_filename = os.path.splitext(filename)[0] + ".h"

with open(filename, "r") as input_file, open(output_filename, "w") as output_file:
    for line in input_file:
        if line == "};\n":
            break
        # Remove the trailing comma and whitespace from the line
        number = line.strip()[:-1]
        # Convert the number to hex and add "0x" prefix
        hex_number = "0x" + hex(int(number))[2:].upper()
        # Output the result in the desired format
        output_file.write(hex_number + ",\n")
