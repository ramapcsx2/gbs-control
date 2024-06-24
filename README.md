<p align="center">

![GBS-Control](./doc/img/logo.jpg)

</p>

<p>

![Version](https://img.shields.io/badge/dynamic/json?url=https%3A%2F%2Fraw.githubusercontent.com%2Fway5%2Fgbs-control%2Fpio%2Fconfigure.json&query=%24.version&logo=C%2B%2B&logoColor=white&style=flat&label=%20Version%3A&color=red)

</p>

<h2>GBS-Control</h2>

GBS-Control is an alternative firmware for Tvia Trueview5725 based upscalers / video converter boards.
Its growing list of features includes:

- very low lag
- sharp and defined upscaling, comparing well to other -expensive- units
- no synchronization loss switching 240p/480i (output runs independent from input, sync to display never drops)
- on demand motion adaptive deinterlacer that engages automatically and only when needed
- works with almost anything: 8 bit consoles, 16/32 bit consoles, 2000s consoles, home computers, etc
- little compromise, eventhough the hardware is very affordable (less than $30 typically)
- lots of useful features and image enhancements
- optional control interface via web browser, utilizing the ESP8266 WiFi capabilities
- good color reproduction with auto gain and auto offset for the tripple 8 bit @ 160MHz ADC
- optional bypass capability to, for example, transcode Component to RGB/HV in high quality
 
Supported standards are NTSC / PAL, the EDTV and HD formats, as well as VGA from 192p to 1600x1200 (earliest DOS, home computers, PC).
Sources can be connected via RGB/HV (VGA), RGBS (game consoles, SCART) or Component Video (YUV).
Various variations are supported, such as the PlayStation 2's VGA modes that run over Component cables.

Gbscontrol is a continuation of previous work by dooklink, mybook4, Ian Stedman and others.

Bob from RetroRGB did an overview video on the project. This is a highly recommended watch!
https://www.youtube.com/watch?v=fmfR0XI5czI

Development threads:
- [shmups.system11.org](https://shmups.system11.org/viewtopic.php?f=6&t=52172)
- [circuit-board.de/forum](https://circuit-board.de/forum/index.php/Thread/15601-GBS-8220-Custom-Firmware-in-Arbeit)

<h2>Table of Contents</h2>
 
- [Toolkit](#toolkit)
    - [Windows](#windows)
    - [MacOS](#macos)
    - [Linux](#linux)
- [Prepare](#prepare)
- [Build and Upload Firmware Image](#build-and-upload-firmware-image)
  - [Using Platformio IDE (preferred)](#using-platformio-ide-preferred)
  - [Using Arduino IDE](#using-arduino-ide)
- [Filesystem Image and UI Translations](#filesystem-image-and-ui-translations)
  - [Platformio IDE (preferred)](#platformio-ide-preferred)
  - [Arduino IDE](#arduino-ide)
    - [HardwareUI](#hardwareui)
    - [WebUI](#webui)
  - [Uploading Filesystem image via Platformio IDE (recommended)](#uploading-filesystem-image-via-platformio-ide-recommended)
  - [Uploading Filesystem image via Arduino IDE](#uploading-filesystem-image-via-arduino-ide)
- [OTA update](#ota-update)
  - [Using Platformio IDE](#using-platformio-ide)
  - [Using Arduino IDE](#using-arduino-ide-1)
- [Theory of operation](#theory-of-operation)
  - [Slots vs Presets](#slots-vs-presets)
  - [Vocabulary](#vocabulary)
  - [How-to switch GBS-Control to upload mode?](#how-to-switch-gbs-control-to-upload-mode)
- [TODO:](#todo)
- [Additional information](#additional-information)
- [Old documentation](#old-documentation)

## Toolkit

#### Windows
- [NodeJS](https://nodejs.org)
- [Git](https://git-scm.com/download/win)
- [Python 3+](https://www.python.org/downloads)

#### MacOS
```bash
brew install node git python@3
```

#### Linux
```bash
your_package_manager install node git python
```

## Prepare

Make sure the latest version of `node-js` installed on your computer. After installing `node-js`, run the following in your OS command prompt from the Project root duirctory:

```bash
npm install -g typescript run-script-os

npm install
```

## Build and Upload Firmware Image<a id="build-n-upload"></a>

<!-- >***PRO Tip:***\
You may consider using the latest compiled binaries from [/builds](./builds/) directory. -->

### Using Platformio IDE (preferred)

>***Please note:***\
If your objective is to make changes to the Project, please use VSCode + Platformio IDE.

1. Clone the repository, open it with your VSCode and press Build/Upload. It's never been easier :)

>***Please note:***\
Platformio IDE enables upload speed limitation on ESP8266. Upload process at any higher upload speed may fail.

### Using Arduino IDE<a id="build-n-upload-arduino"></a>

1. Open "Preferences" in Arduino IDE. In "Additional Boards Manager URLs" put the following source links:

```
https://dl.espressif.com/dl/package_esp32_index.json 
http://arduino.esp8266.com/stable/package_esp8266com_index.json
```

2. Save and close Preferences window. Go to the "Board Manager" and search for ESP8266. Make sure that the latest version of the framework installed.
3. Download/clone the following repositories into your Arduino libraries directory (see: "Preferences - Sketchbook location" + libraries). 
For more intfrmation please refer to http://www.arduino.cc/en/Guide/Libraries

```bash
git clone https://github.com/Links2004/arduinoWebSockets.git
git clone https://github.com/pavelmc/Si5351mcu.git
git clone https://github.com/ThingPulse/esp8266-oled-ssd1306.git
```

If you plan to be using ping-library (see: HAVE_PINGER_LIBRARY in options.h) in addition to the above clone/download the following sources:

```bash
git clone https://github.com/bluemurder/esp8266-ping.git
```

4. In menu "Tools" select the board "LOLIN(WEMOS) D1 R2 & mini". Then change "Flash size" to `4MB (FS:1MB OTA:~1019KB), "CPU frequency" to 160MHz, "SSL Support` to `Basic SSL cyphers`.
5. Now remove src/main.cpp file. Arduino IDE will NOT compile your project if you omit this step. You can always restore main.cpp from main repository/active branch.
6. Build/Upload the Project.

>***Please note:***\
If you erase the whole chip or doing fresh install, you need to upload both, firmware and filesystem images. See how-to below.

## Filesystem Image and UI Translations

HardwareUI (OLED display) can be translated using [`translation.hdwui.json`](./translation.hdwui.json) file, WebUI using [`translation.webui.json`](./translation.webui.json) in the Project root directory. If you wish to add a new language, please use ISO 639-1 formatted locale tag names.

### Platformio IDE (preferred)

By changing value of `ui-lang` parameter in `configure.json` you're changing UI (both Web and Hardware) language. You also may change the UI fonts the same way (`ui-hdw-fonts`, `ui-web-font`).
HardwareUI font size may be specified for every translation block in [translation.hdwui.json](./translation.hdwui.json), the default size if `12`. If you add a font size which is not specified in [configure.json](./configure.json), translation generator will ask you to do that by throwing an error.

- `ui-hdw-fonts` parameter should be formatted as: `FONT_SIZE@FONT_NAME[,FONT_SIZE@FONT_NAME[,...]]`. 
- `ui-web-fonts` parameter must match with font-family name in CSS and the Font file name.

To add a new font (Hardware and Web UI alike) drop it into [/public/assets/fonts](./public/assets/fonts/) directory, only `.ttf` fonts may be used for HardwareUI and only `.woff2` for WebUI.

>***Please note:***\
The default and fallback translation is always "en".

### Arduino IDE

If you're still using Arduino IDE you need to do a few extra steps to generate translated UI. Please follow steps below:

#### HardwareUI

1. Do the necessary changes in ```translation.hdw.json```
2. Make sure you have installed the latest version of Python on your computer
3. Install ***pillow***:

    ```bash
    python -m pip install pillow
    ```

4. Generate translations

    >_The following commands executed from Project root directory._

    4.1. Hardare UI 

    #### Linux / MacOS
    ```bash
    python scripts/generate_translations.py FONT_SIZE@FONT_NAME,FONT_SIZE@FONT_NAME LOCALE
    ```
    #### Windows
    ```bash
    python scripts\generate_translations.py FONT_SIZE@FONT_NAME,FONT_SIZE@FONT_NAME LOCALE
    ```

5. Now you're ready to build and upload the `firmware` image.

#### WebUI

The WebUI consists of two main parts:  `public/src/index.ts` and `public/src/index.html.tpl`. Changin any one of them likewise `translation.webui.json`, require re-build the WebUI.

1. Do the necessary changes in `translation.web.json`
2. Generate WebUI by running the following command:

```bash
npm run build
```

3. Now you're ready to build and upload the `filesystem` image.

### Uploading Filesystem image via Platformio IDE (recommended)

You can use `Platform - Build Filesystem Image` command in PlatformIO tab to get the WebUI re-generated.
Either you may use the manual method described below.

>***Please note:***\
When you first time open the Project you may not find `Platform` section in PlatformIO tab. This happens because `/data` directory will be only available after you run `Build project`. Run `Build`, then press `Refresh Project Tasks` on top of PlatformIO tab, the required menu will be available once it reloads.

### Uploading Filesystem image via Arduino IDE

1. Download the latest release of (ESP8266LittleFS.jar](https://github.com/earlephilhower/arduino-esp8266littlefs-plugin/releases).
2. Extract archive into your Arduino tools directory, keep original directory names and its structure (see: "Preferences - Sketchbook location" + tools)
3. Restart Arduino IDE, open [gbs-control.ino](./gbs-control.ino) file.
    >At this point `/data` directory must exist in the Project root.
4. From the Tools menu, select “ESP8266 LittleFS Data Upload“. The contents of `/data` directory will be converted into a binary image file and the upload process will begin.

## OTA update

>***A word of warning:***\
Do not interrupt the network connection or upload process while updating via OTA. Your device may stop working properly.

Make sure you've enabled OTA mode in Control panel of GBSС.

### Using Platformio IDE

1. Open [platformio.ini](./platformio.ini) and uncomment ```upload_protocol, upload_port``` options. Option ```upload_port``` should be equal to the IP address of your GBSС (ex.: upload_port = 192.168.4.1)
2. Now go to "Platformio - Build and Upload" the firmware.

### Using Arduino IDE

1. Open sketch (gbs-control.ino). Make sure you already completed the steps 1-5 from ["Build and Upload - Using Arduino IDE"](#build-n-upload-arduino)
2. Go to "Tools - Ports". At the very end of dropdown menu find and chouse your device.
3. Proceed with build/upload.

For more details visit: https://github.com/JAndrassy/ArduinoOTA/blob/master/README.md


## Theory of operation

### Slots vs Presets

- Preset - is a collection of predefined runtime values (including output screen resolution, etc). 
  - Custom presets - a collection of predefined runtime values stored on GBSC filesystem. Custom presets are created and updated together with correspondng Slot.
- Slot - is a collection of Presets.
- Preferences - a collection of parameters which are used system-wide independently of Slots and Presets. So when Slot changes which causes Preset to change, Preferences will remain the same until user not to switch them manually.

When you connecting a new device, GBS tries to identify the input video signal. There are 9 signal types (VIDEO_ID), which also used to identify Preset for current Slot while saving/loading preferences. 
>Slots are managed by user while firmware takes care of Presets automatically, depending on input conditions.

While managing Slots the following principles must be taken into account:

1. General parameters of a SLOT are stored in slots.bin, where each of (SLOTS_TOTAL) Slots has structure:
     
    1.1. Slot name\
    <!-- 1.2. input video signal ID (VIDEO_ID)\ -->
    1.2. output resolution\
    ... etc.
    
     Each Slot has its own SLOT_ID which is one character long. The input video signal ID (VIDEO_ID) is not stored in Slots. 
     
     > This allows to use multiple Presets with the same Slot, when current Preset depends on the input video signal ID.
     
2. Preset file names are formatted as `input_video_format.SLOT_ID`, for example:

     - preset_ntsc.SLOT_ID
     - preset_pal.SLOT_ID
     - preset_ntsc_480p.SLOT_ID\
     ...etc.

3. Current/Active SLOT_ID and other auxiliar pararmeters are stored in preferencev2.txt file.

The following diagram represents the structure of system and user configurations:

![gbs-control user configuration structure](./doc/img/slot-preset-prefs.png)

### Vocabulary

- Auto Gain - the functionality or circuit the purpose of which is to maintain a suitable signal amplitude at its output, despite variation of the signal amplitude at the input.
- Clipping - a form of distortion that limits a signal once it exceeds a threshold. Clipping may occur when a signal is recorded by a sensor that has constraints on the range of data it can measure, it can occur when a signal is digitized, or it can occur any other time an analog or digital signal is transformed, particularly in the presence of gain or overshoot and undershoot. [[?]](https://en.wikipedia.org/wiki/Clipping_%28signal_processing)
- FrameTime - the time a frame takes to be rendered and displayed.
- Deinterlacing - the process of converting interlaced video into a non-interlaced or progressive form. [[?]](https://en.wikipedia.org/wiki/Deinterlacing)
  - Bob (a.k.a linear spatial) - vertical interpolation from the same frame, this method interpolates the missing pixels from the pixels located directly above and below them in the same frame. The Bob method avoids motion artifacts, but at the cost of considerable vertical detail.
  - Line doubling (a.k.a linear temporal, weave, static mesh) - this method generates the missing pixels by copying the corresponding pixels from the previous frame (meshing two fields together to create a single frame).
  - Motion adaptive - the method of dynamic weights (functions of measured pixel based motion), it combines the best aspects of both Bob and Weave by isolating the de-interlacing compensation to the pixel level. Spatial and temporal comparisons are performed to decide whether or not an individual pixel has motion. Areas of no motion are statically meshed (weave) and areas where motion is detected are treated with a proprietary filtering technique resulting in very high quality, progressive-scan images.
- Interlacing - a type of video scanning where each frame is made up of 2 images that divide it horizontally by alternating pixel lines (the even and the odd). [[?]](https://en.wikipedia.org/wiki/Interlaced_video)
- Peaking - it takes the high resolution RGB video signal and increases its signal amplitude (the higher peaking the sharper image with more details).
- Progressive scanning (a.k.a noninterlaced scanning) - a format of displaying, storing, or transmitting moving images in which all the lines of each frame are drawn in sequence. [[?]](https://en.wikipedia.org/wiki/Progressive_scan)
- RGBs (Red, Green, Blue and sync) - both the horizontal and vertical sync signals are combined into this one line. [[?]](https://www.retrorgb.com/sync.html)
- RGBHV (RGB Horizontal sync Vertical sync) - is essentially the same as RGBs, however the horizontal and vertical sync signals are sent down their own individual lines, totaling 5 channels. "VGA" uses RGBHV.
- RGsB (Sync on green) - the green cable also carries the horizontal and vertical sync signals, totaling only three cables. The only time you’ll run into RGsB in the classic gaming world is with the PlayStation 2 and it’s usually just better to use component video.
- Scanline - one row in a raster scanning pattern, such as a line of video on a CRT display.
- Step Response - system response to the step input.
- YUV - is the color model found in the PAL analogue color TV standard (excluding PAL-N). A color is described as a Y component (luma) and two chroma components U and V (colorspaces that are encoded using YCbCr).

### How-to switch GBS-Control to upload mode?

ESP8266 version of the factory built GBSC, boots into firmware upload mode by pressing the knob button while the device is off and connecting it to a computer with USB cable.

## TODO:

- [ ] Full height doesn't work (?)
- [ ] Invert Sync issue
- [ ] PassThrough doesn't work
- [ ] preferScalingRgbhv doesn't work (?)
- [x] device disconnects from WiFi when displaying Output status
- [x] disable ambigous preset paramters inside slots
  - [ ] startup runtime parameters loading malfunction
- [ ] De-interlacer noise reduction ctl.
- [ ] color artifacts on high SDRAM refresh rates
- [x] fix OLEDMenu items list offset
- [x] feature request [#553]
- [x] creation/destruction OLEDMenuItem procedure

## Additional information

See [/doc/developer_guide.md](./doc/DEVELOPER_GUIDE.md).

## Old documentation

https://ramapcsx2.github.io/gbs-control