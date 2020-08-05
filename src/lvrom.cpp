/****************************************************************************/
/*              Beebem - (c) David Alan Gilbert 1994                        */
/*              ------------------------------------                        */
/* This program may be distributed freely within the following restrictions:*/
/*                                                                          */
/* 1) You may not charge for this program or for any part of it.            */
/* 2) This copyright message must be distributed with all copies.           */
/* 3) This program must be distributed complete with source code.  Binary   */
/*    only distribution is not permitted.                                   */
/* 4) The author offers no warrenties, or guarentees etc. - you use it at   */
/*    your own risk.  If it messes something up or destroys your computer   */
/*    thats YOUR problem.                                                   */
/* 5) You may use small sections of code from this program in your own      */
/*    applications - but you must acknowledge its use.  If you plan to use  */
/*    large sections then please ask the author.                            */
/*                                                                          */
/* If you do not agree with any of the above then please do not use this    */
/* program.                                                                 */
/* Please report any problems to the author at beebem@treblig.org           */
/****************************************************************************/
/* Modified form Beebem */
/* Linux : http://beebem-unix.bbcmicro.com/ */
/* Mac : http://www.g7jjf.com/download.htm */
/* Windows : http://www.mkw.me.uk/beebem/index.html */

/* LVROM Support for Beebem */
/* Rhys Jones, 2020 */
/*
Work compiled from the public information on the 
Domesday rescue performed by CAMiLEON.
Based on scsi.cc in Beebem.


The LVRom disk image is hard-coded in three locations
(communityS, communityN, nationalA)

*/

#include <emscripten.h>


#include <stdio.h>
#include <stdlib.h>
#include "6502core.h"
#include "main.h"
#include "lvrom.h"
#include "beebmem.h"

enum lvdisc_t {
  nationalA,
  nationalB,
  communityS,
  communityN
};

enum phase_t {
	busfree,
	selection,
	command,
	execute,
	s_read,
	s_write,
	status,
	message
};

typedef struct {
	phase_t phase;
	bool sel;
	bool msg;
	bool cd;
	bool io;
	bool bsy;
	bool req;
	bool irq;
	char cmd[10];
	int status;
	int message;
	char buffer[0x800];
	int blocks;
	int next;
	int offset;
	int length;
	int lastwrite;
	int lun;
	int code;
	int sector;
        char response[256];
} lvrom_t;


lvrom_t lvrom;
FILE *LVROMDisc[4] = {NULL, NULL, NULL, NULL};
int LVROMSize[4];

char LvromDriveEnabled = 1;

// Define the U codes for each disk
char *lvdiscId[] ={
  "986", // National Side A
  "987", // National Side B
  "066", // Community South
  "067", // Community North
};

// Define the disc image filenames
char *lvdiscFilename[]={
  "nationalA.img",
  "nationalB.img",
  "communityS.img",
  "communityN.img"
};

// Which disc are we using?
//lvdisc_t currentDisc=nationalA;
//lvdisc_t currentDisc=communityN;
lvdisc_t currentDisc=communityN;
char* discName="communityN";

void initLVROM(void) {
}


void LVROMReset(void)
{
int i;

	lvrom.code = 0x00;
	lvrom.sector = 0x00;
	
	i=0;

	printf("Welcome to BeebEm 0.0.13+Domesday patch\n");
	printf("Compiled with Emscripten into WebAssembly by Rhys Jones\n");
	printf("\nKeys: f7 - prev, f8 - next, Click/Return - [ACTION], Tab - [CHANGE]\n");
	printf("\nLoading...\n");
	LVROMSize[i] = 0;
	LvromBusFree();
	lvromImageStatus = 0;
	lvromDisplayStatus = 1; // Video on by default
}

