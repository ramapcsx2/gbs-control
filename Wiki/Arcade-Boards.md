---
sort: 14
---

# Arcade Boards notes

User @tracedebrake posted some comments regarding the use of arcade boards with the GBS scaler (or any other scaler).   
I've copied them here, so that arcade machine users are aware of what to look out for.

> Hello @xuserv & @ramapcsx2!
>
> Might I ask, @xuserv, if you have done every recommanded modifications to your GBS 8200, including soldeging a 100ohm resistor from CSYNC to ground & setting all 3 RGB potentiometer to the minimum (left)?
>
> I'm asking this because normally, most arcade SuperGuns will not properly adjust your RGBS signals to be 75 ohms & 0.7v peak-to-peak levels compatible. Arcade video signals have the same timing as NTSC video but the electrical properties are different to a standard TV/video monitor. It's analog signals with TTL level amplitude (3-5 Vpp) while regular gaming consoles are made for home use so they output at 0.7v pk-to-pk for a 75ohms load (your tv) . Arcade monitors also have a very high inputs impedance to match those TTL voltage levels (1k-10k ohms).
>
> This is why the GBS8200 has a high input resistor (1k) on CSYNC; it is made for arcade use by connecting your JAMMA harness directly to the RGBS pins.
>
> By doing the recommanded mods (100ohm resistor + adjusting your pots total left (or removing them)), what you are actually doing is modifing your GBS8200 to be console compatible. If you want to use a SuperGun with a fully modified GBS8200, you need to make sure 1st that your SuperGun properly adjust the RGBS signals to TV levels using a buffer/driver, such as the HAS v3.1
>
> This is exactly why some people blown their expensive OSSC or Framemeister; their SuperGun wasn't properly adjusting video signals. 
   
   
> The main issues with arcade boards are this:
>
> - they never ever been designed to be played outside their cabinet
>
> - there's was no video standard. Full-scale arcades manufacturers had not only the joy to create their own board from ground up but also the one of choosing the right monitor and fine tune it accordingly.
>
> The JAMMA standard that came at the end of the 80s/early 90s was created to allow arcade center operators to use old cabinets again & again only by changing the game board, controls and cabinet side arts/marquees. But still, since no standard was ever made for video, arcade monitors had those adjustments pots mounted either at the back of the monitor or on an external harness like this one to render the monitor ''compatible'' with the new board. That was a pain to adjust right and sometimes the adjustment was buggy at best. Even the JAMMA standard itself wasn't respected for long: some games were designed to use more buttons than what the standard allowed (exemple Street Figther 2) by using dormant pins on the JAMMA connector or by adding new connectors on their boards; see kick harness.
>
> All SuperGuns try to adapt that video chaos for what a 75ohm TV needs but it doesn't mean that they are doing it right. There is mostly 3 types of SuperGuns video designs around:
>
> - Fixed resistor value: You can't just slap some resistors on the RGBS lines and attenuate them that way by hoping to create a voltage divider. RGBS output levels from arcade boards varies too much from a board to an other to use fixed values. Avoid at all cost (exemple: the horrible Vogatek.)
>
> - Potentiometers: Better than having fixed values but same as above since you don't know for sure what you're doing. Having nice colors on your TV doesn't mean that you aren't flooding your AV equipment protection diodes, if they have any. This solution will mostly kill your AV (TV, scaler) as well in the long run. To me it's an avoid at all cost as well. (exemple: the RetroElectronik Pro gamer)
>
> - Impedance matching: very high impedance, video-bandwidth amplifiers/buffers ARE NECESSARY to prevent RGB distorsion and provide a steady impedance match for 75 ohms AV inputs. Same with the sync but it needs to be buffered, attenuated and impedance matched as well. This is the only way to go. (exemple: the current HAS 3.1 (almost impossible to get), the future Sentinel and the open-source DIY Thibaut Varènes design)

A small reiteration regarding the "RetroElectronik Pro" as referenced above  - By user @cglmrfreeman
>I was using gbs-control directly with the RetroElectronik Pro Supergun and had a lot of noise in the black areas, as well as dark rolling waves across the screen when used with an HDMI converter. These flaws didn't appear using a VGA CRT monitor but I decided to do some testing anyway. Attaching the harness from the GBS unit directly to a JAMMA fingerboard resulted in significant improvements and no noise that I could make out.
>
>As the above states, there was no video standard for arcade boards, and JAMMA is nice, but if you can bypass your supergun, such as/specifically the RetroElectronik Pro, and connect directly into the GBS, I highly recommend that route.