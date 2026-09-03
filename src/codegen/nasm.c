#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "codegen/nasm.h"

static void emit_const_expr(struct Expr* expr, FILE* out)
{
	switch (expr->kind)
	{
		case EXPR_PRIMARY:
			fprintf(out, "%.*s", (int)expr->primary.token.length, expr->primary.token.start);
			break;
		case EXPR_BINARY:
			emit_const_expr(expr->binary.left, out);
			fprintf(out, " %.*s ", (int)expr->binary.op.length, expr->binary.op.start);
			emit_const_expr(expr->binary.right, out);
			break;
		default:
			break;
	}
}

static void emit_consts(struct Program* program, FILE* out)
{
	for (size_t i = 0; i < program->const_count; i += 1)
	{
		struct ConstDecl decl = program->consts[i];
		fprintf(out, "%%define %.*s (", (int)decl.name.length, decl.name.start);
		emit_const_expr(decl.value, out);
		fprintf(out, ")\n");
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

struct FloatTable
{
	struct Token* items;
	size_t count;
	size_t capacity;
};

struct Emitter
{
	struct Program* program;
	struct ProcDecl* proc;
	struct FloatTable* floats;
	FILE* out;
	uint32_t label_id;
};

static bool is_float_register(struct Token token)
{
	if (token.length < 4 || memcmp(token.start, "xmm", 3) != 0)
		return false;

	for (size_t i = 3; i < token.length; i += 1)
		if (token.start[i] < '0' || token.start[i] > '9')
			return false;

	return true;
}

static size_t float_index(const struct FloatTable* floats, struct Token literal)
{
	for (size_t i = 0; i < floats->count; i += 1)
		if (floats->items[i].length == literal.length
			&& memcmp(floats->items[i].start, literal.start, literal.length) == 0)
			return i;

	return floats->count;
}

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
	if (token.length > 2 && token.start[0] == '0' && (token.start[1] == 'x' || token.start[1] == 'X'))
	{
		uint64_t value = 0;
		for (size_t i = 2; i < token.length; i += 1)
		{
			char digit = token.start[i];
			uint64_t nibble = digit <= '9' ? (uint64_t)(digit - '0')
				: (uint64_t)((digit | 0x20) - 'a' + 10);
			value = value * 16 + nibble;
		}
		return value;
	}

	if (token.length > 2 && token.start[0] == '0' && (token.start[1] == 'b' || token.start[1] == 'B'))
	{
		uint64_t value = 0;
		for (size_t i = 2; i < token.length; i += 1)
			value = value * 2 + (uint64_t)(token.start[i] - '0');
		return value;
	}

	uint64_t value = 0;
	for (size_t i = 0; i < token.length; i += 1)
		value = value * 10 + (uint64_t)(token.start[i] - '0');

	return value;
}

static bool fold_const(struct Program* program, struct Expr* expr, uint64_t* out);

static bool buffer_offset(struct Emitter* emitter, struct Token name, uint64_t* out_offset)
{
	struct ProcDecl* proc = emitter->proc;
	uint64_t cumulative = 0;
	for (size_t i = 0; i < proc->body_count; i += 1)
	{
		struct Statement* statement = &proc->body[i];
		if (statement->kind != STATEMENT_STACK)
			continue;

		uint64_t size = 0;
		fold_const(emitter->program, statement->stack.size, &size);
		cumulative += size;
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
	return emitter->proc != NULL && buffer_offset(emitter, token, &offset);
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

static bool tokens_equal(struct Token a, struct Token b)
{
	return a.length == b.length && memcmp(a.start, b.start, a.length) == 0;
}

static bool token_matches(struct Token token, const char* text)
{
	size_t length = strlen(text);
	return token.length == length && memcmp(token.start, text, length) == 0;
}

static struct EnumDecl* find_enum(struct Program* program, struct Token name)
{
	for (size_t i = 0; i < program->enum_count; i += 1)
		if (tokens_equal(program->enums[i].name, name))
			return &program->enums[i];

	return NULL;
}

static struct StructDecl* find_struct(struct Program* program, struct Token name)
{
	for (size_t i = 0; i < program->struct_count; i += 1)
		if (tokens_equal(program->structs[i].name, name))
			return &program->structs[i];

	return NULL;
}

static uint64_t store_size_bytes(enum StoreSize size)
{
	switch (size)
	{
		case STORE_SIZE_BYTE:  return 1;
		case STORE_SIZE_WORD:  return 2;
		case STORE_SIZE_DWORD: return 4;
		default:               return 8;
	}
}

static uint64_t char_literal_value(struct Token token)
{
	if (token.length >= 4 && token.start[1] == '\\')
	{
		switch (token.start[2])
		{
			case 'n':  return 10;
			case 't':  return 9;
			case 'r':  return 13;
			case '0':  return 0;
			case '\\': return 92;
			case '\'': return 39;
			default:   return (unsigned char)token.start[2];
		}
	}

	return (unsigned char)token.start[1];
}

static bool fold_member(struct Program* program, struct Expr* object, struct Token member, uint64_t* out)
{
	if (object->kind != EXPR_PRIMARY)
		return false;
	struct Token name = object->primary.token;

	const struct EnumDecl* enumeration = find_enum(program, name);
	if (enumeration != NULL)
	{
		for (size_t i = 0; i < enumeration->member_count; i += 1)
			if (tokens_equal(enumeration->members[i], member))
			{
				*out = i;
				return true;
			}
		return false;
	}

	const struct StructDecl* layout = find_struct(program, name);
	if (layout != NULL)
	{
		uint64_t offset = 0;
		for (size_t i = 0; i < layout->field_count; i += 1)
		{
			if (tokens_equal(layout->fields[i].name, member))
			{
				*out = offset;
				return true;
			}
			offset += store_size_bytes(layout->fields[i].size);
		}
		if (token_matches(member, "size"))
		{
			*out = offset;
			return true;
		}
	}

	return false;
}

// evaluates a compile-time constant expression: integer/char literals, other
// constants, enum values and struct offsets, and + - * / over them
static bool fold_const(struct Program* program, struct Expr* expr, uint64_t* out)
{
	switch (expr->kind)
	{
		case EXPR_PRIMARY:
		{
			struct Token token = expr->primary.token;
			if (token.type == TOKEN_INTEGER)
			{
				*out = token_to_u64(token);
				return true;
			}
			if (token.type == TOKEN_CHAR)
			{
				*out = char_literal_value(token);
				return true;
			}
			if (token.type == TOKEN_IDENTIFIER)
				for (size_t i = 0; i < program->const_count; i += 1)
					if (tokens_equal(program->consts[i].name, token))
						return fold_const(program, program->consts[i].value, out);
			return false;
		}
		case EXPR_BINARY:
		{
			uint64_t left;
			uint64_t right;
			if (!fold_const(program, expr->binary.left, &left)
				|| !fold_const(program, expr->binary.right, &right))
				return false;

			switch (expr->binary.op.type)
			{
				case TOKEN_PLUS:  *out = left + right; return true;
				case TOKEN_MINUS: *out = left - right; return true;
				case TOKEN_STAR:  *out = left * right; return true;
				case TOKEN_SLASH: *out = right != 0 ? left / right : 0; return true;
				default:          return false;
			}
		}
		case EXPR_MEMBER:
			return fold_member(program, expr->member.object, expr->member.member, out);
		case EXPR_DEREF:
			return false;
	}

	return false;
}

// an enum member folds to its 0-based index; a struct member folds to its byte
// offset (or the total size for `.size`)
static bool emit_named_member(struct Emitter* emitter, struct Token object, struct Token member)
{
	const struct EnumDecl* enumeration = find_enum(emitter->program, object);
	if (enumeration != NULL)
	{
		for (size_t i = 0; i < enumeration->member_count; i += 1)
			if (tokens_equal(enumeration->members[i], member))
			{
				fprintf(emitter->out, "%zu", i);
				return true;
			}
	}

	const struct StructDecl* layout = find_struct(emitter->program, object);
	if (layout != NULL)
	{
		uint64_t offset = 0;
		for (size_t i = 0; i < layout->field_count; i += 1)
		{
			if (tokens_equal(layout->fields[i].name, member))
			{
				fprintf(emitter->out, "%llu", (unsigned long long)offset);
				return true;
			}
			offset += store_size_bytes(layout->fields[i].size);
		}
		if (token_matches(member, "size"))
		{
			fprintf(emitter->out, "%llu", (unsigned long long)offset);
			return true;
		}
	}

	return false;
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

			// enum value or struct offset
			if (expr->member.object->kind == EXPR_PRIMARY
				&& emit_named_member(emitter, expr->member.object->primary.token, expr->member.member))
				return true;

			if (!emit_operand(emitter, expr->member.object))
				return false;
			fprintf(emitter->out, ".%.*s", (int)expr->member.member.length, expr->member.member.start);
			return true;
		}
		case EXPR_BINARY:
		case EXPR_DEREF:
			return false;
	}

	return false;
}

// idiv divides rdx:rax by its operand and leaves the quotient in rax, so a
// division computes `dst = dst / divisor` through rax (clobbering rax and rdx).
static void emit_division(struct Emitter* emitter, const char* dst, struct Expr* divisor)
{
	bool dst_is_rax = strcmp(dst, "rax") == 0;

	if (!dst_is_rax)
		fprintf(emitter->out, "\tmov rax, %s\n", dst);
	fprintf(emitter->out, "\tcqo\n");
	fprintf(emitter->out, "\tidiv ");
	emit_operand(emitter, divisor);
	fprintf(emitter->out, "\n");
	if (!dst_is_rax)
		fprintf(emitter->out, "\tmov %s, rax\n", dst);
}

// idiv leaves the remainder in rdx, so a modulo takes its result from there
static void emit_modulo(struct Emitter* emitter, const char* dst, struct Expr* divisor)
{
	if (strcmp(dst, "rax") != 0)
		fprintf(emitter->out, "\tmov rax, %s\n", dst);
	fprintf(emitter->out, "\tcqo\n");
	fprintf(emitter->out, "\tidiv ");
	emit_operand(emitter, divisor);
	fprintf(emitter->out, "\n");
	if (strcmp(dst, "rdx") != 0)
		fprintf(emitter->out, "\tmov %s, rdx\n", dst);
}

static void emit_divide(struct Emitter* emitter, struct AssignStatement* assign)
{
	if (assign->target_deref)
	{
		fprintf(emitter->out, "\t; TODO: unsupported division\n");
		return;
	}

	struct Token target = resolve_register(emitter, assign->target);
	char dst[32];
	snprintf(dst, sizeof(dst), "%.*s", (int)target.length, target.start);

	if (assign->op.type == TOKEN_PERCENT_EQUAL)
		emit_modulo(emitter, dst, assign->value);
	else
		emit_division(emitter, dst, assign->value);
}

// an expression can be evaluated into a register when it is a single term
// (primary or member), or a left-associative chain of binary operators whose
// right operands are plain operands (never a buffer or a nested binary)
static bool expr_supported(struct Emitter* emitter, struct Expr* expr)
{
	switch (expr->kind)
	{
		case EXPR_PRIMARY:
		case EXPR_MEMBER:
		case EXPR_DEREF:
			return true;
		case EXPR_BINARY:
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

// dst = [address], zero-extending narrower loads into the full register
static void emit_load(struct Emitter* emitter, const char* dst, struct DerefExpr* deref)
{
	FILE* out = emitter->out;
	switch (deref->size)
	{
		case STORE_SIZE_BYTE:
			fprintf(out, "\tmovzx %s, byte [", dst);
			break;
		case STORE_SIZE_WORD:
			fprintf(out, "\tmovzx %s, word [", dst);
			break;
		case STORE_SIZE_DWORD:
		{
			const char* dword = sized_register(text_token(dst), STORE_SIZE_DWORD);
			fprintf(out, "\tmov %s, [", dword != NULL ? dword : dst);
			break;
		}
		default:
			fprintf(out, "\tmov %s, [", dst);
			break;
	}

	emit_operand(emitter, deref->address);
	fprintf(out, "]\n");
}

static void emit_expr_into(struct Emitter* emitter, const char* dst, struct Expr* expr)
{
	if (expr->kind == EXPR_DEREF)
	{
		emit_load(emitter, dst, &expr->deref);
		return;
	}

	if (expr->kind == EXPR_BINARY)
	{
		emit_expr_into(emitter, dst, expr->binary.left);

		if (expr->binary.op.type == TOKEN_SLASH)
		{
			emit_division(emitter, dst, expr->binary.right);
			return;
		}
		if (expr->binary.op.type == TOKEN_PERCENT)
		{
			emit_modulo(emitter, dst, expr->binary.right);
			return;
		}

		const char* mnemonic =
			expr->binary.op.type == TOKEN_PLUS ? "add" :
			expr->binary.op.type == TOKEN_MINUS ? "sub" : "imul";
		fprintf(emitter->out, "\t%s %s, ", mnemonic, dst);
		emit_operand(emitter, expr->binary.right);
		fprintf(emitter->out, "\n");
		return;
	}

	if (expr->kind == EXPR_PRIMARY && is_buffer_name(emitter, expr->primary.token))
	{
		uint64_t offset;
		buffer_offset(emitter, expr->primary.token, &offset);
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

static const char* float_mnemonic(enum TokenType op)
{
	switch (op)
	{
		case TOKEN_EQUAL:       return "movsd";
		case TOKEN_PLUS_EQUAL:  return "addsd";
		case TOKEN_MINUS_EQUAL: return "subsd";
		case TOKEN_STAR_EQUAL:  return "mulsd";
		case TOKEN_SLASH_EQUAL: return "divsd";
		default:                return NULL;
	}
}

static bool value_is_float(struct Emitter* emitter, const struct Expr* expr)
{
	if (expr->kind != EXPR_PRIMARY)
		return false;
	if (expr->primary.token.type == TOKEN_FLOAT)
		return true;

	return is_float_register(resolve_register(emitter, expr->primary.token));
}

// floating point: xmm moves and arithmetic, conversions to/from general-purpose
// registers, and float literals loaded from their .data slot
static bool emit_float_assign(struct Emitter* emitter, struct AssignStatement* assign,
	struct Token target, bool target_float)
{
	struct Expr* value = assign->value;

	// float store: ^ptr = xmm  ->  movsd [ptr], xmm
	if (assign->target_deref)
	{
		if (assign->op.type != TOKEN_EQUAL || value->kind != EXPR_PRIMARY)
			return false;
		struct Token source = resolve_register(emitter, value->primary.token);
		if (!is_float_register(source))
			return false;
		fprintf(emitter->out, "\tmovsd [%.*s], %.*s\n",
			(int)target.length, target.start, (int)source.length, source.start);
		return true;
	}

	// float load: xmm = ^ptr  ->  movsd xmm, [ptr]
	if (value->kind == EXPR_DEREF)
	{
		if (!target_float || assign->op.type != TOKEN_EQUAL)
			return false;
		fprintf(emitter->out, "\tmovsd %.*s, [", (int)target.length, target.start);
		emit_operand(emitter, value->deref.address);
		fprintf(emitter->out, "]\n");
		return true;
	}

	if (value->kind == EXPR_PRIMARY && value->primary.token.type == TOKEN_FLOAT)
	{
		if (!target_float || assign->op.type != TOKEN_EQUAL)
			return false;
		size_t index = float_index(emitter->floats, value->primary.token);
		fprintf(emitter->out, "\tmovsd %.*s, [__float%zu]\n",
			(int)target.length, target.start, index);
		return true;
	}

	if (value->kind != EXPR_PRIMARY)
		return false;

	struct Token source = resolve_register(emitter, value->primary.token);
	bool source_float = is_float_register(source);

	if (target_float && source_float)
	{
		const char* mnemonic = float_mnemonic(assign->op.type);
		if (mnemonic == NULL)
			return false;
		fprintf(emitter->out, "\t%s %.*s, %.*s\n", mnemonic,
			(int)target.length, target.start, (int)source.length, source.start);
		return true;
	}

	if (assign->op.type != TOKEN_EQUAL)
		return false;

	if (target_float)
		fprintf(emitter->out, "\tcvtsi2sd %.*s, %.*s\n",
			(int)target.length, target.start, (int)source.length, source.start);
	else
		fprintf(emitter->out, "\tcvttsd2si %.*s, %.*s\n",
			(int)target.length, target.start, (int)source.length, source.start);
	return true;
}

static void emit_assign(struct Emitter* emitter, struct AssignStatement* assign)
{
	struct Token float_target = resolve_register(emitter, assign->target);
	if (is_float_register(float_target) || value_is_float(emitter, assign->value))
	{
		if (!emit_float_assign(emitter, assign, float_target, is_float_register(float_target)))
			fprintf(emitter->out, "\t; TODO: unsupported float assignment\n");
		return;
	}

	if (assign->op.type == TOKEN_SLASH_EQUAL || assign->op.type == TOKEN_PERCENT_EQUAL)
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
	if (mnemonic == NULL || assign->value->kind == EXPR_BINARY
		|| assign->value->kind == EXPR_DEREF || value_is_buffer)
	{
		fprintf(emitter->out, "\t; TODO: unsupported assignment\n");
		return;
	}

	// adding or subtracting a constant zero (e.g. a struct field at offset 0) is a no-op
	uint64_t folded;
	if (!assign->target_deref
		&& (assign->op.type == TOKEN_PLUS_EQUAL || assign->op.type == TOKEN_MINUS_EQUAL)
		&& fold_const(emitter->program, assign->value, &folded) && folded == 0)
		return;

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
	const struct ProcDecl* callee = find_proc(emitter->program, call->name);
	if (callee == NULL || callee->param_count != call->arg_count)
	{
		fprintf(emitter->out, "\t; TODO: unsupported call\n");
		return;
	}

	for (size_t i = 0; i < call->arg_count; i += 1)
	{
		if (call->args[i]->kind == EXPR_BINARY || call->args[i]->kind == EXPR_DEREF)
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

// ucomisd sets the flags like an unsigned compare, so float branches use the
// unsigned jump family (ja/jae/jb/jbe) rather than the signed one
static const char* float_jump_if_false(enum TokenType comparison)
{
	switch (comparison)
	{
		case TOKEN_EQUAL_EQUAL:   return "jne";
		case TOKEN_BANG_EQUAL:    return "je";
		case TOKEN_LESS:          return "jae";
		case TOKEN_LESS_EQUAL:    return "ja";
		case TOKEN_GREATER:       return "jbe";
		case TOKEN_GREATER_EQUAL: return "jb";
		default:                  return NULL;
	}
}

static void emit_float_operand(struct Emitter* emitter, const struct Expr* expr)
{
	if (expr->kind == EXPR_PRIMARY && expr->primary.token.type == TOKEN_FLOAT)
	{
		fprintf(emitter->out, "[__float%zu]", float_index(emitter->floats, expr->primary.token));
		return;
	}

	struct Token token = resolve_register(emitter, expr->primary.token);
	fprintf(emitter->out, "%.*s", (int)token.length, token.start);
}

static void emit_if(struct Emitter* emitter, struct IfStatement* branch)
{
	bool is_float = value_is_float(emitter, branch->left) || value_is_float(emitter, branch->right);

	uint32_t id = emitter->label_id;
	emitter->label_id += 1;

	if (is_float)
	{
		const char* jump = float_jump_if_false(branch->comparison.type);
		bool left_reg = branch->left->kind == EXPR_PRIMARY
			&& is_float_register(resolve_register(emitter, branch->left->primary.token));
		if (jump == NULL || !left_reg || !value_is_float(emitter, branch->right))
		{
			fprintf(emitter->out, "\t; TODO: unsupported if\n");
			return;
		}

		fprintf(emitter->out, "\tucomisd ");
		emit_float_operand(emitter, branch->left);
		fprintf(emitter->out, ", ");
		emit_float_operand(emitter, branch->right);
		fprintf(emitter->out, "\n\t%s .if_end_%u\n", jump, id);

		emit_statement(emitter, branch->body);
		fprintf(emitter->out, ".if_end_%u:\n", id);
		return;
	}

	const char* jump = jump_if_false(branch->comparison.type);
	if (jump == NULL
		|| branch->left->kind == EXPR_BINARY || branch->left->kind == EXPR_DEREF
		|| branch->right->kind == EXPR_BINARY || branch->right->kind == EXPR_DEREF)
	{
		fprintf(emitter->out, "\t; TODO: unsupported if\n");
		return;
	}

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

static uint64_t proc_stack_size(struct Program* program, struct ProcDecl* proc)
{
	uint64_t total = 0;
	for (size_t i = 0; i < proc->body_count; i += 1)
	{
		struct Statement* statement = &proc->body[i];
		if (statement->kind == STATEMENT_STACK)
		{
			uint64_t size = 0;
			fold_const(program, statement->stack.size, &size);
			total += size;
		}
	}

	if (total % 16 != 0)
		total += 16 - (total % 16);

	return total;
}

static void collect_float(struct FloatTable* floats, struct Token token)
{
	if (token.type != TOKEN_FLOAT || float_index(floats, token) != floats->count)
		return;

	if (floats->count == floats->capacity)
	{
		size_t capacity = floats->capacity < 8 ? 8 : floats->capacity * 2;
		floats->items = realloc(floats->items, capacity * sizeof(struct Token));
		floats->capacity = capacity;
	}

	floats->items[floats->count] = token;
	floats->count += 1;
}

static void collect_floats_expr(struct FloatTable* floats, struct Expr* expr)
{
	switch (expr->kind)
	{
		case EXPR_PRIMARY:
			collect_float(floats, expr->primary.token);
			break;
		case EXPR_BINARY:
			collect_floats_expr(floats, expr->binary.left);
			collect_floats_expr(floats, expr->binary.right);
			break;
		case EXPR_MEMBER:
			collect_floats_expr(floats, expr->member.object);
			break;
		case EXPR_DEREF:
			collect_floats_expr(floats, expr->deref.address);
			break;
	}
}

static void collect_floats_statement(struct FloatTable* floats, struct Statement* statement)
{
	switch (statement->kind)
	{
		case STATEMENT_ASSIGN:
			collect_floats_expr(floats, statement->assign.value);
			break;
		case STATEMENT_IF:
			collect_floats_expr(floats, statement->branch.left);
			collect_floats_expr(floats, statement->branch.right);
			collect_floats_statement(floats, statement->branch.body);
			break;
		case STATEMENT_CALL:
			for (size_t i = 0; i < statement->call.arg_count; i += 1)
				collect_floats_expr(floats, statement->call.args[i]);
			break;
		default:
			break;
	}
}

static struct FloatTable collect_floats(struct Program* program)
{
	struct FloatTable floats = { NULL, 0, 0 };
	for (size_t i = 0; i < program->proc_count; i += 1)
		for (size_t j = 0; j < program->procs[i].body_count; j += 1)
			collect_floats_statement(&floats, &program->procs[i].body[j]);

	return floats;
}

static void emit_float_data(const struct FloatTable* floats, FILE* out)
{
	for (size_t i = 0; i < floats->count; i += 1)
		fprintf(out, "__float%zu: dq %.*s\n", i,
			(int)floats->items[i].length, floats->items[i].start);
}

static void emit_proc(struct Program* program, struct FloatTable* floats, struct ProcDecl* proc, FILE* out)
{
	struct Emitter emitter;
	emitter.program = program;
	emitter.proc = proc;
	emitter.floats = floats;
	emitter.out = out;
	emitter.label_id = 0;

	struct Config config = program->config;
	bool is_entry = config.has_entry
		&& proc->name.length == config.entry.length
		&& memcmp(proc->name.start, config.entry.start, proc->name.length) == 0;

	fprintf(out, "%.*s:\n", (int)proc->name.length, proc->name.start);

	uint64_t stack_size = proc_stack_size(program, proc);
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
	struct FloatTable floats = collect_floats(program);

	fprintf(out, "bits %u\n\n", program->config.bits);

	if (program->const_count > 0)
	{
		emit_consts(program, out);
		fprintf(out, "\n");
	}

	emit_data(program, out);
	emit_float_data(&floats, out);
	fprintf(out, "\n");

	fprintf(out, "section .text\n");
	if (program->config.has_entry)
		fprintf(out, "global %.*s\n", (int)program->config.entry.length, program->config.entry.start);

	for (size_t i = 0; i < program->proc_count; i += 1)
	{
		fprintf(out, "\n");
		emit_proc(program, &floats, &program->procs[i], out);
	}

	free(floats.items);
}
