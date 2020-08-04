#include <vector>
#include <functional>
#include <algorithm>

#include "headers/uhdm.h"
#include "frontends/ast/ast.h"
#include "frontends/verilog/verilog_frontend.h"
#include "UhdmAst.h"
#include "vpi_user.h"

YOSYS_NAMESPACE_BEGIN

static void sanitize_symbol_name(std::string &name) {
	if (!name.empty()) {
		// symbol names must begin with '\'
		name.insert(0, "\\");
		std::replace(name.begin(), name.end(), '@','_');
	}
}

void UhdmAst::visit_one_to_many(const std::vector<int> childrenNodeTypes,
								vpiHandle parentHandle, AstNodeList parent,
								const std::function<void(AST::AstNode*)>& f) {
	for (auto child : childrenNodeTypes) {
		vpiHandle itr = vpi_iterate(child, parentHandle);
		while (vpiHandle vpi_child_obj = vpi_scan(itr) ) {
			UhdmAst uhdm_ast(shared);
			auto *childNode = uhdm_ast.visit_object(vpi_child_obj, parent);
			f(childNode);
			vpi_free_object(vpi_child_obj);
		}
		vpi_free_object(itr);
	}
}

void UhdmAst::visit_one_to_one(const std::vector<int> childrenNodeTypes,
							   vpiHandle parentHandle, AstNodeList parent,
							   const std::function<void(AST::AstNode*)>& f) {
	for (auto child : childrenNodeTypes) {
		vpiHandle itr = vpi_handle(child, parentHandle);
		if (itr) {
			UhdmAst uhdm_ast(shared);
			auto *childNode = uhdm_ast.visit_object(itr, parent);
			f(childNode);
		}
		vpi_free_object(itr);
	}
}

void UhdmAst::visit_range(vpiHandle obj_h, AstNodeList parent,
						  const std::function<void(AST::AstNode*)>& f)  {
	std::vector<AST::AstNode*> range_nodes;
	visit_one_to_many({vpiRange},
					  obj_h, parent,
					  [&](AST::AstNode* node) {
						  range_nodes.push_back(node);
					  });
	if (range_nodes.size() > 1) {
		auto multirange_node = new AST::AstNode(AST::AST_MULTIRANGE);
		multirange_node->children = range_nodes;
		f(multirange_node);
	} else if (!range_nodes.empty()) {
		f(range_nodes[0]);
	}
}

AST::AstNode* UhdmAst::make_ast_node(AST::AstNodeType type, vpiHandle obj_h) {
	auto node = new AST::AstNode(type);
	if (auto name = vpi_get_str(vpiName, obj_h)) {
		node->str = name;
	} else if (auto name = vpi_get_str(vpiDefName, obj_h)) {
		node->str = name;
	}
	sanitize_symbol_name(node->str);
	if (auto filename = vpi_get_str(vpiFile, obj_h)) {
		node->filename = filename;
	}
	if (unsigned int line = vpi_get(vpiLineNo, obj_h)) {
		node->location.first_line = node->location.last_line = line;
	}
	return node;
}

void UhdmAst::make_cell(vpiHandle obj_h, AST::AstNode* current_node, const std::string& type, AstNodeList& parent) {
	current_node->type = AST::AST_CELL;
	auto typeNode = new AST::AstNode(AST::AST_CELLTYPE);
	typeNode->str = type;
	current_node->children.push_back(typeNode);
	// Add port connections as arguments
	vpiHandle port_itr = vpi_iterate(vpiPort, obj_h);
	while (vpiHandle port_h = vpi_scan(port_itr) ) {
		std::string arg_name;
		if (auto s = vpi_get_str(vpiName, port_h)) {
			arg_name = s;
			sanitize_symbol_name(arg_name);
		}
		auto arg_node = new AST::AstNode(AST::AST_ARGUMENT);
		arg_node->str = arg_name;
		arg_node->filename = current_node->filename;
		arg_node->location = current_node->location;
		visit_one_to_one({vpiHighConn},
						 port_h, parent,
						 [&](AST::AstNode* node) {
						 	if (node) {
						 		arg_node->children.push_back(node);
						 	}
						 });
		current_node->children.push_back(arg_node);
		shared.report.mark_handled(port_h);
		vpi_free_object(port_h);
	}
	vpi_free_object(port_itr);
}

void UhdmAst::add_typedef(AST::AstNode* current_node, AST::AstNode* type_node) {
	auto typedef_node = new AST::AstNode(AST::AST_TYPEDEF);
	typedef_node->location = type_node->location;
	typedef_node->filename = type_node->filename;
	typedef_node->str = type_node->str;
	if (type_node->type == AST::AST_STRUCT) {
		type_node->str.clear();
		typedef_node->children.push_back(type_node);
		current_node->children.push_back(typedef_node);
	} else if (type_node->type == AST::AST_ENUM) {
		type_node = type_node->clone();
		type_node->str = "$enum" + std::to_string(shared.next_enum_id());
		for (auto* enum_item : type_node->children) {
			enum_item->attributes["\\enum_base_type"] = AST::AstNode::mkconst_str(type_node->str);
		}
		auto wire_node = new AST::AstNode(AST::AST_WIRE);
		wire_node->attributes["\\enum_type"] = AST::AstNode::mkconst_str(type_node->str);
		typedef_node->children.push_back(wire_node);
		current_node->children.push_back(type_node);
		current_node->children.push_back(typedef_node);
	}
}

static void add_or_replace_child(AST::AstNode* parent, AST::AstNode* child) {
	if (!child->str.empty()) {
		auto it = std::find_if(parent->children.begin(),
							   parent->children.end(),
							   [child](AST::AstNode* existing_child) {
								   return existing_child->str == child->str;
							   });
		if (it != parent->children.end()) {
			*it = child;
			return;
		}
	}
	parent->children.push_back(child);
}

AST::AstNode* UhdmAst::handle_design(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_DESIGN, obj_h);
	visit_one_to_many({UHDM::uhdmallInterfaces,
					   UHDM::uhdmallModules,
					   UHDM::uhdmallPackages,
					   UHDM::uhdmtopModules},
					  obj_h, {&parent, current_node},
					  [&](AST::AstNode* node) {
						  if (node) {
							  shared.top_nodes[node->str] = node;
						  }
					  });
	// Once we walked everything, unroll that as children of this node
	for (auto pair : shared.top_nodes) {
		current_node->children.push_back(pair.second);
	}
	return current_node;
}

