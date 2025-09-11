#include "nemu.h"

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <sys/types.h>
#include <regex.h>
#include <string.h>

enum {
	NOTYPE = 256, EQ, NUM, HEX, REG,
	DEREF, NEG

	/* TODO: Add more token types */

};

static struct rule {
	char *regex;
	int token_type;
} rules[] = {

	/* TODO: Add more rules.
	 * Pay attention to the precedence level of different rules.
	 */

	{" +",	NOTYPE},				// spaces
	{"\\+", '+'},					// plus
	{"==", EQ},						// equal
	{"0[xX][0-9a-fA-F]+", HEX},     // hex number, must be before decimal
	{"[0-9]+", NUM},               // decimal number
	{"\\$[a-zA-Z_][a-zA-Z0-9_]*", REG}, // register like $eax
	{"\\(", '('},                // left parenthesis
	{"\\)", ')'},                // right parenthesis
	{"\\*", '*'},                // multiply or deref (contextual)
	{"/", '/'},                    // divide
	{"\\-", '-'} 					// minus or neg (contextual)
};

#define NR_REGEX (sizeof(rules) / sizeof(rules[0]) )

static regex_t re[NR_REGEX];

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
	int i;
	char error_msg[128];
	int ret;

	for(i = 0; i < NR_REGEX; i ++) {
		ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
		if(ret != 0) {
			regerror(ret, &re[i], error_msg, 128);
			Assert(ret == 0, "regex compilation failed: %s\n%s", error_msg, rules[i].regex);
		}
	}
}

typedef struct token {
	int type;
	char str[32];
} Token;

Token tokens[32];
int nr_token;

static bool make_token(char *e) {
	int position = 0;
	int i;
	regmatch_t pmatch;
	
	nr_token = 0;

	while(e[position] != '\0') {
		/* Try all rules one by one. */
		for(i = 0; i < NR_REGEX; i ++) {
			if(regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
				char *substr_start = e + position;
				int substr_len = pmatch.rm_eo;

				Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i, rules[i].regex, position, substr_len, substr_len, substr_start);
				position += substr_len;

				/* TODO: Now a new token is recognized with rules[i]. Add codes
				 * to record the token in the array `tokens'. For certain types
				 * of tokens, some extra actions should be performed.
				 */

				switch(rules[i].token_type) {
					case NOTYPE:
						/* skip spaces */
						break;
					case NUM:
					case HEX:
					case REG: {
						if(nr_token >= (int)(sizeof(tokens)/sizeof(tokens[0]))) {
							panic("Too many tokens\n");
						}
						int copy_len = substr_len;
						if(copy_len >= (int)sizeof(tokens[nr_token].str))
							copy_len = sizeof(tokens[nr_token].str) - 1;
						tokens[nr_token].type = rules[i].token_type;
						memcpy(tokens[nr_token].str, substr_start, copy_len);
						tokens[nr_token].str[copy_len] = '\0';
						nr_token ++;
						break;
					}
					default: {
						if(nr_token >= (int)(sizeof(tokens)/sizeof(tokens[0]))) {
							panic("Too many tokens\n");
						}
						tokens[nr_token].type = rules[i].token_type;
						tokens[nr_token].str[0] = '\0';
						nr_token ++;
						break;
					}
				}

				break;
			}
		}

		if(i == NR_REGEX) {
			printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
			return false;
		}
	}

	return true; 
}

uint32_t expr(char *e, bool *success) {
	if(!make_token(e)) {
		*success = false;
		return 0;
	}

	/* TODO: Insert codes to evaluate the expression. */
	/* 将一元 * 和 - 重新标注为 DEREF 与 NEG */
	int i;
	for (i = 0; i < nr_token; i++) {
		if (tokens[i].type == '*') {
			if (i == 0 || (tokens[i-1].type != NUM && tokens[i-1].type != HEX && tokens[i-1].type != REG && tokens[i-1].type != ')')) {
				tokens[i].type = DEREF;
			}
		} else if (tokens[i].type == '-') {
			if (i == 0 || (tokens[i-1].type != NUM && tokens[i-1].type != HEX && tokens[i-1].type != REG && tokens[i-1].type != ')')) {
				tokens[i].type = NEG;
			}
		}
	}

	/* 声明并调用递归求值 */
	uint32_t eval(int l, int r, bool *ok);
	bool ok = true;
	uint32_t val = eval(0, nr_token - 1, &ok);
	*success = ok;
	return ok ? val : 0;
}

