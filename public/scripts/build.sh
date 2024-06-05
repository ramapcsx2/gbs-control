#!/usr/bin/env bash

# cd ../../src
# gzip -c9 webui.html > webui_html \
#     && xxd -i webui_html > webui_html.h \
#         && rm webui_html \
#             && sed -i -e 's/unsigned char webui_html\[]/const uint8_t webui_html[] PROGMEM/' webui_html.h \
#                 && sed -i -e 's/unsigned int webui_html_len/const unsigned int webui_html_len/' webui_html.h
# rm -f webui_html.h-e webui.html

ROOT=$(pwd)

tsc --project ./tsconfig.json
cd $ROOT/public/scripts
node ./build.js
cd $ROOT/data
gzip -c9 webui.html > __index
rm -f webui.html .??*

echo -e "\xE2\x9C\x85 WebUI is ready\n";