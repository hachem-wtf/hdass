#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
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

static const char* jump_if_false(enum TokenType comparison)
{
	switch (comparison)
	{
		case TOKEN_EQUAL_EQUAL:   return "jne";
		case TOKEN_BANG_EQUAL:    return "je";
		case TOKEN_LESS:          return "jge";
		case TOKEN_LESS_EQUAL:    return "jg";
		case TOKEN_GREATER:       return "jle";
		case TOKEN_GREATER_EQUAL: return "jl";
		default:                  return NULL;
	}
}

static struct ProcDecl* find_proc(struct Program* program, struct Token name)
{
	for (size_t i = 0; i < program->proc_count; i += 1)
	{
		struct ProcDecl* proc = &program->procs[i];
		if (proc->name.length == name.length && memcmp(proc->name.start, name.start, name.length) == 0)
			return proc;
	}

	return NULL;
}

static void emit_call(struct CallStatement* call, struct Program* program, FILE* out)
{
	struct ProcDecl* callee = find_proc(program, call->name);
	if (callee == NULL || callee->param_count != call->arg_count)
	{
		fprintf(out, "\t; TODO: unsupported call\n");
		return;
	}

	for (size_t i = 0; i < call->arg_count; i += 1)
	{
		if (call->args[i]->kind == EXPR_BINARY)
		{
			fprintf(out, "\t; TODO: unsupported call argument\n");
			continue;
		}

		fprintf(out, "\tmov %.*s, ", (int)callee->params[i].reg.length, callee->params[i].reg.start);
		emit_operand(call->args[i], out);
		fprintf(out, "\n");
	}

	fprintf(out, "\tcall %.*s\n", (int)call->name.length, call->name.start);
}

static void emit_statement(struct Statement* statement, struct Program* program, FILE* out, uint32_t* label_id);

static void emit_if(struct IfStatement* branch, struct Program* program, FILE* out, uint32_t* label_id)
{
	const char* jump = jump_if_false(branch->comparison.type);
	if (jump == NULL || branch->left->kind == EXPR_BINARY || branch->right->kind == EXPR_BINARY)
	{
		fprintf(out, "\t; TODO: unsupported if\n");
		return;
	}

	uint32_t id = *label_id;
	*label_id += 1;

	fprintf(out, "\tcmp ");
	emit_operand(branch->left, out);
	fprintf(out, ", ");
	emit_operand(branch->right, out);
	fprintf(out, "\n");
	fprintf(out, "\t%s .if_end_%u\n", jump, id);

	emit_statement(branch->body, program, out, label_id);

	fprintf(out, ".if_end_%u:\n", id);
}

static void emit_statement(struct Statement* statement, struct Program* program, FILE* out, uint32_t* label_id)
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
		case STATEMENT_IF:
			emit_if(&statement->branch, program, out, label_id);
			break;
		case STATEMENT_CALL:
			emit_call(&statement->call, program, out);
			break;
		default:
			fprintf(out, "\t; TODO: unsupported statement\n");
			break;
	}
}

static void emit_proc(struct ProcDecl* proc, struct Program* program, FILE* out)
{
	bool is_entry = proc->name.length == 4 && memcmp(proc->name.start, "main", 4) == 0;
	if (is_entry)
		fprintf(out, "_start:\n");
	else
		fprintf(out, "%.*s:\n", (int)proc->name.length, proc->name.start);

	uint32_t label_id = 0;
	for (size_t i = 0; i < proc->body_count; i += 1)
		emit_statement(&proc->body[i], program, out, &label_id);

	if (!is_entry)
		fprintf(out, "\tret\n");
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
		emit_proc(&program->procs[i], program, out);
	}
}
