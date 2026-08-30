#pragma once

#include <stddef.h>
#include <stdbool.h>

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

enum ExprKind
{
	EXPR_PRIMARY,
	EXPR_BINARY,
	EXPR_MEMBER,
};

struct PrimaryExpr
{
	struct Token token;
};

struct BinaryExpr
{
	struct Expr* left;
	struct Token op;
	struct Expr* right;
};

struct MemberExpr
{
	struct Expr* object;
	struct Token member;
};

struct Expr
{
	enum ExprKind kind;
	union
	{
		struct PrimaryExpr primary;
		struct BinaryExpr binary;
		struct MemberExpr member;
	};
};

enum StoreSize
{
	STORE_SIZE_NONE,
	STORE_SIZE_BYTE,
	STORE_SIZE_WORD,
	STORE_SIZE_DWORD,
	STORE_SIZE_QWORD,
};

enum StatementKind
{
	STATEMENT_ASSIGN,
	STATEMENT_LABEL,
	STATEMENT_GOTO,
	STATEMENT_SYSCALL,
	STATEMENT_IF,
	STATEMENT_CALL,
	STATEMENT_STACK,
};

struct AssignStatement
{
	bool target_deref;
	enum StoreSize store_size;
	struct Token target;
	struct Token op;
	struct Expr* value;
};

struct LabelStatement
{
	struct Token name;
};

struct GotoStatement
{
	struct Token label;
};

struct IfStatement
{
	struct Expr* left;
	struct Token comparison;
	struct Expr* right;
	struct Statement* body;
};

struct CallStatement
{
	struct Token name;
	struct Expr** args;
	size_t arg_count;
	size_t arg_capacity;
};

struct StackStatement
{
	struct Token name;
	struct Token size;
};

struct Statement
{
	enum StatementKind kind;
	union
	{
		struct AssignStatement assign;
		struct LabelStatement label;
		struct GotoStatement jump;
		struct IfStatement branch;
		struct CallStatement call;
		struct StackStatement stack;
	};
};

struct ProcDecl
{
	struct Token name;
	struct Param* params;
	size_t param_count;
	size_t param_capacity;

	struct Statement* body;
	size_t body_count;
	size_t body_capacity;
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
void free_proc(struct ProcDecl* proc);
void add_param(struct ProcDecl* proc, struct Param param);
void add_statement(struct ProcDecl* proc, struct Statement statement);
void add_proc(struct Program* program, struct ProcDecl decl);

void free_expr(struct Expr* expr);
