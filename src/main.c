/**
 * @file main.c
 * @brief Forth Jr. word parser.
 * @author Shrikant Giridhar
 * @version 0.01
 */
#include <err.h>
#include <unistd.h>
#include <getopt.h>
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

enum binop_t {
	AND, OR, LSHIFT, RSHIFT,		/* Bitwise */
	ADD, SUBTRACT, MULTIPLY, DIVIDE, MOD	/* Arithmetic */
};

/* Program options. */
struct popts {
	bool verbose;
	char *filename;
};

/* Forth execution stack */
struct pstack {
	int idx;
	int stack[MAX_STACK_SIZE];
};

struct pstack pstack;

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
	{ { 0 }, 0, 0 },
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
	if (!ps || (ps->idx == sizeof(ps->stack)))
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
		result = op2 + op1;
		break;
	case DIVIDE:
		result = op2 / op1;
		break;
	case MULTIPLY:
		result = op2 * op1;
		break;
	case SUBTRACT:
		result = op2 - op1;
		break;
	case AND:
		result = op2 & op1;
		break;
	case OR:
		result = op2 | op1;
		break;
	case RSHIFT:
		result = op2 >> op1;
		break;
	case LSHIFT:
		result = op2 << op1;
		break;
	case MOD:
		result = op2 % op1;
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

	printf("%d ", pop(ps));
	fflush(stdout);
	return (NULL);
}

void*
show_stack(void *stack, int flags)
{
	(void) flags;
	struct pstack *ps = stack;

	printf("<%d> ", ps->idx+1);

	for (int i = 0; i <= ps->idx; ++i) {
		printf("%d ", ps->stack[i]);	
	}

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

bool
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
	const char COMMENT[] = "\\";
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
			linebuf[i++] = (char) sym;
			linebuf[i] = '\0';
			return (i);
		} else {
			linebuf[i++] = (char) sym;
		}
	}

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
	size_t i, j;

	/* Include terminating NULL in traversal. */
	for (i = j = 0; i < llen && j < wlen; ++i) {
		if (!isspace(line[i])) {
			word[j++] = line[i];
		} else {
			/* Skip leading and consecutive spaces. */
			if ((i > 0 && isspace(line[i-1])) || i == 0)
				continue;

			word[i] = '\0';		/* Word found. */
			return (i+1);
		}
	}

	return (0);
}

#define PROMPT_OK (0)
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
prompt(int err, size_t line, size_t col)
{
	char errmsg[MAX_LINE_SIZE] = {0};

	switch (err) {
		case ENUMTOOBIG:
			snprintf(errmsg, sizeof(errmsg), "Number too big");
			break;
		case ENOTANUM:
			snprintf(errmsg, sizeof(errmsg), "Not a number");
			break;
		case PROMPT_OK:
			break;
		default:
#if (DEBUG_MODE)
			errx(1, "Invalid error code: %d.\n", err);
#endif
			break;
	}

	if (err)
		warnx("Error! %s (line %zu, word %zu).", errmsg, line, col);
}

void
eval(FILE *infd)
{
	size_t llen;
	char line[MAX_STACK_SIZE] = {0};
	pstack.idx = -1;
	size_t lineno = 1;
	size_t col, wlen;

	/*
	 * Look at each word in a given line (except line length
	 * comments). Forth requires that a word which is not a
	 * keyword (callable) be a number.
	 */
	while ((llen = next_line(infd, line, sizeof(line)))) {
		char *words = &line[0];
		char word[MAX_WORD_SIZE] = {0};
		col = wlen = 0;

		/* Execute each word in line. */
		while ((wlen = next_word(words, llen, word, sizeof(word)))) {
			if (!is_keyword(word)) {
				/* Skip rest of the line. */
				if (is_comment(word))
					break;

				prompt(insert(word), lineno, col);
			}

			col += wlen;
			words += wlen;
			llen -= wlen;
		}

		++lineno;
	}
}

void
usage(void)
{
	fprintf(stdout, "Forth Jr: Yet Another Toy Forth Interpreter\n"
			"Usage: forthjr [OPTIONS]\n"
			"\n"
			"With no options, read standard input.\n"
			"\n"
			" -v                    Be verbose.\n"
			" -f <FILE>             Read input from FILE.\n");
}

int
parse_opts(int argc, char *argv[], struct popts *options)
{
	int opt;

	while ((opt = getopt(argc, argv, "vf:")) != -1) {
		switch (opt) {
		case 'v':
			options->verbose = true;
			break;
		case 'f':
			options->filename = optarg;
			break;
		default:
			usage();
			exit(EXIT_FAILURE);
		}
	}

	/* No arguments supplied for options. */
	if (optind > argc)
		errx(1, "Expected argument after options\n");

	return (0);	
}

int
main(int argc, char *argv[])
{
	FILE *input;
	struct popts options;
	parse_opts(argc, argv, &options);

	if (options.filename != NULL) {
		input = fopen(options.filename, "r");
		if (!input)
			errx(1, "%s: Invalid input files\n", argv[0]);
	} else {
		input = stdin;
	}

	eval(input);

#if (DEBUG_MODE)
	printf("Finished processing.\n");
#endif

	return (EXIT_SUCCESS);
}
