#include "window.h"
/*
static void ClientToScreenPt(Window* this, int* x, int* y)
{
	Window* t = this;

	while (t != 0)
	{
		*x += t->rect.X;
		*y += t->rect.Y;

		t = t->parent;
	}
}

static void ClientToScreen(Window* this, Rect* rectClient, Rect* rectScreen)
{
	*rectScreen = *rectClient;
	Window* t = this;

	while (t != 0)
	{
		rectScreen->X += t->rect.X;
		rectScreen->Y += t->rect.Y;

		t = t->parent;
	}
}

static void GetWindowRect(Window* this, Rect* rectScreen)
{
	assert(this != 0);

	*rectScreen = this->rect;
	Window* t = this->parent;

	while (t != 0)
	{
		rectScreen->X += t->rect.X;
		rectScreen->Y += t->rect.Y;

		t = t->parent;
	}
}

static void* _Screen = 0;

Graphic* BeginPaint(Window* this)
{
	// client to screen
	Rect rectScreen;
	GetWindowRect(&rectScreen);

	this->graphic.rect = rectScreen;
	this->graphic.screen = _Screen;

	return &this->graphic;
}

int EndPaint(Graphic* g)
{
}

#define CURSOR_SIZE 4
#define CURSOR_CORLOR 	RGB(100,100,100)

static int InvertCursor(int x, int y)
{
	Rect r =
	{ x, y, CURSOR_SIZE, CURSOR_SIZE };
	_(_Screen, SETCLIP, &r, 0, 0);
	_(_Screen, FILLRECT, &r, (void*) CURSOR_CORLOR, (void*) XOR);
}

static Rect _DefaultRect =
{ 0, 0, 300, 300 };

static Window* OnCreate(Window* this)
{
	this->text = "Window";
	this->rect = _DefaultRect;

	_DefaultRect.X += 10;
	_DefaultRect.Y += 10;

	if (_DefaultRect.X > 700)
		_DefaultRect.X = 10;

	if (_DefaultRect.Y > 500)
		_DefaultRect.Y = 10;

	return this;
}

void GetClientRect(Rect* rect)
{
}

void DrawBorder(Window* this)
{
	Rect rLeft =
	{ 0, 0, 2, this->rect.H };

	FillRect(this, &rLeft, BORDER_COLOR, SET);

	Rect rTop =
	{ 0, 0, this->rect.W, 2 };
	FillRect(this, &rTop, BORDER_COLOR, SET);

	Rect rRight =
	{ this->rect.W - 2, 0, 2, this->rect.H };
	FillRect(this, &rRight, BORDER_COLOR, SET);

	Rect rBottom =
	{ 0, this->rect.H - 2, this->rect.W, 2 };
	FillRect(this, &rBottom, BORDER_COLOR, SET);
}

static Window* OnPaint(Window* this)
{
	Rect r =
	{ 0, 0, this->rect.W, this->rect.H };
	FillRect(this, &r, COLOR_GRAY, SET);

	if (this->border)
		DrawBorder(this);
}

static Window* WindowPaint(Window* this, ClipRegion* clip)
{
	if (this->childs)
	{
		Window* child = this->childs;
		while (child != 0)
		{
			WindowPaint(child, clip);
			child = child->brother;
		}
	}

	Rect screen_rect;
	ClientToScreen(this->parent, &this->rect, &screen_rect);

	//
	ClipItem* t = clip->head;
	Rect rAnd;

	while (t != 0)
	{
		if (RectAnd(&t->rect, &screen_rect, &rAnd))
		{
			_(_Screen, SETCLIP, &rAnd, 0, 0);
			_(this, PAINT, 0, 0, 0);
		}

		t = t->next;
	}

	ClipRegionCut(clip, &screen_rect);
	return this;
}

static Window* OnInvalidate(Window* this, Rect* r)
{
	Rect tmp;

	if (r == 0)
		RectSet(&tmp, 0, 0, this->rect.W, this->rect.H);
	else
		tmp = *r;

	Rect screen_rect;
	ClientToScreen(this, &tmp, &screen_rect);

	ClipRegion clip =
	{ 0 };
	ClipRegionAdd(&clip, &screen_rect);

	Window* family = this;

	while (family->parent != 0)
	{
		Window* family_brother = family->parent->childs;

		while (family_brother != 0 && family_brother != family)
		{
			ClientToScreen(family->parent, &family_brother->rect, &screen_rect);
			ClipRegionCut(&clip, &screen_rect);
			family_brother = family_brother->brother;
		}

		//assert(family_brother == family);

		ClientToScreen(family->parent, &family->rect, &screen_rect);
		ClipRegionAnd(&clip, &family->rect);

		//
		family = family->parent;
	}

	WindowPaint(this, &clip);
	ClipRegionClear(&clip);
	return this;
}

static Window* OnAppend(Window* this, Window* child)
{
	assert(child != 0);
	child->parent = this;
	child->brother = this->childs;
	this->childs = child;

	return this;
}

static Window* OnSetRect(Window* this, Rect* r)
{
	if (this->parent == 0)
		return 0;

	Rect old = this->rect;
	this->rect = *r;

	OnInvalidate(this, 0);

	Rect clip_rect[4];

	int cnt = RectCut(&old, r, clip_rect);

	for (int i = 0; i < cnt; i++)
		OnInvalidate(this->parent, &clip_rect[i]);

	return this;
}

static Window* OnGetRect(Window* this, Rect* r)
{
	*r = this->rect;
	return this;
}

static Window* OnActive(Window* this)
{
	return this;
}

static int OnMouse(Window* this, MOUSE_STATUS* mouse, MOUSE_STATUS* mouse_last)
{
	Rect screen;
	GetWindowRect(this, &screen);

	if (!RectInside(&screen, mouse->X, mouse->Y))
		return 0;

	if (this->childs)
	{
		Window* child = this->childs;
		while (child != 0)
		{
			if (OnMouse(child, mouse, mouse_last))
				return 1;

			child = child->brother;
		}
	}

	// handle move
	int dx = mouse->X - mouse_last->X;
	int dy = mouse->Y - mouse_last->Y;

	//printk("==dx=%d ==dy=%d\n", dx, dy);

	if (dx != 0 || dy != 0)
	{
		if (mouse->LButton)
		{
			Rect new_rect = this->rect;
			new_rect.X += dx;
			new_rect.Y += dy;

			_(this, SETRECT, &new_rect, 0, 0);
		}

		InvertCursor(mouse_last->X, mouse_last->Y);
		InvertCursor(mouse->X, mouse->Y);
	}

	return 1;
}

void* WindowProc(void* this, int msg, void* arg0, void* arg1, void* arg2)
{
	assert(this != 0);

	switch (msg)
	{
		case CREATE:
		return OnCreate((Window*)this);
		case APPEND:
		return OnAppend((Window*)this, (Window*)arg0);
		case INVALIDATE:
		return OnInvalidate((Window*)this, (Rect*)arg0);
		case PAINT:
		return OnPaint((Window*)this);
		case MOUSE:
		return (void*)OnMouse((Window*)this, (MOUSE_STATUS*)arg0, (MOUSE_STATUS*)arg1);
		case SETRECT:
		return OnSetRect((Window*)this, (Rect*)arg0);
		case GETRECT:
		return OnGetRect((Window*)this, (Rect*)arg0);
		case ACTIVE:
		return OnActive((Window*)this);
		break;
	}

	return 0;
}
*/

int mouse_x = 500, mouse_y = 300, mouse_b;

//void fill_invert(int x, int y, int w, int h);
void restore_cursor(int x, int y);
void save_cursor(int x, int y);
void show_cursor(int x, int y);

void mouse_event(int data[])
{
    restore_cursor(mouse_x, mouse_y);
    //fill_invert(mouse_x, mouse_y, 10, 10);

    mouse_b = data[0];
    mouse_x += data[1];
    mouse_y -= data[2];

    if(mouse_x < 0)
        mouse_x = 0;

    if(mouse_x > (800 - 16 -1))
        mouse_x = (800 - 16 -1);

    if(mouse_y < 0)
        mouse_y = 0;

    if(mouse_y > (600 - 16 -1))
        mouse_y = (600 - 16 -1);

    save_cursor(mouse_x, mouse_y);
    show_cursor(mouse_x, mouse_y);
    //fill_invert(mouse_x, mouse_y, 10, 10);
}
