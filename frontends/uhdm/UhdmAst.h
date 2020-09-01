#ifndef _UHDM_AST_H_
#define _UHDM_AST_H_ 1

#include <vector>
#include "frontends/ast/ast.h"
#undef cover

#include "uhdm.h"
#include "astnodelist.h"
#include "uhdmastshared.h"

YOSYS_NAMESPACE_BEGIN

class UhdmAst {
	private:
		// Walks through one-to-many relationships from given parent
		// node through the VPI interface, visiting child nodes belonging to
		// ChildrenNodeTypes that are present in the given object.
		void visit_one_to_many(const std::vector<int> childrenNodeTypes,
				       vpiHandle parentHandle, AstNodeList parent,
				       const std::function<void(AST::AstNode*)>& f);

		// Walks through one-to-one relationships from given parent
		// node through the VPI interface, visiting child nodes belonging to
		// ChildrenNodeTypes that are present in the given object.
		void visit_one_to_one(const std::vector<int> childrenNodeTypes,
				      vpiHandle parentHandle, AstNodeList parent,
				      const std::function<void(AST::AstNode*)>& f);

		// Visit children of type vpiRange that belong to the given parent node.
		void visit_range(vpiHandle obj_h, AstNodeList parent,
				const std::function<void(AST::AstNode*)> &f);

		// Create an AstNode of the specified type with metadata extracted from
		// the given vpiHandle.
		AST::AstNode* make_ast_node(AST::AstNodeType type, vpiHandle obj_h);

		// Makes the passed node a cell node of the specified type
		void make_cell(vpiHandle obj_h, AST::AstNode* node, const std::string& type, AstNodeList& parent);

		// Adds a typedef node to the current node
		void add_typedef(AST::AstNode* current_node, AST::AstNode* type_node);

		// Reports that something went wrong with reading the UHDM file
		void report_error(const char *format, ...) const;

		// Data shared between all UhdmAst objects
		UhdmAstShared& shared;

		// Functions that handle specific types of nodes
		AST::AstNode* handle_design(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_parameter(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_port(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_module(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_struct_typespec(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_typespec_member(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_enum_typespec(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_enum_const(vpiHandle obj_h);
		AST::AstNode* handle_var(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_array_var(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_param_assign(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_cont_assign(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_assignment(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_net(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_array_net(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_package(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_interface(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_modport(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_io_decl(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_always(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_event_control(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_initial(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_begin(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_operation(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_bit_select(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_part_select(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_indexed_part_select(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_var_select(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_if_else(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_for(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_gen_scope_array(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_gen_scope(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_case(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_case_item(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_constant(vpiHandle obj_h);
		AST::AstNode* handle_range(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_return(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_function(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_logic_var(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_sys_func_call(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_func_call(vpiHandle obj_h, AstNodeList& parent);
		AST::AstNode* handle_task_call(vpiHandle obj_h, AstNodeList& parent);

		// Handles assignment patterns for which we didn't have the relevant type information before
		void resolve_assignment_pattern(AST::AstNode* module_node, AST::AstNode* wire_node);

		// Indentation used for debug printing
		std::string indent;

	public:
		UhdmAst(UhdmAstShared& s, const std::string& i = "") : shared(s), indent(i) {}

		// Visits single VPI object and creates proper AST node
		AST::AstNode* handle_object(vpiHandle obj_h, AstNodeList parent = AstNodeList());

		// Visits all VPI design objects and returns created ASTs
		AST::AstNode* visit_designs(const std::vector<vpiHandle>& designs);

};

YOSYS_NAMESPACE_END

#endif
