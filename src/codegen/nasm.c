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

struct Emitter
{
	struct Program* program;
	struct ProcDecl* proc;
	FILE* out;
	uint32_t label_id;
};

static struct Token resolve_token(struct Emitter* emitter, struct Token token)
{
	if (emitter->proc == NULL)
		return token;

	for (size_t i = 0; i < emitter->proc->param_count; i += 1)
	{
		struct Param param = emitter->proc->params[i];
		if (param.name.length == token.length && memcmp(param.name.start, token.start, token.length) == 0)
			return param.reg;
	}

	return token;
}

static bool emit_operand(struct Emitter* emitter, struct Expr* expr)
{
	switch (expr->kind)
	{
		case EXPR_PRIMARY:
		{
			struct Token token = resolve_token(emitter, expr->primary.token);
			fprintf(emitter->out, "%.*s", (int)token.length, token.start);
			return true;
		}
		case EXPR_MEMBER:
			if (!emit_operand(emitter, expr->member.object))
				return false;
			fprintf(emitter->out, ".%.*s", (int)expr->member.member.length, expr->member.member.start);
			return true;
		case EXPR_BINARY:
			return false;
	}

	return false;
}

static void emit_assign(struct Emitter* emitter, struct AssignStatement* assign)
{
	const char* mnemonic = assign_mnemonic(assign->op.type);
	if (mnemonic == NULL || assign->value->kind == EXPR_BINARY)
	{
		fprintf(emitter->out, "\t; TODO: unsupported assignment\n");
		return;
	}

	struct Token target = resolve_token(emitter, assign->target);
	if (assign->target_deref)
		fprintf(emitter->out, "\t%s [%.*s], ", mnemonic, (int)target.length, target.start);
	else
		fprintf(emitter->out, "\t%s %.*s, ", mnemonic, (int)target.length, target.start);

	emit_operand(emitter, assign->value);
	fprintf(emitter->out, "\n");
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

static void emit_call(struct Emitter* emitter, struct CallStatement* call)
{
	struct ProcDecl* callee = find_proc(emitter->program, call->name);
	if (callee == NULL || callee->param_count != call->arg_count)
	{
		fprintf(emitter->out, "\t; TODO: unsupported call\n");
		return;
	}

	for (size_t i = 0; i < call->arg_count; i += 1)
	{
		if (call->args[i]->kind == EXPR_BINARY)
		{
			fprintf(emitter->out, "\t; TODO: unsupported call argument\n");
			continue;
		}

		fprintf(emitter->out, "\tmov %.*s, ", (int)callee->params[i].reg.length, callee->params[i].reg.start);
		emit_operand(emitter, call->args[i]);
		fprintf(emitter->out, "\n");
	}

	fprintf(emitter->out, "\tcall %.*s\n", (int)call->name.length, call->name.start);
}

static void emit_statement(struct Emitter* emitter, struct Statement* statement);

static void emit_if(struct Emitter* emitter, struct IfStatement* branch)
{
	const char* jump = jump_if_false(branch->comparison.type);
	if (jump == NULL || branch->left->kind == EXPR_BINARY || branch->right->kind == EXPR_BINARY)
	{
		fprintf(emitter->out, "\t; TODO: unsupported if\n");
		return;
	}

	uint32_t id = emitter->label_id;
	emitter->label_id += 1;

	fprintf(emitter->out, "\tcmp ");
	emit_operand(emitter, branch->left);
	fprintf(emitter->out, ", ");
	emit_operand(emitter, branch->right);
	fprintf(emitter->out, "\n");
	fprintf(emitter->out, "\t%s .if_end_%u\n", jump, id);

	emit_statement(emitter, branch->body);

	fprintf(emitter->out, ".if_end_%u:\n", id);
}

static void emit_statement(struct Emitter* emitter, struct Statement* statement)
{
	FILE* out = emitter->out;
	switch (statement->kind)
	{
		case STATEMENT_ASSIGN:
			emit_assign(emitter, &statement->assign);
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
			emit_if(emitter, &statement->branch);
			break;
		case STATEMENT_CALL:
			emit_call(emitter, &statement->call);
			break;
		default:
			fprintf(out, "\t; TODO: unsupported statement\n");
			break;
	}
}

static void emit_proc(struct Program* program, struct ProcDecl* proc, FILE* out)
{
	struct Emitter emitter;
	emitter.program = program;
	emitter.proc = proc;
	emitter.out = out;
	emitter.label_id = 0;

	bool is_entry = proc->name.length == 4 && memcmp(proc->name.start, "main", 4) == 0;
	if (is_entry)
		fprintf(out, "_start:\n");
	else
		fprintf(out, "%.*s:\n", (int)proc->name.length, proc->name.start);

	for (size_t i = 0; i < proc->body_count; i += 1)
		emit_statement(&emitter, &proc->body[i]);

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
		emit_proc(program, &program->procs[i], out);
	}
}
