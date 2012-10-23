teject
======

teject is a Tape eject utility for Windows


Usage
=====
teject.exe DEVICENAME [-m NUM]<br />

-m Specifies which ejection method to use. Valid values are 1, 2 or 3:<br />
1 = lock, unmount, check, eject, unlock<br />
2 = Legacy eject<br />
3 = Unloads the tape (may only rewind it)<br />
NOTE: If no method is specified each is attempted in sequence<br />

Examples:<br />
teject.exe TAPE0<br />
teject.exe TAPE0 -m 2<br />

To find your tape device name see: http://www.codeproject.com/KB/system/Tape_drive_operator.aspx?display=Print