---
sort: 16
---

# Tested Video Modes

<span class="anim-fade-in">

* [Atari ST family](atari-st-family)
* [IBM PC and compatibles](ibm-pc-and-compatibles)
* [Macintosh](macintosh)

</span>

## Atari ST family
| Input | Graphics chip | Mode no | Type | Resolution |  Refresh (V)  | Refresh (H) | Works | Comments   |
|-------|---------------|---------|------|------------|---------------|-------------|-------|------------|
|  VGA  |  Shifter      |   Low   |  RGB |   320x200  | 50 Hz / 60 Hz |  15.75 KHz  |   <b style="color:#11ff11">YES</b>  | Jumps on STFM, works on Mega ST and STE ([#198](https://github.com/ramapcsx2/gbs-control/issues/198)) |
|  VGA  |  Shifter      |  Medium |  RGB |   640x200  | 50 Hz / 60 Hz |  15.75 KHz  |  <b style="color:#11ff11">YES</b>  |  Tested on STE          |
|  VGA  |  Shifter      |   High  |  RGB |   640x400  |     72 Hz     |  31.50 KHz  |   <b style="color:red">NO</b>  | Stops working after a few seconds ([#190](https://github.com/ramapcsx2/gbs-control/issues/190)) |

## IBM PC and compatibles

| Input | Graphics chip | Mode no | Type | Resolution | Refresh (V) | Refresh (H) | Works | Comments   |
|-------|---------------|---------|------|------------|-------------|-------------|-------|------------|
|  VGA  |  ATi Mach 64  |   00h   |  VGA |   320x200  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   01h   | BIOS |   320x400  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   01h   |  VGA |   320x200  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   02h   | BIOS |   640x400  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   03h   | BIOS |   640x400  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   03h   |  VGA |   640x200  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   04h   | BIOS |   320x200  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   05h   | BIOS |   320x200  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   06h   | BIOS |   640x200  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   07h   | BIOS |   640x400  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   07h   |  VGA |   720x350  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   0Dh   | BIOS |   320x200  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   0Eh   | BIOS |   640x200  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   0Fh   | BIOS |   640x350  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   10h   | BIOS |   640x350  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   11h   | BIOS |   640x480  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   12h   | BIOS |   640x480  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   13h   | BIOS |   320x200  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   14h   | BIOS |   160x120  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   15h   | BIOS |   256x256  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  | Cut        |
|  VGA  |  ATi Mach 64  |   16h   | BIOS |   296x220  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   17h   | BIOS |   320x200  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   18h   | BIOS |   320x240  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  | Cut        |
|  VGA  |  ATi Mach 64  |   1Ah   |  VGA |   640x350  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   19h   | BIOS |   320x400  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   1Ah   | BIOS |   360x270  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  | Cut        |
|  VGA  |  ATi Mach 64  |   1Bh   | BIOS |   360x360  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   1Bh   |  VGA |   720x400  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   1Ch   | BIOS |   360x480  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  | Cut        |
|  VGA  |  ATi Mach 64  |   1Ch   |  VGA |   720x400  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   1Dh   | BIOS |   400x300  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   1Dh   |  VGA |   640x480  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  | Cut        |
|  VGA  |  ATi Mach 64  |   1Eh   |  VGA |   640x480  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  | Cut        |
|  VGA  |  ATi Mach 64  |   1Fh   |  VGA |   640x480  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  | Cut        |
|  VGA  |  ATi Mach 64  |   21h   | BIOS |   800x400  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   22h   | BIOS |   800x480  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   23h   | BIOS |  1056x400  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   32h   | BIOS |  1056x344  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   33h   | BIOS |  1056x352  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   40h   | BIOS |   512x384  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   41h   | BIOS |   512x384  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   42h   | BIOS |   400x300  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   43h   | BIOS |   400x300  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   44h   | BIOS |   640x350  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   45h   | BIOS |   640x350  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   57h   | BIOS |   640x350  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   58h   | BIOS |   320x200  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   59h   | BIOS |   320x240  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   5Ah   | BIOS |   512x384  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   5Bh   | BIOS |   400x300  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   5Ch   | BIOS |   640x480  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   5Dh   | BIOS |   800x600  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   5Eh   | BIOS |  1024x768  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   5Fh   | BIOS |  1280x1024 |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   60h   | BIOS |   320x200  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   61h   | BIOS |   640x400  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   62h   | BIOS |   640x480  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   63h   | BIOS |   800x600  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   64h   | BIOS |  1024x768  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   65h   | BIOS |  1280x1024 |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   6Bh   | BIOS |   320x240  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   6Dh   | BIOS |   512x384  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   6Eh   | BIOS |   400x300  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   6Fh   | BIOS |   640x350  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   70h   | BIOS |   320x200  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   71h   | BIOS |   640x480  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   72h   | BIOS |   640x480  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   73h   | BIOS |   800x600  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   74h   | BIOS |   800x600  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   75h   | BIOS |  1024x768  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   76h   | BIOS |  1024x768  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   77h   | BIOS |  1280x1024 |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   78h   | BIOS |  1280x1024 |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   79h   | BIOS |   320x240  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   7Ah   | BIOS |   320x200  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   7Bh   | BIOS |   320x240  |    ?????    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |   97h   |  VGA |   720x400  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   A1h   |  VGA |   320x240  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   A3h   |  VGA |   640x350  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   C1h   |  VGA |   360x400  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |   C3h   |  VGA |   720x400  |    ?????    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  100h   | VESA |   640x400  |    70 Hz    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  101h   | VESA |   640x480  |    60 Hz    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |  103h   | VESA |   800x600  |    56 Hz    |    ?????    |  <b style="color:#11ff11">YES</b>  | Misaligned |
|  VGA  |  ATi Mach 64  |  105h   | VESA |  1024x768  |    87 Hz    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  107h   | VESA |  1280x1024 |    87 Hz    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  10Dh   | VESA |   320x200  |    70 Hz    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  10Eh   | VESA |   320x200  |    70 Hz    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  10Fh   | VESA |   320x200  |    70 Hz    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  110h   | VESA |   640x480  |    60 Hz    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |  111h   | VESA |   640x480  |    60 Hz    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |  112h   | VESA |   640x480  |    60 Hz    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |  113h   | VESA |   800x600  |    56 Hz    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |  114h   | VESA |   800x600  |    56 Hz    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |  115h   | VESA |   800x600  |    56 Hz    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |  116h   | VESA |  1024x768  |    87 Hz    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  117h   | VESA |  1024x768  |    87 Hz    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  118h   | VESA |  1024x768  |    87 Hz    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  119h   | VESA |  1280x1024 |    87 Hz    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  11Ah   | VESA |  1280x1024 |    87 Hz    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  11Bh   | VESA |  1280x1024 |    87 Hz    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  202h   | VESA |   320x200  |    70 Hz    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  212h   | VESA |   320x240  |    60 Hz    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |  213h   | VESA |   320x240  |    60 Hz    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |  214h   | VESA |   320x240  |    60 Hz    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |  215h   | VESA |   320x240  |    60 Hz    |    ?????    |  <b style="color:#11ff11">YES</b>  |            |
|  VGA  |  ATi Mach 64  |  222h   | VESA |   512x384  |    70 Hz    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  223h   | VESA |   512x384  |    70 Hz    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  224h   | VESA |   512x384  |    70 Hz    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  225h   | VESA |   512x384  |    70 Hz    |    ?????    |   <b style="color:red">NO</b>  |            |
|  VGA  |  ATi Mach 64  |  232h   | VESA |   400x300  |    ?????    |    ?????    |  ???  |            |
|  VGA  |  ATi Mach 64  |  233h   | VESA |   400x300  |    ?????    |    ?????    |  ???  |            |
|  VGA  |  ATi Mach 64  |  234h   | VESA |   400x300  |    ?????    |    ?????    |  ???  |            |
|  VGA  |  ATi Mach 64  |  235h   | VESA |   400x300  |    ?????    |    ?????    |  ???  |            |
|  VGA  |  ATi Mach 64  |  242h   | VESA |   640x350  |    ?????    |    ?????    |  ???  |            |
|  VGA  |  ATi Mach 64  |  243h   | VESA |   640x350  |    ?????    |    ?????    |  ???  |            |
|  VGA  |  ATi Mach 64  |  244h   | VESA |   640x350  |    ?????    |    ?????    |  ???  |            |
|  VGA  |  ATi Mach 64  |  245h   | VESA |   640x350  |    ?????    |    ?????    |  ???  |            |
|  VGA  |  ATi Mach 64  |  302h   | VESA |  1600x1200 |    ?????    |    ?????    |  ???  |            |
|  VGA  |  ATi Mach 64  |  303h   | VESA |  1600x1200 |    ?????    |    ?????    |  ???  |            |
|  VGA  |  ATi Mach 64  |  304h   | VESA |  1600x1200 |    ?????    |    ?????    |  ???  |            |

## Macintosh

| Input | Graphics chip | Mode no | Type | Resolution | Refresh (V) | Refresh (H) | Works | Comments   |
|-------|---------------|---------|------|------------|-------------|-------------|-------|------------|
|  VGA  |      N/A      |   ????  | ???? |  512×342   |   60.15Hz   |  22.25KHz   | YES*  | Tested with a Power R Model 2703 on a Mac SE. *Right side shows a big vertical green bar instead of proper output. Similar [adapter](http://www.waveguide.se/?article=compact-mac-video-adapter) to output video from Macintosh SE. |

* BIOS means the video mode is set up using INT 10h to call the VGA BIOS
* VGA means the video card registers are changed directly bypassing the BIOS
* VESA means the video mode is set up using INT 10h to call the VESA BIOS EXTENSIONS
* BIOS and VGA modes are expected to always produce the same refresh rates, VESA modes can change the rate
