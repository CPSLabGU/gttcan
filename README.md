# GTTCan
A time-triggered CAN Bus communication protocol loosely based on TTCan

Each node on the network will need to be able to communicate at 1mbps on the CAN Bus.
The NUoT (Network Unit of Time) is 0.1us. Each device should maintain its own local time at best-effort resolution. No device on the network should start transmitting until it has received a reference frame to make sure it is synchronised with the other devices.

##Reference Frame

A reference frame is a CAN frame from a node with an ID of 0-7 (the first 7 ids are reserved for time masters) with the most significant bit of the 8-byte data payload set to 1. Another bit is kept as a reserved bit, followed by 62 timing bits. 

 8 byte (64 bit) payload for reference frame
> Fr*tttttt* *tttttttt* *tttttttt* *tttttttt*

> F = Reference Frame

> r = reserved

> *t* = timevalue (62 bit)

If a time master wishes to transmit other information (not reference frames), the MSB must be set to 0. As the next bit is reservedThis does limit the masters transmit range to 0x0 - 0xEFFFFFFFFFFFFFFF
Whilst reference frames should be transmitted in the corresponding timeslot in the schedule, it is important that all nodes respond to the reference frame and immediately update their time
##Schedule
Tha main part of 


