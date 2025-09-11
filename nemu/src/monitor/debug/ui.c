#include "monitor/monitor.h"
#include "monitor/expr.h"
#include "monitor/watchpoint.h"
#include "nemu.h"

#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>

void cpu_exec(uint32_t);

/* We use the `readline' library to provide more flexibility to read from stdin. */
char* rl_gets() {
	static char *line_read = NULL;

	if (line_read) {
		free(line_read);
		line_read = NULL;
	}

	line_read = readline("(nemu) ");

	if (line_read && *line_read) {
		add_history(line_read);
	}

	return line_read;
}

static int cmd_c(char *args) {
	cpu_exec(-1);
	return 0;
}

static int cmd_q(char *args) {
	return -1;
}

static int cmd_help(char *args);

static int cmd_si(char *args);

static int cmd_info(char *args);
static int cmd_x(char *args);
static int cmd_p(char *args);
static struct {
    char *name;
    char *description;
    int (*handler) (char *);
} cmd_table [] = {
    { "help", "Display informations about all supported commands", cmd_help },
    { "c", "Continue the execution of the program", cmd_c },
    { "q", "Exit NEMU", cmd_q },
    { "si", "Excute N instructions one by one and then halt.", cmd_si },
    { "info", "display the register status", cmd_info },
    { "x", "Find the value of the expression ExpR and use the result as the starting memory address to output n consecutive four bytes in hexadecimal format", cmd_x },
    { "p", "Evaluate an expression and print its value", cmd_p },
    
    /* TODO: Add more commands */
};

#define NR_CMD (sizeof(cmd_table) / sizeof(cmd_table[0]))

static int cmd_help(char *args) {
	/* extract the first argument */
	char *arg = strtok(NULL, " ");
	int i;

	if(arg == NULL) {
		/* no argument given */
		for(i = 0; i < NR_CMD; i ++) {
			printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
		}
	}
	else {
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(arg, cmd_table[i].name) == 0) {
				printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
				return 0;
			}
		}
		printf("Unknown command '%s'\n", arg);
	}
	return 0;
}

static int cmd_si(char *args){
    /* 单步执行命令：si [N]
     * - 无参数时默认执行 1 步
     * - 有参数时解析为无符号整数，非法输入给出提示并不执行
     */
    uint32_t N = 1;           
    char *endp = NULL;

    if (args != NULL) {
        /* 跳过前导空格，避免空格影响解析 */
        while (*args == ' ') args++;
        if (*args != '\0') {
            unsigned long tmp = strtoul(args, &endp, 10);
            /* endp 指向第一个非数字字符，如果不是字符串末尾且不是纯空格，说明有多余输入 */
            while (endp && *endp == ' ') endp++;
            if (endp == NULL || *endp != '\0') {
                printf("Error: si 的参数应为一个非负整数，例如: si 10\n");
                return 0;
            }
            N = (uint32_t)tmp;
            if (N == 0) N = 1;  /* 执行 0 步没有意义，按 1 步处理 */
        }
    }

    cpu_exec(N);
    return 0;
}

static int cmd_info(char *args){
    /* info 子命令：
     * - info r: 打印通用寄存器和 eip
     * 其余子命令未实现时给出提示
     */
    if (args == NULL) {
        printf("Usage: info r\n");
        return 0;
    }

    /* 跳过前导空格 */
    while (*args == ' ') args++;

    if (args[0] == 'r' && (args[1] == '\0' || args[1] == ' ')) {
        int i;
        for(i = R_EAX; i <= R_EDI; i++){
            /* 显示寄存器名、十六进制与无符号十进制的值 */
            printf("$%s\t0x%08x\t%u\n", regsl[i], (unsigned int)reg_l(i), (unsigned int)reg_l(i));
        }
        printf("$eip\t0x%08x\t%u\n", (unsigned int)cpu.eip, (unsigned int)cpu.eip);
    } else {
        printf("Unsupported subcommand. Try: info r\n");
    }
    return 0;
}

