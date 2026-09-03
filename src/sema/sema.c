#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "sema/sema.h"

static bool token_is(struct Token token, const char* text)
{
	size_t length = strlen(text);
	return token.length == length && memcmp(token.start, text, length) == 0;
}

static bool names_equal(struct Token a, struct Token b)
{
	return a.length == b.length && memcmp(a.start, b.start, a.length) == 0;
}

static struct ProcDecl* find_proc(struct Program* program, struct Token name)
{
	for (size_t i = 0; i < program->proc_count; i += 1)
		if (names_equal(program->procs[i].name, name))
			return &program->procs[i];

	return NULL;
}

static bool check_duplicate_names(struct Source source, struct Program* program)
{
	size_t count = program->const_count + program->data_count
		+ program->enum_count + program->struct_count + program->proc_count;
	if (count == 0)
		return true;

	struct Token* names = malloc(count * sizeof(struct Token));
	if (names == NULL)
		return true;
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
	for (size_t i = 0; i < program->enum_count; i += 1)
	{
		names[n] = program->enums[i].name;
		n += 1;
	}
	for (size_t i = 0; i < program->struct_count; i += 1)
	{
		names[n] = program->structs[i].name;
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
				char message[128];
				snprintf(message, sizeof(message), "'%.*s' is already defined",
					(int)names[i].length, names[i].start);
				report_error(source, names[i], message);
				ok = false;
			}

	free(names);
	return ok;
}

static bool check_entry_point(struct Source source, struct Program* program)
{
	if (!program->config.has_entry)
		return true;

	if (find_proc(program, program->config.entry) != NULL)
		return true;

	struct Token entry = program->config.entry;
	char message[128];
	snprintf(message, sizeof(message), "entry point '%.*s' is not defined",
		(int)entry.length, entry.start);
	report_error(source, entry, message);
	return false;
}

static bool is_program_const(const struct Program* program, struct Token name)
{
	for (size_t i = 0; i < program->const_count; i += 1)
		if (names_equal(program->consts[i].name, name))
			return true;

	return false;
}

static struct Token first_token(struct Expr* expr)
{
	switch (expr->kind)
	{
		case EXPR_BINARY: return first_token(expr->binary.left);
		case EXPR_MEMBER: return first_token(expr->member.object);
		case EXPR_DEREF:  return first_token(expr->deref.address);
		default:          return expr->primary.token;
	}
}

static bool check_const_value(struct Source source, struct Program* program, struct Expr* expr)
{
	if (expr->kind == EXPR_BINARY)
	{
		bool left = check_const_value(source, program, expr->binary.left);
		bool right = check_const_value(source, program, expr->binary.right);
		return left && right;
	}

	if (expr->kind == EXPR_PRIMARY)
	{
		struct Token token = expr->primary.token;
		if (token.type == TOKEN_INTEGER || token.type == TOKEN_CHAR)
			return true;

		if (token.type == TOKEN_IDENTIFIER && is_program_const(program, token))
			return true;

		char message[128];
		snprintf(message, sizeof(message), "'%.*s' is not a constant",
			(int)token.length, token.start);
		report_error(source, token, message);
		return false;
	}

	report_error(source, first_token(expr), "constant must be an integer expression");
	return false;
}

static bool check_const_values(struct Source source, struct Program* program)
{
	bool ok = true;
	for (size_t i = 0; i < program->const_count; i += 1)
		if (!check_const_value(source, program, program->consts[i].value))
			ok = false;

	return ok;
}

struct RefCheck
{
	struct Source source;
	struct Program* program;
	struct ProcDecl* proc;
	bool ok;
};

static void ref_error(struct RefCheck* check, struct Token token, const char* format, ...)
{
	char message[256];
	va_list args;
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	report_error(check->source, token, message);
	check->ok = false;
}

static bool is_arch_register(struct Token token)
{
	static const char* names[] = {
		"rax", "eax", "ax", "al", "ah",
		"rbx", "ebx", "bx", "bl", "bh",
		"rcx", "ecx", "cx", "cl", "ch",
		"rdx", "edx", "dx", "dl", "dh",
		"rsi", "esi", "si", "sil",
		"rdi", "edi", "di", "dil",
		"rbp", "ebp", "bp", "bpl",
		"rsp", "esp", "sp", "spl",
		"r8",  "r8d",  "r8w",  "r8b",
		"r9",  "r9d",  "r9w",  "r9b",
		"r10", "r10d", "r10w", "r10b",
		"r11", "r11d", "r11w", "r11b",
		"r12", "r12d", "r12w", "r12b",
		"r13", "r13d", "r13w", "r13b",
		"r14", "r14d", "r14w", "r14b",
		"r15", "r15d", "r15w", "r15b",
		"rip",
		"xmm0", "xmm1", "xmm2",  "xmm3",  "xmm4",  "xmm5",  "xmm6",  "xmm7",
		"xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15",
	};

	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i += 1)
		if (token_is(token, names[i]))
			return true;

	return false;
}