AST::AstNode* UhdmAst::handle_parameter(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_PARAMETER, obj_h);
	vpiHandle typespec_h = vpi_handle(vpiTypespec, obj_h);
	if (typespec_h) {
		int typespec_type = vpi_get(vpiType, typespec_h);
		switch (typespec_type) {
			case vpiBitTypespec:
			case vpiLogicTypespec: {
				current_node->is_logic = true;
				visit_range(typespec_h, parent,
							[&](AST::AstNode* node) {
								current_node->children.push_back(node);
							});
				shared.report.mark_handled(typespec_h);
				break;
			}
			case vpiEnumTypespec:
			case vpiIntTypespec: {
				shared.report.mark_handled(typespec_h);
				break;
			}
			default: {
				error("Encountered unhandled typespec: %d", typespec_type);
			}
		}
	} else {
		AST::AstNode* constant_node = handle_constant(obj_h);
		if (constant_node) {
			constant_node->filename = current_node->filename;
			constant_node->location = current_node->location;
			current_node->children.push_back(constant_node);
		}
	}
	return current_node;
}

AST::AstNode* UhdmAst::handle_port(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_WIRE, obj_h);
	current_node->port_id = shared.next_port_id();
	vpiHandle lowConn_h = vpi_handle(vpiLowConn, obj_h);
	if (lowConn_h) {
		vpiHandle actual_h = vpi_handle(vpiActual, lowConn_h);
		auto actual_type = vpi_get(vpiType, actual_h);
		switch (actual_type) {
		case vpiModport: {
				vpiHandle iface_h = vpi_handle(vpiInterface, actual_h);
				if (iface_h) {
					std::string cellName, ifaceName;
					if (auto s = vpi_get_str(vpiName, actual_h)) {
						cellName = s;
						sanitize_symbol_name(cellName);
					}
					if (auto s = vpi_get_str(vpiDefName, iface_h)) {
						ifaceName = s;
						sanitize_symbol_name(ifaceName);
					}
					current_node->type = AST::AST_INTERFACEPORT;
					auto typeNode = new AST::AstNode(AST::AST_INTERFACEPORTTYPE);
					// Skip '\' in cellName
					typeNode->str = ifaceName + '.' + cellName.substr(1, cellName.length());
					current_node->children.push_back(typeNode);
					shared.report.mark_handled(actual_h);
					shared.report.mark_handled(iface_h);
				}
				break;
			}
			case vpiInterface: {
				auto typeNode = new AST::AstNode(AST::AST_INTERFACEPORTTYPE);
				if (auto s = vpi_get_str(vpiDefName, actual_h)) {
					typeNode->str = s;
					sanitize_symbol_name(typeNode->str);
				}
				current_node->type = AST::AST_INTERFACEPORT;
				current_node->children.push_back(typeNode);
				shared.report.mark_handled(actual_h);
				break;
			}
			case vpiLogicNet: {
				visit_range(actual_h, parent,
							[&](AST::AstNode* node) {
								current_node->children.push_back(node);
							});
				shared.report.mark_handled(actual_h);
			}
		}
		shared.report.mark_handled(lowConn_h);
	}
	if (const int n = vpi_get(vpiDirection, obj_h)) {
		if (n == vpiInput) {
			current_node->is_input = true;
		} else if (n == vpiOutput) {
			current_node->is_output = true;
			current_node->is_reg = true;
		} else if (n == vpiInout) {
			current_node->is_input = true;
			current_node->is_output = true;
		}
	}
	return current_node;
}

AST::AstNode* UhdmAst::handle_module(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_NONE, obj_h);
	std::string type = vpi_get_str(vpiDefName, obj_h);
	sanitize_symbol_name(type);
	if (current_node->str == type) {
		if (shared.top_nodes.find(type) != shared.top_nodes.end()) {
			delete current_node;
			current_node = shared.top_nodes[type];
			visit_one_to_many({vpiModule,
							   vpiInterface,
							   vpiParameter,
							   vpiParamAssign,
							   vpiNet,
							   vpiArrayNet,
							   vpiPort,
							   vpiGenScopeArray,
							   vpiVariables},
							  obj_h, {&parent, current_node},
							  [&](AST::AstNode* node) {
								  if (node) {
									  add_or_replace_child(current_node, node);
									  resolve_assignment_pattern(current_node, node);
								  }
							  });
		} else {
			current_node->type = AST::AST_MODULE;
			current_node->str = type;
			shared.top_nodes[current_node->str] = current_node;
			visit_one_to_many({vpiTypedef},
							  obj_h, {&parent, current_node},
							  [&](AST::AstNode* node) {
								  if (node) {
									  add_typedef(current_node, node);
								  }
							  });
			visit_one_to_many({vpiModule,
							   vpiInterface,
							   vpiParameter,
							   vpiParamAssign,
							   vpiNet,
							   vpiArrayNet,
							   vpiPort,
							   vpiGenScopeArray,
							   vpiContAssign,
							   vpiProcess,
							   vpiTaskFunc},
							  obj_h, {&parent, current_node},
							  [&](AST::AstNode* node) {
								  add_or_replace_child(current_node, node);
							  });
		}
	} else {
		// Not a top module, create instance
		bool cloned = false;
		auto module_node = shared.top_nodes[type];
		if (!module_node) {
			module_node = new AST::AstNode(AST::AST_MODULE);
			module_node->str = type;
			shared.top_nodes[module_node->str] = module_node;
		}
		visit_one_to_many({vpiVariables},
						  obj_h, {&parent, module_node},
						  [&](AST::AstNode* node) {
							  if (node) {
								  add_or_replace_child(module_node, node);
								  resolve_assignment_pattern(current_node, node);
							  }
						  });
		visit_one_to_many({vpiParameter},
						  obj_h, {&parent, module_node},
						  [&](AST::AstNode* node) {
							  if (node) {
								  if (!cloned) {
									  module_node = module_node->clone();
									  module_node->str += '$' + current_node->str;
									  shared.top_nodes[module_node->str] = module_node;
									  cloned = true;
								  }
								  add_or_replace_child(module_node, node);
							  }
						  });
		visit_one_to_many({vpiInterface,
						   vpiModule,
						   vpiNet,
						   vpiNetArray,
						   vpiPort,
						   vpiGenScopeArray},
						  obj_h, {&parent, module_node},
						  [&](AST::AstNode* node) {
							  if (node) {
								  add_or_replace_child(module_node, node);
							  }
						  });
		make_cell(obj_h, current_node, type, parent);
	}
	return current_node;
}

