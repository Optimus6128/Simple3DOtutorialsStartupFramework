#include "main.h"
#include "tools.h"


static ScreenContext screen;

static Item bitmapItems[SCREEN_PAGES];
static Bitmap *bitmaps[SCREEN_PAGES];

static Item vsyncItem;
static Item VRAMIOReq;
static IOInfo ioInfo;

static int visibleScreenPage = 0;
static int frameNum = 0;

ubyte *backgroundBufferPtr = NULL;

unsigned char *celBitmapData = NULL;

#define MYCCB_WIDTH 32
#define MYCCB_HEIGHT 32
#define NUM_SPRITES 64
CCB *myCCB[NUM_SPRITES];

void loadData()
{
	backgroundBufferPtr = LoadImage("data/background.img", NULL, (VdlChunk **)NULL, &screen);
}

void initSPORTwriteValue(unsigned value)
{
	memset(&ioInfo,0,sizeof(ioInfo));
	ioInfo.ioi_Command = FLASHWRITE_CMD;
	ioInfo.ioi_CmdOptions = 0xffffffff;
	ioInfo.ioi_Offset = value; // background colour
	ioInfo.ioi_Recv.iob_Buffer = bitmaps[0]->bm_Buffer;
	ioInfo.ioi_Recv.iob_Len = SCREEN_SIZE_IN_BYTES;
}

void initSPORTcopyImage(ubyte *srcImage)
{
	memset(&ioInfo,0,sizeof(ioInfo));
	ioInfo.ioi_Command = SPORTCMD_COPY;
	ioInfo.ioi_Offset = 0xffffffff; // mask
	ioInfo.ioi_Send.iob_Buffer = srcImage;
	ioInfo.ioi_Send.iob_Len = SCREEN_SIZE_IN_BYTES;
	ioInfo.ioi_Recv.iob_Buffer = bitmaps[0]->bm_Buffer;
	ioInfo.ioi_Recv.iob_Len = SCREEN_SIZE_IN_BYTES;
}

void initSPORTcloneImage(ubyte *srcImage)
{
	memset(&ioInfo,0,sizeof(ioInfo));
	ioInfo.ioi_Command = SPORTCMD_CLONE;
	ioInfo.ioi_Offset = 0xffffffff; // mask
	ioInfo.ioi_Send.iob_Buffer = srcImage;
	ioInfo.ioi_Send.iob_Len = GetPageSize(MEMTYPE_VRAM);	// This has to be the size of vram page (typically 2048 bytes, but could change)
	ioInfo.ioi_Recv.iob_Buffer = bitmaps[0]->bm_Buffer;
	ioInfo.ioi_Recv.iob_Len = SCREEN_SIZE_IN_BYTES;
}

void initSPORT()
{
	VRAMIOReq = CreateVRAMIOReq(); // Obtain an IOReq for all SPORT operations
	initSPORTwriteValue(0);
}

void initGraphics()
{
	int i;

	CreateBasicDisplay(&screen, DI_TYPE_DEFAULT, SCREEN_PAGES);

	for(i=0; i<SCREEN_PAGES; i++)
	{
		bitmapItems[i] = screen.sc_BitmapItems[i];
		bitmaps[i] = screen.sc_Bitmaps[i];
	}

	loadData();

	initSPORT();

	vsyncItem = GetVBLIOReq();
}

void initSystem()
{
	OpenMathFolio();
    OpenGraphicsFolio();
    InitEventUtility(1,0,LC_Observer);
	OpenAudioFolio();
}


unsigned int getBackgroundColor32(short color)
{
	return (color << 16) | color;
}

int getFrameNum()
{
	return frameNum;
}

void display()
{
    DisplayScreen(screen.sc_Screens[visibleScreenPage], 0 );

	if (ioInfo.ioi_Command != SPORTCMD_COPY && ioInfo.ioi_Command != SPORTCMD_CLONE) {
		WaitVBL(vsyncItem, 1);
	}

    visibleScreenPage = (visibleScreenPage + 1) % SCREEN_PAGES;

	ioInfo.ioi_Recv.iob_Buffer = bitmaps[visibleScreenPage]->bm_Buffer;
	DoIO(VRAMIOReq, &ioInfo);

	++frameNum;
}

int getJoystickState()
{
	ControlPadEventData cpaddata;
	cpaddata.cped_ButtonBits = 0;
	GetControlPad(1,0,&cpaddata);

	return cpaddata.cped_ButtonBits;
}

void inputScript()
{
	int joybits = getJoystickState();

	if(joybits & ControlA) {
		initSPORTwriteValue(getBackgroundColor32(MakeRGB15(31,15,7)));
	}

	if(joybits & ControlB) {
		initSPORTcopyImage(backgroundBufferPtr);
	}

	if(joybits & ControlC) {
		initSPORTcloneImage(backgroundBufferPtr);
	}
}

void runStuff()
{
	int i;
	const int ticks = getTicks();

	for (i=0; i<NUM_SPRITES; ++i) {
		const int t = ((ticks >> 4) + 3*i) & 255;
		const int s1 = SinF16(t << 16);
		const int c1 = CosF16(t << 16);
		const int s2 = SinF16((2*t) << 16);
		const int posX = (160 - MYCCB_WIDTH/2) + ((s1 * 100) >> 16);
		const int posY = (120 - MYCCB_HEIGHT/2) + ((s2 * 88) >> 16);

		myCCB[i]->ccb_XPos = (posX << 16);
		myCCB[i]->ccb_YPos = (posY << 16);

		myCCB[i]->ccb_HDX = c1 << 4;
		myCCB[i]->ccb_HDY = s1 << 4;

		myCCB[i]->ccb_VDX = s1;
		myCCB[i]->ccb_VDY = -c1;
	}

	DrawCels(bitmapItems[visibleScreenPage], myCCB[0]);
}

void initStuff()
{
	int i;
	unsigned short *bitmapData;

	// Init all CELs
	myCCB[0] = CreateCel(MYCCB_WIDTH, MYCCB_HEIGHT, 16, CREATECEL_UNCODED, NULL);
	bitmapData = (unsigned short*)myCCB[0]->ccb_SourcePtr;
	for (i=1; i<NUM_SPRITES; ++i) {
		myCCB[i] = CreateCel(MYCCB_WIDTH, MYCCB_HEIGHT, 16, CREATECEL_UNCODED, bitmapData);
		LinkCel(myCCB[i-1], myCCB[i]);
	}

	// Fill bitmap data with random colors
	for (i=0; i<MYCCB_WIDTH * MYCCB_HEIGHT; ++i) {
		*bitmapData++ = rand();
	}
}

void script()
{
	runStuff();

	displayFPS(bitmapItems[visibleScreenPage]);
}

int main()
{
	initSystem();
	initGraphics();
	initTools();
	
	initStuff();

	while(true) {
		script();
		inputScript();
		display();
	}
}
