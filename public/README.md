# GBSControl webui

A design & code minimalistic UI for the GBSControl.

## Building

- `npm install`
- Make changes
- `npm run build` to generate the necesary files webui_html.h
- Wifi connection page can be edited in src/wifi.txt and then manually change the contents of `gbs-control.ino` to change the output from `server.on("/wifi/connect"...` code to wifi.txt contents