AST::AstNode* UhdmAst::handle_struct_typespec(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_STRUCT, obj_h);
	visit_one_to_many({vpiTypespecMember},
					  obj_h, {&parent, current_node},
					  [&](AST::AstNode* node) {
						  current_node->children.push_back(node);
					  });
	return current_node;
}

AST::AstNode* UhdmAst::handle_typespec_member(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_STRUCT_ITEM, obj_h);
	current_node->str = current_node->str.substr(1);
	vpiHandle typespec_h = vpi_handle(vpiTypespec, obj_h);
	int typespec_type = vpi_get(vpiType, typespec_h);
	switch (typespec_type) {
		case vpiStructTypespec: {
			auto struct_node = visit_object(typespec_h, parent);
			struct_node->str = current_node->str;
			delete current_node;
			current_node = struct_node;
			shared.report.mark_handled(typespec_h);
			break;
		}
		case vpiEnumTypespec: {
			current_node->children.push_back(visit_object(typespec_h, parent));
			shared.report.mark_handled(typespec_h);
			break;
		}
		case vpiBitTypespec:
		case vpiLogicTypespec: {
			current_node->is_logic = true;
			visit_range(typespec_h, parent,
						[&](AST::AstNode* node) {
							current_node->children.push_back(node);
						});
			shared.report.mark_handled(typespec_h);
			break;
		}
		default: {
			error("Encountered unhandled typespec: %d", typespec_type);
			break;
		}
	}
	return current_node;
}

AST::AstNode* UhdmAst::handle_enum_typespec(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_ENUM, obj_h);
	visit_one_to_many({vpiEnumConst},
					  obj_h, {&parent, current_node},
					  [&](AST::AstNode* node) {
						  current_node->children.push_back(node);
					  });
	vpiHandle typespec_h = vpi_handle(vpiBaseTypespec, obj_h);
	int typespec_type = vpi_get(vpiType, typespec_h);
	switch (typespec_type) {
		case vpiLogicTypespec: {
			current_node->is_logic = true;
			visit_range(typespec_h, parent,
						[&](AST::AstNode* node) {
							for (auto child : current_node->children) {
								child->children.push_back(node->clone());
							}
							delete node;
						});
			shared.report.mark_handled(typespec_h);
			break;
		}
		case vpiIntTypespec: {
			current_node->is_signed = true;
			shared.report.mark_handled(typespec_h);
			break;
		}
		default: {
			error("Encountered unhandled typespec: %d", typespec_type);
			break;
		}
	}
	return current_node;
}

AST::AstNode* UhdmAst::handle_enum_const(vpiHandle obj_h) {
	auto current_node = make_ast_node(AST::AST_ENUM_ITEM, obj_h);
	AST::AstNode* constant_node = handle_constant(obj_h);
	if (constant_node) {
		constant_node->filename = current_node->filename;
		constant_node->location = current_node->location;
		current_node->children.push_back(constant_node);
	}
	return current_node;
}

AST::AstNode* UhdmAst::handle_var(vpiHandle obj_h) {
	auto current_node = make_ast_node(AST::AST_WIRE, obj_h);
	vpiHandle typespec_h = vpi_handle(vpiTypespec, obj_h);
	std::string name = vpi_get_str(vpiName, typespec_h);
	sanitize_symbol_name(name);
	auto wiretype_node = new AST::AstNode(AST::AST_WIRETYPE);
	wiretype_node->str = name;
	current_node->is_custom_type = true;
	current_node->children.push_back(wiretype_node);
	return current_node;
}

AST::AstNode* UhdmAst::handle_array_var(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_MEMORY, obj_h);
	vpiHandle itr = vpi_iterate(vpiReg, obj_h);
	while (vpiHandle reg_h = vpi_scan(itr)) {
		if (vpi_get(vpiType, reg_h) == vpiStructVar) {
			vpiHandle typespec_h = vpi_handle(vpiTypespec, reg_h);
			std::string name = vpi_get_str(vpiName, typespec_h);
			sanitize_symbol_name(name);
			auto wiretype_node = new AST::AstNode(AST::AST_WIRETYPE);
			wiretype_node->str = name;
			current_node->children.push_back(wiretype_node);
			shared.report.mark_handled(reg_h);
			shared.report.mark_handled(typespec_h);
		}
		vpi_free_object(reg_h);
	}
	vpi_free_object(itr);
	visit_range(obj_h, {&parent, current_node},
				[&](AST::AstNode* node) {
					current_node->children.push_back(node);
				});
	return current_node;
}

AST::AstNode* UhdmAst::handle_param_assign(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_PARAMETER, obj_h);
	visit_one_to_one({vpiLhs,
					  vpiRhs},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 if (node) {
							 if (node->type == AST::AST_PARAMETER) {
								 current_node->str = node->str;
							 } else {
								 current_node->children.push_back(node);
							 }
						 }
					 });
	return current_node;
}

AST::AstNode* UhdmAst::handle_cont_assign(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_ASSIGN, obj_h);
	visit_one_to_one({vpiLhs,
					  vpiRhs},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 if (node) {
							 current_node->children.push_back(node);
						 }
					 });
	return current_node;
}

AST::AstNode* UhdmAst::handle_assignment(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_ASSIGN_EQ, obj_h);
	visit_one_to_one({vpiLhs,
					  vpiRhs},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 if (node) {
							 current_node->children.push_back(node);
						 }
					 });
	return current_node;
}

AST::AstNode* UhdmAst::handle_net(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_WIRE, obj_h);
	auto net_type = vpi_get(vpiNetType, obj_h);
	current_node->is_reg = net_type == vpiReg;
	current_node->is_output = net_type == vpiOutput;
	visit_range(obj_h, {&parent, current_node},
				[&](AST::AstNode* node) {
					current_node->children.push_back(node);
				});
	return current_node;
}

