/*
 *  yosys -- Yosys Open SYnthesis Suite
 *
 *  Copyright (C) 2019 Antmicro

 *  Based on frontends/json/jsonparse.cc
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "kernel/yosys.h"
#include "frontends/ast/ast.h"
#include "frontends/ast/json.hpp"

YOSYS_NAMESPACE_BEGIN

static void parseTree(nlohmann::json& json, struct AST::AstNode *current_ast)
{
	std::string type = json.find("type").value();

	if (type == "AST_TOP") {
		auto modules = json.find("nodes");
		for (auto itr = modules->begin() ; itr != modules->end() ; ++itr)
			parseTree(itr.value(), current_ast);
		return ;
	}

	// TODO: str2type?
	enum AST::AstNodeType ast_type = AST::AST_NONE;
	if (0) { do { } while (0); }
#define eitem(x) else if (type == #x) ast_type = AST::x;
	eitem(AST_INITIAL)
	eitem(AST_BLOCK)
	eitem(AST_ASSIGN_EQ)
	eitem(AST_ASSIGN_LE)
	eitem(AST_IDENTIFIER)
	eitem(AST_CONSTANT)
	eitem(AST_ALWAYS)
	eitem(AST_POSEDGE)
	eitem(AST_ASSIGN)
	eitem(AST_BIT_NOT)
	eitem(AST_WIRE)
	eitem(AST_MODULE)
#undef eitem
	else log("Unknown type: \'%s\'\n", type.c_str());

	AST::AstNode *node = new AST::AstNode(ast_type);

	auto name = json.find("name");
	if (name != json.end())
		node->str = std::string("\\" + std::string(name.value()));

	auto value = json.find("value");
	if (value != json.end())
		node->integer = value.value();

	auto port = json.find("port");
	if (port != json.end()) {
		std::string inout = port.value();
		if (inout == "input")
			node->is_input  = true;
		else
			node->is_output = true;
	}

	auto reg = json.find("reg");
	if (reg != json.end())
		node->is_reg = true;

	current_ast->children.push_back(node);

	auto nodes = json.find("nodes");
	if (nodes != json.end()) {
		for (auto itr = nodes->begin() ; itr != nodes->end() ; ++itr)
			parseTree(itr.value(), node);
	}
}

/* Stub for AST::process */
static void
set_line_num(int)
{
}

/* Stub for AST::process */
static int
get_line_num(void)
{
	return 1;
}

struct JsonAstFrontend : public Frontend {
	JsonAstFrontend() : Frontend("jsonast", "read JSON-AST file") { }
	void help() YS_OVERRIDE
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    read_jsonast [filename]\n");
		log("\n");
		log("Load design from a JSON-AST file into the current design\n");
		log("\n");
	}
	void execute(std::istream *&f, std::string filename, std::vector<std::string> args, RTLIL::Design *design) YS_OVERRIDE
	{
		log_header(design, "Executing JSON-AST frontend.\n");

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			// std::string arg = args[argidx];
			// if (arg == "-sop") {
			// 	sop_mode = true;
			// 	continue;
			// }
			break;
		}
		extra_args(f, filename, args, argidx);

		AST::current_filename = filename;
		AST::set_line_num = &set_line_num;
		AST::get_line_num = &get_line_num;
		struct AST::AstNode *current_ast;
		current_ast = new AST::AstNode(AST::AST_DESIGN);

		std::ifstream filein(filename);
		std::stringstream ss;
		ss << filein.rdbuf();
		nlohmann::json json = nlohmann::json::parse(ss.str());
		parseTree(json, current_ast);

		AST::process(design, current_ast,
			false, false, false, false, false, false, false, false, false, false,
			false, false, false, false, false, false, false, false, false, false
		);
	}
} JsonAstFrontend;

YOSYS_NAMESPACE_END

