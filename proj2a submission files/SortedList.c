//NAME: Ege Tanboga
//EMAIL: ege72282@gmail.com
//ID: 304735411

#include "SortedList.h"
#include <sched.h>
#include <string.h>
#include <stdio.h>

void SortedList_insert(SortedList_t *list, SortedListElement_t *element) {
    if (list == NULL) {
        return;
    }
    SortedListElement_t* curr = list->next;
    while(curr->key != NULL && strcmp(curr->key, element->key) < 0) {
        curr = curr->next;
    }
    curr->prev->next = element;
    element->prev = curr->prev;
    if(opt_yield & INSERT_YIELD) {
        sched_yield();
    }
    element->next = curr;
    curr->prev = element;
    
}

int SortedList_delete( SortedListElement_t *element) {
    if ((element == NULL) || (element->prev->next != element->next->prev)) {
        //list is corrupted
        return 1;
    }
    if (opt_yield & DELETE_YIELD) {
        sched_yield();
    }
    element->prev->next = element->next;
    element->next->prev = element->prev;
    return 0;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key) {
    if ((key == NULL) || (list == NULL)) {
        return NULL;
    }
    SortedListElement_t *curr = list->next;
    while (curr != list) {
        if (strcmp(curr->key, key) > 0) { //we went past element
            return NULL;
        }
        else if (strcmp(curr->key, key) == 0) {
            return curr;
        }
        if (opt_yield & LOOKUP_YIELD) {
            sched_yield();
        }
        curr = curr->next;
    }
    return NULL;
}


int SortedList_length(SortedList_t *list) {
    if (list == NULL) {
        return -1;
    }
    int length = 0;
    SortedListElement_t *curr = list->next;
    while (curr != list) {
        if ((curr->next->prev != curr) || (curr->prev->next != curr)) {
            //then our list is corrupted
            return -1;
        }
        length += 1;
        if (opt_yield & LOOKUP_YIELD) {
            sched_yield();
        }
        curr = curr->next;
    }
    return length;
}