void LVROMWrite(int Address, int Value) 
{

  // ARJ
  // LVRom seems to XOR value with 0xff ??
  Value=Value^0xff;

  if (!LvromDriveEnabled)
    return;

  //WriteLog("Write lv Address = 0x%02x, Value = 0x%02x, Phase = %d, PC = 0x%04x\n", Address, Value, lvrom.phase, ProgramCounter);
  //printf("Write lv Address = 0x%02x, Value = 0x%02x, Phase = %d, PC = 0x%04x\n", Address, Value, lvrom.phase, ProgramCounter);
	
    switch (Address)
    {
		case 0x00:
			lvrom.sel = true;
			LvromWriteData(Value);
			break;
		case 0x01:
			lvrom.sel = true;
			break;
		case 0x02:
			lvrom.sel = false;
			LvromWriteData(Value);
			break;
		case 0x03:
			lvrom.sel = true;
			//if (Value == 0xff)
			if (Value == 0x00) // ARJ
			{
				lvrom.irq = true;
				intStatus |= (1<<hdc);
				lvrom.status = 0x00;
//				//WriteLog("Setting HDC Interrupt\n");
			}
			else
			{
				lvrom.irq = false;
				intStatus &= ~(1<<hdc);
//				//WriteLog("Clearing HDC Interrupt\n");
			}
				
			break;
    }
}

int LVROMRead(int Address)
{
int data = 0xff;


    if (!LvromDriveEnabled)
        return data;
    switch (Address)
    {
    case 0x00 :         // Data Register
        data = LvromReadData();
        break;
    case 0x01:			// Status Register
                data = 0x20;	// Hmmm.. don't know why req has to always be active ? If start at 0x00, ADFS lock up on entry
		if (lvrom.cd) data |= 0x80;
		if (lvrom.io) data |= 0x40;
		if (lvrom.req) data |= 0x20;
		if (lvrom.irq) data |= 0x10;
		if (lvrom.bsy) data |= 0x02;
		if (lvrom.msg) data |= 0x01;
        break;
    case 0x02:
        break;
    case 0x03:
        break;
    }

            //WriteLog("Read  lv Address = 0x%02x, Value = 0x%02x, Phase = %d, PC = 0x%04x\n", Address, data, lvrom.phase, ProgramCounter);
    //printf("Read  lv Address = 0x%02x, Value = 0x%02x, Phase = %d, PC = 0x%04x\n", Address, data, lvrom.phase, ProgramCounter);

    //printf("Address : %d : %d : %d\n", Address, lvrom.phase, data);
	
    return data;
}


int LvromReadData(void)
{
	int data;
	
	////WriteLog("LvromReadData - Phase = %d, PC = 0x%04x\n", lvrom.phase, ProgramCounter);

	switch (lvrom.phase)
	{
		case status :
			data = lvrom.status;
			lvrom.req = false;
			LvromMessage();
			return data;
			
		case message :
			data = lvrom.message;
			lvrom.req = false;
			LvromBusFree();
			return data;
			
		case s_read :
			data = lvrom.buffer[lvrom.offset];
			lvrom.offset++;
			lvrom.length--;
			lvrom.req = false;
			
			if (lvrom.length == 0) {
				lvrom.blocks--;
				if (lvrom.blocks == 0) {
					LvromStatus();
					return data;
				}
				
				lvrom.length = LvromReadSector(lvrom.buffer, lvrom.next);
				if (lvrom.length <= 0) {
					lvrom.status = (lvrom.lun << 5) | 0x02;
					lvrom.message = 0x00;
					LvromStatus();
					return data;
				}
				lvrom.offset = 0;
				lvrom.next++;
			}
			return data;
			break;
	}

	if (lvrom.phase == busfree)
		return lvrom.lastwrite;

	LvromBusFree();
	return lvrom.lastwrite;
}

