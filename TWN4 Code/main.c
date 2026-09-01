// ******************************************************************
//
//    File: main_uart_esp32.c
//
// Purpose: TWN4 App - reads a card and forwards its ID to an ESP32
//          over a direct UART link, using a small framed protocol so
//          the ESP32 can reliably tell where one message ends and the
//          next begins (plain "just send the bytes" over a serial
//          line has no message boundaries otherwise).
//
//    Frame layout (little-endian where it matters):
//      [0]        STX           = 0xAA
//      [1]        TagType       (as returned by SearchTag)
//      [2..3]     IDBitCount    (uint16, low byte first)
//      [4]        IDByteCount   = number of ID bytes that follow
//      [5..]      ID bytes      (IDByteCount bytes)
//      [next]     Checksum      = XOR of everything from TagType
//                                 through the last ID byte
//      [next]     ETX           = 0x55
//
//    Wiring: this channel's TX/RX go to the ESP32's RX/TX (crossed),
//    plus a shared GND. If nothing arrives on the ESP32 side, try
//    switching UART_CHANNEL to CHANNEL_COM2 - which physical port on
//    the devkit header maps to which COM channel varies by model.
//
// ******************************************************************

#include "twn4.sys.h"

#define UART_CHANNEL   CHANNEL_COM2
#define UART_BAUDRATE  9600

#define STX  0xAA
#define ETX  0x55

#define MAX_ID_BYTES  16

#define NoteD1               1174.659
#define NoteD2               2349.318
#define NoteA                1760.000
#define NoteAb              1661.219
#define NoteG                1567.982
#define NoteF                1396.913
#define NoteCS              1108.731
#define NoteC               1046.502

#define NoteE    1318.510   // E6
#define NoteB     987.767   // B5
#define NoteA5    880.000   // A5

static byte ComputeChecksum(const byte *Data, int Length)
{
    byte cs = 0;
    for (int i = 0; i < Length; i++)
        cs ^= Data[i];
    return cs;
}

int main(void)
{
    // ---- LEDs / feedback ----
    LEDInit(REDLED | GREENLED | YELLOWLED);

    // ---- UART setup ----
    TCOMParameters ComParams;
    ComParams.BaudRate    = UART_BAUDRATE;
    ComParams.WordLength  = COM_WORDLENGTH_8;
    ComParams.Parity      = COM_PARITY_NONE;
    ComParams.StopBits    = COM_STOPBITS_1;
    ComParams.FlowControl = COM_FLOWCONTROL_NONE;

    InitChannel(UART_CHANNEL);
    SetCOMParameters(UART_CHANNEL, &ComParams);

    // ---- Tag types to search for - trim to what you actually use ----
    SetTagTypes(TAGMASK(LFTAG_EM4102) | TAGMASK(LFTAG_HITAG2),
                TAGMASK(HFTAG_MIFARE) | TAGMASK(HFTAG_ISO15693));

    while (true)
    {
        int  TagType;
        int  IDBitCount;
        byte ID[MAX_ID_BYTES];

        if (SearchTag(&TagType, &IDBitCount, ID, MAX_ID_BYTES))
        {
            LEDOn(GREENLED);


            Beep(25, NoteD1, 100, 0);
            Beep(25, NoteD1, 100, 0);
            Beep(25, NoteD2, 200, 0);
            Beep(25, NoteA, 200, 25);
            Beep(25, NoteAb, 100, 100);
            Beep(25, NoteG, 100, 100);
            Beep(25, NoteF, 200, 0);
            Beep(25, NoteD1, 100, 0);
            Beep(25, NoteF, 100, 0);
            Beep(25, NoteG, 100, 0);

            Beep(25, NoteC, 100, 0);
            Beep(25, NoteC, 100, 0);
            Beep(25, NoteD2, 200, 0);
            Beep(25, NoteA, 200, 100);
            Beep(25, NoteAb, 100, 100);
            Beep(25, NoteG, 100, 100);
            Beep(25, NoteF, 200, 0);
            Beep(25, NoteD1, 100, 0);
            Beep(25, NoteF, 100, 0);
            Beep(25, NoteG, 100, 0);

            

            int IDByteCount = (IDBitCount + 7) / 8;
            if (IDByteCount > MAX_ID_BYTES)
                IDByteCount = MAX_ID_BYTES;

            // ---- Build the frame ----
            byte Frame[7 + MAX_ID_BYTES];
            int  Pos = 0;

            Frame[Pos++] = STX;
            Frame[Pos++] = (byte)TagType;
            Frame[Pos++] = LOBYTE(IDBitCount);
            Frame[Pos++] = HIBYTE(IDBitCount);
            Frame[Pos++] = (byte)IDByteCount;

            CopyBytes(&Frame[Pos], ID, IDByteCount);
            Pos += IDByteCount;

            byte Checksum = ComputeChecksum(&Frame[1], Pos - 1); // everything after STX
            Frame[Pos++] = Checksum;
            Frame[Pos++] = ETX;

            WriteBytes(UART_CHANNEL, Frame, Pos);

            LEDOff(GREENLED);

            SetRFOff();
            Sleep(500, 0); // debounce so one tap doesn't send 20 frames
        }
    }
}