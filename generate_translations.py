import json

from PIL import Image, ImageDraw, ImageFont

import json
import os.path
import pathlib
import sys
from argparse import ArgumentParser

from PIL import Image, ImageDraw, ImageFont

MENU_WIDTH = 128
MENU_HEIGHT = 64
MENU_STATUS_BAR_HEIGHT = MENU_HEIGHT / 4
LEFT_RIGHT_PADDING = 0
TOP_BOTTOM_PADDING = 1
X_OFFSET = 0
Y_OFFSET = -1
DEFAULT_FONT_SIZE = 12

menu_items = [
    {
        "tag": "OM_STATUS_CUSTOM",
        "en-US": "Main Menu",
        "zh-CN": "主菜单",
        # should not be larger than 12 (or adjust according to MENU_STATUS_BAR_HEIGHT)
        "size": 12,
    },
    {
        "tag": "OM_STATUS_BAR_BACK",
        "en-US": "←Back",
        # should not be larger than 12 (or adjust according to MENU_STATUS_BAR_HEIGHT)
        "size": 12
    },
    {
        "tag": "OM_SCREEN_SAVER",
        "en-US": "Press Any Key",
    },
    {
        "tag": "OM_RESOLUTION",
        "en-US": "Resolutions",
    },
    {
        "tag": "OM_PASSTHROUGH",
        "en-US": "Passthrough",
    },
    {
        "tag": "OM_DOWNSCALE",
        "en-US": "Down-Scale",
    },
    {
        "tag": "OM_PRESET",
        "en-US": "Presets",
    },
    {
        "tag": "OM_RESET_RESTORE",
        "en-US": "Reset/Restore",
    },
    {
        "tag": "OM_RESET_GBS",
        "en-US": "Reset GBS",
    },
    {
        "tag": "OM_RESET_WIFI",
        "en-US": "Clear WiFi Connections",
    },
    {
        "tag": "OM_RESTORE_FACTORY",
        "en-US": "Restore Factory",
    },
    {
        "tag": "OM_CURRENT",
        "en-US": "Current Output",
    },
    {
        "tag": "OM_WIFI",
        "en-US": "WiFi Info",
    },
    {
        "tag": "TEXT_NO_PRESETS",
        "en-US": "No Presets. Please use the Web UI to create one first.",
    },
    {
        "tag": "TEXT_TOO_MANY_PRESETS",
        "en-US": "Please use WebUI to access more presets.",
    },
    {
        "tag": "TEXT_RESETTING_GBS",
        "en-US": "Resetting GBS\nPlease wait\n...",
        "size": 16,
    },
    {
        "tag": "TEXT_RESETTING_WIFI",
        "en-US": "Resetting WiFi\nPlease wait\n...",
        "size": 16,
    },
    {
        "tag": "TEXT_RESTORING",
        "en-US": "Factory Restoring\nPlease wait\n...",
        "size": 16,
    },
    {
        "tag": "TEXT_WIFI_CONNECT_TO",
        "en-US": "Connect to the following SSID (password) before using the Web UI",
    },
    {
        "tag": "TEXT_WIFI_CONNECTED",
        "en-US": "Status: Connected",
    },
    {
        "tag": "TEXT_WIFI_DISCONNECTED",
        "en-US": "Status: Disconnected",
    },
    {
        "tag": "TEXT_WIFI_URL",
        "en-US": "Use one of the following URLs to use the Web UI",
    },
    {
        "tag": "TEXT_LOADED",
        "en-US": "Loaded",
        "size": 16
    },
    {
        "tag": "TEXT_NO_INPUT",
        "en-US": "No Input",
        "size": 16
    },
    {
        "tag": "OM_OSD",
        "en-US": "Open OSD Menu",
    },


]

res = """
#define %(name)s_WIDTH %(width)s
#define %(name)s_HEIGHT %(height)s
const unsigned char %(name)s [] PROGMEM = {
%(array)s
};
"""

tags_map = {}
fonts_map = {}
default_font = None


