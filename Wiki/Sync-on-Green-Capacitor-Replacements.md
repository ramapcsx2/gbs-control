---
sort: 17
---

**Update:**   
This modification is not required anymore.   
Gbscontrol now uses an undocumented clamping level bit to work well with the default capacitors.   
It isn't necessary to do this modification anymore, but you can still do it and improve sync performance.   
**///////////**

Sync pulses for RGBS and Component Video sources are AC coupled to the TV5725 scaler IC.   
The GBS8200 board designs (all variants I know of) don't follow the TV5725 datasheet for these "SOG" coupling capacitors.   
The value for "C33" and "C35" should be 1nF (1000pF). Instead, the installed capacitors have a value of 100nF.   
   
Due to the high capacitance, the SOG extraction circuit in the 5725 sees too much of any eventual video content, instead of just the synchronization pulses.   
This leads to frequent short sync losses when the picture content changes rapidly, for example on a flashing screen.   

Previously, it was recommended to use a Sync Stripper circuit to address the dropouts.   
It is better to replace C33 and C35 with 1nF parts, thus allowing the SOG separation circuit to do the job it was designed to do.   
Due to the densely populated work area, good soldering equipment and a little experience is required.   
For the capacitors, I recommend looking for 1nF SMD NP0 (or X7R) parts. Sizes of "0805" or "0603" will fit well.  
 
<span class="anim-fade-in">

![](https://i.imgur.com/hgpaVER.png)

</span>
<span class="anim-fade-in">

![](https://i.imgur.com/S2jw69Q.jpg)

</span>