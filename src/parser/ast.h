#pragma once

#include <stddef.h>

#include "lexer/lexer.h"

struct ConstDecl
{
	struct Token name;
	struct Token value;
};

struct DataDecl
{
	struct Token name;
	struct Token value;
};

struct Param
{
	struct Token name;
	struct Token reg;
};

struct ProcDecl
{
	struct Token name;
	struct Param* params;
	size_t param_count;
	size_t param_capacity;
};

struct Program
{
	struct ConstDecl* consts;
	size_t const_count;
	size_t const_capacity;

	struct DataDecl* data_decls;
	size_t data_count;
	size_t data_capacity;

	struct ProcDecl* procs;
	size_t proc_count;
	size_t proc_capacity;
};

struct Program create_program(void);
void free_program(struct Program* program);
void add_const(struct Program* program, struct ConstDecl decl);
void add_data(struct Program* program, struct DataDecl decl);

struct ProcDecl create_proc(void);
void add_param(struct ProcDecl* proc, struct Param param);
void add_proc(struct Program* program, struct ProcDecl decl);
