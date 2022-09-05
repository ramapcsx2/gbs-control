# SCART RGB to VGA adapter
If you want to build an adapter, here is some good inspiration: https://tomdalby.com/other/gbs8200.html Use the "GBS-8200 SCART Circuit v2" picture as a guide and ignore the LM1881 chip, just connect the wires from the 8 pin JST HX connector to the female SCART socket then add the 100 Ohm resistor (if needed, see below for the details).

The ArcadeForge Sync Strike is a solution that will work, if you don't want to build an adapter yourself.   
http://arcadeforge.net/Scaler-and-Strike-Devices/Sync-Strike::15.html?language=en   

# Sync notes
Every GBS board comes with ~500 Ohm termination on the sync input. This termination is meant for (PC) VGA and most arcade boards.

Csync from consoles should be fine without additional termination but the input may not behave well with other kinds of sync.


If you want to use the RGBS input for regular video-level sync sources, the GBS requires one additional 100 Ohm resistor connected to ground from the sync input.<br>Together with the ~500 Ohm factory resistor, this will bring the total termination closer to TV levels (75 Ohms).

# Using a Sync Stripper
Compatibility with Sync-on-Composite or Sync-on-Luma can be improved by using a sync stripper, such as the LM1881.   
It is recommended that you build your circuit so that it has a 75 Ohm to ground (termination) resistor on the LM1881 video input, as well as a 470 Ohm series (attenuation) resistor on the sync output. You will no longer need the 100 ohm terminating resistor with this sync stripper built into your SCART adapter.<br>
The 75 Ohm resistor on the chip's input provides a properly-terminated connection for the source signal.<br>
The 470 Ohm series resistor on the CSync output of the LM1881 lowers the voltage to safe levels for the GBS.

Your extracted Sync signal can now be connected to the "S" (sync input) pin of the GBS.   

User viletim shows a good LM1881 circuit here:   
https://shmups.system11.org/viewtopic.php?f=6&t=55948&p=1153713#p1153713   
https://shmups.system11.org/viewtopic.php?p=1153077#p1153077   

Note that his circuit omits the _Rset_ resistor and capacitor. This works for CSync generation, but the VSync output will not work. This is not a problem for this application, as we are only interested in the CSync signal.   

Also note that the LM1881 requires 5V to operate, but the GBS only makes 3.3V easily available on its headers.
The 5V power for the LM1881 needs to be sourced elsewhere. The GBS board's power input may be a good option, assuming your DC power supply is decent.
  
One user has successfully used the sync stripper at just 3.3V with the output 470 Ohm resistor removed.   
This is not recommended though, since the LM1881 will have a hard time with some signals when undervolted like this.   