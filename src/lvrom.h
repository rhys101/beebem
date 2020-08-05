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

*/

#ifndef LVROM_HEADER
#define LVROM_HEADER

extern char LvromDriveEnabled;
extern int lvromImageStatus;
extern int lvromDisplayStatus;

void initLVROM(void);
void LVROMReset(void);
void LVROMWrite(int Address, int Value) ;
int LVROMRead(int Address);
int LvromReadData(void);
void LvromWriteData(int data);
void LvromBusFree(void);
void LvromMessage(void);
void LvromSelection(int data);
void LvromCommand(void);
void LvromExecute(void);
void LvromStatus(void);
void LvromTestUnitReady(void);
void LvromRequestSense(void);
int LvromDiscRequestSense(char *cdb, char *buf);
void LvromRead6(void);
void LvromWrite6(void);
int LvromReadSector(char *buf, int block);
bool LvromWriteSector(char *buf, int block);
void LvromStartStop(void);
void LvromModeSense(void);
int LvromDiscModeSense(char *cdb, char *buf);
void LvromModeSelect(void);
bool LvromWriteGeometry(char *buf);
bool LvromDiscFormat(char *buf);
void LvromFormat(void);
bool LvromDiscVerify(char *buf);
void LvromVerify(void);
void LvromTranslate(void);
void LvromFRead(void);
void LvromFWrite(void);
#endif
