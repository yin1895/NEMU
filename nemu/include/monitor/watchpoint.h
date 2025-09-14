#ifndef __WATCHPOINT_H__
#define __WATCHPOINT_H__

#include "common.h"

typedef struct watchpoint {
	int NO;
	struct watchpoint *next;

	/* TODO: Add more members if necessary */
	/* 新增用于监视点的成员 */
    char expr[128];   /* 存放用户输入的表达式（以 NEMU 的 expr() 为准） */
    uint32_t val;     /* 最近一次求值的值 */

} WP;

/* 池接口 */
WP* new_wp();
void free_wp(WP *wp);

/* 监视点相关接口，供命令处理和 cpu_exec 调用 */
int wp_create(char *e);    /* 新建监视点，返回监视点编号或 -1 */
void wp_delete(int no);    /* 删除监视点编号 no */
void wp_info();            /* 打印当前所有监视点信息 */
int wp_check();            /* 检查所有监视点，若有触发返回 1（并输出 hint），否则返回 0 */


#endif