AST::AstNode* UhdmAst::handle_array_net(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_MEMORY, obj_h);
	vpiHandle itr = vpi_iterate(vpiNet, obj_h);
	while (vpiHandle net_h = vpi_scan(itr)) {
		if (vpi_get(vpiType, net_h) == vpiLogicNet) {
			visit_range(net_h, parent,
						[&](AST::AstNode* node) {
							current_node->children.push_back(node);
						});
			shared.report.mark_handled(net_h);
		}
		vpi_free_object(net_h);
	}
	vpi_free_object(itr);
	visit_range(obj_h, {&parent, current_node},
				[&](AST::AstNode* node) {
					current_node->children.push_back(node);
				});
	return current_node;
}

AST::AstNode* UhdmAst::handle_package(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_PACKAGE, obj_h);
	visit_one_to_many({vpiParameter,
					   vpiParamAssign},
					  obj_h, {&parent, current_node},
					  [&](AST::AstNode* node) {
						  if (node) {
							  add_or_replace_child(current_node, node);
						  }
					  });
	visit_one_to_many({vpiTypedef},
					  obj_h, {&parent, current_node},
					  [&](AST::AstNode* node) {
						  if (node) {
							  add_typedef(current_node, node);
						  }
					  });
	visit_one_to_many({vpiTaskFunc},
					  obj_h, {&parent, current_node},
					  [&](AST::AstNode* node) {
						  if (node) {
							  current_node->children.push_back(node);
						  }
					  });
	return current_node;
}

AST::AstNode* UhdmAst::handle_interface(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_NONE, obj_h);
	std::string type = vpi_get_str(vpiDefName, obj_h);
	sanitize_symbol_name(type);
	AST::AstNode* elaboratedInterface;
	// Check if we have encountered this object before
	if (shared.top_nodes.find(type) != shared.top_nodes.end()) {
		// Was created before, fill missing
		elaboratedInterface = shared.top_nodes[type];
		visit_one_to_many({vpiPort},
						  obj_h, {&parent, current_node},
						  [&](AST::AstNode* node) {
							  if (node) {
								  add_or_replace_child(elaboratedInterface, node);
							  }
						  });
	} else {
		// Encountered for the first time
		elaboratedInterface = new AST::AstNode(AST::AST_INTERFACE);
		elaboratedInterface->str = current_node->str;
		visit_one_to_many({vpiNet,
						   vpiPort,
						   vpiModport},
						   obj_h, {&parent, current_node},
						   [&](AST::AstNode* node) {
							   if (node) {
								   add_or_replace_child(elaboratedInterface, node);
							   }
						   });
	}
	shared.top_nodes[elaboratedInterface->str] = elaboratedInterface;
	if (current_node->str != type) {
		// Not a top module, create instance
		make_cell(obj_h, current_node, type, parent);
	} else {
		delete current_node;
		current_node = elaboratedInterface;
	}
	return current_node;
}

AST::AstNode* UhdmAst::handle_modport(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_MODPORT, obj_h);
	visit_one_to_many({vpiIODecl},
					  obj_h, {&parent, current_node},
					  [&](AST::AstNode* node) {
						  if (node) {
							  current_node->children.push_back(node);
						  }
					  });
	return current_node;
}

AST::AstNode* UhdmAst::handle_io_decl(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_MODPORTMEMBER, obj_h);
	if (const int n = vpi_get(vpiDirection, obj_h)) {
		if (n == vpiInput) {
			current_node->is_input = true;
		} else if (n == vpiOutput) {
			current_node->is_output = true;
		} else if (n == vpiInout) {
			current_node->is_input = true;
			current_node->is_output = true;
		}
	}
	visit_range(obj_h, {&parent, current_node},
				[&](AST::AstNode* node) {
					current_node->children.push_back(node);
				});
	visit_one_to_one({vpiExpr},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 delete current_node;
						 current_node = node;
					 });
	return current_node;
}

AST::AstNode* UhdmAst::handle_always(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_ALWAYS, obj_h);
	switch (vpi_get(vpiAlwaysType, obj_h)) {
		case vpiAlwaysComb:
		case vpiAlwaysFF: {
			visit_one_to_one({vpiStmt},
							 obj_h, {&parent, current_node},
							 [&](AST::AstNode* node) {
								 current_node->children.push_back(node);
							 });
			break;
		}
		default: {
			visit_one_to_one({vpiStmt},
							 obj_h, {&parent, current_node},
							 [&](AST::AstNode* node) {
								 if (node) {
									 current_node->children.push_back(node);
								 }
							 });
			break;
		}
	}
	return current_node;
}

AST::AstNode* UhdmAst::handle_event_control(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_BLOCK, obj_h);
	visit_one_to_one({vpiCondition},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 if (node) {
							 auto process_node = parent.find({AST::AST_ALWAYS});
							 process_node->children.push_back(node);
						 }
						 // is added inside vpiOperation
					 });
	visit_one_to_one({vpiStmt},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 if (node) {
							 current_node->children.push_back(node);
						 }
					 });
	return current_node;
}

AST::AstNode* UhdmAst::handle_initial(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_INITIAL, obj_h);
	visit_one_to_one({vpiStmt},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 if (node) {
							 current_node->children.push_back(node);
						 }
					 });
	return current_node;
}

AST::AstNode* UhdmAst::handle_begin(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_BLOCK, obj_h);
	visit_one_to_many({vpiStmt},
					  obj_h, {&parent, current_node},
					  [&](AST::AstNode* node) {
						  if (node) {
							  if (node->type == AST::AST_ASSIGN_EQ && node->children.size() == 1) {
								  auto func_node = parent.find({AST::AST_FUNCTION, AST::AST_TASK});
								  if (!func_node) return;
								  auto wire_node = new AST::AstNode(AST::AST_WIRE);
								  wire_node->type = AST::AST_WIRE;
								  wire_node->str = node->children[0]->str;
								  func_node->children.push_back(wire_node);
								  delete node;
							  } else {
								  current_node->children.push_back(node);
							  }
						  }
					  });
	return current_node;
}

