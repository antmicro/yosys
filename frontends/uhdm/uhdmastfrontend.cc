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

namespace UHDM {
	extern void visit_object (vpiHandle obj_h, int indent, const char *relation, std::set<const BaseClass*>* visited, std::ostream& out);
}


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
		log("    read_uhdm [options] [filename]\n");
		log("\n");
		log("Load design from a UHDM file into the current design\n");
		log("\n");
		log("    -report [directory]\n");
		log("        write a coverage report for the UHDM file\n");
		log("\n");
	}
	void execute(std::istream *&f, std::string filename, std::vector<std::string> args, RTLIL::Design *design) YS_OVERRIDE
	{
		log_header(design, "Executing UHDM frontend.\n");

		UhdmAst parser;

		std::string report_directory;
		for (size_t i = 1; i < args.size(); i++) {
			if (args[i] == "-report" && ++i < args.size()) {
				report_directory = args[i];
				parser.stop_on_error = false;
			}
		}
		extra_args(f, filename, args, args.size() - 1);

		AST::current_filename = filename;
		AST::set_line_num = &set_line_num;
		AST::get_line_num = &get_line_num;
		struct AST::AstNode *current_ast;

		UHDM::Serializer serializer;

		std::vector<vpiHandle> restoredDesigns = serializer.Restore(filename);
		for (auto design : restoredDesigns) {
			UHDM::visit_object(design, 1, "", &parser.report.unhandled, std::cout);
		}
		current_ast = parser.visit_designs(restoredDesigns);
		if (report_directory != "") {
			parser.report.write(report_directory);
		}

		bool dump_ast1 = true;
		bool dump_ast2 = true;
		bool dont_redefine = false;
		bool default_nettype_wire = true;
		AST::process(design, current_ast,
			dump_ast1, dump_ast2, false, false, false, false, false, false, false, false,
			false, false, false, false, false, false, dont_redefine, false, false, default_nettype_wire
		);
	}
} UhdmAstFrontend;

YOSYS_NAMESPACE_END

