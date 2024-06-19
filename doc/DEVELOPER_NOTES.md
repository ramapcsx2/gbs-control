# TV5725 REGISTERS

![register map](./img/reg-map.jpg)

## (S0_41) Display clock tuning register

![display clock tuning](./img/display-clock-tuning-reg.jpg)

## (S0_90_1) Osd horizontal zoom select

![Osd horizontal zoom select](./img/osd-horizontal-zoom-select.jpg)

## (S0_90_4) Osd vertical zoom select

![Osd vertical zoom select](./img/osd-vertical-zoom-select.jpg)

## (S0_91_0) Osd menu icons select

![Osd vertical zoom select](./img/osd-menu-icons-select.jpg)

## (S0_91_4) Osd icons modification select

![Osd vertical zoom select](./img/osd-icons-modification-select.jpg)

## (S1_00) Select 16bit data

![Select 16bit data](./img/select-16bit-data.jpg)

## (S1_01) Y data pipes control in YUV422to444 conversion

![Y data pipes control in YUV422to444 conversion](./img/y-data-pipes-YUV422to444-conv.jpg)

## (S1_02) Y data pipes control in YUV444to422 conversion

![Y data pipes control in YUV444to422 conversion](./img/y-data-pipes-YUV444to422-conv.jpg) 

## (S1_62) Horizontal Stable Estimation Error Range Control

![Horizontal Stable Estimation Error Range Control](./img/horizontal-stable-est-error-range.jpg)

## (S2_00) Diagonal Bob Low pass Filter Coefficient Selection 

![diagonal bob coeff. selection](./img/diagonal-bob-lpf-coeff.jpg)

## (S2_17) Y delay pipe control

![Y delay pipe control](./img/y-delay-pipe-control.jpg)

## (S2_17) UV delay pipe control

![UV delay pipe control](./img/uv-delay-pipe-control.jpg)

## (S2_3A) Delay pipe control for motion index feedback-bit

![Delay pipe control for MI](./img/pipe-control-motion-index.jpg)

## (S2_3C) Motion index delay control

![Motion index delay control](./img/motion-index-delay-control.jpg)

## (S3_00) External sync enable

![External sync enable](./img/vds-sync-enable.jpg)

## (S3_1F) Programmable repeat frame number

![Programmable repeat frame number](./img/progm-repeat-frame-num.jpg)

## (S3_24_4) Y compensation delay

![Y compensation delay](./img/y-compensation-delay.jpg)

## (S3_24_6) Compensation delay control

![Compensation delay control](./img/compensation-delay-ctl.jpg)

## (S3_2A) UV step response data select

![UV step response data select](./img/uv-step-response-data-sel.jpg)

U/V2 is 2 clocks delay of input U/V, UV3 is 3 clocks delay of input U/V, and so on.

## (S3_32) SVM data generation select control

![SVM data generation select control](./img/svm-data-generation-select-ctl.jpg)

A1 is one pipe delay of a0, a2 is one pipe delay of a1, a3 is one pipe delay of a2, a4 is one pipe delay of a3, here a* is the input data y for generate SVM signal.

## (S3_40) SVM delay be V2CLK control

![SVM delay be V2CLK control](./img/svm-delay-be-v2clk-ctl.jpg)

This field define the SVM delay from 1 to 4 V2CLKs

## (S3_55) Menu mode control for global still index

![Menu mode control for global still index](./img/menu-mode-control-4-glob-still-ind.jpg)

This bit is the user defined menu mode for global still signal, when it is 1, the global still signal is 1, the following is the detail.

## (S3_73) Blue extend range control bit 

![Blue extend range control bit](./img/blue-extend-range-ctl-bit.jpg)

This field defines the range for blue extend.

## (S3_80) Y compensation delay control

![Y compensation delay control](./img/y-compensation-delay-ctl.jpg)

To compensation the pipe of UV, program this field can delay Y from 1 to 4 clocks.

## (S4_00) SDRAM Idle Period Control and IDLE Done Select

![SDRAM Idle Period Control and IDLE Done Select](./img/sdram-idle-period-ctl-n-idle.jpg)

## (S4_28) Enable playback request mode

![Enable playback request mode](./img/enable-playback-request-mode.jpg)

## (S4_4D_0) Read buffer page select from 1 to 16

![Read buffer page select from 1 to 16](./img/read-buffer-page-select-1-16.jpg)

## (S4_4D_5) Enable read FIFO request mode

![Enable read FIFO request mode](./img/en-read-fifo-request-mode.jpg)

## (S5_16) CKO2 - PLLAD CKO2 output clock selection

![PLLAD CKO2 output clock selection](./img/pllad-cko2.jpg)


