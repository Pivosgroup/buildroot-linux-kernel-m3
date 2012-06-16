#include "hdmi_vmode.h"
 
// Table is derived from "High-Definition Multimedia Interface Specification Version 1.3a"  Undefined entries are set to 0:
unsigned N_Val[] = { 6144,	//according to 48Khz sampling frequency
	4096,			//according to 32Khz sampling frequency: 128*32
	6272,			//according to 44.1Khz sampling frequency
	6144,			//according to 48Khz sampling frequency
	12544,			//according to 88.2Khz sampling frequency
	12288,			//according to 96Khz sampling frequency
	22579,			//according to 176.4Khz sampling frequency
	24576,			//according to 192Khz sampling frequency
};

VModeInfoType VModeTables[NMODES] =  {
	
//  ModeIdType  PixClk    TagType                                     HVPos       HVRes           Aspect  _656Type                      PixRep
	{ {
	6, 7}, 2700, {
		InterlaceVNegHNeg, 6000, {
	1716, 264}}, {
	119, 18}, {
	720, 480}, _4or16, {
	0x03, 62, 3, 15, 114, 17, 5, 429}, 1},	// 6,7: 720 x 480i, pix repl
	     { {
	21, 22}, 2700, {
		InterlaceVNegHNeg, 5000, {
	1728, 625}}, {
	132, 22}, {
	720, 576}, _4, {
	3, 63, 3, 19, 138, 24, 2, 432}, 1},	// 21,22: 1440 x 576i
	     { {
	2, 3}, 2700, {
		ProgrVNegHNeg, 6000, {
	858, 525}}, {
	122, 36}, {
	720, 480}, _4or16, {
	0, 62, 6, 30, 60, 19, 9, 0}, 0},	// 2,3: 720 x 480p
	     { {
	17, 18}, 2700, {
		ProgrVNegHNeg, 5000, {
	864, 625}}, {
	132, 44}, {
	720, 576}, _4or16, {
	0, 64, 5, 39, 68, 12, 5, 0}, 0},	// 17,18: 720 x 576p
	     { {
	4, 0}, 7425, {
		ProgrVPosHPos, 6000, {
	1650, 750}}, {
	260, 25}, {
	1280, 720}, _16, {
	0, 40, 5, 20, 220, 110, 5, 0}, 0},	// 4: 1280 x 720p @60Hz
	     { {
	19, 0}, 7425, {
		ProgrVPosHPos, 5000, {
	1980, 750}}, {
	260, 25}, {
	1280, 720}, _16, {
	0, 40, 5, 20, 220, 440, 5, 0}, 0},	// 19: 1280 x 720p @50Hz
	     { {
	5, 0}, 7425, {
		InterlaceVPosHPos, 6000, {
	2200, 562}}, {
	192, 20}, {
	1920, 1080}, _16, {
	0, 44, 5, 15, 148, 88, 2, 1100}, 0},	// 5: 1920 x 1080i @60Hz
	     { {
	20, 0}, 7425, {
		InterlaceVPosHPos, 5000, {
	2640, 1125}}, {
	192, 20}, {
	1920, 1080}, _16, {
	0, 44, 5, 15, 148, 528, 2, 1320}, 0},	// 20: 1920 x 1080i @50Hz
	     { {
	16, 0}, 14850, {
		ProgrVPosHPos, 6000, {
	2200, 1125}}, {
	192, 41}, {
	1920, 1080}, _16, {
	0, 44, 5, 36, 148, 88, 4, 0}, 0},	// 16: 1920 x 1080p @60Hz
	     { {
	31, 0}, 14850, {
		ProgrVPosHPos, 5000, {
	2640, 1125}}, {
	192, 41}, {
	1920, 1080}, _16, {
	0, 44, 5, 36, 148, 528, 4, 0}, 0},	// 31(1): 1920 x 1080p @50Hz
};


//--------------------------------------------------------
//unsigned short GetPixelClock(unsigned char Index)
//{
//   return  VModeTables[Index].PixClk;
//}
    