void LvromWriteData(int data)
{
	lvrom.lastwrite = data;
	data = data & 0xff; // ARJ
	
	switch (lvrom.phase)
	{
		case busfree :
			if (lvrom.sel) {
				LvromSelection(data);
			}
			return;

		case selection :
			if (!lvrom.sel) {
				LvromCommand();
				return;
			}
			break;
			
		case command :
			lvrom.cmd[lvrom.offset] = data;
			if (lvrom.offset == 0) {
				if ((data >= 0x20) && (data <= 0x3f)) {
					lvrom.length = 10;
				}
			}
			lvrom.offset++;
			lvrom.length--;
			lvrom.req = false;

			if (lvrom.length == 0) {
				LvromExecute();
				return;
			}
			return;
			
		case s_write :

		  ////WriteLog("Adding %d to buffer at offset %d, length remaining %d\n", data, lvrom.offset, lvrom.length - 1);
			
			lvrom.buffer[lvrom.offset] = data;
			lvrom.offset++;
			lvrom.length--;
			lvrom.req = false;
			
			if (lvrom.length > 0)
				return;
				
			switch (lvrom.cmd[0] & 0xff) {
 			        case 0xca : return LvromFWrite();
				case 0x0a :
				case 0x15 :
				case 0x2a :
				case 0x2e :
					break;
				default :
					LvromStatus();
					return;
			}

			switch (lvrom.cmd[0]& 0xff) {
				case 0x0a :

				  //					//WriteLog("Buffer now full, writing sector\n");

					if (!LvromWriteSector(lvrom.buffer, lvrom.next - 1)) {
						lvrom.status = (lvrom.lun << 5) | 0x02;
						lvrom.message = 0;
						LvromStatus();
						return;
					}
					break;
				case 0x15 :
					if (!LvromWriteGeometry(lvrom.buffer)) {
						lvrom.status = (lvrom.lun << 5) | 0x02;
						lvrom.message = 0;
						LvromStatus();
						return;
					}
					break;
			}
				
			lvrom.blocks--;
			
//			//WriteLog("Blocks remaining %d\n", lvrom.blocks);

			if (lvrom.blocks == 0) {
				LvromStatus();
				return;
			}
			lvrom.length = 256;
			lvrom.next++;
			lvrom.offset = 0;
			return;
	}

	LvromBusFree();
}

void LvromBusFree(void)
{
  //  //WriteLog("BusFree\n");
	lvrom.msg = false;
	lvrom.cd = false;
	lvrom.io = false;
	lvrom.bsy = false;
	lvrom.req = false;
	lvrom.irq = false;
	
	lvrom.phase = busfree;

	//	LEDs.HDisc[0] = 0;
	//	LEDs.HDisc[1] = 0;
	//	LEDs.HDisc[2] = 0;
	//	LEDs.HDisc[3] = 0;
}

void LvromSelection(int data)
{
  //  //WriteLog("Selection\n");
	lvrom.bsy = true;
	lvrom.phase = selection;
}


void LvromCommand(void)

{
  //  //WriteLog("Command\n");
	lvrom.phase = command;
	
	lvrom.io = false;
	lvrom.cd = true;
	lvrom.msg = false;
	
	lvrom.offset = 0;
	lvrom.length = 6;
}

void LvromExecute(void)
{
	lvrom.phase = execute;
	/*
		if (lvrom.cmd[0] <= 0x1f) {
			printf("LvromExecute 0x%02x, Param 1=0x%02x, Param 2=0x%02x, Param 3=0x%02x, Param 4=0x%02x, Param 5=0x%02x, Phase = %d, PC = 0x%04x\n", lvrom.cmd[0], lvrom.cmd[1], lvrom.cmd[2], lvrom.cmd[3], lvrom.cmd[4], lvrom.cmd[5], lvrom.phase, ProgramCounter);
		} else {
			printf("LvromExecute 0x%02x, Param 1=0x%02x, Param 2=0x%02x, Param 3=0x%02x, Param 4=0x%02x, Param 5=0x%02x, Param 6=0x%02x, Param 7=0x%02x, Param 8=0x%02x, Param 9=0x%02x, Phase = %d, PC = 0x%04x\n", lvrom.cmd[0], lvrom.cmd[1], lvrom.cmd[2], lvrom.cmd[3], lvrom.cmd[4], lvrom.cmd[5], lvrom.cmd[6], lvrom.cmd[7], lvrom.cmd[8], lvrom.cmd[9], lvrom.phase, ProgramCounter);
		}
	*/
	lvrom.lun = (lvrom.cmd[1]) >> 5;

	//	LEDs.HDisc[lvrom.lun] = 1;
	//printf("Execute : %d\n", lvrom.cmd[0] & 0xff);
	switch (lvrom.cmd[0] & 0xff) {
		case 0x00 :
			LvromTestUnitReady();
			return;
		case 0x03 :
			LvromRequestSense();
			return;
		case 0x04 :
			LvromFormat();
			return;
		case 0x08 :
			LvromRead6();
			return;
		case 0x0a :
			LvromWrite6();
			return;
		case 0x0f :
			LvromTranslate();
			return;
		case 0x15 :
			LvromModeSelect();
			return;
		case 0x1a :
			LvromModeSense();
			return;
		case 0x1b :
			LvromStartStop();
			return;
		case 0x2f :
			LvromVerify();
			return;
	        case 0xc8 :
    		        LvromFRead();
			return;
        	case 0xca :
		        LvromWrite6();
			return;
	}
	
	
	lvrom.status = (lvrom.lun << 5) | 0x02;
	lvrom.message = 0x00;
	LvromStatus();
}

