#include <stdio.h>
#include <stddef.h>

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
}
