#include "graphic.h"
#include "window.h"
#include "printk.h"

int RectSet(Rect *r, int x, int y, int w, int h)
{
	assert(r != 0 && w> 0 && h>0);

	r->X = x;
	r->Y = y;
	r->W = w;
	r->H = h;

	return TRUE;
}

int RectAnd(Rect *r1, Rect *r2, Rect *r)
{
	assert(r1 != 0 && r2 != 0 && r != 0);

	r->X = MAX(r1->X, r2->X);
	r->Y = MAX(r1->Y, r2->Y);
	r->W = MIN(r1->X + r1->W, r2->X + r2->W) - r->X;
	r->H = MIN(r1->Y + r1->H, r2->Y + r2->H) - r->Y;

	if (r->W <= 0 || r->H <= 0)
		return FALSE;

	return TRUE;
}

void RectNormalize(Rect *r)
{
	assert(r != 0);

	if (r->W < 0)
	{
		r->W = -r->W;
		r->X -= r->W;
	}

	if (r->H < 0)
	{
		r->H = -r->H;
		r->H -= r->H;
	}
}

int RectCut(Rect *r1, Rect *r2, Rect* r)
{
	RectNormalize(r1);
	RectNormalize(r2);

	Rect rAnd;
	if (RectAnd(r1, r2, &rAnd) == 0) // 没有交集
	{
		r[0] = *r1;
		return 1;
	}

	int i = 0;
	if (r1->Y != rAnd.Y)
	{
		RectSet(&r[i++], r1->X, r1->Y, r1->W, rAnd.Y - r1->Y);
	}

	if (r1->Y + r1->H != rAnd.Y + rAnd.H)
	{
		RectSet(&r[i++], r1->X, rAnd.Y + rAnd.H, r1->W, (r1->Y + r1->H)
				- (rAnd.Y + rAnd.H));
	}

	if (r1->X != rAnd.X)
	{
		RectSet(&r[i++], r1->X, rAnd.Y, rAnd.X - r1->X, rAnd.H);
	}

	if (r1->X + r1->W != rAnd.X + rAnd.W)
	{
		RectSet(&r[i++], rAnd.X + rAnd.W, rAnd.Y, (r1->X + r1->W) - (rAnd.X
				+ rAnd.W), rAnd.H);
	}

	return i;
}

int RectInside(Rect *r1, int x, int y)
{
	if (x >= r1->X && x <= r1->X + r1->W && y >= r1->Y && y <= r1->Y + r1->H)
		return 1;

	return 0;
}
/*
void ClipRegionAdd(ClipRegion* clip, Rect* rect)
{
	assert(clip != 0);
	assert(rect != 0);

	ClipItem* t = (ClipItem*)malloc(sizeof(ClipItem));
	t->rect = *rect;
	t->next = clip->head;
	clip->head = t;

	return;
}

void ClipRegionClear(ClipRegion* clip)
{
	assert(clip != 0);

	ClipItem* current = clip->head;
	ClipItem* next;

	while(current != 0)
	{
		next = current->next;
		free(current);
		current = next;
	}

	return;
}

void ClipRegionAnd(ClipRegion* clip, Rect* rect)
{
	assert(clip != 0);
	assert(rect != 0);

	ClipItem* t = clip->head;
	Rect rect_tmp;

	while(t != 0)
	{
		if(RectAnd(&t->rect, rect, &rect_tmp ))
		{
			t->rect = rect_tmp;
		}

		t = t->next;
	}
}

void ClipRegionCut(ClipRegion* clip, Rect* rect)
{
	assert(clip != 0);
	assert(rect != 0);

	ClipItem* t = clip->head;
	ClipRegion reg =
	{	0};
	Rect rect_tmp[4];

	while(t != 0)
	{
		int cnt = RectCut(&t->rect, rect, rect_tmp );
		for(int i=0; i < cnt; i ++)
		{
			ClipRegionAdd(&reg, &rect_tmp[i]);
		}

		t = t->next;
	}

	ClipRegionClear(clip);
	*clip = reg;
}
*/

int FillRect(Graphic* g, Rect* rect, int color, DrawMode mode)
{
	assert(g != 0);
	assert(rect != 0);

	//_(g->screen, FILLRECT, &screen_rect, (void*)color, (void*)mode);
	return 0;
}

int DrawRect(Graphic* g, Rect* rect, int width, int color, DrawMode mode)
{
    return TRUE;
}

int DrawText(Graphic* g, Rect* rect, char* str, int color)
{
    return TRUE;
}

int DrawBitmap(Graphic* g, Rect* rect, char* str, int color);
int GraphicMove(int dx, int dy)
{
    return TRUE;
}



