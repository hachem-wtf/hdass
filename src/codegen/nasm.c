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
		fprintf(out, "%%define %.*s %.*s\n",
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

static const char* sized_register(struct Token reg, enum StoreSize size);

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

static struct Token text_token(const char* text)
{
	struct Token token;
	token.type = TOKEN_IDENTIFIER;
	token.start = text;
	token.length = strlen(text);
	token.line = 0;
	return token;
}

// with the logical_registers extension, r1..r14 name the general-purpose
// registers; rsp/rbp and the instruction pointer keep their dedicated names.
static const char* logical_register_base(struct Token token)
{
	static const char* registers[] = {
		"rax", "rbx", "rcx", "rdx", "rsi", "rdi",
		"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
	};

	if (token.length < 2 || token.start[0] != 'r')
		return NULL;

	uint32_t index = 0;
	for (size_t i = 1; i < token.length; i += 1)
	{
		char digit = token.start[i];
		if (digit < '0' || digit > '9')
			return NULL;
		index = index * 10 + (uint32_t)(digit - '0');
	}

	if (index < 1 || index > sizeof(registers) / sizeof(registers[0]))
		return NULL;

	return registers[index - 1];
}

static struct Token resolve_register(struct Emitter* emitter, struct Token token)
{
	struct Token resolved = resolve_token(emitter, token);

	if (emitter->program->config.logical_registers)
	{
		const char* base = logical_register_base(resolved);
		if (base != NULL)
			return text_token(base);
	}

	return resolved;
}

static uint64_t token_to_u64(struct Token token)
{
	uint64_t value = 0;
	for (size_t i = 0; i < token.length; i += 1)
		value = value * 10 + (uint64_t)(token.start[i] - '0');

	return value;
}

static bool buffer_offset(struct ProcDecl* proc, struct Token name, uint64_t* out_offset)
{
	uint64_t cumulative = 0;
	for (size_t i = 0; i < proc->body_count; i += 1)
	{
		struct Statement* statement = &proc->body[i];
		if (statement->kind != STATEMENT_STACK)
			continue;

		cumulative += token_to_u64(statement->stack.size);
		if (statement->stack.name.length == name.length
			&& memcmp(statement->stack.name.start, name.start, name.length) == 0)
		{
			*out_offset = cumulative;
			return true;
		}
	}

	return false;
}

static bool is_buffer_name(struct Emitter* emitter, struct Token token)
{
	uint64_t offset;
	return emitter->proc != NULL && buffer_offset(emitter->proc, token, &offset);
}

static enum StoreSize size_from_int(struct Token token)
{
	switch (token_to_u64(token))
	{
		case 8:  return STORE_SIZE_BYTE;
		case 16: return STORE_SIZE_WORD;
		case 32: return STORE_SIZE_DWORD;
		case 64: return STORE_SIZE_QWORD;
		default: return STORE_SIZE_NONE;
	}
}

static bool emit_operand(struct Emitter* emitter, struct Expr* expr)
{
	switch (expr->kind)
	{
		case EXPR_PRIMARY:
		{
			struct Token token = resolve_register(emitter, expr->primary.token);
			fprintf(emitter->out, "%.*s", (int)token.length, token.start);
			return true;
		}
		case EXPR_MEMBER:
		{
			// a register size suffix: r1.64 -> rax, r1.8 -> al
			if (expr->member.member.type == TOKEN_INTEGER &&
				expr->member.object->kind == EXPR_PRIMARY)
			{
				enum StoreSize size = size_from_int(expr->member.member);
				struct Token base = resolve_register(emitter, expr->member.object->primary.token);
				const char* sized = sized_register(base, size);
				if (sized != NULL)
					fprintf(emitter->out, "%s", sized);
				else
					fprintf(emitter->out, "%.*s", (int)base.length, base.start);
				return true;
			}

			if (!emit_operand(emitter, expr->member.object))
				return false;
			fprintf(emitter->out, ".%.*s", (int)expr->member.member.length, expr->member.member.start);
			return true;
		}
		case EXPR_BINARY:
			return false;
	}

	return false;
}

static void emit_divide(struct Emitter* emitter, struct AssignStatement* assign)
{
	struct Token target = resolve_register(emitter, assign->target);
	bool target_is_rax = target.length == 3 && memcmp(target.start, "rax", 3) == 0;
	if (assign->target_deref || !target_is_rax)
	{
		fprintf(emitter->out, "\t; TODO: unsupported division\n");
		return;
	}

	fprintf(emitter->out, "\tcqo\n");
	fprintf(emitter->out, "\tidiv ");
	emit_operand(emitter, assign->value);
	fprintf(emitter->out, "\n");
}

// an expression can be evaluated into a register when it is a single term
// (primary or member), or a left-associative chain of '+'/'-' whose right
// operands are plain operands (never a buffer or a nested binary)
static bool expr_supported(struct Emitter* emitter, struct Expr* expr)
{
	switch (expr->kind)
	{
		case EXPR_PRIMARY:
		case EXPR_MEMBER:
			return true;
		case EXPR_BINARY:
			if (expr->binary.op.type != TOKEN_PLUS && 
				expr->binary.op.type != TOKEN_MINUS)
				return false;
			if (expr->binary.right->kind != EXPR_PRIMARY && 
				expr->binary.right->kind != EXPR_MEMBER)
				return false;
			if (expr->binary.right->kind == EXPR_PRIMARY && 
				is_buffer_name(emitter, expr->binary.right->primary.token))
				return false;
			return expr_supported(emitter, expr->binary.left);
	}

	return false;
}

static void emit_expr_into(struct Emitter* emitter, const char* dst, struct Expr* expr)
{
	if (expr->kind == EXPR_BINARY)
	{
		emit_expr_into(emitter, dst, expr->binary.left);

		const char* mnemonic = expr->binary.op.type == TOKEN_PLUS ? "add" : "sub";
		fprintf(emitter->out, "\t%s %s, ", mnemonic, dst);
		emit_operand(emitter, expr->binary.right);
		fprintf(emitter->out, "\n");
		return;
	}

	if (expr->kind == EXPR_PRIMARY && is_buffer_name(emitter, expr->primary.token))
	{
		uint64_t offset;
		buffer_offset(emitter->proc, expr->primary.token, &offset);
		fprintf(emitter->out, "\tlea %s, [rbp - %llu]\n", dst, (unsigned long long)offset);
		return;
	}

	fprintf(emitter->out, "\tmov %s, ", dst);
	emit_operand(emitter, expr);
	fprintf(emitter->out, "\n");
}

static const char* store_size_keyword(enum StoreSize size)
{
	switch (size)
	{
		case STORE_SIZE_BYTE:  return "byte ";
		case STORE_SIZE_WORD:  return "word ";
		case STORE_SIZE_DWORD: return "dword ";
		case STORE_SIZE_QWORD: return "qword ";
		default:               return "";
	}
}

// maps a full 64-bit register to its byte/word/dword sub-register for a sized
// store, so `^byte rsi = rdx` writes `dl` rather than the whole register.
// returns NULL when the token is not a full register, or no resizing applies.
static const char* sized_register(struct Token reg, enum StoreSize size)
{
	if (size == STORE_SIZE_NONE || size == STORE_SIZE_QWORD)
		return NULL;

	static const struct RegisterSizes
	{
		const char* quad;
		const char* dword;
		const char* word;
		const char* byte;
	} registers[] =
	{
		{ "rax", "eax",  "ax",   "al"   },
		{ "rbx", "ebx",  "bx",   "bl"   },
		{ "rcx", "ecx",  "cx",   "cl"   },
		{ "rdx", "edx",  "dx",   "dl"   },
		{ "rsi", "esi",  "si",   "sil"  },
		{ "rdi", "edi",  "di",   "dil"  },
		{ "rbp", "ebp",  "bp",   "bpl"  },
		{ "rsp", "esp",  "sp",   "spl"  },
		{ "r8",  "r8d",  "r8w",  "r8b"  },
		{ "r9",  "r9d",  "r9w",  "r9b"  },
		{ "r10", "r10d", "r10w", "r10b" },
		{ "r11", "r11d", "r11w", "r11b" },
		{ "r12", "r12d", "r12w", "r12b" },
		{ "r13", "r13d", "r13w", "r13b" },
		{ "r14", "r14d", "r14w", "r14b" },
		{ "r15", "r15d", "r15w", "r15b" },
	};

	for (size_t i = 0; i < sizeof(registers) / sizeof(registers[0]); i += 1)
	{
		const struct RegisterSizes* entry = &registers[i];
		size_t length = strlen(entry->quad);
		if (reg.length != length || memcmp(reg.start, entry->quad, length) != 0)
			continue;

		switch (size)
		{
			case STORE_SIZE_DWORD: return entry->dword;
			case STORE_SIZE_WORD:  return entry->word;
			case STORE_SIZE_BYTE:  return entry->byte;
			default:               return NULL;
		}
	}

	return NULL;
}

static void emit_assign(struct Emitter* emitter, struct AssignStatement* assign)
{
	if (assign->op.type == TOKEN_SLASH_EQUAL)
	{
		emit_divide(emitter, assign);
		return;
	}

	struct Token target = resolve_register(emitter, assign->target);

	if (assign->op.type == TOKEN_EQUAL && !assign->target_deref)
	{
		if (!expr_supported(emitter, assign->value))
		{
			fprintf(emitter->out, "\t; TODO: unsupported assignment\n");
			return;
		}

		char dst[32];
		snprintf(dst, sizeof(dst), "%.*s", (int)target.length, target.start);
		emit_expr_into(emitter, dst, assign->value);
		return;
	}

	// deref store or compound assignment: needs a plain operand, not a buffer or binary
	const char* mnemonic = assign_mnemonic(assign->op.type);
	bool value_is_buffer = assign->value->kind == EXPR_PRIMARY 
		                && is_buffer_name(emitter, assign->value->primary.token);
	if (mnemonic == NULL || assign->value->kind == EXPR_BINARY || value_is_buffer)
	{
		fprintf(emitter->out, "\t; TODO: unsupported assignment\n");
		return;
	}

	if (assign->target_deref)
	{
		fprintf(emitter->out, "\t%s %s[%.*s], ", mnemonic,
			store_size_keyword(assign->store_size), (int)target.length, target.start);

		const char* sized = NULL;
		if (assign->value->kind == EXPR_PRIMARY)
		{
			struct Token value = resolve_register(emitter, assign->value->primary.token);
			sized = sized_register(value, assign->store_size);
			if (sized != NULL)
				fprintf(emitter->out, "%s", sized);
		}

		if (sized == NULL)
			emit_operand(emitter, assign->value);
	}
	else
	{
		fprintf(emitter->out, "\t%s %.*s, ", mnemonic, (int)target.length, target.start);
		emit_operand(emitter, assign->value);
	}

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

		struct Token reg = resolve_register(emitter, callee->params[i].reg);
		fprintf(emitter->out, "\tmov %.*s, ", (int)reg.length, reg.start);
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
		case STATEMENT_STACK:
			break;
		default:
			fprintf(out, "\t; TODO: unsupported statement\n");
			break;
	}
}

