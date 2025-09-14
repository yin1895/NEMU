#include "monitor/watchpoint.h"
#include "monitor/expr.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

/* cpu.eip 用于在触发时打印地址 */
#include "cpu/reg.h"

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

/* 创建监视点，返回监视点编号（>=0）或 -1 表示失败 */
int wp_create(char *e) {
    if (e == NULL) return -1;

    WP *wp = new_wp();
    if (wp == NULL) return -1;

    strncpy(wp->expr, e, sizeof(wp->expr) - 1);
    wp->expr[sizeof(wp->expr) - 1] = '\0';

    bool success = false;
    uint32_t v = expr(wp->expr, &success);
    if (!success) {
        /* 表达式解析失败，释放监视点并返回失败 */
        free_wp(wp);
        return -1;
    }
    wp->val = v;
    return wp->NO;
}

/* 删除指定编号的监视点 */
void wp_delete(int no) {
    if (head == NULL) return;

    WP *p = head, *prev = NULL;
    while (p != NULL) {
        if (p->NO == no) {
            if (prev == NULL) head = p->next;
            else prev->next = p->next;
            /* 将 p 放回空闲池 */
            p->next = free_;
            p->expr[0] = '\0';
            p->val = 0;
            free_ = p;
            return;
        }
        prev = p;
        p = p->next;
    }
    /* 如果没找到，不做任何事 */
}

/* 打印当前使用中的监视点信息（简洁版，可按需扩展） */
void wp_info() {
    printf("Num\tExpr\t\t\tValue\n");
    WP *p = head;
    while (p) {
        printf("%d\t%s\t\t0x%08x\n", p->NO, p->expr, p->val);
        p = p->next;
    }
}

/* 检查所有监视点；若有任一触发则打印 Hint 并返回 1（并更新该监视点的值） */
int wp_check() {
    WP *p = head;
    while (p) {
        bool success = false;
        uint32_t newv = expr(p->expr, &success);
        if (success && newv != p->val) {
            /* 按要求输出 hint，包括监视点编号和触发指令的 eip（十六进制） */
            printf("Hint watchpoint %d at address 0x%08x\n", p->NO, cpu.eip);
            /* 更新保存的值 */
            p->val = newv;
            return 1;
        }
        p = p->next;
    }
    return 0;
}