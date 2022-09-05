# Yellow Button "V5.0" or "2017"
Those newer GBS-8200 boards have an LDO oscillation problem.   
It works, but there will be a lot of noise in the picture.   
The easy fix is to remove one SMD capacitor, circled in red.   
(A better fix would be to replace this capacitor with an electrolytic of 22uF to 47uF.)   
![](https://i.imgur.com/XWDD0Ss.jpg ) 
[Background](http://www.ti.com/product/LM1117/datasheet/application_and_implementation#snos4127440)   
The chosen SMD capacitor on these boards has far too little capacitance (120nF measured) AND has very little ESR. It causes the LDO to permanently oscillate at around 20Mhz. There is an electrolytic capacitor further down the line that is sufficient for stable operation of the LDO.

# Original GBS-8220
Same LDO oscillation problem, but here it is limited to setups that use a power supply of > 5.0V.   
I get wild oscillation at 7.6V from a common PSOne supply.   
The same C11 removal fix works here as well.   
![](https://ianstedman.files.wordpress.com/2014/12/gbs-8220-v3-medium.jpg)   
These variants also use a special video driver for the VGA output.   
The driver IC has a charge pump that it uses to generate negative voltage.   
This would be a useful feature for driving YPbPr signals, but here they just seem to dump the current onto the ground plane (from what I can tell at least).   
That charge pump is generating noise however, and since no ferrite bead is used at location R58, a noise pattern easily develops in the image.   
With a bead installed and some extra SMD capacitors, the image is fine again.   

![](https://i.imgur.com/UiSPbwW.jpg)