AST::AstNode* UhdmAst::handle_operation(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_NONE, obj_h);
	auto operation = vpi_get(vpiOpType, obj_h);
	switch (operation) {
		case vpiListOp:
		case vpiEventOrOp: {
			// Add all operands as children of process node
			if (auto process_node = parent.find({AST::AST_ALWAYS})) {
				visit_one_to_many({vpiOperand},
								  obj_h, {&parent, current_node},
								  [&](AST::AstNode* node) {
									  // add directly to process node
									  if (node) {
										  process_node->children.push_back(node);
									  }
								  });
			}
			// Do not return a node
			// Parent should not use returned value
			shared.report.mark_handled(obj_h);
			delete current_node;
			return nullptr;
		}
		case vpiCastOp: {
			visit_one_to_many({vpiOperand},
							  obj_h, {&parent, current_node},
							  [&](AST::AstNode* node) {
								  delete current_node;
								  current_node = node;
							  });
			vpiHandle typespec_h = vpi_handle(vpiTypespec, obj_h);
			shared.report.mark_handled(typespec_h);
			vpi_free_object(typespec_h);
			break;
		}
		case vpiInsideOp: {
			current_node->type = AST::AST_EQ;
			AST::AstNode* lhs = nullptr;
			visit_one_to_many({vpiOperand},
							  obj_h, {&parent, current_node},
							  [&](AST::AstNode* node) {
								  if (!lhs) {
									  lhs = node;
								  }
								  if (current_node->children.size() < 2) {
									  current_node->children.push_back(node);
								  } else {
									  auto or_node = new AST::AstNode(AST::AST_LOGIC_OR);
									  or_node->filename = current_node->filename;
									  or_node->location = current_node->location;
									  auto eq_node = new AST::AstNode(AST::AST_EQ);
									  eq_node->filename = current_node->filename;
									  eq_node->location = current_node->location;
									  or_node->children.push_back(current_node);
									  or_node->children.push_back(eq_node);
									  eq_node->children.push_back(lhs->clone());
									  eq_node->children.push_back(node);
									  current_node = or_node;
								  }
							  });
			break;
		}
		default: {
			visit_one_to_many({vpiOperand},
							  obj_h, {&parent, current_node},
							  [&](AST::AstNode* node) {
								  current_node->children.push_back(node);
							  });
			switch(operation) {
				case vpiMinusOp: current_node->type = AST::AST_NEG; break;
				case vpiPlusOp: current_node->type = AST::AST_POS; break;
				case vpiPosedgeOp: current_node->type = AST::AST_POSEDGE; break;
				case vpiNegedgeOp: current_node->type = AST::AST_NEGEDGE; break;
				case vpiUnaryAndOp: current_node->type = AST::AST_REDUCE_AND; break;
				case vpiUnaryOrOp: current_node->type = AST::AST_REDUCE_OR; break;
				case vpiUnaryXorOp: current_node->type = AST::AST_REDUCE_XOR; break;
				case vpiUnaryXNorOp: current_node->type = AST::AST_REDUCE_XNOR; break;
				case vpiUnaryNandOp: {
					current_node->type = AST::AST_REDUCE_AND;
					auto not_node = new AST::AstNode(AST::AST_BIT_NOT, current_node);
					current_node = not_node;
					break;
				}
				case vpiUnaryNorOp: {
					current_node->type = AST::AST_REDUCE_OR;
					auto not_node = new AST::AstNode(AST::AST_BIT_NOT, current_node);
					current_node = not_node;
					break;
				}
				case vpiBitNegOp: current_node->type = AST::AST_BIT_NOT; break;
				case vpiBitAndOp: current_node->type = AST::AST_BIT_AND; break;
				case vpiBitOrOp: current_node->type = AST::AST_BIT_OR; break;
				case vpiBitXorOp: current_node->type = AST::AST_BIT_XOR; break;
				case vpiBitXnorOp: current_node->type = AST::AST_BIT_XNOR; break;
				case vpiLShiftOp: current_node->type = AST::AST_SHIFT_LEFT; break;
				case vpiRShiftOp: current_node->type = AST::AST_SHIFT_RIGHT; break;
				case vpiNotOp: current_node->type = AST::AST_LOGIC_NOT; break;
				case vpiLogAndOp: current_node->type = AST::AST_LOGIC_AND; break;
				case vpiLogOrOp: current_node->type = AST::AST_LOGIC_OR; break;
				case vpiEqOp: current_node->type = AST::AST_EQ; break;
				case vpiNeqOp: current_node->type = AST::AST_NE; break;
				case vpiGtOp: current_node->type = AST::AST_GT; break;
				case vpiGeOp: current_node->type = AST::AST_GE; break;
				case vpiLtOp: current_node->type = AST::AST_LT; break;
				case vpiLeOp: current_node->type = AST::AST_LE; break;
				case vpiSubOp: current_node->type = AST::AST_SUB; break;
				case vpiAddOp: current_node->type = AST::AST_ADD; break;
				case vpiMultOp: current_node->type = AST::AST_MUL; break;
				case vpiDivOp: current_node->type = AST::AST_DIV; break;
				case vpiModOp: current_node->type = AST::AST_MOD; break;
				case vpiArithLShiftOp: current_node->type = AST::AST_SHIFT_SLEFT; break;
				case vpiArithRShiftOp: current_node->type = AST::AST_SHIFT_SRIGHT; break;
				case vpiPowerOp: current_node->type = AST::AST_POW; break;
				case vpiPostIncOp: // TODO: Make this an actual post-increment op (currently it's a pre-increment)
				case vpiPreIncOp: {
					current_node->type = AST::AST_ASSIGN_EQ;
					auto id = current_node->children[0]->clone();
					auto add_node = new AST::AstNode(AST::AST_ADD, id, AST::AstNode::mkconst_int(1, true));
					add_node->filename = current_node->filename;
					add_node->location = current_node->location;
					current_node->children.push_back(add_node);
					break;
				}
				case vpiPostDecOp: // TODO: Make this an actual post-decrement op (currently it's a pre-decrement)
				case vpiPreDecOp: {
					current_node->type = AST::AST_ASSIGN_EQ;
					auto id = current_node->children[0]->clone();
					auto add_node = new AST::AstNode(AST::AST_SUB, id, AST::AstNode::mkconst_int(1, true));
					add_node->filename = current_node->filename;
					add_node->location = current_node->location;
					current_node->children.push_back(add_node);
					break;
				}
				case vpiConditionOp: current_node->type = AST::AST_TERNARY; break;
				case vpiAssignmentPatternOp: {
					auto assign_node = parent.find({AST::AST_ASSIGN, AST::AST_ASSIGN_EQ});
					if (assign_node) {
						auto lhs_node = assign_node->children[0];
						auto module_node = parent.find({AST::AST_MODULE});
						shared.unresolved_assignment_patterns[module_node->str][lhs_node->str].push_back(assign_node);
					}
					current_node->type = AST::AST_CONCAT;
					std::reverse(current_node->children.begin(), current_node->children.end());
					break;
				}
				case vpiConcatOp: {
					current_node->type = AST::AST_CONCAT;
					std::reverse(current_node->children.begin(), current_node->children.end());
					break;
				}
				case vpiMultiConcatOp: current_node->type = AST::AST_REPLICATE; break;
				case vpiAssignmentOp: current_node->type = AST::AST_ASSIGN_EQ; break;
				default: {
					error("Encountered unhandled operation: %d", operation);
				}
			}
			break;
		}
	}
	return current_node;
}

