#ifndef __HDMI_VMODE_H__
#define __HDMI_VMODE_H__

#define ProgrVPosHPos 0x03
#define ProgrVNegHNeg 0x00
#define InterlaceVPosHPos 0x07
#define InterlaceVNegHNeg 0x04
    typedef enum { _4 = 0,	// 4:3
	_4or16 = 1,		// 4:3 or 16:9
	_16 = 2			// 16:9
} Aspect_ratio_t;

#define NMODES 10		//44+48
    typedef struct {
	unsigned char Mode_C1;
	 unsigned char Mode_C2;
} ModeIdType;
typedef struct {
	unsigned short Pixels;
	 unsigned short Lines;
} PxlLnTotalType;
typedef struct {
	unsigned short H;	//(HSYNC width) + (horizontal back porch) + (horizontal left border), and is used only for DE generation
	unsigned short V;	//(VSYNC width) + (vertical back porch) + (vertical top border)
} HVPositionType;
typedef struct {
	unsigned short H;
	 unsigned short V;
} HVResolutionType;
typedef struct {
	unsigned char RefrTypeVHPol;
	 unsigned short VFreq;
	 PxlLnTotalType Total;
} TagType;
typedef struct {
	HVPositionType HVPos;
	HVResolutionType HVRes;
} DEType;
typedef struct {
	unsigned char IntAdjMode;
	 unsigned short HLength;
	 unsigned short VLength;
	 unsigned short Top;
	 unsigned short Dly;
	 unsigned short HBit2HSync;
	 unsigned short VBit2VSync;
	 unsigned short Field2Offset;
} _656Type;
typedef struct {
	ModeIdType ModeId;
	unsigned short PixClk;
	 TagType Tag;
	 HVPositionType Pos;
	 HVResolutionType Res;
	 unsigned char AspectRatio;
	 _656Type _656;
	 unsigned char PixRep;
} VModeInfoType;

#endif	/*  */
    
