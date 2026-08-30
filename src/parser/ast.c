#include <stdlib.h>

#include "parser/ast.h"

struct Program create_program(void)
{
	struct Program program;
	program.consts = NULL;
	program.const_count = 0;
	program.const_capacity = 0;
	program.data_decls = NULL;
	program.data_count = 0;
	program.data_capacity = 0;
	return program;
}

void free_program(struct Program* program)
{
	free(program->consts);
	free(program->data_decls);
	program->consts = NULL;
	program->const_count = 0;
	program->const_capacity = 0;
	program->data_decls = NULL;
	program->data_count = 0;
	program->data_capacity = 0;
}

// TODO: generalize this growable-array boilerplate once a third list appears
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

void add_data(struct Program* program, struct DataDecl decl)
{
	if (program->data_count == program->data_capacity)
	{
		size_t capacity = program->data_capacity < 8 ? 8 : program->data_capacity * 2;
		program->data_decls = realloc(program->data_decls, capacity * sizeof(struct DataDecl));
		program->data_capacity = capacity;
	}

	program->data_decls[program->data_count] = decl;
	program->data_count += 1;
}