static int cmd_x(char *args){
    /* 内存查看命令：x N EXPR
     * - N 为需要输出的双字(4 字节)数量，十进制
     * - EXPR 为表达式，可能包含空格，通过 expr() 计算起始地址
     * - 每行打印 4 个双字，从起始地址开始按 4 字节递增
     */
    if (args == NULL) {
        printf("Usage: x N EXPR\n");
        return 0;
    }

    /* 解析第一个 token 作为 N，其余作为表达式（允许空格） */
    while (*args == ' ') args++;
    if (*args == '\0') {
        printf("Usage: x N EXPR\n");
        return 0;
    }

    /* 定位第一个空格，用于切分 N 与 EXPR */
    char *space = strchr(args, ' ');
    if (space == NULL) {
        printf("Error: 缺少表达式参数。示例: x 10 $esp\n");
        return 0;
    }

    /* 解析 N */
    *space = '\0';                 /* 暂时把空格改为字符串结束符，便于解析 N */
    char *n_str = args;
    char *expr_str = space + 1;    /* N 后面的所有内容作为表达式 */
    while (*expr_str == ' ') expr_str++;

    if (*expr_str == '\0') {
        printf("Error: 表达式为空。示例: x 4 0x1000 或 x 8 $esp+0x10\n");
        *space = ' ';              /* 复原被改写的空格 */
        return 0;
    }

    char *endp = NULL;
    unsigned long n_ul = strtoul(n_str, &endp, 10);
    while (endp && *endp == ' ') endp++;
    if (endp == NULL || *endp != '\0') {
        printf("Error: N 应为十进制非负整数。示例: x 16 $eip\n");
        *space = ' ';              /* 复原空格 */
        return 0;
    }
    if (n_ul == 0) {
        printf("Warning: N 为 0，无输出。\n");
        *space = ' ';              /* 复原空格 */
        return 0;
    }
    uint32_t n = (uint32_t)n_ul;

    /* 解析表达式得到起始地址 */
    bool success = true;
    swaddr_t addr = (swaddr_t)expr(expr_str, &success);
    *space = ' ';                  /* 复原空格（虽然后续不再使用 args，这样更稳妥） */
    if (!success) {
        printf("Error: 表达式解析失败: %s\n", expr_str);
        return 0;
    }

    /* 从 addr 开始，连续读取 n 个双字，每行打印 4 个 */
    int i; /* C89 要求在块首声明变量 */
    for (i = 0; i < (int)n; i++) {
        uint32_t val = swaddr_read(addr, 4);
        if (i % 4 == 0) {
            printf("0x%08x: 0x%08x", (unsigned int)addr, (unsigned int)val);
        } else if ((i + 1) % 4 == 0) {
            printf(" 0x%08x\n", (unsigned int)val);
        } else {
            printf(" 0x%08x", (unsigned int)val);
        }
        addr += 4;
    }
    if (n % 4 != 0) printf("\n");
    return 0;
}

static int cmd_p(char *args){
    /* 表达式求值命令：p EXPR */
    if (args == NULL) {
        printf("Usage: p EXPR\n");
        return 0;
    }
    while (*args == ' ') args++;
    if (*args == '\0') {
        printf("Usage: p EXPR\n");
        return 0;
    }

    bool success = true;
    uint32_t val = expr(args, &success);
    if (!success) {
        printf("Error: 表达式解析失败: %s\n", args);
        return 0;
    }
    printf("%u (0x%08x)\n", (unsigned int)val, (unsigned int)val);
    return 0;
}

void ui_mainloop() {
	while(1) {
		char *str = rl_gets();
		char *str_end = str + strlen(str);

		/* extract the first token as the command */
		char *cmd = strtok(str, " ");
		if(cmd == NULL) { continue; }

		/* treat the remaining string as the arguments,
		 * which may need further parsing
		 */
		char *args = cmd + strlen(cmd) + 1;
		if(args >= str_end) {
			args = NULL;
		}

#ifdef HAS_DEVICE
		extern void sdl_clear_event_queue(void);
		sdl_clear_event_queue();
#endif

		int i;
		for(i = 0; i < NR_CMD; i ++) {
			if(strcmp(cmd, cmd_table[i].name) == 0) {
				if(cmd_table[i].handler(args) < 0) { return; }
				break;
			}
		}

		if(i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
	}
}