void setVPStatus(int vp) {
  printf("setVPStatus : %d\n", vp);
}

void setEStatus(int e) {
  // display statys
  // 0 : dont show video
  // 1 : show video, with screen overlayed
  // 2 : show video, with screen blended at 50% alpha
  if (e==0) {  lvromDisplayStatus = 0; }
  else if (lvromDisplayStatus != 2) {
    lvromDisplayStatus = 1;
  }

  printf("setEStatus : %d\n", e);
}

void onLoadWget(const char* fname){
  lvromImageStatus = 1;
 }

void onErrorWget(const char* fname){
  //printf("wget failed: %s", fname);
  lvromImageStatus = 0;
}

void LvromFWrite(void) {
  // Emulate LV-ROM F codes
  FILE *ifp;

  //WriteLog("F Write : %s\n", lvrom.buffer);

  printf("F Write : %s@\n", lvrom.buffer);

  strcpy(lvrom.response, "");

  if (!strncmp(lvrom.buffer, "?U", 2)) {
    sprintf(lvrom.response, "U1=%s\r", lvdiscId[currentDisc]);
  }
  if (!strncmp(lvrom.buffer, "VP1", 3)) {
    lvromImageStatus = 3;
    if (lvromDisplayStatus>=1) {lvromDisplayStatus = 1;}
    // Show background only
    strcpy(lvrom.response, "VP1\r");
    setVPStatus(1);
  }
  if (!strncmp(lvrom.buffer, "VP2", 3)) {
    lvromImageStatus = 0;
    if (lvromDisplayStatus>=1) {lvromDisplayStatus = 1;}
    // Show screen only
    strcpy(lvrom.response, "VP2\r");
    setVPStatus(2);
  }
  if (!strncmp(lvrom.buffer, "VP3", 3)) {
    // Show background and screen
    lvromImageStatus = 1;
    if (lvromDisplayStatus>=1) {lvromDisplayStatus = 1;}
    strcpy(lvrom.response, "VP3\r");
    setVPStatus(3);
  }
  if (!strncmp(lvrom.buffer, "VP4", 3)) {
    lvromDisplayStatus = 2;
    strcpy(lvrom.response, "VP4\r");
    setVPStatus(4);
  }
  if (!strncmp(lvrom.buffer, "VP5", 3)) {
    lvromDisplayStatus = 2;
    strcpy(lvrom.response, "VP5\r");
    setVPStatus(5);
  }
  if (!strncmp(lvrom.buffer, "A0", 2)) {
    strcpy(lvrom.response, "A\r");
  }
  if (!strncmp(lvrom.buffer, "B0", 2)) {
    strcpy(lvrom.response, "A\r");
  }
  if (!strncmp(lvrom.buffer, "A1", 2)) {
    strcpy(lvrom.response, "A\r");
  }
  if (!strncmp(lvrom.buffer, "B1", 2)) {
    strcpy(lvrom.response, "A\r");
  }
  if (!strncmp(lvrom.buffer, "E0", 2)) {
    strcpy(lvrom.response, "A\r");
    setEStatus(0);
  }
  if (!strncmp(lvrom.buffer, "E1", 2)) {
    strcpy(lvrom.response, "A\r"); 
    setEStatus(1);
  }
  if (!strncmp(lvrom.buffer, "I0", 2)) {
    strcpy(lvrom.response, "A\r");
  }
  if (!strncmp(lvrom.buffer, "I1", 2)) {
    strcpy(lvrom.response, "A\r");
  }
  if (!strncmp(lvrom.buffer, "J0", 2)) {
    strcpy(lvrom.response, "A\r");
  }
  if (!strncmp(lvrom.buffer, "J1", 2)) {
    strcpy(lvrom.response, "A\r");
  }
  if (!strncmp(lvrom.buffer, "$0", 2)) {
    // Need to have an open/closed eject status here
    //    strcpy(lvrom.response, "A\r");
    strcpy(lvrom.response, "O\r");
  }

  if (!strncmp(lvrom.buffer, "'", 1)) {
    // Eject disk and swap for other side
    if (currentDisc==nationalA) {currentDisc=nationalB;}
    else if (currentDisc==nationalB) {currentDisc=nationalA;}
    else if (currentDisc==communityN) {currentDisc=communityS;}
    else if (currentDisc==communityS) {currentDisc=communityN;}
    strcpy(lvrom.response, "O\r");
  }

  if (!strncmp(lvrom.buffer, ",1", 2)) {
    // Load disk
    strcpy(lvrom.response, "S\r");
  }

  if (!strncmp(lvrom.buffer, "/", 1)) {
    // Pause
    strcpy(lvrom.response, "A\r");
  }

  if (!strncmp(lvrom.buffer, "X", 1)) {
    // Clear
    strcpy(lvrom.response, "A\r");
  }

  if  (lvrom.buffer[0]=='F') {
    if (lvrom.buffer[strlen(lvrom.buffer)-2]=='R') {
      // Show image
      //printf("Display image : %s\n", lvrom.buffer);
      char b[32];
      strncpy(b, lvrom.buffer+1, strlen(lvrom.buffer)-2);
      b[strlen(lvrom.buffer)-3]=0;
      char url[256];
      if (strlen(lvrom.buffer)>7) {
	sprintf(url, "/images/%s/%c%c/%s.jpg", discName, b[0], b[1], b);
      } else if (strlen(lvrom.buffer)>6) {
	sprintf(url, "/images/%s/0%c/%s.jpg", discName, b[0], b);
      } else {
	sprintf(url, "/images/%s/00/%s.jpg", discName, b);
      }
      emscripten_async_wget(url, "/img.jpg", onLoadWget, onErrorWget);
      strcpy(lvrom.response, "A0\r");
    }
    //if (lvrom.buffer[strlen(lvrom.buffer)-2]=='N') {
      // Play from Frame no
      ////WriteLog("Display video : %s\n", lvrom.buffer);
    //strcpy(lvrom.response, "A1\r");
    //}
    if (lvrom.buffer[strlen(lvrom.buffer)-2]=='S') {
      // Set stop frame
      //WriteLog("Display video : %s\n", lvrom.buffer);
      strcpy(lvrom.response, "A2\r");
    }
    //      strcpy(lvrom.response, "A0\r");
  }

  if  (lvrom.buffer[0]=='Q') {
    if (lvrom.buffer[strlen(lvrom.buffer)-2]=='S') {
      // Show video
      //printf("Display video: %s\n", lvrom.buffer);
      // Once completed...
      strcpy(lvrom.response, "A7\r");
    }
  }

  if  (lvrom.buffer[0]=='N') {
    // Normal play
    strcpy(lvrom.response, "A1\r");
  }

  printf(">%s\n", lvrom.response);
  
  lvrom.status = (lvrom.lun << 5) | 0x02;
  lvrom.message = 0;
  LvromStatus();
  return;

  }

