#ifndef _UHDM_AST_CONTEXT_H_
#define _UHDM_AST_CONTEXT_H_ 1

#include <string>
#include <unordered_map>
#include "kernel/yosys.h"

YOSYS_NAMESPACE_BEGIN

class UhdmAstContext {
	private:
		// Nodes added to this context
		std::unordered_map<std::string, AST::AstNode*> nodes;

		// Optional parent context (searched if this context does not contain a given node)
		UhdmAstContext* parent = nullptr;

	public:
		UhdmAstContext() {}

		UhdmAstContext(UhdmAstContext* p) {
			parent = p;
		}

		// Start iterator of the context
		auto begin() -> decltype(nodes.begin()) {
			return nodes.begin();
		}

		// End iterator of the context
		auto end() -> decltype(nodes.end()) {
			return nodes.end();
		}

		// Check if the context or its parent context contains a given node
		bool contains(const std::string& key) const {
			if (nodes.find(key) == parent->nodes.end()) {
				return parent && parent->contains(key);
			}
			return true;
		}
		
		// Get a node from the context or its parent context;
		// Creates a null pointer under given key if no node is found
		AST::AstNode*& operator[](const std::string& key) {
			auto it = nodes.find(key);
			if (it != nodes.end()) {
				return it->second;
			}
			if (parent && parent->contains(key)) {
				return (*parent)[key];
			}
			return nodes[key];
		}
};

YOSYS_NAMESPACE_END

#endif
