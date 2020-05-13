#ifndef _UHDM_AST_H_
#define _UHDM_AST_H_ 1

#include <vector>

#include "uhdm.h"
#include "frontends/ast/ast.h"

YOSYS_NAMESPACE_BEGIN

class UhdmAst {

	private:

		// Walks through one-to-many relationships from given parent
		// node through the VPI interface, visiting child nodes belonging to
		// ChildrenNodeTypes that are present in the given object.
		void visit_one_to_many (const std::vector<int> childrenNodeTypes,
				vpiHandle parentHandle,
				std::set<const UHDM::BaseClass*> visited,
				std::map<std::string, AST::AstNode*>* top_nodes,
				const std::function<void(AST::AstNode*)> &f);

		// Walks through one-to-one relationships from given parent
		// node through the VPI interface, visiting child nodes belonging to
		// ChildrenNodeTypes that are present in the given object.
		void visit_one_to_one (const std::vector<int> childrenNodeTypes,
				vpiHandle parentHandle,
				std::set<const UHDM::BaseClass*> visited,
				std::map<std::string, AST::AstNode*>* top_nodes,
				const std::function<void(AST::AstNode*)> &f);

		// Makes the passed node a cell node of the specified type
		void make_cell(vpiHandle obj_h, AST::AstNode* node, const std::string& type);

	public:
		UhdmAst(){};
		// Visits single VPI object and creates proper AST node
		AST::AstNode* visit_object (vpiHandle obj_h,
				std::set<const UHDM::BaseClass*> visited,
				std::map<std::string, AST::AstNode*>* top_nodes);

		// Visits all VPI design objects and returns created ASTs
		AST::AstNode* visit_designs (const std::vector<vpiHandle>& designs);
};
#endif	// Guard

YOSYS_NAMESPACE_END
