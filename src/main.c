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
#define MAX_LINE_SIZE 256UL
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
	{ 0, 0, 0 },
};

size_t
min(size_t x, size_t y)
{
	return x < y ? x : y;
}

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

	printf(">> %d\n", pop(ps));
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

int
is_comment(char *word)
{
	const char *const COMMENT = "\\";
	return !strncmp(word, COMMENT, sizeof(COMMENT));
}

/**
 * @brief Read a new line from input buffer.
 * @param infd Input file descriptor.
 * @param linebuf Line buffer.
 * @param len Length of line buffer.
 * @return Number of bytes read (including NULL terminator).
 */
size_t
next_line(FILE *infd, char *linebuf, size_t len)
{
	int sym;
	size_t i = 0;

	if (!linebuf || !len)
		return (0);

	while ((sym = getc(infd)) != EOF) {
		/* Until newline. */
		if (sym == '\n') {
			linebuf[i] = '\0';
			return (i);
		} else {
			linebuf[i++] = (char) sym;
		}
	}

	/* EOF */
	return (0);
}

/**
 * @brief Get next word from input stream.
 * @param line Line to read word from.
 * @param llen Length of line.
 * @param word Word buffer.
 * @param wlen Length of word buffer.
 * @return No. of characters read (including terminating NULL character).
 */
size_t
next_word(char *line, size_t llen, char *word, size_t wlen)
{
	/* Include terminating NULL in traversal. */
	for (size_t i = 0; i < min(wlen+1, llen+1); ++i) {
		if (isspace(line[i]) || line[i] == '\0') {
			word[i] = '\0';		/* Word found. */
			return (i+1);
		}

		word[i] = line[i];
	}

	return 0;
}

#define ENOTANUM (-1)
#define ENUMTOOBIG (-2)

int
insert(char *word)
{
	char *eptr;
	long int nword;

	/* Value is number if not found in the dictionary. */
	errno = 0;
	nword = strtol(word, &eptr, 10);

	/* Conversion failed. */
	if (eptr == word)
		return (ENOTANUM);

	/* Overflow/undeflow. */
	if (errno == ERANGE && (nword == LONG_MAX || nword == LONG_MIN))
		return (ENUMTOOBIG);

	push(&pstack, (int) nword);

	return (0);
}

void
eval(FILE *infd)
{
	size_t llen;
	char line[MAX_STACK_SIZE] = {0};
	pstack.idx = -1;
	size_t lineno = 0;
	size_t col, wlen;

	while ((llen = next_line(infd, line, sizeof(line)))) {
		char *words = &line[0];
		char word[MAX_WORD_SIZE] = {0};
		col = wlen = 0;

		while ((wlen = next_word(words, llen, word, sizeof(word)))) {
			col += wlen;

			if (!is_keyword(word)) {
				/* Skip rest of the line. */
				if (is_comment(word))
					break;

				/* TODO: Handle errors. */
				insert(word);	/* Push a number. */
			}

			words += wlen;
			llen -= wlen;
		}

		++lineno;
	}
}

int
main(int argc, char *argv[])
{
	eval(stdin);
	printf("Finished processing.\n");

	return (EXIT_SUCCESS);
}
