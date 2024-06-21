<!doctype html>
<html lang="en">

<head>
    <meta charset="UTF-8" />
    <title>GBS-Control</title>
    <link rel="manifest" href="${manifest}" />
    <style>${styles}</style>
    <meta name="apple-mobile-web-app-capable" content="yes" />
    <link rel="icon" type="image/png" href="${favicon}" />
    <link rel="apple-touch-icon" href="${icon1024}" />
    <meta name="apple-mobile-web-app-status-bar-style" content="black" />
    <meta name="viewport"
        content="viewport-fit=cover, user-scalable=no, width=device-width, initial-scale=1, maximum-scale=1" />
</head>

<body tabindex="0" class="gbs-help-hide gbs-output-hide">
    <div class="gbs-container">
        <div class="gbs-menu">
            <svg version="1.0" xmlns="http://www.w3.org/2000/svg" viewBox="0,0,284,99" class="gbs-menu__logo">
                <path fill-rule="evenodd" clip-rule="evenodd" fill="#010101" d="M283.465 97.986H0V0h283.465v97.986z" />
                <path fill-rule="evenodd" clip-rule="evenodd" fill="#00c0fb"
                    d="M270.062 75.08V60.242h-17.04v10.079c0 2.604-2.67 5.02-5.075 5.02h-20.529c-4.983 0-5.43-4.23-5.43-8.298v-37.93c0-2.668 1.938-4.863 4.88-4.863h20.995c2.684 0 5.158 1.492 5.158 4.482V38.86h17.04V27.63c0-7.867-4.26-15.923-13.039-15.923H221.19c-7.309 0-15.604 4.235-15.604 12.652v50.387c0 6.508 4.883 13.068 12.42 13.068h38.47c6.606 0 13.587-5.803 13.587-12.734zM190.488 5.562H6.617L6.585 91.91h183.91l-.007-86.348z" />
                <text transform="translate(12.157 81.95)" fill="#010101" font-family="'AmsiPro-BoldItalic'"
                    font-size="92.721" letter-spacing="-9">
                    GBS
                </text>
                <g>
                    <path fill-rule="evenodd" clip-rule="evenodd" fill="#010101"
                        d="M586.93 97.986H303.464V0h283.464v97.986z" />
                    <path fill-rule="evenodd" clip-rule="evenodd" fill="#FFF"
                        d="M573.526 75.08V60.242h-17.04v10.079c0 2.604-2.669 5.02-5.075 5.02h-20.528c-4.984 0-5.43-4.23-5.43-8.298v-37.93c0-2.668 1.937-4.863 4.88-4.863h20.995c2.683 0 5.157 1.492 5.157 4.482V38.86h17.04V27.63c0-7.867-4.26-15.923-13.038-15.923h-35.833c-7.31 0-15.605 4.235-15.605 12.652v50.387c0 6.508 4.884 13.068 12.42 13.068h38.471c6.606 0 13.586-5.803 13.586-12.734zM493.953 5.562H310.08l-.032 86.348h183.91l-.006-86.348z" />
                    <text transform="translate(315.621 81.95)" fill="#010101" font-family="'AmsiPro-BoldItalic'"
                        font-size="92.721" letter-spacing="-9">
                        GBS
                    </text>
                </g>
            </svg>
            <div class="gbs-menu__button-group">
                <button gbs-section="presets" class="gbs-button gbs-button__menu gbs-icon" active>
                    input
                </button>
                <button gbs-section="control" class="gbs-button gbs-button__menu gbs-icon">
                    control_camera
                </button>
                <button gbs-section="filters" class="gbs-button gbs-button__menu gbs-icon">
                    blur_on
                </button>
            </div>
            <button gbs-section="preferences" class="gbs-button gbs-button__menu gbs-icon gbs-button__uwidth">
                tune
            </button>
            <button gbs-section="developer" class="gbs-button gbs-button__menu gbs-icon gbs-button__uwidth" hidden>
                developer_mode
            </button>
            <button gbs-section="system" class="gbs-button gbs-button__menu gbs-icon gbs-button__uwidth">
                bolt
            </button>
        </div>
        <div class="gbs-scroll">
            <section name="presets">
                <fieldset class="gbs-fieldset presets">
                    <legend class="gbs-fieldset__legend gbs-fieldset__legend--help">
                        <div class="gbs-icon">input</div>
                        <div>L{SLOTS}</div>
                    </legend>
                    <!-- prettier-ignore -->
                    <ul class="gbs-help">
                        <li>L{SLOTS_HELP_1}</li>
                    </ul>
                    <div class="gbs-presets" gbs-slot-html></div>
                    <div class="gbs-flex">
                        <button class="gbs-button gbs-button__control-action" gbs-element-ref="buttonRemoveSlot"
                            disabled>
                            <div class="gbs-icon">delete</div>
                            <div>L{REMOVE_SLOT}</div>
                        </button>
                        <button class="gbs-button gbs-button__control-action gbs-button__secondary" gbs-element-ref="buttonCreateSlot"
                            active>
                            <div class="gbs-icon">fiber_manual_record</div>
                            <div>L{SAVE_PRESET}</div>
                        </button>
                    </div>
                </fieldset>
            </section>

            <section name="control" hidden>
                <fieldset class="gbs-fieldset" style="padding: 8px 2px">
                    <legend class="gbs-fieldset__legend gbs-fieldset__legend--help">
                        <div class="gbs-icon">aspect_ratio</div>
                        <div>L{OUTPUT_RESOLUTION}</div>
                    </legend>
                    <!-- prettier-ignore -->
                    <ul class="gbs-help">
                        <li>L{OUT_RES_HELP_1}</li>
                        <li>L{OUT_RES_HELP_2}</li>
                    </ul>
                    <div class="gbs-resolution">
                        <div class="gbs-flex gbs-margin__bottom--8">
                            <button class="gbs-button gbs-button__resolution" gbs-message="s" gbs-message-type="user"
                                gbs-click="normal" gbs-element-ref="button1080p" gbs-role="preset">
                                1920 x 1080
                            </button>
                            <button class="gbs-button gbs-button__resolution" gbs-message="p" gbs-message-type="user"
                                gbs-click="normal" gbs-element-ref="button1024p" gbs-role="preset">
                                1280 x 1024
                            </button>
                            <button class="gbs-button gbs-button__resolution" gbs-message="f" gbs-message-type="user"
                                gbs-click="normal" gbs-element-ref="button960p" gbs-role="preset">
                                1280 x 960
                            </button>
                            <button class="gbs-button gbs-button__resolution" gbs-message="g" gbs-message-type="user"
                                gbs-click="normal" gbs-element-ref="button720p" gbs-role="preset">
                                1280 x 720
                            </button>
                            <button class="gbs-button gbs-button__resolution" gbs-message="k" gbs-message-type="user"
                                gbs-click="normal" gbs-element-ref="button576p" gbs-role="preset">
                                768 x 576
                            </button>
                            <button class="gbs-button gbs-button__resolution" gbs-message="h" gbs-message-type="user"
                                gbs-click="normal" gbs-element-ref="button480p" gbs-role="preset">
                                720 x 480
                            </button>
                            <button class="gbs-button gbs-button__resolution" gbs-message="j" gbs-message-type="user"
                                gbs-click="normal" gbs-element-ref="button240p" gbs-role="preset">
                                240p
                            </button>
                        </div>
                        <div class="gbs-flex">
                            <button gbs-message="L" gbs-message-type="user" gbs-click="normal"
                                class="gbs-button gbs-button__resolution gbs-button__resolution--center gbs-button__secondary"
                                gbs-element-ref="button15kHz" gbs-role="preset">
                                <div class="gbs-icon">tv</div>
                                <div>15KHz/L{DOWNSCALE}</div>
                            </button>
                            <button gbs-message="K" gbs-message-type="action" gbs-click="normal"
                                class="gbs-button gbs-button__resolution gbs-button__resolution--center gbs-button__secondary"
                                gbs-element-ref="buttonSourcePassThrough" gbs-role="preset">
                                <div class="gbs-icon">swap_calls</div>
                                <!-- <div class="gbs-button__resolution--pass-through"> -->
                                <div>
                                    L{PASS_THROUGH}
                                </div>
                            </button>
                        </div>
                    </div>
                </fieldset>
                <fieldset class="gbs-fieldset">
                    <legend class="gbs-fieldset__legend gbs-fieldset__legend--help">
                        <div class="gbs-icon">wb_sunny</div>
                        <div>L{ADC_GAIN_LEGEND}</div>
                    </legend>
                    <!-- prettier-ignore -->
                    <ul class="gbs-help">
                        <li>L{ADC_GAIN_LEGEND_HELP_1}</li>
                    </ul>
                    <div class="gbs-flex gbs-margin__bottom--16">
                        <button gbs-message="o" gbs-message-type="user" gbs-click="repeat"
                            class="gbs-button gbs-button__control">
                            <div class="gbs-icon">
                                remove_circle_outline
                            </div>
                            <div>L{GAIN_BUTTON}</div>
                        </button>
                        <button gbs-message="n" gbs-message-type="user" gbs-click="repeat"
                            class="gbs-button gbs-button__control">
                            <div class="gbs-icon">add_circle_outline</div>
                            <div>L{GAIN_BUTTON}</div>
                        </button>
                        <button gbs-message="T" gbs-message-type="action" gbs-click="normal" gbs-toggle="adcAutoGain"
                            class="gbs-button gbs-button__control gbs-button__secondary">
                            <div class="gbs-icon">brightness_auto</div>
                            <div>L{AUTO_GAIN_BUTTON}</div>
                        </button>
                    </div>
                </fieldset>
                <fieldset class="gbs-fieldset gbs-controls">
                    <legend class="gbs-fieldset__legend">
                        <div class="gbs-icon">control_camera</div>
                        <div>L{PICTURE_CONTROL_LEGEND}</div>
                    </legend>
                    <div class="gbs-flex">
                        <button class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
                            gbs-control-key="left">
                            keyboard_arrow_left
                        </button>
                        <button class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
                            gbs-control-key="up">
                            keyboard_arrow_up
                        </button>
                        <button class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
                            gbs-control-key="right">
                            keyboard_arrow_right
                        </button>
                    </div>
                    <div class="gbs-flex gbs-margin__bottom--16">
                        <button class="gbs-button gbs-button__control gbs-icon" disabled>
                            south_west
                        </button>
                        <button class="gbs-button gbs-button__control gbs-icon gbs-button__secondary"
                            gbs-control-key="down">
                            keyboard_arrow_down
                        </button>
                        <button class="gbs-button gbs-button__control gbs-icon" disabled>
                            south_east
                        </button>
                    </div>
                    <div class="gbs-flex">
                        <button class="gbs-button gbs-button__control" gbs-control-target="move" active>
                            <div class="gbs-icon">open_with</div>
                            <div>L{MOVE_BUTTON}</div>
                        </button>
                        <button class="gbs-button gbs-button__control" gbs-control-target="scale">
                            <div class="gbs-icon">zoom_out_map</div>
                            <div>L{SCALE_BUTTON}</div>
                        </button>
                        <button class="gbs-button gbs-button__control" gbs-control-target="borders">
                            <div class="gbs-icon">crop_free</div>
                            <div>L{BORDERS_BUTTON}</div>
                        </button>
                    </div>
                </fieldset>
                <fieldset class="gbs-fieldset gbs-controls__desktop">
                    <legend class="gbs-fieldset__legend">
                        <div class="gbs-icon">control_camera</div>
                        <div>L{PICTURE_CONTROL_LEGEND}</div>
                    </legend>
                    <div class="gbs-flex">
                        <button gbs-message="7" gbs-message-type="action" gbs-click="repeat"
                            class="gbs-button gbs-button__control gbs-icon gbs-button__secondary">
                            keyboard_arrow_left
                        </button>
                        <button gbs-message="*" gbs-message-type="action" gbs-click="repeat"
                            class="gbs-button gbs-button__control gbs-icon gbs-button__secondary">
                            keyboard_arrow_up
                        </button>
                        <button gbs-message="6" gbs-message-type="action" gbs-click="repeat"
                            class="gbs-button gbs-button__control gbs-icon gbs-button__secondary">
                            keyboard_arrow_right
                        </button>
                    </div>
                    <div class="gbs-flex gbs-margin__bottom--16">
                        <button class="gbs-button gbs-button__control" active>
                            <div class="gbs-icon">open_with</div>
                            <div>L{MOVE_BUTTON}</div>
                        </button>
                        <button gbs-message="/" gbs-message-type="action" gbs-click="repeat"
                            class="gbs-button gbs-button__control gbs-icon gbs-button__secondary">
                            keyboard_arrow_down
                        </button>
                        <button class="gbs-button gbs-button__control gbs-icon" disabled>
                            south_east
                        </button>
                    </div>

                    <div class="gbs-flex">
                        <button gbs-message="h" gbs-message-type="action" gbs-click="repeat"
                            class="gbs-button gbs-button__control gbs-icon gbs-button__secondary">
                            keyboard_arrow_left
                        </button>
                        <button gbs-message="4" gbs-message-type="action" gbs-click="repeat"
                            class="gbs-button gbs-button__control gbs-icon gbs-button__secondary">
                            keyboard_arrow_up
                        </button>
                        <button gbs-message="z" gbs-message-type="action" gbs-click="repeat"
                            class="gbs-button gbs-button__control gbs-icon gbs-button__secondary">
                            keyboard_arrow_right
                        </button>
                    </div>

                    <div class="gbs-flex gbs-margin__bottom--16">
                        <button class="gbs-button gbs-button__control" active>
                            <div class="gbs-icon">zoom_out_map</div>
                            <div>L{SCALE_BUTTON}</div>
                        </button>
                        <button gbs-message="5" gbs-message-type="action" gbs-click="repeat"
                            class="gbs-button gbs-button__control gbs-icon gbs-button__secondary">
                            keyboard_arrow_down
                        </button>
                        <button class="gbs-button gbs-button__control gbs-icon" disabled>
                            south_east
                        </button>
                    </div>
                    <div class="gbs-flex">
                        <button gbs-message="B" gbs-message-type="user" gbs-click="repeat"
                            class="gbs-button gbs-button__control gbs-icon gbs-button__secondary">
                            keyboard_arrow_left
                        </button>
                        <button gbs-message="C" gbs-message-type="user" gbs-click="repeat"
                            class="gbs-button gbs-button__control gbs-icon gbs-button__secondary">
                            keyboard_arrow_up
                        </button>
                        <button gbs-message="A" gbs-message-type="user" gbs-click="repeat"
                            class="gbs-button gbs-button__control gbs-icon gbs-button__secondary">
                            keyboard_arrow_right
                        </button>
                    </div>

                    <div class="gbs-flex gbs-margin__bottom--16">
                        <button class="gbs-button gbs-button__control" gbs-control-target="borders" active>
                            <div class="gbs-icon">crop_free</div>
                            <div>L{BORDERS_BUTTON}</div>
                        </button>
                        <button gbs-message="D" gbs-message-type="user" gbs-click="repeat"
                            class="gbs-button gbs-button__control gbs-icon gbs-button__secondary">
                            keyboard_arrow_down
                        </button>
                        <button class="gbs-button gbs-button__control gbs-icon" disabled>
                            south_east
                        </button>
                    </div>
                </fieldset>
            </section>

            <section name="filters" hidden>
                <fieldset class="gbs-fieldset filters">
                    <legend class="gbs-fieldset__legend gbs-fieldset__legend--help">
                        <div class="gbs-icon">blur_on</div>
                        <div>L{FILTERS_LEGEND}</div>
                    </legend>
                    <div class="gbs-margin__bottom--16">
                        <div class="gbs-flex gbs-margin__bottom--16">
                            <button gbs-message="7" gbs-message-type="user" gbs-click="normal" gbs-toggle="scanlines"
                                class="gbs-button gbs-button__control gbs-button__secondary">
                                <div class="gbs-icon">gradient</div>
                                <div>L{SCANLINES_BUTTON}</div>
                            </button>
                            <button gbs-message="K" gbs-message-type="user" gbs-click="normal"
                                class="gbs-button gbs-button__control">
                                <div class="gbs-icon">gradientbolt</div>
                                <div>L{INTENSITY_BUTTON}</div>
                            </button>
                            <button gbs-message="m" gbs-message-type="user" gbs-click="normal"
                                gbs-toggle="vdsLineFilter" class="gbs-button gbs-button__control gbs-button__secondary">
                                <div class="gbs-icon">power_input</div>
                                <div>L{LINE_FILTER_BUTTON}</div>
                            </button>
                        </div>
                        <ul class="gbs-help">
                            <!-- prettier-ignore -->
                            <li>L{FILTERS_HELP_1}</li>
                            <li>L{FILTERS_HELP_2}</li>
                        </ul>
                        <div class="gbs-flex">
                            <button gbs-message="f" gbs-message-type="action" gbs-click="normal" gbs-toggle="peaking"
                                class="gbs-button gbs-button__control gbs-button__secondary">
                                <div class="gbs-icon">blur_linear</div>
                                <div>L{PEAKING_BUTTON}</div>
                            </button>
                            <button gbs-message="V" gbs-message-type="action" gbs-click="normal"
                                gbs-toggle="stepResponse" class="gbs-button gbs-button__control gbs-button__secondary">
                                <div class="gbs-icon">grain</div>
                                <div>L{STEP_RESPONSE_BUTTON}</div>
                            </button>
                        </div>
                        <ul class="gbs-help">
                            <!-- prettier-ignore -->
                            <li>L{PEAKING_HELP_1}</li>
                            <li>L{PEAKING_HELP_2}</li>
                        </ul>
                    </div>
                </fieldset>
                <fieldset class="gbs-fieldset">
                    <legend class="gbs-fieldset__legend gbs-fieldset__legend--help">
                        <div class="gbs-icon">settings_brightness</div>
                        <div>L{SLOT_SETTINGS_LEGEND}</div>
                    </legend>
                    <table class="gbs-preferences">
                        <tr>
                            <td>
                                L{FULL_HEIGHT_SWITCH}
                                <!-- prettier-ignore -->
                                <ul class="gbs-help">
                                    <li>L{FULL_HEIGHT_SWITCH_HELP_1}</li>
                                    <li>L{FULL_HEIGHT_SWITCH_HELP_2}</li>
                                    <li>L{FULL_HEIGHT_SWITCH_HELP_3}</li>
                                </ul>
                            </td>
                            <td gbs-message="v" gbs-message-type="user" gbs-click="normal" class="gbs-icon"
                                gbs-toggle-switch="fullHeight">
                                toggle_off
                            </td>
                        </tr>
                        <tr>
                            <td>
                                L{FORCE_FRAME_RATE_SWITCH}
                                <!-- prettier-ignore -->
                                <ul class="gbs-help">
                                    <li>L{FORCE_FRAME_RATE_SWITCH_HELP_1}</li>
                                </ul>
                            </td>
                            <td gbs-message="0" gbs-message-type="user" gbs-click="normal" class="gbs-icon"
                                gbs-toggle-switch="palForce60">
                                toggle_off
                            </td>
                        </tr>
                        <tr>
                            <td colspan="2" class="gbs-preferences__child">
                                L{ACTIVE_FRAME_LOCK_SWITCH}
                                <!-- prettier-ignore -->
                                <ul class="gbs-help">
                                    <li>L{ACTIVE_FRAME_LOCK_SWITCH_HELP_1}</li>
                                    <li>L{ACTIVE_FRAME_LOCK_SWITCH_HELP_2}</li>
                                </ul>
                            </td>
                        </tr>
                        <tr>
                            <td class="gbs-padding__left-16">
                                L{FRAME_LOCK_SWITCH_GROUP}
                            </td>
                            <td class="gbs-icon" gbs-message="5" gbs-message-type="user" gbs-click="normal"
                                gbs-toggle-switch="frameTimeLock">
                                toggle_off
                            </td>
                        </tr>
                        <tr>
                            <td class="gbs-padding__left-16">
                                L{LOCK_METHOD_SWITCH}
                            </td>
                            <td class="gbs-icon" gbs-message="i" gbs-message-type="user" gbs-click="normal"
                                style="cursor: pointer">
                                swap_horiz
                            </td>
                        </tr>
                        <tr>
                            <td colspan="2" class="gbs-preferences__child">
                                L{DEINTERLACE_METHOD_SWITCH_GROUP}
                                <!-- prettier-ignore -->
                                <ul class="gbs-help">
                                    <li>L{DEINTERLACE_METHOD_SWITCH_GROUP_HELP_1}</li>
                                    <li>L{DEINTERLACE_METHOD_SWITCH_GROUP_HELP_2}</li>
                                    <li>L{DEINTERLACE_METHOD_SWITCH_GROUP_HELP_3}</li>
                                    <li>L{DEINTERLACE_METHOD_SWITCH_GROUP_HELP_4}</li>
                                </ul>
                            </td>
                        </tr>
                        <tr>
                            <td class="gbs-padding__left-16">
                                L{MOTION_ADAPTIVE_SWITCH}
                            </td>
                            <td gbs-message="q" gbs-message-type="user" gbs-click="normal" class="gbs-icon"
                                gbs-toggle-switch="motionAdaptive">
                                toggle_off
                            </td>
                        </tr>
                        <tr>
                            <td class="gbs-padding__left-16">
                                L{BOB_SWITCH}
                            </td>
                            <td gbs-message="q" gbs-message-type="user" gbs-click="normal" class="gbs-icon"
                                gbs-toggle-switch="bob">
                                toggle_off
                            </td>
                        </tr>
                    </table>
                </fieldset>
            </section>

            <section name="preferences" hidden>
                <fieldset class="gbs-fieldset">
                    <legend class="gbs-fieldset__legend gbs-fieldset__legend--help">
                        <div class="gbs-icon">tune</div>
                        <div>L{SYSTEM_PREFERENCES_LEGEND}</div>
                    </legend>
                    <table class="gbs-preferences">
                        <tr>
                            <td>
                                L{LOW_RES_UPSCALE_SWITCH}
                                <ul class="gbs-help">
                                    <li>L{LOW_RES_UPSCALE_SWITCH_HELP_1}</li>
                                    <li>L{LOW_RES_UPSCALE_SWITCH_HELP_2}</li>
                                    <li>L{LOW_RES_UPSCALE_SWITCH_HELP_3}</li>
                                    <li>L{LOW_RES_UPSCALE_SWITCH_HELP_4}</li>
                                    <li>L{LOW_RES_UPSCALE_SWITCH_HELP_5}</li>
                                </ul>
                            </td>
                            <td gbs-message="x" gbs-message-type="user" gbs-click="normal" class="gbs-icon"
                                gbs-toggle-switch="preferScalingRgbhv">
                                toggle_off
                            </td>
                        </tr>
                        <tr>
                            <td>
                                L{FORCE_COMPONENET_OUT_SWITCH}
                                <!-- prettier-ignore -->
                                <ul class="gbs-help">
                                    <li>L{FORCE_COMPONENET_OUT_SWITCH_HELP_1}</li>
                                    <li>L{FORCE_COMPONENET_OUT_SWITCH_HELP_2}</li>
                                </ul>
                            </td>
                            <td gbs-message="L" gbs-message-type="action" gbs-click="normal" class="gbs-icon"
                                gbs-toggle-switch="wantOutputComponent">
                                toggle_off
                            </td>
                        </tr>
                        <tr>
                            <td>
                                L{DISABLE_EXT_CLOCK_SWITCH}
                                <!-- prettier-ignore -->
                                <ul class="gbs-help">
                                    <li>L{DISABLE_EXT_CLOCK_SWITCH_HELP_1}</li>
                                    <li>L{DISABLE_EXT_CLOCK_SWITCH_HELP_2}</li>
                                </ul>
                            </td>
                            <td gbs-message="X" gbs-message-type="user" gbs-click="normal" class="gbs-icon"
                                gbs-toggle-switch="disableExternalClockGenerator">
                                toggle_off
                            </td>
                        </tr>
                        <tr>
                            <td>
                                L{ADC_CALIBRATION_SWITCH}
                                <!-- prettier-ignore -->
                                <ul class="gbs-help">
                                    <li>L{ADC_CALIBRATION_SWITCH_HELP_1}</li>
                                    <li>L{ADC_CALIBRATION_SWITCH_HELP_2}</li>
                                </ul>
                            </td>
                            <td gbs-message="w" gbs-message-type="user" gbs-click="normal" class="gbs-icon"
                                gbs-toggle-switch="enableCalibrationADC">
                                toggle_off
                            </td>
                        </tr>
                        <tr gbs-dev-switch>
                            <td>
                                L{DEVELOPER_MODE_SWITCH}
                                <!-- prettier-ignore -->
                                <ul class="gbs-help">
                                    <li>L{DEVELOPER_MODE_SWITCH_HELP_1}</li>
                                </ul>
                            </td>
                            <td gbs-message="d" gbs-message-type="user" gbs-click="normal"
                                class="gbs-icon">toggle_off</td>
                        </tr>
                    </table>
                </fieldset>
            </section>

            <section name="developer" hidden>
                <fieldset class="gbs-fieldset">
                    <legend class="gbs-fieldset__legend">
                        <div class="gbs-icon">input</div>
                        <div>L{DEVELOPER_LEGEND}</div>
                    </legend>
                    <div class="gbs-flex gbs-padding__bottom-8">
                        <div class="gbs-input-group">
                            <legend>L{REGISTER_CMD_SEGMENT}</legend>
                            <select gbs-register-section>
                                <option value="0">0 (status [ro], misc, OSD)</option>
                                <option value="1">1 (inp.formt, HD-bypass, mode detect)</option>
                                <option value="2">2 (de-interlace)</option>
                                <option value="3">3 (video proc., PIP)</option>
                                <option value="4">4 (mem., capture&playb., r/w FIFO)</option>
                                <option value="5">5 (ADC, sync. proc.)</option>
                            </select>
                        </div>
                        <div class="gbs-input-group gbs-margin__left--8">
                            <legend>L{REGISTER_CMD_OPERATION}</legend>
                            <select gbs-register-operation>
                                <option value="0">L{WRITE}</option>
                                <option value="1">L{READ}</option>
                            </select>
                        </div>
                    </div>
                    <div class="gbs-flex gbs-margin__bottom--16 gbs-input-group">
                        <legend>L{REGISTER_DATA_LEGEND}</legend>
                        <textarea gbs-register-data></textarea>
                        <button class="gbs-button gbs-button__secondary gbs-button__reg-cmd-submit" gbs-register-submit>
                            <div class="gbs-icon">send_time_extension</div>
                        </button>
                    </div>
                    <div class="gbs-flex gbs-margin__bottom--16">
                        <button gbs-message="-" gbs-message-type="action" gbs-click="normal"
                            class="gbs-button gbs-button__secondary">
                            <div class="gbs-icon">keyboard_arrow_left</div>
                            <div>L{MEM_LEFT_BUTTON}</div>
                        </button>
                        <button gbs-message="+" gbs-message-type="action" gbs-click="normal"
                            class="gbs-button gbs-button__secondary">
                            <div class="gbs-icon">keyboard_arrow_right</div>
                            <div>L{MEM_RIGHT_BUTTON}</div>
                        </button>
                        <button gbs-message="1" gbs-message-type="action" gbs-click="normal"
                            class="gbs-button gbs-button__secondary">
                            <div class="gbs-icon">keyboard_arrow_left</div>
                            <div>L{HS_LEFT_BUTTON}</div>
                        </button>
                        <button gbs-message="0" gbs-message-type="action" gbs-click="normal"
                            class="gbs-button gbs-button__secondary">
                            <div class="gbs-icon">keyboard_arrow_right</div>
                            <div>L{HS_RIGHT_BUTTON}</div>
                        </button>
                    </div>
                    <div class="gbs-flex">
                        <button class="gbs-button" gbs-output-toggle>
                            <div class="gbs-icon">code</div>
                            <div>L{TOGGLE_CONSOLE_BUTTON}</div>
                        </button>
                        <button gbs-message="e" gbs-message-type="user" gbs-click="normal" class="gbs-button">
                            <div class="gbs-icon">list</div>
                            <div>L{LIST_OPTIONS_BUTTON}</div>
                        </button>
                        <button gbs-message="i" gbs-message-type="action" gbs-click="normal" class="gbs-button">
                            <div class="gbs-icon">info</div>
                            <div>L{PRINT_INFO_BUTTON}</div>
                        </button>
                    </div>

                    <div class="gbs-flex">
                        <button gbs-message="," gbs-message-type="action" gbs-click="normal" class="gbs-button">
                            <div class="gbs-icon">alarm</div>
                            <div>L{GET_VIDEO_TIMING_BUTTON}</div>
                        </button>
                        <button gbs-message="F" gbs-message-type="user" gbs-click="normal"
                            class="gbs-button">
                            <div class="gbs-icon">add_a_photo</div>
                            <div>L{FREEZE_CAPTURE_BUTTON}</div>
                        </button>
                    </div>

                    <div class="gbs-flex">
                        <button gbs-message="F" gbs-message-type="action" gbs-click="normal" class="gbs-button">
                            <div class="gbs-icon">wb_sunny</div>
                            <div>L{ADC_FILTER_BUTTON}</div>
                        </button>
                        <button gbs-message="l" gbs-message-type="user" gbs-click="normal" class="gbs-button">
                            <div class="gbs-icon">memory</div>
                            <div>L{CYCLE_SDRAM_BUTTON}</div>
                        </button>
                    </div>
                    <div class="gbs-flex">
                        <button gbs-message="D" gbs-message-type="action" gbs-click="normal" class="gbs-button">
                            <div class="gbs-icon">bug_report</div>
                            <div>L{DEBUG_VIEW_BUTTON}</div>
                        </button>
                    </div>
                    <div class="gbs-flex">
                        <button gbs-message="a" gbs-message-type="action" gbs-click="normal" class="gbs-button">
                            <div class="gbs-icon">add_circle_outline</div>
                            <div>L{HTOTAL_INCR_BUTTON}</div>
                        </button>
                        <button gbs-message="A" gbs-message-type="action" gbs-click="normal" class="gbs-button">
                            <div class="gbs-icon">
                                remove_circle_outline
                            </div>
                            <div>L{HTOTAL_DECR_BUTTON}</div>
                        </button>
                        <button gbs-message="." gbs-message-type="action" gbs-click="normal"
                            class="gbs-button gbs-button__secondary">
                            <div class="gbs-icon">sync_problem</div>
                            <div>L{RESYNC_HTOTAL_BUTTON}</div>
                        </button>
                    </div>
                    <div class="gbs-flex">
                        <button gbs-message="n" gbs-message-type="action" gbs-click="normal" class="gbs-button">
                            <div class="gbs-icon">calculate</div>
                            <div>L{PLL_DIVIDER_INCR_BUTTON}</div>
                        </button>
                        <button gbs-message="8" gbs-message-type="action" gbs-click="normal" class="gbs-button">
                            <div class="gbs-icon">invert_colors</div>
                            <div>L{INVERT_SYNC_BUTTON}</div>
                        </button>
                    </div>
                    <div class="gbs-flex">
                        <button gbs-message="m" gbs-message-type="action" gbs-click="normal" class="gbs-button">
                            <div class="gbs-icon">devices_other</div>
                            <div>L{SYNC_WATCHER_BUTTON}</div>
                        </button>

                        <button gbs-message="l" gbs-message-type="action" gbs-click="normal"
                            class="gbs-button gbs-button__secondary">
                            <div class="gbs-icon">
                                settings_backup_restore
                            </div>
                            <div>L{SYNC_PROCESSOR_BUTTON}</div>
                        </button>
                    </div>
                    <div class="gbs-flex">
                        <button gbs-message="o" gbs-message-type="action" gbs-click="normal" class="gbs-button">
                            <div class="gbs-icon">insights</div>
                            <div>L{OVERSAMPLING_BUTTON}</div>
                        </button>
                        <button gbs-message="S" gbs-message-type="action" gbs-click="normal" class="gbs-button">
                            <div class="gbs-icon">settings_input_hdmi</div>
                            <div>L{HDMI_50FREQ60_BUTTON}</div>
                        </button>

                        <button gbs-message="E" gbs-message-type="user" gbs-click="normal" class="gbs-button">
                            <div class="gbs-icon">bug_report</div>
                            <div>L{IF_AUTO_OFFSET_BUTTON}</div>
                        </button>
                    </div>
                    <div class="gbs-flex">
                        <button gbs-message="z" gbs-message-type="user" gbs-click="normal" class="gbs-button">
                            <div class="gbs-icon">format_align_justify</div>
                            <div>L{SOG_LEVEL_DECR_BUTTON}</div>
                        </button>

                        <button gbs-message="q" gbs-message-type="action" gbs-click="normal"
                            class="gbs-button gbs-button__secondary">
                            <div class="gbs-icon">model_training</div>
                            <div>L{RESET_CHIP_BUTTON}</div>
                        </button>
                    </div>
                </fieldset>
            </section>

            <section name="system" hidden>
                <fieldset class="gbs-fieldset">
                    <legend class="gbs-fieldset__legend">
                        <div class="gbs-icon">bolt</div>
                        <div>L{DEVICE_MANAGEMENT_LEGEND}</div>
                    </legend>
                    <div class="gbs-flex">
                        <button gbs-message="c" gbs-message-type="action" gbs-click="normal"
                            class="gbs-button gbs-button__control">
                            <div class="gbs-icon">system_update_alt</div>
                            <div>L{ENABLE_OTA_BUTTON}</div>
                        </button>
                        <button gbs-message="a" gbs-message-type="user" gbs-click="normal"
                            class="gbs-button gbs-button__control">
                            <div class="gbs-icon">
                                restart_alt
                            </div>
                            <div>L{RESTART_DEVICE_BUTTON}</div>
                        </button>
                        <button gbs-message="2" gbs-message-type="user" gbs-click="normal"
                            class="gbs-button gbs-button__control">
                            <div class="gbs-icon">
                                settings_backup_restore
                            </div>
                            <div>L{RESET_ACTIVE_SLOT}</div>
                        </button>
                    </div>
                    <div class="gbs-flex">
                        <button gbs-message="1" gbs-message-type="user" gbs-click="normal"
                            class="gbs-button gbs-button__control gbs-button__secondary">
                            <div class="gbs-icon">
                                settings_backup_restore offline_bolt
                            </div>
                            <div>L{RESET_DEFAULTS_BUTTON}</div>
                        </button>
                    </div>
                </fieldset>
                <fieldset class="gbs-fieldset">
                    <legend class="gbs-fieldset__legend gbs-fieldset__legend--help">
                        <div class="gbs-icon">sd_card</div>
                        <div>L{BACKUP_LEGEND}</div>
                    </legend>
                    <!-- prettier-ignore -->
                    <ul class="gbs-help">
                        <li>L{BACKUP_BUTTON_HELP_1}</li>
                    </ul>
                    <div class="gbs-flex">
                        <button class="gbs-button gbs-button__control gbs-button__secondary gbs-backup-button">
                            <div class="gbs-icon">cloud_download</div>
                            <div gbs-progress gbs-progress-backup>
                                L{BACKUP_BUTTON}
                            </div>
                        </button>
                        <button class="gbs-button gbs-button__control gbs-button__secondary">
                            <div class="gbs-icon">cloud_upload</div>
                            <input type="file" class="gbs-backup-input" accept=".bin" />
                            <div gbs-progress gbs-progress-restore>
                                L{RESTORE_BUTTON}
                            </div>
                        </button>
                    </div>
                </fieldset>
                <fieldset class="gbs-fieldset">
                    <legend class="gbs-fieldset__legend">
                        <div class="gbs-icon">wifi</div>
                        <div>L{WIFI_LEGEND}</div>
                    </legend>

                    <div class="gbs-flex gbs-margin__bottom--16">
                        <button class="gbs-button gbs-button__control" gbs-wifi-ap>
                            <div class="gbs-icon">location_on</div>
                            <div>L{ACCESS_POINT_MODE_BUTTON}</div>
                        </button>
                        <button class="gbs-button gbs-button__control" gbs-wifi-station>
                            <div class="gbs-icon">radio</div>
                            <div gbs-wifi-station-ssid>L{STATION_MODE_BUTTON}</div>
                        </button>
                    </div>
                    <fieldset class="gbs-fieldset" gbs-wifi-list hidden>
                        <legend class="gbs-fieldset__legend">
                            <div class="gbs-icon">router</div>
                            <div>L{SSID_SELECT_LEGEND}</div>
                        </legend>
                        <table class="gbs-wifi__list"></table>
                    </fieldset>
                    <fieldset class="gbs-fieldset gsb-wifi__connect" hidden>
                        <legend class="gbs-fieldset__legend">
                            <div class="gbs-icon">login</div>
                            <div>L{CONNECTO_TO_SSID_BUTTON}</div>
                        </legend>
                        <div class="gbs-flex">
                            <input class="gbs-button gbs-wifi__input" placeholder="L{SSID_PLACEHOLDER}" type="text" readonly
                                gbs-input="ssid" />
                        </div>
                        <div class="gbs-flex">
                            <input class="gbs-button gbs-wifi__input" placeholder="L{PASSWORD_PLACEHOLDER}" type="password"
                                gbs-input="password" />
                        </div>
                        <div class="gbs-flex">
                            <button gbs-wifi-connect-button
                                class="gbs-button gbs-button__control gbs-button__secondary">
                                <div class="gbs-icon">network_check</div>
                                <div>L{CONNECT_BUTTON}</div>
                            </button>
                        </div>
                    </fieldset>
                </fieldset>
            </section>
            <section name="prompt" hidden>
                <fieldset class="gbs-fieldset">
                    <legend class="gbs-fieldset__legend">
                        <div class="gbs-icon">keyboard</div>
                        <div gbs-prompt-content>L{PROMPT_LEGEND}</div>
                    </legend>
                    <div class="gbs-flex gbs-margin__bottom--16">
                        <input class="gbs-button" type="text" gbs-input="prompt-input" maxlength="25" />
                    </div>
                    <div class="gbs-flex">
                        <button gbs-prompt-cancel class="gbs-button gbs-button__control">
                            <div class="gbs-icon">close</div>
                            <div>L{CANCEL_BUTTON}</div>
                        </button>
                        <button gbs-prompt-ok class="gbs-button gbs-button__control gbs-button__secondary">
                            <div class="gbs-icon">done</div>
                            <div>L{OK_BUTTON}</div>
                        </button>
                    </div>
                </fieldset>
            </section>
            <section name="alert" hidden>
                <fieldset class="gbs-fieldset">
                    <legend class="gbs-fieldset__legend">
                        <div class="gbs-icon">warning</div>
                        <div>L{ALERT_LEGEND}</div>
                    </legend>
                    <div class="gbs-padding__hor-16 gbs-modal__message" gbs-alert-content></div>
                    <div class="gbs-flex">
                        <button gbs-alert-act class="gbs-button gbs-button__control" disabled></button>
                        <button gbs-alert-ack class="gbs-button gbs-button__control gbs-button__secondary"></button>
                    </div>
                </fieldset>
            </section>
            <div class="gbs-output">
                <fieldset class="gbs-fieldset gbs-fieldset-output">
                    <legend class="gbs-fieldset__legend">
                        <div class="gbs-icon">code</div>
                        <div>L{CONSOLE_OUTPUT_LEGEND}</div>
                    </legend>
                    <div class="gbs-flex gbs-margin__bottom--16" gbs-output-clear>
                        <button class="gbs-button gbs-icon">
                            delete_outline
                        </button>
                    </div>
                    <div class="gbs-flex">
                        <textarea id="outputTextArea" class="gbs-output__textarea" readonly></textarea>
                    </div>
                </fieldset>
            </div>
        </div>
        <div class="gbs-scroll__footer">
            <div>
                <a target="_blank" href="https://github.com/ramapcsx2/gbs-control">
                    <svg focusable="false" aria-hidden="true" viewBox="0 0 24 24" width="12px" height="12px" data-testid="GitHubIcon" aria-label="fontSize small">
                        <path fill="#FFFFFF" d="M12 1.27a11 11 0 00-3.48 21.46c.55.09.73-.28.73-.55v-1.84c-3.03.64-3.67-1.46-3.67-1.46-.55-1.29-1.28-1.65-1.28-1.65-.92-.65.1-.65.1-.65 1.1 0 1.73 1.1 1.73 1.1.92 1.65 2.57 1.2 3.21.92a2 2 0 01.64-1.47c-2.47-.27-5.04-1.19-5.04-5.5 0-1.1.46-2.1 1.2-2.84a3.76 3.76 0 010-2.93s.91-.28 3.11 1.1c1.8-.49 3.7-.49 5.5 0 2.1-1.38 3.02-1.1 3.02-1.1a3.76 3.76 0 010 2.93c.83.74 1.2 1.74 1.2 2.94 0 4.21-2.57 5.13-5.04 5.4.45.37.82.92.82 2.02v3.03c0 .27.1.64.73.55A11 11 0 0012 1.27"></path>
                    </svg>
                    GBS-CONTROL
                </a>
            </div>
            <div>fw:${VERSION_FIRMWARE} / ui:${VERSION_UI}</div>
        </div>
    </div>
    <div class="gbs-wifi-warning" id="websocketWarning">
        <div class="gbs-icon blink_me">signal_wifi_off</div>
    </div>
    <div class="gbs-loader"><img /></div>
    <script>${js}</script>
</body>

</html>