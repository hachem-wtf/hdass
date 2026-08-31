#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "diag/diag.h"

static bool colored(void)
{
	return getenv("NO_COLOR") == NULL;
}

void report_error(struct Source source, struct Token token, const char* message)
{
	const char* name = source.name != NULL ? source.name : "<source>";
	const char* text = source.text;

	const char* line_start = token.start;
	while (line_start > text && line_start[-1] != '\n')
		line_start -= 1;

	const char* line_end = token.start;
	while (*line_end != '\0' && *line_end != '\n')
		line_end += 1;

	uint32_t column = (uint32_t)(token.start - line_start) + 1;

	size_t span = token.length > 0 ? token.length : 1;
	if ((size_t)(line_end - token.start) < span)
		span = (size_t)(line_end - token.start);
	if (span == 0)
		span = 1;

	const char* bold  = colored() ? "\033[1m" : "";
	const char* red   = colored() ? "\033[31m" : "";
	const char* blue  = colored() ? "\033[34m" : "";
	const char* reset = colored() ? "\033[0m" : "";

	int gutter = snprintf(NULL, 0, "%u", token.line);

	fprintf(stderr, "%s%serror:%s %s%s%s\n", bold, red, reset, bold, message, reset);
	fprintf(stderr, "%*s %s-->%s %s:%u:%u\n", gutter, "", blue, reset, name, token.line, column);
	fprintf(stderr, "%*s %s|%s\n", gutter, "", blue, reset);
	fprintf(stderr, "%s%u%s %s|%s %.*s\n", blue, token.line, reset, blue, reset,
		(int)(line_end - line_start), line_start);
	fprintf(stderr, "%*s %s|%s ", gutter, "", blue, reset);

	for (const char* character = line_start; character < token.start; character += 1)
		fputc(*character == '\t' ? '\t' : ' ', stderr);

	fprintf(stderr, "%s", red);
	for (size_t i = 0; i < span; i += 1)
		fputc('^', stderr);
	fprintf(stderr, "%s\n", reset);
}

void report_error_message(const char* message)
{
	const char* bold  = colored() ? "\033[1m" : "";
	const char* red   = colored() ? "\033[31m" : "";
	const char* reset = colored() ? "\033[0m" : "";

	fprintf(stderr, "%s%serror:%s %s%s%s\n", bold, red, reset, bold, message, reset);
}
