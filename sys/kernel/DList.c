
#include "kernel.h"
#include "printk.h"
#include "Dlist.h"

int DListCount(DList* list)
{
	assert(list);
	return list->count;
}

DListNode* DListGetNext(DListNode* node)
{
	assert(node);
	return node->next;
}

DListNode* DListGetPrev(DListNode* node)
{
	assert(node);
	return node->prev;
}

void DListAddHead(DList* list, DListNode* node)
{
	assert(list);
	assert(node);

	spin_lock(&list->lock);

	if(list->head)
	{
		list->head->prev = node;
		node->next = list->tail;
		node->prev = NULL;
		list->head = node;
	}
	else// empty
	{
		list->head = list->tail = node;
		node->prev = node->next = NULL;
	}

	list->count ++;

	spin_unlock(&list->lock);
}

void DListAddTail(DList* list, DListNode* node)
{
	assert(list);
	assert(node);

	spin_lock(&list->lock);

	if(list->tail)
	{
		list->tail->next = node;
		node->prev = list->tail;
		node->next = NULL;
		list->tail = node;
	}
	else// empty
	{
		list->head = list->tail = node;
		node->prev = node->next = NULL;
	}

	list->count ++;

	spin_unlock(&list->lock);
}

DListNode* DListRemoveHead(DList* list)
{
	if(list->head == NULL)
		return NULL;

	return DListRemove(list, list->head);
}

DListNode* DListRemove(DList* list, DListNode* node)
{
	assert(list);
	assert(node);

	DListNode* prev = node->prev;
	DListNode* next = node->next;

	spin_lock(&list->lock);

	if(prev)
		prev->next = node->next;
	else
		list->head = node->next; // head

	if(next)
		next->prev = node->prev;
	else
		list->tail = node->prev; // last

	list->count --;
	spin_unlock(&list->lock);

	return node;
}

