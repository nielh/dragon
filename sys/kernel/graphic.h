#ifndef GRAPHIC_H
#define GRAPHIC_H

#include "kernel.h"

#define MAX(a,b)    ((a)>=(b) ?(a):(b))
#define MIN(a,b)    ((a)>=(b) ?(b):(a))

typedef struct MOUSE_STATUS
{
	char LButton;
	char MButton;
	char RButton;
	int X;
	int Y;
}MOUSE_STATUS;


typedef enum DrawMode
{
	SET, XOR
} DrawMode;

enum MESSAGE
{
	CREATE, PAINT, ACTIVE, SETRECT,
	GETRECT, INVALIDATE,SETCLIP, MOUSE,
};

typedef struct Rect
{
	int X;
	int Y;
	int W;
	int H;

} Rect;

int RectCut(Rect *r1, Rect *r2, Rect* r);
int RectAnd(Rect *r1, Rect *r2, Rect *r);

int RectSet(Rect *r, int x, int y, int w, int h);
int RectInside(Rect *r1, int x, int y);

//void* UICreate(int size, PROC proc);
void RectNormalize(Rect *r);

typedef struct ClipItem
{
	struct ClipItem* next;
	Rect rect;

} ClipItem;

typedef struct ClipRegion
{
	ClipItem* head;

} ClipRegion;

void ClipRegionCut(ClipRegion* clip, Rect* rect);
void ClipRegionAdd(ClipRegion* clip, Rect* rect);
void ClipRegionAnd(ClipRegion* clip, Rect* rect);
void ClipRegionClear(ClipRegion* clip);

#define RGB(r,g,b)		((r<< 16) + (g << 8) + b)
#define COLOR_GRAY 		RGB(236, 233, 216)
#define COLOR_BLACK		RGB(0, 0, 0)
#define BORDER_COLOR 	0xff00

typedef struct Graphic
{
	Rect rect;
	void* screen;

}Graphic;

int FillRect(Graphic* g, Rect* rect, int color, DrawMode mode);
int DrawRect(Graphic* g, Rect* rect, int width, int color, DrawMode mode);
int DrawText(Graphic* g, Rect* rect, char* str, int color);
int DrawBitmap(Graphic* g, Rect* rect, char* str, int color);
int GraphicMove(int dx, int dy);

#endif