void LvromFRead(void) {


  //	//WriteLog("F Read\n");
  // printf("F Read\n");
	lvrom.blocks = 1;

	strcpy(lvrom.buffer, lvrom.response);

	lvrom.length =strlen(lvrom.buffer);
	lvrom.buffer[lvrom.length]=0x00;
	lvrom.length=256;
	//	//WriteLog("Buffer len: %d, Buffer : %s\n", lvrom.length, lvrom.buffer);
	//	printf("Buffer len: %d, Buffer : %s\n", lvrom.length, lvrom.buffer);
	

	if (lvrom.length <= 0) {
		lvrom.status = (lvrom.lun << 5) | 0x02;
		lvrom.message = 0x00;
		LvromStatus();
		return;
	}
	
	lvrom.status = (lvrom.lun << 5) | 0x00;
	lvrom.message = 0x00;
	
	lvrom.offset = 0;
	lvrom.next = 2;
	
	lvrom.phase = s_read;
	lvrom.io = true;
	lvrom.cd = false;
	lvrom.req = true;

  }

void LvromStatus(void)
{
	lvrom.phase = status;
	
	lvrom.io = true;
	lvrom.cd = true;
	lvrom.req = true;
}

void LvromMessage(void)
{
	lvrom.phase = message;
	
	lvrom.msg = true;
	lvrom.req = true;
}

