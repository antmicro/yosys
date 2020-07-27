#ifndef _UHDM_AST_H_
#define _UHDM_AST_H_ 1

#include <vector>

#include "uhdm.h"
#include "frontends/ast/ast.h"
#include "uhdmastcontext.h"
#include "uhdmastreport.h"

YOSYS_NAMESPACE_BEGIN

class UhdmAst {

	private:

		// Walks through one-to-many relationships from given parent
		// node through the VPI interface, visiting child nodes belonging to
		// ChildrenNodeTypes that are present in the given object.
		void visit_one_to_many (const std::vector<int> childrenNodeTypes,
				vpiHandle parentHandle,
				std::set<const UHDM::BaseClass*> visited,
				UhdmAstContext& context,
				const std::function<void(AST::AstNode*)> &f);

		// Walks through one-to-one relationships from given parent
		// node through the VPI interface, visiting child nodes belonging to
		// ChildrenNodeTypes that are present in the given object.
		void visit_one_to_one (const std::vector<int> childrenNodeTypes,
				vpiHandle parentHandle,
				std::set<const UHDM::BaseClass*> visited,
				UhdmAstContext& context,
				const std::function<void(AST::AstNode*)> &f);

		// Visit children of type vpiRange that belong to the given parent node.
		void visit_range(vpiHandle obj_h,
				std::set<const UHDM::BaseClass*> visited,
				UhdmAstContext& context,
				const std::function<void(AST::AstNode*)> &f);

		// Visit a node of type vpiConstant.
		AST::AstNode* visit_constant(vpiHandle obj_h);

		// Makes the passed node a cell node of the specified type
		void make_cell(vpiHandle obj_h, AST::AstNode* node, const std::string& type,
			       std::set<const UHDM::BaseClass*> visited,
			       UhdmAstContext& context);

		// Adds a typedef node to the current node
		void add_typedef(AST::AstNode* current_node, AST::AstNode* type_node);

		// Reports that something went wrong with reading the AST
		void error(std::string message, unsigned object_type) const;
		void error(std::string message) const;

		unsigned enum_count = 0;

		// AST node of the module currently being processed
		AST::AstNode* current_module;

	public:
		UhdmAst(){};
		// Visits single VPI object and creates proper AST node
		AST::AstNode* visit_object (vpiHandle obj_h,
				std::set<const UHDM::BaseClass*> visited,
				UhdmAstContext& context);

		// Visits all VPI design objects and returns created ASTs
		AST::AstNode* visit_designs (const std::vector<vpiHandle>& designs);

		UhdmAstReport report;
		bool stop_on_error = true;
		bool debug_flag = false;
};
#endif	// Guard

YOSYS_NAMESPACE_END
