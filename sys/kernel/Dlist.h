

#ifndef DLIST_H
#define DLIST_H

#include "spinlock.h"

typedef struct DListNode
{
    struct DListNode* prev;
    struct DListNode* next;
}DListNode;

typedef struct DList
{
	spinlock_t lock;
    DListNode* head;
    DListNode* tail;
    int count;
}DList;

int DListCount(DList* list);
DListNode* DListGetNext(DListNode* node);
DListNode* DListGetPrev(DListNode* node);
DListNode* DListRemoveHead(DList* list);
DListNode* DListRemove(DList* list, DListNode* node);

void DListAddHead(DList* list, DListNode* node);
void DListAddTail(DList* list, DListNode* node);

#endif