bool LvromDiscTestUnitReady(char *buf)

{
  // ARJARJ
  //if (LVROMDisc[lvrom.lun] == NULL) return false;
	return true;
}

void LvromTestUnitReady(void)
{
	bool status;
	
	status = LvromDiscTestUnitReady(lvrom.cmd);
	if (status) {
		lvrom.status = (lvrom.lun << 5) | 0x00;
		lvrom.message = 0x00;
	} else {
		lvrom.status = (lvrom.lun << 5) | 0x02;
		lvrom.message = 0x00;
	}
	LvromStatus();
}

bool LvromDiscStartStop(char *buf)

{
	if (buf[4] & 0x02) {

// Eject Disc
		
	}
	return true;
}

void LvromStartStop(void)
{
	bool status;
	
	status = LvromDiscStartStop(lvrom.cmd);
	if (status) {
		lvrom.status = (lvrom.lun << 5) | 0x00;
		lvrom.message = 0x00;
	} else {
		lvrom.status = (lvrom.lun << 5) | 0x02;
		lvrom.message = 0x00;
	}
	LvromStatus();
}

void LvromRequestSense(void)
{
	lvrom.length = LvromDiscRequestSense(lvrom.cmd, lvrom.buffer);
	
	if (lvrom.length > 0) {
		lvrom.offset = 0;
		lvrom.blocks = 1;
		lvrom.phase = s_read;
		lvrom.io = TRUE;
		lvrom.cd = FALSE;
		
		lvrom.status = (lvrom.lun << 5) | 0x00;
		lvrom.message = 0x00;
		
		lvrom.req = true;
	}
	else
	{
		lvrom.status = (lvrom.lun << 5) | 0x02;
		lvrom.message = 0x00;
		LvromStatus();
	}
}

int LvromDiscRequestSense(char *cdb, char *buf)
{
	int size;
	
	size = cdb[4];
	if (size == 0)
		size = 4;
	
	switch (lvrom.code) {
		case 0x00 :
			buf[0] = 0x00;
			buf[1] = 0x00;
			buf[2] = 0x00;
			buf[3] = 0x00;
			break;
		case 0x21 :
			buf[0] = 0x21;
			buf[1] = (lvrom.sector >> 16) & 0xff;
			buf[2] = (lvrom.sector >> 8) & 0xff;
			buf[3] = (lvrom.sector & 0xff);
			break;
	}
	
	lvrom.code = 0x00;
	lvrom.sector = 0x00;
	
	return size;
}

void LvromRead6(void)
{
	int record;
	
	record = lvrom.cmd[1] & 0x1f;
	record <<= 8;
	record |= lvrom.cmd[2]&0xff; // ARJ
	record <<= 8;
	record |= lvrom.cmd[3]&0xff; // ARJ
	lvrom.blocks = lvrom.cmd[4]&0xff; // ARJ
	if (lvrom.blocks == 0)
		lvrom.blocks = 0x100;
	lvrom.length = LvromReadSector(lvrom.buffer, record);

	//	//WriteLog("Read Data : %d, %04x, %d\n", lvrom.lun, record, lvrom.blocks);
	//printf("Read Data : %d, %04x, %d\n", lvrom.lun, record, lvrom.blocks);
	
	if (lvrom.length <= 0) {
		lvrom.status = (lvrom.lun << 5) | 0x02;
		lvrom.message = 0x00;
		LvromStatus();
		return;
	}
	
	lvrom.status = (lvrom.lun << 5) | 0x00;
	lvrom.message = 0x00;
	
	lvrom.offset = 0;
	lvrom.next = record + 1;
	
	lvrom.phase = s_read;
	lvrom.io = true;
	lvrom.cd = false;
	
	lvrom.req = true;
}

