#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include "codegen/nasm.h"

static void emit_consts(struct Program* program, FILE* out)
{
	for (size_t i = 0; i < program->const_count; i += 1)
	{
		struct ConstDecl decl = program->consts[i];
		fprintf(out, "%.*s equ %.*s\n",
			(int)decl.name.length, decl.name.start,
			(int)decl.value.length, decl.value.start);
	}
}

static void emit_data(struct Program* program, FILE* out)
{
	fprintf(out, "section .data\n");

	for (size_t i = 0; i < program->data_count; i += 1)
	{
		struct DataDecl decl = program->data_decls[i];

		// the value lexeme keeps its surrounding double quotes; NASM backtick
		// strings interpret the same escapes, so re-wrap the inner content
		fprintf(out, "%.*s: db `%.*s`\n",
			(int)decl.name.length, decl.name.start,
			(int)(decl.value.length - 2), decl.value.start + 1);
		fprintf(out, ".len equ $ - %.*s\n",
			(int)decl.name.length, decl.name.start);
	}
}

static const char* assign_mnemonic(enum TokenType op)
{
	switch (op)
	{
		case TOKEN_EQUAL:      return "mov";
		case TOKEN_PLUS_EQUAL:  return "add";
		case TOKEN_MINUS_EQUAL: return "sub";
		case TOKEN_STAR_EQUAL:  return "imul";
		default:               return NULL;
	}
}

static bool emit_operand(struct Expr* expr, FILE* out)
{
	switch (expr->kind)
	{
		case EXPR_PRIMARY:
			fprintf(out, "%.*s", (int)expr->primary.token.length, expr->primary.token.start);
			return true;
		case EXPR_MEMBER:
			if (!emit_operand(expr->member.object, out))
				return false;
			fprintf(out, ".%.*s", (int)expr->member.member.length, expr->member.member.start);
			return true;
		case EXPR_BINARY:
			return false;
	}

	return false;
}

static void emit_assign(struct AssignStatement* assign, FILE* out)
{
	const char* mnemonic = assign_mnemonic(assign->op.type);
	if (mnemonic == NULL || assign->value->kind == EXPR_BINARY)
	{
		fprintf(out, "\t; TODO: unsupported assignment\n");
		return;
	}

	if (assign->target_deref)
		fprintf(out, "\t%s [%.*s], ", mnemonic, (int)assign->target.length, assign->target.start);
	else
		fprintf(out, "\t%s %.*s, ", mnemonic, (int)assign->target.length, assign->target.start);

	emit_operand(assign->value, out);
	fprintf(out, "\n");
}

static void emit_statement(struct Statement* statement, FILE* out)
{
	switch (statement->kind)
	{
		case STATEMENT_ASSIGN:
			emit_assign(&statement->assign, out);
			break;
		case STATEMENT_LABEL:
			fprintf(out, "%.*s:\n", (int)statement->label.name.length, statement->label.name.start);
			break;
		case STATEMENT_GOTO:
			fprintf(out, "\tjmp %.*s\n", (int)statement->jump.label.length, statement->jump.label.start);
			break;
		case STATEMENT_SYSCALL:
			fprintf(out, "\tsyscall\n");
			break;
		default:
			fprintf(out, "\t; TODO: unsupported statement\n");
			break;
	}
}

static void emit_proc(struct ProcDecl* proc, FILE* out)
{
	bool is_entry = proc->name.length == 4 && memcmp(proc->name.start, "main", 4) == 0;
	if (is_entry)
		fprintf(out, "_start:\n");
	else
		fprintf(out, "%.*s:\n", (int)proc->name.length, proc->name.start);

	for (size_t i = 0; i < proc->body_count; i += 1)
		emit_statement(&proc->body[i], out);
}

void generate_nasm(struct Program* program, FILE* out)
{
	if (program->const_count > 0)
	{
		emit_consts(program, out);
		fprintf(out, "\n");
	}

	emit_data(program, out);
	fprintf(out, "\n");

	fprintf(out, "section .text\n");
	fprintf(out, "global _start\n");

	for (size_t i = 0; i < program->proc_count; i += 1)
	{
		fprintf(out, "\n");
		emit_proc(&program->procs[i], out);
	}
}
