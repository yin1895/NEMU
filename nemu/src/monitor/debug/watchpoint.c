#include "monitor/watchpoint.h"
#include "monitor/expr.h"

#define NR_WP 32

static WP wp_pool[NR_WP];
static WP *head, *free_;

void init_wp_pool() {
	int i;
	for(i = 0; i < NR_WP; i ++) {
		wp_pool[i].NO = i;
		wp_pool[i].next = &wp_pool[i + 1];
	}
	wp_pool[NR_WP - 1].next = NULL;

	head = NULL;
	free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

/* Allocate a new watchpoint from free_ list and insert it into head list */
WP* new_wp() {
    /* If no free watchpoint, abort as instructed */
    if (free_ == NULL) {
        assert(0);
        return NULL;
    }

    WP *wp = free_;
    /* pop from free_ */
    free_ = free_->next;

    /* insert at the front of head (used list) */
    wp->next = head;
    head = wp;

    return wp;
}

/* Free a watchpoint: remove it from head list and push it back to free_ list */
void free_wp(WP *wp) {
    if (wp == NULL) return;

    /* remove wp from head list */
    if (head == wp) {
        head = head->next;
    } else {
        WP *p = head;
        while (p != NULL && p->next != wp) {
            p = p->next;
        }
        if (p != NULL) {
            p->next = wp->next;
        } else {
            /* wp not found in used list: ignore silently */
        }
    }

    /* push wp back to free_ list */
    wp->next = free_;
    free_ = wp;
}