EM_JS(void, get_lvrom_sector, (char* buf, int block_start), {
  // Note how we return the output of handleSleep() here.
  return Asyncify.handleSleep(function(wakeUp) {
      if (window.lvromcache==undefined) {window.lvromcache=[];}
      let block_end = block_start+255;
      let start64=block_start>>16;
      let start64s=''+start64;
      while(start64s.length < 4) {start64s = "0" + start64s;}
      var data;
      let p=block_start-(start64*65536);
      let q=p+256;

      if (window.lvromcache[start64]) {
	Module.writeArrayToMemory(window.lvromcache[start64].subarray(p,q), buf);
	wakeUp();
      } else {
	fetch("http://127.0.0.1:8080/communityN/lvrom_"+start64s, {})
	  .then(response => {
	      if (response.ok) {
		return response.arrayBuffer();
	      }
	    })
	  .then(data => {
	      window.lvromcache[start64] = new Uint8Array(data);
	      Module.writeArrayToMemory(window.lvromcache[start64].subarray(p,q), buf);
	      wakeUp();
	    });
      }
    });
  });

int LvromReadSector(char *buf, int block)

{
  //  printf("Read Sector. Lun %d, block %d\n", lvrom.lun, block);

  //if (LVROMDisc[lvrom.lun] == NULL) return 0;

  get_lvrom_sector(buf, block*256);
	//fseek(LVROMDisc[lvrom.lun], block * 256, SEEK_SET);
	
	//fread(buf, 256, 1, LVROMDisc[lvrom.lun]);
	
    
	return 256;
}

bool LvromWriteSector(char *buf, int block)

{
  //	if (LVROMDisc[lvrom.lun] == NULL) return false;

  // ARJ
  //fseek(LVROMDisc[lvrom.lun], block * 256, SEEK_SET);
	
    //WriteLog("!!! fwrite ignored !!!\n");
    //	fwrite(buf, 256, 1, LVROMDisc[lvrom.lun]);
    
	return true;
}

void LvromWrite6(void)
{
	int record;
	
	record = lvrom.cmd[1] & 0x1f;
	record <<= 8;
	record |= lvrom.cmd[2];
	record <<= 8;
	record |= lvrom.cmd[3];
	lvrom.blocks = lvrom.cmd[4];
	if (lvrom.blocks == 0)
		lvrom.blocks = 0x100;

	lvrom.length = 256;
	
	lvrom.status = (lvrom.lun << 5) | 0x00;
	lvrom.message = 0x00;
	
	lvrom.next = record + 1;
	lvrom.offset = 0;
	
	lvrom.phase = s_write;
	lvrom.cd = false;
	
	lvrom.req = true;
}

void LvromModeSense(void)

{
	lvrom.length = LvromDiscModeSense(lvrom.cmd, lvrom.buffer);
	
	if (lvrom.length > 0) {
		lvrom.offset = 0;
		lvrom.blocks = 1;
		lvrom.phase = s_read;
		lvrom.io = TRUE;
		lvrom.cd = FALSE;
		
		lvrom.status = (lvrom.lun << 5) | 0x00;
		lvrom.message = 0x00;
		
		lvrom.req = true;
	}
	else
	{
		lvrom.status = (lvrom.lun << 5) | 0x02;
		lvrom.message = 0x00;
		LvromStatus();
	}
}

int LvromDiscModeSense(char *cdb, char *buf)
{
	FILE *f;
	
	int size;
	
	char buff[256];
		
	return 0;

	if (LVROMDisc[lvrom.lun] == NULL) return 0;

	sprintf(buff, "%s/diskimg/lvrom%d.dsc", RomPath, lvrom.lun);
			
	f = fopen(buff, "rb");
			
	if (f == NULL) return 0;

	size = cdb[4];
	if (size == 0)
		size = 22;

	size = fread(buf, 1, size, f);
	
// heads = buf[15];
// cyl   = buf[13] * 256 + buf[14];
// step  = buf[21];
// rwcc  = buf[16] * 256 + buf[17];
// lz    = buf[20];
	
	fclose(f);

	return size;
}

