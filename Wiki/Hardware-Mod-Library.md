---
sort: 7
---

# Hardware Mods Library

This page lists all of the known modifications which people have attempted on the GBS82XX series boards, what they do, and if they should be performed or not.   
Note that in most cases, gbscontrol will work without any of these modifications.   

|Key| |
|---|---|
| <b style="color:#11ff11">!</b> | Essential |
| <b style="color:blue">O</b> | Optional / Recommended |
| <b style="color:red">X</b> | Not needed / Deprecated |

|Mod Description | _GBS8220_|_GBS8200_ |_GBS8200(2017)_| Effect/Issue Fixed by Mod |
|----------------|----------|---------|---------------|---------------------------|
|Add 100ohm resistor across sync and ground for RGBs input 													| <b style="color:#11ff11">!</b> | <b style="color:#11ff11">!</b> 	| <b style="color:#11ff11">!</b> | Corrects sync level for 75 Ohm sources (such as game consoles) |
|Replace R34 with 75 Ohm resistor 																			| <b style="color:blue">O</b>    |<b style="color:blue">O</b>		|<b style="color:blue">O</b>	 | Permanent 75 Ohm termination, instead of the mod above |
|Remove C11, optionally replace with 22uF (6.3V to 16V) electrolytic cap									| <b style="color:blue">O</b>  	 |<b style="color:blue">O</b>		|<b style="color:blue">O</b>	 | [Stabilizes 1.8V LDO, removes noise from image](https://github.com/ramapcsx2/gbs-control/wiki/GBS-8200-Variants) |
|Remove the 3 RGB potentiometers, replace with direct link 													| <b style="color:blue">O</b> 	 | <b style="color:blue">O</b> 		| <b style="color:blue">O</b> 	 | [Improves Colors](https://github.com/ramapcsx2/gbs-control/wiki/RGB-Potentiometers) |
|Add 10uf / 22uf ceramic SMD capacitors in parallel to stock ones (x4) C23, C41(alternative: C43), C42, C48 | <b style="color:blue">O</b>	 |<b style="color:blue">O</b>		|<b style="color:blue">O</b>	 | [Provide local charge reservoir to help power supply bypassing. May reduce noise and waves in the image](https://github.com/ramapcsx2/gbs-control/wiki/Power-supply-bypass-capacitors) |
|Add a Sync Stripper circuit to RGBs input 																	| <b style="color:blue">O</b>	 |<b style="color:blue">O</b>		|<b style="color:blue">O</b>	 | Try this if sync is never quite stable |
|Replace R58 with 120 Ohm ferrite bead 																		| <b style="color:blue">O</b>	 |<b style="color:red">X</b>		|<b style="color:red">X</b>		 | For GBS8220 (2 output version); decouples charge pump |
|Add 100n cap between R59 and R58 																			| <b style="color:blue">O</b>	 |<b style="color:red">X</b>		|<b style="color:red">X</b>		 | For GBS8220 (2 output version); decouples charge pump |
|Replace C33 and C35 with 1nf (1000pf) SMD capacitors (0605/0805 50V C0G)									| <b style="color:red">X</b> 	 |<b style="color:red">X</b>  		|<b style="color:red">X</b> 	 | [Deprecated Sync fix](https://github.com/ramapcsx2/gbs-control/wiki/Sync-on-Green-Capacitor-Replacements) |
|Foil tape over digital signal lines 																		| <b style="color:red">X</b> 	 |<b style="color:red">X</b>		|<b style="color:red">X</b>		 | This was fixed in software (SDRAM delays) |
|Replace L1 L2 L3 with 220R 100v ferrite beads 																| <b style="color:red">X</b> 	 |<b style="color:red">X</b>		|<b style="color:red">X</b>		 | Not recommended until testing has concluded |