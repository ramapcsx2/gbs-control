<!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <title>GBS-Control</title>
    <link
      rel="manifest"
      href="${manifest}"
    />
    <style>${styles}</style>
    <meta name="apple-mobile-web-app-capable" content="yes" />
    <link
      rel="apple-touch-icon"
      href="${icon1024}"
    />
    <meta name="apple-mobile-web-app-status-bar-style" content="black" />
    <meta
      name="viewport"
      content="viewport-fit=cover, user-scalable=no, width=device-width, initial-scale=1, maximum-scale=1"
    />
  </head>
  <body tabindex="0" class="hide-help">
    <div class="container">
      <div class="menu">
        <svg
          version="1.0"
          xmlns="http://www.w3.org/2000/svg"
          viewBox="0,0,284,99"
          width="75"
          class="menu--logo"
        >
          <path
            fill-rule="evenodd"
            clip-rule="evenodd"
            fill="#010101"
            d="M283.465 97.986H0V0h283.465v97.986z"
          />
          <path
            fill-rule="evenodd"
            clip-rule="evenodd"
            fill="#00c0fb"
            d="M270.062 75.08V60.242h-17.04v10.079c0 2.604-2.67 5.02-5.075 5.02h-20.529c-4.983 0-5.43-4.23-5.43-8.298v-37.93c0-2.668 1.938-4.863 4.88-4.863h20.995c2.684 0 5.158 1.492 5.158 4.482V38.86h17.04V27.63c0-7.867-4.26-15.923-13.039-15.923H221.19c-7.309 0-15.604 4.235-15.604 12.652v50.387c0 6.508 4.883 13.068 12.42 13.068h38.47c6.606 0 13.587-5.803 13.587-12.734zM190.488 5.562H6.617L6.585 91.91h183.91l-.007-86.348z"
          />
          <text
            transform="translate(12.157 81.95)"
            fill="#010101"
            font-family="'AmsiPro-BoldItalic'"
            font-size="92.721"
            letter-spacing="-9"
          >
            GBS
          </text>
          <g>
            <path
              fill-rule="evenodd"
              clip-rule="evenodd"
              fill="#010101"
              d="M586.93 97.986H303.464V0h283.464v97.986z"
            />
            <path
              fill-rule="evenodd"
              clip-rule="evenodd"
              fill="#FFF"
              d="M573.526 75.08V60.242h-17.04v10.079c0 2.604-2.669 5.02-5.075 5.02h-20.528c-4.984 0-5.43-4.23-5.43-8.298v-37.93c0-2.668 1.937-4.863 4.88-4.863h20.995c2.683 0 5.157 1.492 5.157 4.482V38.86h17.04V27.63c0-7.867-4.26-15.923-13.038-15.923h-35.833c-7.31 0-15.605 4.235-15.605 12.652v50.387c0 6.508 4.884 13.068 12.42 13.068h38.471c6.606 0 13.586-5.803 13.586-12.734zM493.953 5.562H310.08l-.032 86.348h183.91l-.006-86.348z"
            />
            <text
              transform="translate(315.621 81.95)"
              fill="#010101"
              font-family="'AmsiPro-BoldItalic'"
              font-size="92.721"
              letter-spacing="-9"
            >
              GBS
            </text>
          </g>
        </svg>
        <button section="pressets" class="btn material-icons" active>
          input
        </button>
        <button section="control" class="btn material-icons">
          control_camera
        </button>
        <button section="filters" class="btn material-icons">blur_on</button>
        <button section="preferences" class="btn material-icons">tune</button>
        <button section="developer" class="btn material-icons">
          developer_mode
        </button>
        <button section="system" class="btn material-icons">
          dynamic_form
        </button>
      </div>
      <div class="scroll">
        <section name="pressets">
          <fieldset class="fieldset pressets">
            <legend>
              <div class="material-icons">input</div>
              <div>Presets</div>
            </legend>

            <div class="row">
              <button
                class="btn"
                ws-user="b"
                id="slot1"
                role="slot"
                name="slot 1"
              ></button>
              <button
                class="btn"
                ws-user="c"
                id="slot2"
                role="slot"
                name="slot 2"
              ></button>
              <button
                class="btn"
                ws-user="d"
                id="slot3"
                role="slot"
                name="slot 3"
              ></button>
            </div>
            <div class="row">
              <button
                class="btn"
                ws-user="j"
                id="slot4"
                role="slot"
                name="slot 4"
              ></button>
              <button
                class="btn"
                ws-user="k"
                id="slot5"
                role="slot"
                name="slot 5"
              ></button>
              <button
                class="btn"
                ws-user="G"
                id="slot6"
                role="slot"
                name="slot 6"
              ></button>
            </div>
            <div class="row mb-16">
              <button
                class="btn"
                ws-user="H"
                id="slot7"
                role="slot"
                name="slot 7"
              ></button>
              <button
                class="btn"
                ws-user="I"
                id="slot8"
                role="slot"
                name="slot 8"
              ></button>
              <button
                class="btn"
                ws-user="J"
                id="slot9"
                role="slot"
                name="slot 9"
              ></button>
            </div>

            <button
              class="btn secondary"
              ws-user="3"
              id="buttonLoadCustomPreset"
              role="presset"
            >
              <div class="material-icons">play_arrow</div>
              <div>load presset</div>
            </button>
            <button class="btn secondary" onclick="savePresset()">
              <div class="material-icons">fiber_manual_record</div>
              <div>save presset</div>
            </button>
            <p class="text-small">
              If you want to save your customizations, first select a slot for
              your new preset, then save to or load from that slot. Selecting a
              slot also makes it the new startup preset.
            </p>
          </fieldset>
        </section>

        <section name="control" hidden>
          <fieldset class="fieldset">
            <legend>
              <div class="material-icons">tv</div>
              <div>Resolution</div>
            </legend>
            <div class="fixed-width mb-16">
              <div>
                <div class="mb-16 resolution">
                  <button
                    class="btn"
                    ws-user="s"
                    id="button1920x1080"
                    role="presset"
                  >
                    1920 x1080
                  </button>
                  <button
                    class="btn"
                    ws-user="p"
                    id="button1280x1024"
                    role="presset"
                  >
                    1280 x1024
                  </button>
                  <button
                    class="btn"
                    ws-user="f"
                    id="button1280x960"
                    role="presset"
                  >
                    1280 x960
                  </button>
                  <button
                    class="btn"
                    ws-user="g"
                    id="button1280x720"
                    role="presset"
                  >
                    1280 x720
                  </button>
                  <button
                    class="btn small"
                    ws-user="h"
                    id="button720x480"
                    role="presset"
                  >
                    720x480 768x576
                  </button>
                </div>
                <div class="resolution-down">
                  <button
                    ws-user="L"
                    class="btn secondary -block"
                    id="button15kHzScaleDown"
                    role="presset"
                  >
                    15kHz
                    <div>DownScale</div>
                  </button>
                  <button
                    ws-user="K"
                    class="btn secondary -block"
                    id="buttonSourcePassThrough"
                    role="presset"
                  >
                    Source
                    <div>Pass-through</div>
                  </button>
                </div>
              </div>
            </div>
            <p class="text-small">
              Choose an output resolution from these presets. Your selection
              will also be used for startup. 1280x960 is recommended for NTSC
              sources, 1280x1024 for PAL. Use the "Matched Presets" option to
              switch between the two automatically (Preferences tab)
            </p>
          </fieldset>

          <fieldset class="fieldset controls">
            <legend>
              <div class="material-icons">control_camera</div>
              <div>Picture Control</div>
            </legend>
            <div class="fixed-width">
              <div>
                <button
                  class="btn material-icons secondary"
                  controls-key="left"
                >
                  keyboard_arrow_left
                </button>
                <button class="btn material-icons secondary" controls-key="up">
                  keyboard_arrow_up
                </button>
                <button
                  class="btn material-icons secondary"
                  controls-key="right"
                >
                  keyboard_arrow_right
                </button>
              </div>
              <div class="mb-16">
                <button class="btn material-icons" disabled>south_west</button>
                <button
                  class="btn material-icons secondary"
                  controls-key="down"
                >
                  keyboard_arrow_down
                </button>
                <button class="btn material-icons" disabled>south_east</button>
              </div>
              <div>
                <button class="btn" controls="move" active>
                  <div class="material-icons">open_with</div>
                  <div>move</div>
                </button>
                <button class="btn" controls="scale">
                  <div class="material-icons">zoom_out_map</div>
                  <div>scale</div>
                </button>
                <button class="btn" controls="borders">
                  <div class="material-icons">crop_free</div>
                  <div>borders</div>
                </button>
              </div>
            </div>
          </fieldset>

          <fieldset class="fieldset controls-desktop">
            <legend>
              <div class="material-icons">control_camera</div>
              <div>Picture Control</div>
            </legend>
            <div class="fixed-width">
              <button active class="btn direction">
                <div class="material-icons">open_with</div>
                <div>move</div>
              </button>
              <div class="keyboard">
                <div>
                  <button ws-action="7" class="btn material-icons secondary">
                    keyboard_arrow_left
                  </button>
                  <button ws-action="*" class="btn material-icons secondary">
                    keyboard_arrow_up
                  </button>
                  <button ws-action="6" class="btn material-icons secondary">
                    keyboard_arrow_right
                  </button>
                </div>

                <div class="mb-16">
                  <button class="btn material-icons" disabled>
                    south_west
                  </button>
                  <button ws-action="/" class="btn material-icons secondary">
                    keyboard_arrow_down
                  </button>
                  <button class="btn material-icons" disabled>
                    south_east
                  </button>
                </div>
              </div>
            </div>
            <div class="fixed-width">
              <button class="btn direction" active>
                <div class="material-icons">zoom_out_map</div>
                <div>scale</div>
              </button>
              <div class="keyboard">
                <div>
                  <button ws-action="h" class="btn material-icons secondary">
                    keyboard_arrow_left
                  </button>
                  <button ws-action="4" class="btn material-icons secondary">
                    keyboard_arrow_up
                  </button>
                  <button ws-action="z" class="btn material-icons secondary">
                    keyboard_arrow_right
                  </button>
                </div>

                <div class="mb-16">
                  <button class="btn material-icons" disabled>
                    south_west
                  </button>
                  <button ws-action="5" class="btn material-icons secondary">
                    keyboard_arrow_down
                  </button>
                  <button class="btn material-icons" disabled>
                    south_east
                  </button>
                </div>
              </div>
            </div>
            <div class="fixed-width">
              <button class="btn direction" active>
                <div class="material-icons">crop_free</div>
                <div>borders</div>
              </button>
              <div class="keyboard">
                <div>
                  <button ws-user="B" class="btn material-icons secondary">
                    keyboard_arrow_left
                  </button>
                  <button ws-user="C" class="btn material-icons secondary">
                    keyboard_arrow_up
                  </button>
                  <button ws-user="A" class="btn material-icons secondary">
                    keyboard_arrow_right
                  </button>
                </div>

                <div class="mb-16">
                  <button class="btn material-icons" disabled>
                    south_west
                  </button>
                  <button ws-user="D" class="btn material-icons secondary">
                    keyboard_arrow_down
                  </button>
                  <button class="btn material-icons" disabled>
                    south_east
                  </button>
                </div>
              </div>
            </div>
          </fieldset>
        </section>

        <section name="filters" hidden>
          <fieldset class="fieldset filters">
            <legend>
              <div class="material-icons">wb_sunny</div>
              <div>ADC Gain (brightness)</div>
            </legend>

            <div class="fixed-width mb-16">
              <button class="btn" gain-key="+">
                <div class="material-icons">add_circle_outline</div>
                <div>gain</div>
              </button>
              <button class="btn" gain-key="-">
                <div class="material-icons">remove_circle_outline</div>
                <div>gain</div>
              </button>
              <button ws-action="T" toggle="adcAutoGain" class="btn secondary">
                <div class="material-icons">brightness_auto</div>
                <div>Auto Gain</div>
              </button>
            </div>
            <p class="text-small">
              Gain +/- adjusts the gain for the currently loaded preset.
            </p>
          </fieldset>
          <fieldset class="fieldset filters">
            <legend>
              <div class="material-icons">blur_on</div>
              <div>Filters</div>
            </legend>
            <div class="fixed-width mb-16">
              <div class="mb-16">
                <button ws-user="7" toggle="scanlines" class="btn secondary">
                  <div class="material-icons">line_weight</div>
                  <div>scanlines</div>
                </button>
                <button ws-user="K" class="btn">
                  <div class="material-icons">line_weight</div>
                  <div>strength</div>
                </button>
                <button
                  ws-user="m"
                  toggle="vdsLineFilter"
                  class="btn secondary"
                >
                  <div class="material-icons">texture</div>
                  <div>line filter</div>
                </button>
              </div>
              <button ws-action="f" toggle="peaking" class="btn secondary">
                <div class="material-icons">blur_linear</div>
                <div>peaking</div>
              </button>
              <button ws-action="V" toggle="step" class="btn secondary">
                <div class="material-icons">texture</div>
                <div>step response</div>
              </button>
            </div>
            <p class="text-small">
              Scanlines only work with 240p sources. They look best with the
              Line Filter enabled. Peaking and Step Response are subtle
              sharpening filters and recommended.
            </p>
          </fieldset>
        </section>

        <section name="preferences" hidden>
          <fieldset class="fieldset">
            <legend>
              <div class="material-icons">tune</div>
              <div>Settings</div>
            </legend>
            <table>
              <tr>
                <td>
                  Matched Presets
                  <p class="text-small">
                    If enabled, default to 1280x960 for NTSC 60 and 1280x1024
                    for PAL 50 (does not apply for 720p / 1080p presets).
                  </p>
                </td>
                <td
                  ws-action="Z"
                  class="material-icons"
                  toggle-switch="matched"
                >
                  toggle_off
                </td>
              </tr>
              <tr>
                <td>
                  Full Height
                  <p class="text-small">
                    Some presets default to not using the entire vertical output
                    resolution, leaving some lines black. With Full Height
                    enabled, these presets will instead scale to fill more of
                    the screen height. (This currently only affects 1920 x 1080)
                  </p>
                </td>
                <td
                  ws-user="v"
                  class="material-icons"
                  toggle-switch="fullHeight"
                >
                  toggle_off
                </td>
              </tr>
              <tr>
                <td>
                  Low Res: Use Upscaling
                  <p class="text-small">
                    Some presets default to not using the entire vertical output
                    resolution, leaving some lines black. With Full Height
                    enabled, these presets will instead scale to fill more of
                    the screen height. (This currently only affects 1920 x 1080)
                  </p>
                </td>
                <td
                  ws-user="x"
                  class="material-icons"
                  toggle-switch="preferScalingRgbhv"
                >
                  toggle_off
                </td>
              </tr>
              <tr>
                <td>
                  RGBHV/Component Toggle
                  <p class="text-small">
                    The default output mode is RGBHV, suitable for use with VGA
                    cables or HDMI converters. An experimental YPbPr mode can
                    also be selected. Compatibility is still spotty.
                  </p>
                </td>
                <td
                  ws-user="L"
                  class="material-icons"
                  toggle-switch="wantOutputComponent"
                >
                  toggle_off
                </td>
              </tr>
              <tr>
                <td>
                  Force PAL 60Hz
                  <p class="text-small">
                    Output Frame Rate: Force PAL 50Hz to 60Hz If your TV does
                    not support 50Hz sources (displaying unknown format, no
                    matter the preset), try this option. The frame rate will not
                    be as smooth. Reboot required.
                  </p>
                </td>
                <td
                  ws-action="0"
                  class="material-icons"
                  toggle-switch="palForce60"
                >
                  toggle_off
                </td>
              </tr>
              <tr>
                <td>
                  ADC calibration
                  <p class="text-small">
                    ADC auto offset calibration Gbscontrol calibrates the ADC
                    offsets on startup. In case of color shift problems, try
                    disabling this function.
                  </p>
                </td>
                <td
                  ws-user="w"
                  class="material-icons"
                  toggle-switch="enableCalibrationADC"
                >
                  toggle_off
                </td>
              </tr>
              <tr>
                <td colspan="2" class="settings-title">
                  Active FrameTime Lock
                  <div class="text-small">
                    <p>
                      This option keeps the input and output timings aligned,
                      fixing the horizontal tear line that can appear sometimes.
                    </p>
                    <p>
                      Two methods are available. Try switching methods if your
                      display goes blank.
                    </p>
                  </div>
                </td>
              </tr>
              <tr>
                <td class="pl-16">FrameTime Lock</td>
                <td
                  class="material-icons"
                  ws-user="5"
                  toggle-switch="frameTimeLock"
                >
                  toggle_off
                </td>
              </tr>
              <tr>
                <td class="pl-16">Switch Lock Method</td>
                <td class="material-icons" ws-user="i" style="cursor: pointer">
                  swap_horiz
                </td>
              </tr>
              <tr>
                <td colspan="2" class="settings-title">
                  Deinterlace Method
                  <div class="text-small">
                    <p>
                      Gbscontrol detects interlaced content and automatically
                      toggles deinterlacing.
                    </p>
                    - Bob Method: essentially no deinterlacing, no added lag but
                    flickers<br />
                    - Motion Adaptive: removes flicker, adds 1 frame of lag and
                    shows some artefacts in moving details
                    <p>
                      If possible, configure the source for progressive output.
                      Otherwise, using Motion Adaptive is recommended.
                    </p>
                  </div>
                </td>
              </tr>
              <tr>
                <td class="pl-16">Motion Adaptive</td>
                <td ws-user="r" class="material-icons" toggle-switch="bob">
                  toggle_off
                </td>
              </tr>
              <tr>
                <td class="pl-16">Bob</td>
                <td
                  ws-user="q"
                  class="material-icons"
                  toggle-switch="motionAdaptive"
                >
                  toggle_off
                </td>
              </tr>
            </table>
          </fieldset>
        </section>

        <section name="developer" hidden>
          <fieldset class="fieldset">
            <legend>
              <div class="material-icons">input</div>
              <div>Developer</div>
            </legend>
            <div class="small-buttons">
              <p class="text-small">Move Picture (memory blank, HS)</p>
              <button ws-action="-" class="btn secondary">
                <div class="material-icons">keyboard_arrow_left</div>
                <div>Left</div>
              </button>
              <button ws-action="+" class="btn secondary">
                <div class="material-icons">keyboard_arrow_right</div>
                <div>Right</div>
              </button>
              <button ws-action="1" class="btn secondary">
                <div class="material-icons">keyboard_arrow_left</div>
                <div>HS Left</div>
              </button>
              <button ws-action="0" class="btn secondary">
                <div class="material-icons">keyboard_arrow_right</div>
                <div>HS Right</div>
              </button>
            </div>
            <div class="commands">
              <p class="text-small">Information</p>
              <button ws-action="i" class="btn">
                <div class="material-icons">info</div>
                <div>Print Infos</div>
              </button>
              <button ws-action="," class="btn">
                <div class="material-icons">alarm</div>
                <div>Get Video Timings</div>
              </button>
            </div>

            <div>
              <p class="text-small">Commands</p>
              <button ws-user="F" class="btn freeze mb-16">
                <div class="material-icons">add_a_photo</div>
                <div>Freeze Capture</div>
              </button>
            </div>

            <div class="commands">
              <button ws-action="F" class="btn">
                <div class="material-icons">wb_sunny</div>
                <div>ADC Filter</div>
              </button>
              <button ws-user="l" class="btn">
                <div class="material-icons">memory</div>
                <div>Cycle SDRAM</div>
              </button>
              <button ws-action="D" class="btn">
                <div class="material-icons">bug_report</div>
                <div>Debug View</div>
              </button>
              <button ws-action="c" class="btn">
                <div class="material-icons">system_update_alt</div>
                <div>Enable OTA</div>
              </button>
              <button ws-action="a" class="btn">
                <div class="material-icons">add_circle_outline</div>
                <div>HTotal++</div>
              </button>
              <button ws-action="A" class="btn">
                <div class="material-icons">remove_circle_outline</div>
                <div>HTotal--</div>
              </button>
              <button ws-action="." class="btn secondary">
                <div class="material-icons">sync_problem</div>
                <div>Resync HTotal</div>
              </button>
              <button ws-action="n" class="btn">
                <div class="material-icons">calculate</div>
                <div>PLL divider++</div>
              </button>
              <button ws-action="8" class="btn">
                <div class="material-icons">invert_colors</div>
                <div>Invert Sync</div>
              </button>
              <button ws-action="m" class="btn">
                <div class="material-icons">devices_other</div>
                <div>SyncWatcher</div>
              </button>

              <button ws-action="l" class="btn secondary">
                <div class="material-icons">settings_backup_restore</div>
                <div>SyncProcessor</div>
              </button>
              <button ws-action="o" class="btn">
                <div class="material-icons">insights</div>
                <div>Oversampling</div>
              </button>
              <button ws-action="S" class="btn">
                <div class="material-icons">settings_input_hdmi</div>
                <div>60/50Hz HDMI</div>
              </button>

              <button ws-user="E" class="btn">
                <div class="material-icons">bug_report</div>
                <div>IF Auto Offset</div>
              </button>
              <button ws-user="z" class="btn">
                <div class="material-icons">format_align_justify</div>
                <div>SOG Level--</div>
              </button>

              <button ws-action="q" class="btn secondary">
                <div class="material-icons">model_training</div>
                <div>Reset Chip</div>
              </button>
            </div>
          </fieldset>
        </section>

        <section name="system" hidden>
          <fieldset class="fieldset system">
            <legend>
              <div class="material-icons">input</div>
              <div>System</div>
            </legend>
            <div class="mb-16">
              <button class="btn" onclick="window.location.href='/wifi.htm'">
                <div class="material-icons">wifi</div>
                <div>Connect</div>
              </button>
              <button ws-user="e" class="btn">
                <div class="material-icons">list</div>
                <div>List Options</div>
              </button>
              <button ws-user="a" class="btn">
                <div class="material-icons">settings_backup_restore</div>
                <div>Restart</div>
              </button>
            </div>
            <button ws-user="1" class="btn secondary restart">
              <div class="material-icons">offline_bolt</div>
              <div>Reset Defaults + Restart</div>
            </button>
          </fieldset>
        </section>

        <div class="output">
          <fieldset class="fieldset fieldset-output">
            <legend>
              <div class="material-icons">code</div>
              <div>Output</div>
            </legend>
            <button class="btn clear material-icons">delete_outline</button>
            <textarea id="outputTextArea"></textarea>
          </fieldset>
        </div>
      </div>
    </div>
    <div class="toaster" id="overlayNoWs">
      <div class="material-icons blink_me">signal_wifi_off</div>
    </div>
    <script>${js}</script>
  </body>
</html>
