#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sema/sema.h"

static bool names_equal(struct Token a, struct Token b)
{
	return a.length == b.length && memcmp(a.start, b.start, a.length) == 0;
}

static bool check_duplicate_names(struct Program* program)
{
	size_t count = program->const_count + program->data_count + program->proc_count;
	if (count == 0)
		return true;

	struct Token* names = malloc(count * sizeof(struct Token));
	size_t n = 0;
	for (size_t i = 0; i < program->const_count; i += 1)
	{
		names[n] = program->consts[i].name;
		n += 1;
	}
	for (size_t i = 0; i < program->data_count; i += 1)
	{
		names[n] = program->data_decls[i].name;
		n += 1;
	}
	for (size_t i = 0; i < program->proc_count; i += 1)
	{
		names[n] = program->procs[i].name;
		n += 1;
	}

	bool ok = true;
	for (size_t i = 0; i < count; i += 1)
		for (size_t j = 0; j < i; j += 1)
			if (names_equal(names[i], names[j]))
			{
				fprintf(stderr, "error: line %u: '%.*s' is already defined\n",
					names[i].line, (int)names[i].length, names[i].start);
				ok = false;
			}

	free(names);
	return ok;
}

static bool check_entry_point(struct Program* program)
{
	if (!program->config.has_entry)
		return true;

	struct Token entry = program->config.entry;
	for (size_t i = 0; i < program->proc_count; i += 1)
		if (names_equal(program->procs[i].name, entry))
			return true;

	fprintf(stderr, "error: line %u: entry point '%.*s' is not defined\n",
		entry.line, (int)entry.length, entry.start);
	return false;
}

bool analyze_program(struct Program* program)
{
	bool ok = true;

	if (!check_duplicate_names(program))
		ok = false;
	if (!check_entry_point(program))
		ok = false;

	return ok;
}