AST::AstNode* UhdmAst::handle_bit_select(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_IDENTIFIER, obj_h);
	visit_one_to_one({vpiIndex},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 auto range_node = new AST::AstNode(AST::AST_RANGE, node);
						 range_node->filename = current_node->filename;
						 range_node->location = current_node->location;
						 current_node->children.push_back(range_node);
					 });
	return current_node;
}

AST::AstNode* UhdmAst::handle_part_select(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_IDENTIFIER, obj_h);
	visit_one_to_one({vpiParent},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 current_node->str = node->str;
						 delete node;
					 });
	auto range_node = new AST::AstNode(AST::AST_RANGE);
	range_node->filename = current_node->filename;
	range_node->location = current_node->location;
	visit_one_to_one({vpiLeftRange,
					  vpiRightRange},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 range_node->children.push_back(node);
					 });
	current_node->children.push_back(range_node);
	return current_node;
}

AST::AstNode* UhdmAst::handle_indexed_part_select(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_IDENTIFIER, obj_h);
	visit_one_to_one({vpiParent},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 current_node->str = node->str;
						 delete node;
					 });
	auto range_node = new AST::AstNode(AST::AST_RANGE);
	range_node->filename = current_node->filename;
	range_node->location = current_node->location;
	visit_one_to_one({vpiBaseExpr},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 range_node->children.push_back(node);
					 });
	visit_one_to_one({vpiWidthExpr},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 auto right_range_node = new AST::AstNode(AST::AST_ADD);
						 right_range_node->children.push_back(range_node->children[0]->clone());
						 right_range_node->children.push_back(node);
						 range_node->children.push_back(right_range_node);
					 });
	current_node->children.push_back(range_node);
	return current_node;
}

AST::AstNode* UhdmAst::handle_var_select(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_IDENTIFIER, obj_h);
	visit_one_to_many({vpiIndex},
					  obj_h, {&parent, current_node},
					  [&](AST::AstNode* node) {
						  if (node->type == AST::AST_IDENTIFIER) {
							  for (auto child : node->children) {
								  current_node->children.push_back(child->clone());
							  }
							  delete node;
						  } else if (node->type == AST::AST_CONSTANT) {
							  auto range_node = new AST::AstNode(AST::AST_RANGE);
							  range_node->filename = current_node->filename;
							  range_node->location = current_node->location;
							  range_node->children.push_back(node);
							  current_node->children.push_back(range_node);
						  } else {
							  current_node->children.push_back(node);
						  }
					  });
	if (current_node->children.size() > 1) {
		auto multirange_node = new AST::AstNode(AST::AST_MULTIRANGE);
		multirange_node->children = current_node->children;
		current_node->children.clear();
		current_node->children.push_back(multirange_node);
	}
	return current_node;
}

AST::AstNode* UhdmAst::handle_if_else(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_BLOCK, obj_h);
	auto case_node = new AST::AstNode(AST::AST_CASE);
	visit_one_to_one({vpiCondition},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 case_node->children.push_back(node);
					 });
	// If true:
	auto *condition = new AST::AstNode(AST::AST_COND);
	auto *constant = AST::AstNode::mkconst_int(1, false, 1);
	condition->children.push_back(constant);
	visit_one_to_one({vpiStmt},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 auto *statements = new AST::AstNode(AST::AST_BLOCK);
						 statements->children.push_back(node);
						 condition->children.push_back(statements);
					 });
	case_node->children.push_back(condition);
	// Else:
	if (vpi_get(vpiType, obj_h) == vpiIfElse) {
		auto *condition = new AST::AstNode(AST::AST_COND);
		auto *elseBlock = new AST::AstNode(AST::AST_DEFAULT);
		condition->children.push_back(elseBlock);
		visit_one_to_one({vpiElseStmt},
						 obj_h, {&parent, current_node},
						 [&](AST::AstNode* node) {
							 auto *statements = new AST::AstNode(AST::AST_BLOCK);
							 statements->children.push_back(node);
							 condition->children.push_back(statements);
						 });
		case_node->children.push_back(condition);
	}
	current_node->children.push_back(case_node);
	return current_node;
}

AST::AstNode* UhdmAst::handle_for(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_FOR, obj_h);
	AST::AstNode* loop_parent_node = parent.find({AST::AST_ALWAYS, AST::AST_FUNCTION, AST::AST_TASK});
	visit_one_to_many({vpiForInitStmt},
					  obj_h, {&parent, current_node},
					  [&](AST::AstNode* node) {
						  current_node->children.push_back(node);
						  auto wire_node = new AST::AstNode(AST::AST_WIRE);
						  wire_node->range_left=31;
						  wire_node->is_reg=true;
						  wire_node->str = node->children[0]->str;
						  loop_parent_node->children.push_back(wire_node);
					  });
	visit_one_to_one({vpiCondition},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 current_node->children.push_back(node);
					 });
	visit_one_to_many({vpiForIncStmt},
					  obj_h, {&parent, current_node},
					  [&](AST::AstNode* node) {
						  current_node->children.push_back(node);
					  });
	visit_one_to_one({vpiStmt},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 auto *statements = new AST::AstNode(AST::AST_BLOCK);
						 statements->children.push_back(node);
						 current_node->children.push_back(statements);
					 });
	return current_node;
}

AST::AstNode* UhdmAst::handle_gen_scope_array(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_GENBLOCK, obj_h);
	visit_one_to_many({vpiGenScope},
					  obj_h, {&parent, current_node},
					  [&](AST::AstNode* node) {
						  if (node->type == AST::AST_GENBLOCK) {
							  current_node->children.insert(current_node->children.end(),
															node->children.begin(),
															node->children.end());
						  } else {
							  current_node->children.push_back(node);
						  }
					  });
	return current_node;
}

