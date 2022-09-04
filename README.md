# mz25key
A PS/2 to Sharp MZ-2500/MZ-2800 Interface

The mz25key now uses the SharKey firmware. If the build configuration (menuconfig -> SharpKey Configuration -> Build target -> mz25key
is set to an MZ2500 or MZ2800 then no changes are needed to hardware, just upload the firmware via the programmer port and once uploaded
future updates can be made via OTA. 

A modification can be made to an mz25key to allow it to run SharpKey firmware without specific build target. This can be achieved with a
100R resistor placed between pin14 (GPIO12) of the ESP32 and pin 8 of the host interface header J1. The advantage of this modification is it makes
the mz25key compatible with the SharpKey and if you used a 9pin header on the host interface, you can swap cables around to target different 
hosts.

Please see my website, https://eaw.app for more documentation and recent updates. 

The source code for the mz25key is identical to the SharpKey, just the build time configuration via the KConfig menu. The provided sdkconfig
is setup so that the source code will build the mz25key.
