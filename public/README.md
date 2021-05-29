# GBSControl webui

Redesigned UI for the GBSControl with added features:

- 72 Named Slots avaliable
- Slots save current filters state.
- Slots filter state can be toggled between local/global
- Backup / Restore system
- Option to enable / disable developer options (hidden by default)
- Integrated wifi management in system menu

## Building

- `npm install`
- Make changes
- `npm run build` to generate the necesary files webui_html.h
- Compile & upload GBSControl project in Arduino IDE

## Tips

* Before every push do a `npm run build` to be sure bin files are updated to the latest.
