#pragma once

#include <stddef.h>

#include "lexer/lexer.h"

struct ConstDecl
{
	struct Token name;
	struct Token value;
};

struct Program
{
	struct ConstDecl* consts;
	size_t const_count;
	size_t const_capacity;
};

struct Program create_program(void);
void free_program(struct Program* program);
void add_const(struct Program* program, struct ConstDecl decl);
