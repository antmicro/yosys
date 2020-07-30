#ifndef _AST_NODE_LIST_H_
#define _AST_NODE_LIST_H_ 1

#include <string>
#include <unordered_set>
#include "frontends/ast/ast.h"

YOSYS_NAMESPACE_BEGIN

// Provides UhdmAst objects with access to parent nodes in the generated AST
class AstNodeList {
	private:
		// Optional parent node (searched if this node does not contain a given node)
		AstNodeList* parent = nullptr;

	public:
		// AST node assigned to this node
		AST::AstNode* node;

		AstNodeList(AstNodeList* p = nullptr, AST::AstNode* n = nullptr) {
			parent = p;
			node = n;
		}

		// Go down the list to find a parent node of the specified type
		AST::AstNode* find(const std::unordered_set<AST::AstNodeType>& types) {
			auto searched_node = this;
			while (searched_node && searched_node->node) {
				if (types.find(searched_node->node->type) != types.end()) {
					return searched_node->node;
				}
				searched_node = searched_node->parent;
			}
			return nullptr;
		}
};

YOSYS_NAMESPACE_END

#endif
