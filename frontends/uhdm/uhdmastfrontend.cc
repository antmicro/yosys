/*
 *  yosys -- Yosys Open SYnthesis Suite
 *
 *  Copyright (C) 2020 Antmicro

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
#include "uhdm.h"
#include "vpi_visitor.h"
#include "UhdmAst.h"


YOSYS_NAMESPACE_BEGIN

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

struct UhdmAstFrontend : public Frontend {
	UhdmAstFrontend() : Frontend("uhdm", "read UHDM file") { }
	void help() YS_OVERRIDE
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    read_uhdm [filename]\n");
		log("\n");
		log("Load design from a UHDM file into the current design\n");
		log("\n");
	}
	void execute(std::istream *&f, std::string filename, std::vector<std::string> args, RTLIL::Design *design) YS_OVERRIDE
	{
		log_header(design, "Executing UHDM frontend.\n");

		size_t argidx;
		for (argidx = 1; argidx < args.size(); argidx++) {
			break;
		}
		extra_args(f, filename, args, argidx);

		AST::current_filename = filename;
		AST::set_line_num = &set_line_num;
		AST::get_line_num = &get_line_num;
		struct AST::AstNode *current_ast;

		UHDM::Serializer serializer;

		std::vector<vpiHandle> restoredDesigns = serializer.Restore(filename);

		std::cout << UHDM::visit_designs(restoredDesigns) << std::endl;
		UhdmAst parser;

		current_ast = parser.visit_designs(restoredDesigns);

		AST::process(design, current_ast,
			false, false, false, false, false, false, false, false, false, false,
			false, false, false, false, false, false, false, false, false, false
		);
	}
} UhdmAstFrontend;

YOSYS_NAMESPACE_END

