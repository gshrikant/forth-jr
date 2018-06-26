#define _GNU_SOURCE
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <stdbool.h>
#include <inttypes.h>

#define MAX_WORD_SIZE 32UL
#define MAX_STACK_SIZE 1024UL

#define DEBUG_MODE 0

enum binop_t {
	AND, OR, LSHIFT, RSHIFT,		/* Bitwise */
	ADD, SUBTRACT, MULTIPLY, DIVIDE, MOD	/* Arithmetic */
};

/* Forth execution stack */
struct pstack {
	int idx;
	int stack[MAX_STACK_SIZE];
	ssize_t slen;
};

struct pstack pstack = { .slen = MAX_STACK_SIZE };

typedef void* (*wordfn_t)(void*, int);

struct dict {
	char word[MAX_WORD_SIZE];
	wordfn_t wordfn;
	int flags;
};

void* binop(void*, int);
void* dup_word(void*, int);
void* swap_word(void*, int);
void* drop_word(void*, int);
void* print_word(void*, int);
void* show_stack(void*, int);

/*
 * Default program keyword dictionary.
 */
struct dict pdict[] = {
	{ "+", binop, ADD },
	{ "-", binop, SUBTRACT },
	{ "*", binop, MULTIPLY },
	{ "/", binop, DIVIDE },
	{ ".", print_word, 0 },
	{ "and", binop, AND },
	{ "or", binop, OR },
	{ ">>", binop, RSHIFT },
	{ "<<", binop, LSHIFT },
	{ "mod", binop, MOD },
	{ "print", print_word, 0 },
	{ "dup", dup_word, 0 },
	{ "drop", drop_word, 0 },
	{ "swap", swap_word, 0 },
	{ ".s", show_stack, 0 },
	{ 0, 0 },
};

/* Stack utilities */
int
get(struct pstack *ps)
{
	return ps->stack[ps->idx];
}

int
put(struct pstack *ps, int word)
{
	ps->stack[ps->idx] = word;
	return (word);
}

int
push(struct pstack *ps, int word)
{
	if (!ps || (ps->idx == ps->slen))
		return (-1);

	ps->stack[++ps->idx] = word;
	return (word);
}

int
pop(struct pstack *ps)
{
	return ps->stack[ps->idx--];
}

void*
binop(void *stack, int flags)
{
	int op1, op2, result;

	struct pstack *ps = stack;
	op1 = pop(ps);
	op2 = get(ps);
	
	switch (flags) {
	case ADD:
		result = op1 + op2;
		break;
	case DIVIDE:
		result = op1 / op2;
		break;
	case MULTIPLY:
		result = op1 * op2;
		break;
	case SUBTRACT:
		result = op1 - op2;
		break;
	case AND:
		result = op1 & op2;
		break;
	case OR:
		result = op1 | op2;
		break;
	case RSHIFT:
		result = op1 >> op2;
		break;
	case LSHIFT:
		result = op1 << op2;
		break;
	case MOD:
		result = op1 % op2;
		break;
	default:
		/* Debug only. */
#if (DEBUG_MODE)
		err(1, "Invalid binary operation flag: %d\n", flags);
#else
		return (NULL);
#endif
		break;
	}

	put(ps, result);
	return (NULL);
}

void*
swap_word(void *stack, int flags)
{
	(void) flags;
	int a, b;
	struct pstack *ps = stack;

	a = pop(ps);
	b = pop(ps);
	push(ps, a);
	push(ps, b);	/* Make 'b' the new stack top. */

	return (NULL);
}

void*
dup_word(void *stack, int flags)
{
	(void) flags;
	struct pstack *ps = stack;

	push(ps, get(ps));
	return (NULL);
}

void*
print_word(void *stack, int flags)
{
	(void) flags;
	struct pstack *ps = stack;

	/* TODO: Handle all types */

	printf("\n>> %d\n", pop(ps));
	return (NULL);
}

void*
show_stack(void *stack, int flags)
{
	(void) flags;
	struct pstack *ps = stack;

	printf("<%d> ", ps->idx);

	for (int i = 0; i < ps->idx; ++i) {
		printf("%d ", ps->stack[ps->idx]);	
	}

	printf("\n");
	return (NULL);
}

void*
drop_word(void *stack, int flags)
{
	(void) flags;
	struct pstack *ps = stack;
	pop(ps);	/* Pop and ignore top. */
	return (NULL);
}

int
is_keyword(char *word)
{
	struct dict *record = &pdict[0];

	/* Map a word into dictionary and
	 * call the appropriate function */
	while (record->wordfn) {
		if (!strncmp(record->word, word, MAX_WORD_SIZE)) {
			record->wordfn(&pstack, record->flags);
			return (true);
		} 

		++record;
	}

	return (false);
}

size_t
next_word(FILE* infd, char *buf, size_t len)
{
	int sym;
	size_t i = 0;

	while (((sym = getc(infd)) != EOF) && i < len) {
		/* Word found. */
		if (isspace(sym)) {
			buf[i] = '\0';
			return (i);
		}

		buf[i++] = (char) sym;
	}

	return (0);
}

void
eval(void)
{
	pstack.idx = -1;
	char word[MAX_WORD_SIZE] = {0};

	while (next_word(stdin, word, sizeof(word))) {
		/* Value is number if not found in the dictionary. */
		if (!is_keyword(word)) {
			char *eptr;
			errno = 0;
			long int nword = strtol(word, &eptr, 10);
			if (eptr == word) {
				err(1, "Error: not a number.\n");
			} else if (errno == ERANGE) {
				if (nword == LONG_MAX || nword == LONG_MIN )
					err(1, "Error: Number too big.\n");
			}

			pstack.stack[++pstack.idx] = (int) nword;
		}
	}
}

int
main(int argc, char *argv[])
{
	eval();
	printf("Finished processing.\n");
	return (0);
}
