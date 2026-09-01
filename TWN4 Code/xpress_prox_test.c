#include "twn4.sys.h"
#include "apptools.h"

const unsigned char AppManifest[] = { EXECUTE_APP, 1, EXECUTE_APP_ALWAYS, TLV_END };

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

#define MAXIDBYTES  8
#define MAXIDBITS   (MAXIDBYTES*8)



byte ID[MAXIDBYTES];
int IDBitCnt;
int TagType;

byte LastID[MAXIDBYTES];
int LastIDBitCnt;
int LastTagType;

byte ucInput;


const char *GetTagName(int TagType)
{
	switch (TagType)
	{
	//125kHz
	case LFTAG_EM4102:    return "EM4x02/CASI-RUSCO";
	case LFTAG_HITAG1S:   return "HITAG 1/HITAG S";
	case LFTAG_HITAG2:    return "HITAG 2";
	case LFTAG_EM4150:    return "EM4x50";
	case LFTAG_AT5555:    return "T55x7";
	case LFTAG_ISOFDX:    return "ISO FDX-B";
	case LFTAG_HIDPROX:   return "HID Prox";
	case LFTAG_TIRIS:     return "ISO HDX/TIRIS";
	case LFTAG_COTAG:     return "Cotag";
	case LFTAG_IOPROX:    return "ioProx";
	case LFTAG_INDITAG:   return "Indala";
	case LFTAG_HONEYTAG:  return "NexWatch";
	case LFTAG_AWID:      return "AWID";
	case LFTAG_GPROX:     return "G-Prox";
	case LFTAG_PYRAMID:   return "Pyramid";
	case LFTAG_KERI:      return "Keri";
	//13.56MHz
	case HFTAG_MIFARE:    return "ISO14443A/MIFARE";
	case HFTAG_ISO14443B: return "ISO14443B";
	case HFTAG_ISO15693:  return "ISO15693";
	case HFTAG_LEGIC:     return "LEGIC";
	case HFTAG_HIDICLASS: return "HID iCLASS";
	case HFTAG_FELICA:    return "FeliCa";
	case HFTAG_SRX:       return "SRX";
	case HFTAG_NFCP2P:    return "NFC Peer-to-Peer";
	}
	return "Unknown";
}

void makeMusic(void) 
{
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
}

// ******************************************************************

int main(void)
{
    //	bool bWakeupSent = false;
	// Show the startup message

	// COM1 is running @ 9600 Baud 8N1 by default. The code below shows how to
	// modify default baud rate/parity/stop bits.
	
	TCOMParameters COMParameters;
	COMParameters.BaudRate = 19200;
  	COMParameters.WordLength = COM_WORDLENGTH_8;
	COMParameters.Parity = COM_PARITY_NONE;
  	COMParameters.StopBits = COM_STOPBITS_1;
	COMParameters.FlowControl = COM_FLOWCONTROL_NONE;
	
	//SetHostChannel(CHANNEL_COM1);
	//SetCOMParameters(CHANNEL_COM1,&COMParameters);
	//HostWriteString("Kilroy was here COM1\r\n");
	//SetHostChannel(CHANNEL_COM2);
	//SetCOMParameters(CHANNEL_COM2,&COMParameters);
	//HostWriteString("Kilroy was here COM2\r\n");
	SetHostChannel(CHANNEL_USB);
	HostWriteString("Kilroy was here USB\r\n");
	
    //SetTagTypes(0xFFFFFFFF,0xFFFFFFFF);
    SetTagTypes(LFTAG_HIDPROX, HFTAG_HIDICLASS | HFTAG_MIFARE);

    // No transponder found up to now
    LastTagType = NOTAG;
	LEDInit(REDLED | GREENLED);
	LEDOff(REDLED);
	LEDOn(GREENLED);
    while (true)
    {

        // Search a transponder
        if (SearchTag(&TagType,&IDBitCnt,ID,sizeof(ID)))
		{
            LEDOff(GREENLED);
			LEDOn(REDLED);
			makeMusic();
			// Is this transponder new to us?
			if (TagType != LastTagType || IDBitCnt != LastIDBitCnt || !CompBits(ID,0,LastID,0,MAXIDBITS))
			{
				SetHostChannel(CHANNEL_COM1);
				//HostWriteString("HelloWorld COM1\r\n");				
				//SetHostChannel(CHANNEL_COM2);
				//HostWriteString("HelloWorld COM2\r\n");
				//SetHostChannel(CHANNEL_USB);
				//HostWriteString("HelloWorld USB\r\n");

				// Save this as known ID, before modifying the ID for proper output format
				CopyBits(LastID,0,ID,0,MAXIDBITS);
				LastIDBitCnt = IDBitCnt;
				LastTagType = TagType;
				HostWriteString("0x");
				HostWriteHex((byte*)&IDBitCnt,8,1);
					// Write ID with appropriate number of digits
				HostWriteHex(ID,IDBitCnt,(IDBitCnt+7)/8*2);
	        	HostWriteString("\r\n");

			}
		
			// Start a timeout of two seconds
			StartTimer(500);
        }
        if (TestTimer())
        {
            LEDOff(REDLED);
            LEDOn(GREENLED);
            LastTagType = NOTAG;
            memset(LastID,0,MAXIDBYTES);
        }

        while (HostTestChar())
        {
			SetHostChannel(CHANNEL_USB);
			LEDOff(GREENLED);
			LEDOff(REDLED);	
            memset(LastID,0,MAXIDBYTES);
            ucInput = HostReadChar();
            if((ucInput=='v') || (ucInput=='V')) {          // Version
			   	HostWriteString("XPressProx v1.51\r\n");
			} 
			else if((ucInput=='a') ||(ucInput=='A')) {      // Alive
			   	HostWriteString("XPressProx Alive\r\n");
			}
			else if(ucInput=='?') {                         // Commands

			   	HostWriteString("a - Alive\r\n");
			   	HostWriteString("v - Version\r\n");
			   	HostWriteString("? - Help\r\n");
			}
			else
			{
	            HostWriteByte(ucInput);
			}
			
			LEDOn(GREENLED);
			SetHostChannel(CHANNEL_COM1);

        }
    }
	return 0;
}