void LvromModeSelect(void)
{

	lvrom.length = lvrom.cmd[4];
	lvrom.blocks = 1;
	
	lvrom.status = (lvrom.lun << 5) | 0x00;
	lvrom.message = 0x00;
	
	lvrom.next = 0;
	lvrom.offset = 0;
	
	lvrom.phase = s_write;
	lvrom.cd = false;
	
	lvrom.req = true;
}

bool LvromWriteGeometry(char *buf)
{
	FILE *f;
	
	char buff[256];
	// ARJARJ
	return true;
	
	if (LVROMDisc[lvrom.lun] == NULL) return false;
	
	sprintf(buff, "%s/diskimg/lvrom%d.dsc", RomPath, lvrom.lun);
	
	f = fopen(buff, "wb");
	
	if (f == NULL) return false;

	//WriteLog("!!! fwrite ignored !!!\n");
	
	//	fwrite(buf, 22, 1, f);
	
	fclose(f);
	
	return true;
}


bool LvromDiscFormat(char *buf)

{
// Ignore defect list
//  printf(">>>> LvromDiscFormat\n");
	FILE *f;
	char buff[256];
	
	if (LVROMDisc[lvrom.lun] != NULL) {
		fclose(LVROMDisc[lvrom.lun]);
		LVROMDisc[lvrom.lun] = NULL;
	}
	
	sprintf(buff, "%s/diskimg/lvrom%d.dat", RomPath, lvrom.lun);
	
	LVROMDisc[lvrom.lun] = fopen(buff, "wb");
	if (LVROMDisc[lvrom.lun] != NULL) fclose(LVROMDisc[lvrom.lun]);
	LVROMDisc[lvrom.lun] = fopen(buff, "rb+");
	
	if (LVROMDisc[lvrom.lun] == NULL) return false;

	sprintf(buff, "%s/diskimg/lvrom%d.dsc", RomPath, lvrom.lun);
	
	f = fopen(buff, "rb");
	
	if (f != NULL)
	{
		fread(buff, 1, 22, f);
		
		// heads = buf[15];
		// cyl   = buf[13] * 256 + buf[14];
		
		LVROMSize[lvrom.lun] = buff[15] * (buff[13] * 256 + buff[14]) * 33;		// Number of sectors on disk = heads * cyls * 33
		
		fclose(f);
	
	}
	
	return true;
}

void LvromFormat(void)
{
	bool status;
	
	status = LvromDiscFormat(lvrom.cmd);
	if (status) {
		lvrom.status = (lvrom.lun << 5) | 0x00;
		lvrom.message = 0x00;
	} else {
		lvrom.status = (lvrom.lun << 5) | 0x02;
		lvrom.message = 0x00;
	}
	LvromStatus();
}

bool LvromDiscVerify(char *buf)

{
	int sector;
	
	sector = lvrom.cmd[1] & 0x1f;
	sector <<= 8;
	sector |= lvrom.cmd[2];
	sector <<= 8;
	sector |= lvrom.cmd[3];
	
	if (sector >= LVROMSize[lvrom.lun])
	{
		lvrom.code = 0x21;
		lvrom.sector = sector;
		return false;
	}

	return true;
}

void LvromVerify(void)
{
	bool status;
	
	status = LvromDiscVerify(lvrom.cmd);
	if (status) {
		lvrom.status = (lvrom.lun << 5) | 0x00;
		lvrom.message = 0x00;
	} else {
		lvrom.status = (lvrom.lun << 5) | 0x02;
		lvrom.message = 0x00;
	}
	LvromStatus();
}

void LvromTranslate(void)
{
	int record;
	
	record = lvrom.cmd[1] & 0x1f;
	record <<= 8;
	record |= lvrom.cmd[2];
	record <<= 8;
	record |= lvrom.cmd[3];

	lvrom.buffer[0] = lvrom.cmd[3];
	lvrom.buffer[1] = lvrom.cmd[2];
	lvrom.buffer[2] = lvrom.cmd[1] & 0x1f;
	lvrom.buffer[3] = 0x00;
		
	lvrom.length = 4;
	
	lvrom.offset = 0;
	lvrom.blocks = 1;
	lvrom.phase = s_read;
	lvrom.io = TRUE;
	lvrom.cd = FALSE;
	
	lvrom.status = (lvrom.lun << 5) | 0x00;
	lvrom.message = 0x00;
		
	lvrom.req = true;
}