AST::AstNode* UhdmAst::handle_gen_scope(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_GENBLOCK, obj_h);
	visit_one_to_many({vpiParameter,
					   vpiNet,
					   vpiArrayNet,
					   vpiProcess,
					   vpiContAssign,
					   vpiParamAssign,
					   vpiModule,
					   vpiGenScopeArray},
					   obj_h, {&parent, current_node},
					   [&](AST::AstNode* node) {
						   if (node) {
							   current_node->children.push_back(node);
						   }
					   });
	return current_node;
}

AST::AstNode* UhdmAst::handle_case(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_CASE, obj_h);
	visit_one_to_one({vpiCondition},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 current_node->children.push_back(node);
					 });
	visit_one_to_many({vpiCaseItem},
					  obj_h, {&parent, current_node},
					  [&](AST::AstNode* node) {
						  current_node->children.push_back(node);
					  });
	return current_node;
}

AST::AstNode* UhdmAst::handle_case_item(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_COND, obj_h);
	visit_one_to_many({vpiExpr},
					  obj_h, {&parent, current_node},
					  [&](AST::AstNode* node) {
						  current_node->children.push_back(node);
					  });
	if (current_node->children.empty()) {
		current_node->children.push_back(new AST::AstNode(AST::AST_DEFAULT));
	}
	visit_one_to_one({vpiStmt},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 if (node->type != AST::AST_BLOCK) {
							 auto block_node = new AST::AstNode(AST::AST_BLOCK);
							 block_node->children.push_back(node);
							 node = block_node;
						 }
						 current_node->children.push_back(node);
					 });
	return current_node;
}

AST::AstNode* UhdmAst::handle_constant(vpiHandle obj_h) {
	s_vpi_value val;
	vpi_get_value(obj_h, &val);
	if (val.format) { // Needed to handle parameter nodes without typespecs and constants
		switch (val.format) {
			case vpiScalarVal: return AST::AstNode::mkconst_int(val.value.scalar, false);
			case vpiBinStrVal:
			case vpiHexStrVal: return VERILOG_FRONTEND::const2ast(val.value.str, 0, false);
			case vpiIntVal: return AST::AstNode::mkconst_int(val.value.integer, false);
			case vpiRealVal: return AST::AstNode::mkconst_real(val.value.real);
			case vpiStringVal: return AST::AstNode::mkconst_str(val.value.str);
			default: error("Encountered unhandled constant format: %d", val.format);
		}
	}
	return nullptr;
}

AST::AstNode* UhdmAst::handle_range(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_RANGE, obj_h);
	visit_one_to_one({vpiLeftRange,
					  vpiRightRange},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 current_node->children.push_back(node);
					 });
	return current_node;
}

AST::AstNode* UhdmAst::handle_return(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_ASSIGN_EQ, obj_h);
	auto func_node = parent.find({AST::AST_FUNCTION, AST::AST_TASK});
	if (!func_node->children.empty()) {
		auto lhs = new AST::AstNode(AST::AST_IDENTIFIER);
		lhs->str = func_node->children[0]->str;
		current_node->children.push_back(lhs);
	}
	visit_one_to_one({vpiCondition},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 current_node->children.push_back(node);
					 });
	return current_node;
}

AST::AstNode* UhdmAst::handle_function(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(vpi_get(vpiType, obj_h) == vpiFunction ? AST::AST_FUNCTION : AST::AST_TASK, obj_h);
	visit_one_to_one({vpiReturn},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 if (node) {
							 current_node->children.push_back(node);
							 node->str = current_node->str;
						 }
					 });
	visit_one_to_one({vpiStmt},
					 obj_h, {&parent, current_node},
					 [&](AST::AstNode* node) {
						 if (node) {
							 current_node->children.push_back(node);
						 }
					 });
	visit_one_to_many({vpiIODecl},
					  obj_h, {&parent, current_node},
					  [&](AST::AstNode* node) {
						  node->type = AST::AST_WIRE;
						  current_node->children.push_back(node);
					  });
	return current_node;
}

AST::AstNode* UhdmAst::handle_logic_var(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_WIRE, obj_h);
	visit_range(obj_h, {&parent, current_node},
				[&](AST::AstNode* node) {
					current_node->children.push_back(node);
				});
	return current_node;
}

AST::AstNode* UhdmAst::handle_sys_func_call(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_FCALL, obj_h);
	if (current_node->str == "\\$signed") {
		current_node->type = AST::AST_TO_SIGNED;
	} else if (current_node->str == "\\$unsigned") {
		current_node->type = AST::AST_TO_UNSIGNED;
	} else if (current_node->str == "\\$display" || current_node->str == "\\$time") {
		current_node->type = AST::AST_TCALL;
		current_node->str = current_node->str.substr(1);
	}
	visit_one_to_many({vpiArgument},
					  obj_h, {&parent, current_node},
					  [&](AST::AstNode* node) {
						  if (node) {
							  current_node->children.push_back(node);
						  }
					  });
	return current_node;
}

AST::AstNode* UhdmAst::handle_func_call(vpiHandle obj_h, AstNodeList& parent) {
	auto current_node = make_ast_node(AST::AST_FCALL, obj_h);
	visit_one_to_many({vpiArgument},
					  obj_h, {&parent, current_node},
					  [&](AST::AstNode* node) {
						  if (node) {
							  current_node->children.push_back(node);
						  }
					  });
	return current_node;
}

void UhdmAst::resolve_assignment_pattern(AST::AstNode* module_node, AST::AstNode* wire_node) {
	if (wire_node->type != AST::AST_WIRE || wire_node->children.empty()
		|| wire_node->children[0]->type != AST::AST_WIRETYPE) return;
	auto typedef_node = module_node->find_child(AST::AST_TYPEDEF, wire_node->children[0]->str);
	if (typedef_node) {
		auto struct_node = typedef_node->children[0];
		for (auto assign_node : shared.unresolved_assignment_patterns[module_node->str][wire_node->str]) {
			auto concat_node = assign_node->children[1];
			// Erase every other node on the RHS as they're duplicated in UHDM
			for (auto it = concat_node->children.begin(); it != concat_node->children.end(); ++it) {
				concat_node->children.erase(it);
			}
			assign_node->children.resize(1); // Remove the RHS from the assignment node
			// Create assignment nodes for all the fields
			for (size_t i = 0; i < concat_node->children.size(); i++) {
				auto asgn_node = new AST::AstNode(assign_node->type);
				auto id_node = new AST::AstNode(AST::AST_IDENTIFIER);
				id_node->str = assign_node->children[0]->str + '.' + struct_node->children[i]->str;
				asgn_node->children.push_back(id_node);
				asgn_node->children.push_back(concat_node->children[concat_node->children.size() - 1 - i]);
				assign_node->children.push_back(asgn_node);
			}
			assign_node->children.erase(assign_node->children.begin());
			auto it = std::find(module_node->children.begin(), module_node->children.end(), assign_node);
			if (it != module_node->children.end()) { // If the assignment is directly in a module
				// Move all the assignments directly to the module
				module_node->children.erase(it);
				module_node->children.insert(module_node->children.end(), assign_node->children.begin(), assign_node->children.end());
			} else { // If the assignment is in a process
				assign_node->type = AST::AST_BLOCK;
			}
			shared.unresolved_assignment_patterns[module_node->str][wire_node->str].clear();
		}
	}
}