static uint64_t proc_stack_size(struct ProcDecl* proc)
{
	uint64_t total = 0;
	for (size_t i = 0; i < proc->body_count; i += 1)
	{
		struct Statement* statement = &proc->body[i];
		if (statement->kind == STATEMENT_STACK)
			total += token_to_u64(statement->stack.size);
	}

	if (total % 16 != 0)
		total += 16 - (total % 16);

	return total;
}

static void emit_proc(struct Program* program, struct ProcDecl* proc, FILE* out)
{
	struct Emitter emitter;
	emitter.program = program;
	emitter.proc = proc;
	emitter.out = out;
	emitter.label_id = 0;

	struct Config config = program->config;
	bool is_entry = config.has_entry
		&& proc->name.length == config.entry.length
		&& memcmp(proc->name.start, config.entry.start, proc->name.length) == 0;

	fprintf(out, "%.*s:\n", (int)proc->name.length, proc->name.start);

	uint64_t stack_size = proc_stack_size(proc);
	if (stack_size > 0)
	{
		fprintf(out, "\tpush rbp\n");
		fprintf(out, "\tmov rbp, rsp\n");
		fprintf(out, "\tsub rsp, %llu\n", (unsigned long long)stack_size);
	}

	for (size_t i = 0; i < proc->body_count; i += 1)
		emit_statement(&emitter, &proc->body[i]);

	if (!is_entry)
	{
		if (stack_size > 0)
			fprintf(out, "\tleave\n");
		fprintf(out, "\tret\n");
	}
}

void generate_nasm(struct Program* program, FILE* out)
{
	fprintf(out, "bits %u\n\n", program->config.bits);

	if (program->const_count > 0)
	{
		emit_consts(program, out);
		fprintf(out, "\n");
	}

	emit_data(program, out);
	fprintf(out, "\n");

	fprintf(out, "section .text\n");
	if (program->config.has_entry)
		fprintf(out, "global %.*s\n", (int)program->config.entry.length, program->config.entry.start);

	for (size_t i = 0; i < program->proc_count; i += 1)
	{
		fprintf(out, "\n");
		emit_proc(program, &program->procs[i], out);
	}
}
