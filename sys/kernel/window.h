
#ifndef WINDOW_H
#define WINDOW_H

#include "kernel.h"
#include "DList.h"

#include "graphic.h"

typedef struct Window
{
	Rect rect;
	struct Window* parent;

	DListNode brother;
	DList childlist;

	void* screen;

	int bgcolor;
	char* text;

	int border;

}Window;


void* WindowProc(void* this, int msg, void* arg0, void* arg1, void* arg2);

int BeginPaint(Graphic* g, Window* wnd);
int EndPaint(Graphic* g);

#endif