static bool is_logical_register(struct Token token)
{
	if (token.length < 2 || token.start[0] != 'r')
		return false;

	uint32_t index = 0;
	for (size_t i = 1; i < token.length; i += 1)
	{
		char digit = token.start[i];
		if (digit < '0' || digit > '9')
			return false;
		index = index * 10 + (uint32_t)(digit - '0');
	}

	return index >= 1 && index <= 14;
}

static bool is_register(struct RefCheck* check, struct Token token)
{
	if (is_arch_register(token))
		return true;

	return check->program->config.logical_registers && is_logical_register(token);
}

static bool is_param(const struct RefCheck* check, struct Token token)
{
	for (size_t i = 0; i < check->proc->param_count; i += 1)
		if (names_equal(check->proc->params[i].name, token))
			return true;

	return false;
}

static bool is_const(const struct RefCheck* check, struct Token token)
{
	for (size_t i = 0; i < check->program->const_count; i += 1)
		if (names_equal(check->program->consts[i].name, token))
			return true;

	return false;
}

static bool is_data(const struct RefCheck* check, struct Token token)
{
	for (size_t i = 0; i < check->program->data_count; i += 1)
		if (names_equal(check->program->data_decls[i].name, token))
			return true;

	return false;
}

static struct EnumDecl* find_enum(struct RefCheck* check, struct Token token)
{
	for (size_t i = 0; i < check->program->enum_count; i += 1)
		if (names_equal(check->program->enums[i].name, token))
			return &check->program->enums[i];

	return NULL;
}

static struct StructDecl* find_struct(struct RefCheck* check, struct Token token)
{
	for (size_t i = 0; i < check->program->struct_count; i += 1)
		if (names_equal(check->program->structs[i].name, token))
			return &check->program->structs[i];

	return NULL;
}

static bool is_stack_buffer(struct RefCheck* check, struct Token token)
{
	for (size_t i = 0; i < check->proc->body_count; i += 1)
	{
		const struct Statement* statement = &check->proc->body[i];
		if (statement->kind == STATEMENT_STACK && names_equal(statement->stack.name, token))
			return true;
	}

	return false;
}

static bool is_label(struct RefCheck* check, struct Token token)
{
	for (size_t i = 0; i < check->proc->body_count; i += 1)
	{
		const struct Statement* statement = &check->proc->body[i];
		if (statement->kind == STATEMENT_LABEL && names_equal(statement->label.name, token))
			return true;
	}

	return false;
}

static void check_value_name(struct RefCheck* check, struct Token name)
{
	if (is_register(check, name) || is_param(check, name) || is_const(check, name)
		|| is_data(check, name) || is_stack_buffer(check, name))
		return;

	ref_error(check, name, "undefined name '%.*s'", (int)name.length, name.start);
}

static void check_expr(struct RefCheck* check, const struct Expr* expr)
{
	switch (expr->kind)
	{
		case EXPR_PRIMARY:
			if (expr->primary.token.type == TOKEN_IDENTIFIER)
				check_value_name(check, expr->primary.token);
			break;
		case EXPR_BINARY:
			check_expr(check, expr->binary.left);
			check_expr(check, expr->binary.right);
			break;
		case EXPR_DEREF:
		{
			const struct Expr* address = expr->deref.address;
			if (address->kind == EXPR_PRIMARY
				&& (is_register(check, address->primary.token) || is_param(check, address->primary.token)))
				break;

			if (address->kind == EXPR_PRIMARY)
				ref_error(check, address->primary.token, "dereference address must be a register");
			else
				check_expr(check, address);
			break;
		}
		case EXPR_MEMBER:
		{
			const struct Expr* object = expr->member.object;
			struct Token member = expr->member.member;

			if (member.type == TOKEN_INTEGER)
			{
				if (object->kind != EXPR_PRIMARY || !is_register(check, object->primary.token))
					ref_error(check, member, "size suffix requires a register");
				break;
			}

			if (object->kind == EXPR_PRIMARY)
			{
				const struct EnumDecl* enumeration = find_enum(check, object->primary.token);
				if (enumeration != NULL)
				{
					bool found = false;
					for (size_t i = 0; i < enumeration->member_count; i += 1)
						if (names_equal(enumeration->members[i], member))
							found = true;
					if (!found)
						ref_error(check, member, "enum '%.*s' has no member '%.*s'",
							(int)object->primary.token.length, object->primary.token.start,
							(int)member.length, member.start);
					break;
				}

				const struct StructDecl* layout = find_struct(check, object->primary.token);
				if (layout != NULL)
				{
					bool found = token_is(member, "size");
					for (size_t i = 0; i < layout->field_count; i += 1)
						if (names_equal(layout->fields[i].name, member))
							found = true;
					if (!found)
						ref_error(check, member, "struct '%.*s' has no field '%.*s'",
							(int)object->primary.token.length, object->primary.token.start,
							(int)member.length, member.start);
					break;
				}

				if (is_data(check, object->primary.token))
				{
					if (!token_is(member, "len"))
						ref_error(check, member, "unknown member '%.*s'", (int)member.length, member.start);
					break;
				}
			}

			check_expr(check, object);
			break;
		}
	}
}