AST::AstNode* UhdmAst::visit_object(vpiHandle obj_h, AstNodeList parent) {
	// Current object data
	const uhdm_handle* const handle = (const uhdm_handle*) obj_h;
	const UHDM::BaseClass* const object = (const UHDM::BaseClass*) handle->object;

	const unsigned object_type = vpi_get(vpiType, obj_h);
	if (shared.debug_flag) {
		const char* object_name = vpi_get_str(vpiName, obj_h);
		std::cout << "Object: " << (object_name ? object_name : "") << " of type " << object_type << std::endl;
	}

	if (shared.visited.find(object) != shared.visited.end()) {
		return shared.visited[object];
	}

	AST::AstNode* node = nullptr;
	switch(object_type) {
		case vpiDesign: node = handle_design(obj_h, parent); break;
		case vpiParameter: node = handle_parameter(obj_h, parent); break;
		case vpiPort: node = handle_port(obj_h, parent); break;
		case vpiModule: node = handle_module(obj_h, parent); break;
		case vpiStructTypespec: node = handle_struct_typespec(obj_h, parent); break;
		case vpiTypespecMember: node = handle_typespec_member(obj_h, parent); break;
		case vpiEnumTypespec: node = handle_enum_typespec(obj_h, parent); break;
		case vpiEnumConst: node = handle_enum_const(obj_h); break;
		case vpiEnumVar:
		case vpiStructVar: node = handle_var(obj_h); break;
		case vpiArrayVar: node = handle_array_var(obj_h, parent); break;
		case vpiParamAssign: node = handle_param_assign(obj_h, parent); break;
		case vpiContAssign: node = handle_cont_assign(obj_h, parent); break;
		case vpiAssignStmt:
		case vpiAssignment: node = handle_assignment(obj_h, parent); break;
		case vpiRefObj: node = make_ast_node(AST::AST_IDENTIFIER, obj_h); break;
		case vpiNet: node = handle_net(obj_h, parent); break;
		case vpiArrayNet: node = handle_array_net(obj_h, parent); break;
		case vpiPackage: node = handle_package(obj_h, parent); break;
		case vpiInterface: node = handle_interface(obj_h, parent); break;
		case vpiModport: node = handle_modport(obj_h, parent); break;
		case vpiIODecl: node = handle_io_decl(obj_h, parent); break;
		case vpiAlways: node = handle_always(obj_h, parent); break;
		case vpiEventControl: node = handle_event_control(obj_h, parent); break;
		case vpiInitial: node = handle_initial(obj_h, parent); break;
		case vpiNamedBegin:
		case vpiBegin: node = handle_begin(obj_h, parent); break;
		case vpiIntVar: node = make_ast_node(AST::AST_IDENTIFIER, obj_h); break;
		case vpiCondition:
		case vpiOperation: node = handle_operation(obj_h, parent); break;
		case vpiBitSelect: node = handle_bit_select(obj_h, parent); break;
		case vpiPartSelect: node = handle_part_select(obj_h, parent); break;
		case vpiIndexedPartSelect: node = handle_indexed_part_select(obj_h, parent); break;
		case vpiVarSelect: node = handle_var_select(obj_h, parent); break;
		case vpiIf:
		case vpiIfElse: node = handle_if_else(obj_h, parent); break;
		case vpiFor: node = handle_for(obj_h, parent); break;
		case vpiGenScopeArray: node = handle_gen_scope_array(obj_h, parent); break;
		case vpiGenScope: node = handle_gen_scope(obj_h, parent); break;
		case vpiCase: node = handle_case(obj_h, parent); break;
		case vpiCaseItem: node = handle_case_item(obj_h, parent); break;
		case vpiConstant: node = handle_constant(obj_h); break;
		case vpiRange: node = handle_range(obj_h, parent); break;
		case vpiReturn: node = handle_return(obj_h, parent); break;
		case vpiFunction:
		case vpiTask: node = handle_function(obj_h, parent); break;
		case vpiLogicVar: node = handle_logic_var(obj_h, parent); break;
		case vpiSysFuncCall: node = handle_sys_func_call(obj_h, parent); break;
		case vpiFuncCall: node = handle_func_call(obj_h, parent); break;
		case vpiTaskCall: node = make_ast_node(AST::AST_TCALL, obj_h); break;
		case vpiProgram:
		default: error("Encountered unhandled object type: %d", object_type); break;
	}

	// Check if we initialized the node in switch-case
	if (node) {
		if (node->type != AST::AST_NONE) {
			shared.visited.insert(std::make_pair(object, node));
			shared.report.mark_handled(object);
			return node;
		}
		delete node;
	}
	return nullptr;
}

AST::AstNode* UhdmAst::visit_designs(const std::vector<vpiHandle>& designs) {
	auto *top_design = new AST::AstNode(AST::AST_DESIGN);
	for (auto design : designs) {
		auto *nodes = visit_object(design);
		// Flatten multiple designs into one
		for (auto child : nodes->children) {
			top_design->children.push_back(child);
		}
	}
	return top_design;
}

void UhdmAst::error(std::string message, unsigned object_type) const {
	message += '\n';
	if (shared.stop_on_error) {
		log_error(message.c_str(), object_type);
	} else {
		log_warning(message.c_str(), object_type);
	}
}

void UhdmAst::error(std::string message) const {
	message += '\n';
	if (shared.stop_on_error) {
		log_error(message.c_str());
	} else {
		log_warning(message.c_str());
	}
}

YOSYS_NAMESPACE_END

