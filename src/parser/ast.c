#include <stdlib.h>

#include "parser/ast.h"

struct Program create_program(void)
{
	struct Program program;
	program.consts = NULL;
	program.const_count = 0;
	program.const_capacity = 0;
	return program;
}

void free_program(struct Program* program)
{
	free(program->consts);
	program->consts = NULL;
	program->const_count = 0;
	program->const_capacity = 0;
}

void add_const(struct Program* program, struct ConstDecl decl)
{
	if (program->const_count == program->const_capacity)
	{
		size_t capacity = program->const_capacity < 8 ? 8 : program->const_capacity * 2;
		program->consts = realloc(program->consts, capacity * sizeof(struct ConstDecl));
		program->const_capacity = capacity;
	}

	program->consts[program->const_count] = decl;
	program->const_count += 1;
}