static void check_target(struct RefCheck* check, struct Token target)
{
	if (is_register(check, target) || is_param(check, target))
		return;

	ref_error(check, target, "cannot assign to '%.*s': not a register", (int)target.length, target.start);
}

// a stack size must be a compile-time constant: integer/char literals, other
// constants, enum values, struct sizes/offsets, and arithmetic over them
static void check_stack_size(struct RefCheck* check, struct Expr* expr)
{
	switch (expr->kind)
	{
		case EXPR_BINARY:
			check_stack_size(check, expr->binary.left);
			check_stack_size(check, expr->binary.right);
			break;
		case EXPR_MEMBER:
		{
			const struct Expr* object = expr->member.object;
			if (object->kind == EXPR_PRIMARY
				&& (find_enum(check, object->primary.token) != NULL
					|| find_struct(check, object->primary.token) != NULL))
				check_expr(check, expr);
			else
				ref_error(check, first_token(expr), "stack size must be a constant");
			break;
		}
		case EXPR_PRIMARY:
		{
			struct Token token = expr->primary.token;
			if (token.type == TOKEN_INTEGER || token.type == TOKEN_CHAR)
				break;
			if (token.type == TOKEN_IDENTIFIER && is_const(check, token))
				break;
			ref_error(check, token, "stack size must be a constant");
			break;
		}
		default:
			ref_error(check, first_token(expr), "stack size must be a constant");
			break;
	}
}

static void check_statement(struct RefCheck* check, struct Statement* statement)
{
	switch (statement->kind)
	{
		case STATEMENT_ASSIGN:
			check_target(check, statement->assign.target);
			check_expr(check, statement->assign.value);
			break;
		case STATEMENT_GOTO:
			if (!is_label(check, statement->jump.label))
				ref_error(check, statement->jump.label, "undefined label '%.*s'",
					(int)statement->jump.label.length, statement->jump.label.start);
			break;
		case STATEMENT_IF:
			check_expr(check, statement->branch.left);
			check_expr(check, statement->branch.right);
			check_statement(check, statement->branch.body);
			break;
		case STATEMENT_CALL:
		{
			struct CallStatement* call = &statement->call;
			struct ProcDecl* callee = find_proc(check->program, call->name);
			if (callee == NULL)
				ref_error(check, call->name, "undefined procedure '%.*s'",
					(int)call->name.length, call->name.start);
			else if (callee->param_count != call->arg_count)
				ref_error(check, call->name, "'%.*s' expects %zu argument(s), got %zu",
					(int)call->name.length, call->name.start, callee->param_count, call->arg_count);

			for (size_t i = 0; i < call->arg_count; i += 1)
				check_expr(check, call->args[i]);
			break;
		}
		case STATEMENT_STACK:
			check_stack_size(check, statement->stack.size);
			break;
		case STATEMENT_LABEL:
		case STATEMENT_SYSCALL:
			break;
	}
}

static bool check_references(struct Source source, struct Program* program)
{
	bool ok = true;
	for (size_t i = 0; i < program->proc_count; i += 1)
	{
		struct RefCheck check = { source, program, &program->procs[i], true };
		for (size_t j = 0; j < program->procs[i].body_count; j += 1)
			check_statement(&check, &program->procs[i].body[j]);

		if (!check.ok)
			ok = false;
	}

	return ok;
}

bool analyze_program(struct Source source, struct Program* program)
{
	bool ok = true;

	if (!check_duplicate_names(source, program))
		ok = false;
	if (!check_entry_point(source, program))
		ok = false;
	if (!check_const_values(source, program))
		ok = false;
	if (!check_references(source, program))
		ok = false;

	return ok;
}
