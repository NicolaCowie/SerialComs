# SerialComs
On the Husky Hunter and Hawk the commands to transfer files to and from the serial port are SEND and IMP.

In this Git project are versions of these ulitiies for the Windows 10/11 console.

To Transfer from Husky to PC
----------------------------
1) Run the command INP on the PC (ie INP data.bin COM2 4800)
2) run the command SEND data.bin on the Husky
3) When the Husky finishes transmitting, press ESC on the PC to finish reception.

To Transfer from PC to Husky
----------------------------
1) Run the command INP 999 data.bin on the Husky
2) Run the command SEND on the PC (ie SEND data.bin COM2 4800)
3) When transmission is complete press ESC on the Husky

Communications require a 5 way crossed cable

  Tx  ----\ /---- Tx
           x
  Rx  ----/ \---- Rx

  Gnd ----------- GND

  CTS ----\ /---- CTS
           x
  RTS ----/ \---- RTS

  Communications on the Husky should be set to CTS=y and RTS=hold.