/* ================= 内部辅助函数（求值） ================ */
static bool check_parentheses(int l, int r, bool *ok) {
	if (l > r) { *ok = false; return false; }
	if (tokens[l].type != '(' || tokens[r].type != ')') return false;
	int bal = 0;
	int i;
	for (i = l; i <= r; i++) {
		if (tokens[i].type == '(') bal++;
		else if (tokens[i].type == ')') {
			bal--;
			if (bal < 0) { *ok = false; return false; }
		}
		if (i < r && bal == 0) return false; // 在 r 之前闭合，说明不是一对包住整个区间
	}
	if (bal != 0) { *ok = false; return false; }
	return true;
}

static int precedence(int t) {
	switch (t) {
		case EQ: return 1;          // 最低优先级
		case '+':
		case '-': return 2;
		case '*':
		case '/': return 3;
		case DEREF:
		case NEG: return 4;         // 一元最高
		default: return -1;
	}
}

static int find_dominant_op(int l, int r, bool *ok) {
	int pos = -1, min_pri = 100;
	int bal = 0;
	int i;
	for (i = l; i <= r; i++) {
		int t = tokens[i].type;
		if (t == '(') { bal++; continue; }
		if (t == ')') { bal--; if (bal < 0) { *ok = false; return -1; } continue; }
		if (bal != 0) continue; // 忽略括号内部

		int pri = precedence(t);
		if (pri < 0) continue;

		// 选择最低优先级，若同级则取最右（实现左结合）
		if (pri < min_pri || (pri == min_pri && i > pos)) {
			min_pri = pri;
			pos = i;
		}
	}
	return pos;
}

static uint32_t parse_number_token(const Token *tk, bool *ok) {
	if (tk->type == NUM) {
		uint32_t v = 0;
		const char *p;
		for (p = tk->str; *p; ++p) {
			if (*p < '0' || *p > '9') { *ok = false; return 0; }
			v = v * 10 + (uint32_t)(*p - '0');
		}
		return v;
	} else if (tk->type == HEX) {
		uint32_t v = 0;
		const char *p = tk->str;
		if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
		while (*p) {
			char c = *p;
			uint32_t d;
			if (c >= '0' && c <= '9') d = c - '0';
			else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
			else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
			else { *ok = false; return 0; }
			v = (v << 4) | d;
			p++;
		}
		return v;
	}
	*ok = false; return 0;
}

static uint32_t read_reg_by_name(const char *name, bool *ok) {
	// name 形如 "$eax"
	const char *p = name;
	if (*p == '$') p++;
	if (strcmp(p, "eip") == 0) return cpu.eip;
	int i;
	for (i = R_EAX; i <= R_EDI; i++) {
		if (strcmp(p, regsl[i]) == 0) {
			return reg_l(i);
		}
	}
	*ok = false; return 0;
}

static uint32_t mem_deref(uint32_t addr, bool *ok) {
	// NEMU 的监控台中常用 swaddr_read 读 4 字节
	return swaddr_read(addr, 4);
}

uint32_t eval(int l, int r, bool *ok) {
	if (!*ok) return 0;
	if (l > r) { *ok = false; return 0; }
	if (l == r) {
		if (tokens[l].type == NUM || tokens[l].type == HEX) return parse_number_token(&tokens[l], ok);
		if (tokens[l].type == REG) return read_reg_by_name(tokens[l].str, ok);
		*ok = false; return 0;
	}

	if (check_parentheses(l, r, ok)) {
		return eval(l + 1, r - 1, ok);
	}
	if (!*ok) return 0;

	int op = find_dominant_op(l, r, ok);
	if (op < 0 || !*ok) { *ok = false; return 0; }

	int t = tokens[op].type;
	if (t == NEG || t == DEREF) {
		uint32_t rhs = eval(op + 1, r, ok);
		if (!*ok) return 0;
		if (t == NEG) return (uint32_t)(-(int32_t)rhs);
		else return mem_deref(rhs, ok);
	}

	uint32_t lhs = eval(l, op - 1, ok);
	if (!*ok) return 0;
	uint32_t rhs = eval(op + 1, r, ok);
	if (!*ok) return 0;

	switch (t) {
		case '+': return lhs + rhs;
		case '-': return lhs - rhs;
		case '*': return lhs * rhs;
		case '/': if (rhs == 0) { *ok = false; return 0; } return lhs / rhs;
		case EQ: return (lhs == rhs);
		default: *ok = false; return 0;
	}
}