def convert(text, font):
    img = Image.new('L', (0, 0), color=0)
    draw = ImageDraw.Draw(img)
    _, _, width, height = draw.textbbox((0, 0), text, font)
    width += 2 * LEFT_RIGHT_PADDING
    height += 2 * TOP_BOTTOM_PADDING  # expand top and bottom
    img = img.resize((width, height))
    draw = ImageDraw.Draw(img)
    draw.text((LEFT_RIGHT_PADDING + X_OFFSET, TOP_BOTTOM_PADDING +
               Y_OFFSET), text, 255, font, align='center')
    byte_index = 0
    number = 0
    data = list(img.getdata())
    bytes_arr = []
    for index, pixel in enumerate(data):
        if pixel >= 128:
            number += 2 ** byte_index
        byte_index += 1
        if byte_index == 8:
            byte_index = -1
        # if this was the last pixel of a row or the last pixel of the
        # image, fill up the rest of our byte with zeroes so it always contains 8 bits
        if (index != 0 and (index + 1) % width == 0) or index == len(data):
            byte_index = -1
        if byte_index < 0:
            bytes_arr.append(number)
            number = 0
            byte_index = 0
    img.save(f'i18n_preview_{tag}.jpg')
    return width, height, bytes_arr


def collect(lang):
    key = lang
    if not lang:
        key = "en-US"
    for obj in menu_items:
        if not isinstance(obj, dict):
            raise TypeError(f"{obj} is not a dict")
        if 'tag' not in obj:
            raise KeyError(f'Key "tag" is missing in {obj}')
        if key not in obj:
            raise KeyError(
                f'Key "{key}" does not exist in {json.dumps(obj, ensure_ascii=False)}')
        size = obj.get('size')
        tag = obj['tag']
        if tag in tags_map:
            raise ValueError(f"Duplicated tag: {tag}")
        translated = obj[key]
        tags_map[tag] = translated, size


template = """
#define %(name)s_WIDTH %(width)s
#define %(name)s_HEIGHT %(height)s
const unsigned char %(name)s [] PROGMEM = {
%(array)s
};
"""
if __name__ == '__main__':
    parser = ArgumentParser()
    parser.add_argument('lang', help='Language code', nargs='?')
    parser.add_argument('--fonts', '-f', nargs='*', default=[])
    parser.add_argument('--output', '-o', default='OLEDMenuTranslations.h')
    args = parser.parse_args()
    for font in args.fonts:
        tokens = font.split('@')
        if len(tokens) == 1:
            default_font = tokens[0]
        elif len(tokens) > 2:
            raise ValueError(f"Too many tokens in {font}. Format: FONT_SIZE@PATH_TO_FONT") from None
        else:
            font_size, font_path = tokens
            try:
                font_size = int(font_size)
            except ValueError:
                raise ValueError(f"No a valid integer: {font}.") from None
            fonts_map[font_size] = font_path
    collect(args.lang)
    tag_res_map = {}

    for _, (_, size) in tags_map.items():
        # pre-checks to avoid a corrupted header file.
        if not size:
            size = DEFAULT_FONT_SIZE
        font = fonts_map.get(size, default_font)
        if not font:
            raise FileNotFoundError(f"No font for size {size}.") from None
        try:
            font = ImageFont.truetype(font)
        except (OSError, FileNotFoundError):
            raise FileNotFoundError(f"Font does not exist: {font}") from None

    with open(args.output, 'w') as fp:
        fp.write('#ifndef OLED_MENU_TRANSLATIONS_H_\n')
        fp.write('#define OLED_MENU_TRANSLATIONS_H_\n')
        for tag, (text, size) in tags_map.items():
            if not size:
                size = DEFAULT_FONT_SIZE
            font = fonts_map.get(size, default_font)
            font = ImageFont.truetype(font, size=size)
            width, height, byte_array = convert(text, font)
            tmp_str = ""
            i = 0
            while i < len(byte_array):
                if i + 16 < len(byte_array):
                    end = i + 16
                else:
                    end = len(byte_array)
                tmp_str += ','.join([hex(x)
                                     for x in byte_array[i:end]]) + ',\n'
                i = end
            fp.write(template % {'array': tmp_str,
                                 'width': width, 'height': height, 'name': tag})
        fp.write('#endif')
        print(f"Finished. Output file: {pathlib.Path(args.output).absolute()}")
