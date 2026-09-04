#include <stdlib.h>

#include "parser/ast.h"

void free_expr(struct Expr* expr)
{
	if (expr == NULL)
		return;

	switch (expr->kind)
	{
		case EXPR_PRIMARY:
			break;
		case EXPR_BINARY:
			free_expr(expr->binary.left);
			free_expr(expr->binary.right);
			break;
		case EXPR_MEMBER:
			free_expr(expr->member.object);
			break;
		case EXPR_DEREF:
			free_expr(expr->deref.address);
			break;
	}

	free(expr);
}

void free_statement(struct Statement* statement)
{
	switch (statement->kind)
	{
		case STATEMENT_ASSIGN:
			free_expr(statement->assign.value);
			break;
		case STATEMENT_IF:
			free_expr(statement->branch.left);
			free_expr(statement->branch.right);
			for (size_t i = 0; i < statement->branch.body_count; i += 1)
				free_statement(&statement->branch.body[i]);
			free(statement->branch.body);
			for (size_t i = 0; i < statement->branch.else_count; i += 1)
				free_statement(&statement->branch.else_body[i]);
			free(statement->branch.else_body);
			break;
		case STATEMENT_CALL:
			for (size_t i = 0; i < statement->call.arg_count; i += 1)
				free_expr(statement->call.args[i]);
			free(statement->call.args);
			break;
		case STATEMENT_STACK:
			free_expr(statement->stack.size);
			break;
		default:
			break;
	}
}

void free_proc(struct ProcDecl* proc)
{
	free(proc->params);

	for (size_t i = 0; i < proc->body_count; i += 1)
		free_statement(&proc->body[i]);
	free(proc->body);
}

struct Program create_program(void)
{
	struct Program program;
	program.config.bits = 64;
	program.config.has_entry = false;
	program.config.logical_registers = false;
	program.consts = NULL;
	program.const_count = 0;
	program.const_capacity = 0;
	program.data_decls = NULL;
	program.data_count = 0;
	program.data_capacity = 0;
	program.enums = NULL;
	program.enum_count = 0;
	program.enum_capacity = 0;
	program.structs = NULL;
	program.struct_count = 0;
	program.struct_capacity = 0;
	program.procs = NULL;
	program.proc_count = 0;
	program.proc_capacity = 0;
	return program;
}

void free_program(struct Program* program)
{
	for (size_t i = 0; i < program->const_count; i += 1)
		free_expr(program->consts[i].value);
	free(program->consts);
	free(program->data_decls);

	for (size_t i = 0; i < program->enum_count; i += 1)
		free(program->enums[i].members);
	free(program->enums);

	for (size_t i = 0; i < program->struct_count; i += 1)
		free(program->structs[i].fields);
	free(program->structs);

	for (size_t i = 0; i < program->proc_count; i += 1)
		free_proc(&program->procs[i]);
	free(program->procs);

	program->consts = NULL;
	program->const_count = 0;
	program->const_capacity = 0;
	program->data_decls = NULL;
	program->data_count = 0;
	program->data_capacity = 0;
	program->enums = NULL;
	program->enum_count = 0;
	program->enum_capacity = 0;
	program->structs = NULL;
	program->struct_count = 0;
	program->struct_capacity = 0;
	program->procs = NULL;
	program->proc_count = 0;
	program->proc_capacity = 0;
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

struct EnumDecl create_enum(void)
{
	struct EnumDecl decl = { 0 };
	return decl;
}

void add_enum_member(struct EnumDecl* decl, struct Token member)
{
	if (decl->member_count == decl->member_capacity)
	{
		size_t capacity = decl->member_capacity < 8 ? 8 : decl->member_capacity * 2;
		decl->members = realloc(decl->members, capacity * sizeof(struct Token));
		decl->member_capacity = capacity;
	}

	decl->members[decl->member_count] = member;
	decl->member_count += 1;
}

void add_enum(struct Program* program, struct EnumDecl decl)
{
	if (program->enum_count == program->enum_capacity)
	{
		size_t capacity = program->enum_capacity < 8 ? 8 : program->enum_capacity * 2;
		program->enums = realloc(program->enums, capacity * sizeof(struct EnumDecl));
		program->enum_capacity = capacity;
	}

	program->enums[program->enum_count] = decl;
	program->enum_count += 1;
}

struct StructDecl create_struct(void)
{
	struct StructDecl decl = { 0 };
	return decl;
}

void add_struct_field(struct StructDecl* decl, struct StructField field)
{
	if (decl->field_count == decl->field_capacity)
	{
		size_t capacity = decl->field_capacity < 8 ? 8 : decl->field_capacity * 2;
		decl->fields = realloc(decl->fields, capacity * sizeof(struct StructField));
		decl->field_capacity = capacity;
	}

	decl->fields[decl->field_count] = field;
	decl->field_count += 1;
}

void add_struct(struct Program* program, struct StructDecl decl)
{
	if (program->struct_count == program->struct_capacity)
	{
		size_t capacity = program->struct_capacity < 8 ? 8 : program->struct_capacity * 2;
		program->structs = realloc(program->structs, capacity * sizeof(struct StructDecl));
		program->struct_capacity = capacity;
	}

	program->structs[program->struct_count] = decl;
	program->struct_count += 1;
}

struct ProcDecl create_proc(void)
{
	struct ProcDecl proc = { 0 };
	return proc;
}

void add_param(struct ProcDecl* proc, struct Param param)
{
	if (proc->param_count == proc->param_capacity)
	{
		size_t capacity = proc->param_capacity < 4 ? 4 : proc->param_capacity * 2;
		proc->params = realloc(proc->params, capacity * sizeof(struct Param));
		proc->param_capacity = capacity;
	}

	proc->params[proc->param_count] = param;
	proc->param_count += 1;
}

void add_statement(struct ProcDecl* proc, struct Statement statement)
{
	if (proc->body_count == proc->body_capacity)
	{
		size_t capacity = proc->body_capacity < 8 ? 8 : proc->body_capacity * 2;
		proc->body = realloc(proc->body, capacity * sizeof(struct Statement));
		proc->body_capacity = capacity;
	}

	proc->body[proc->body_count] = statement;
	proc->body_count += 1;
}

void add_proc(struct Program* program, struct ProcDecl decl)
{
	if (program->proc_count == program->proc_capacity)
	{
		size_t capacity = program->proc_capacity < 8 ? 8 : program->proc_capacity * 2;
		program->procs = realloc(program->procs, capacity * sizeof(struct ProcDecl));
		program->proc_capacity = capacity;
	}

	program->procs[program->proc_count] = decl;
	program->proc_count += 1;
}
