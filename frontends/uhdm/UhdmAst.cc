#include <vector>
#include <functional>
#include <algorithm>

#include "headers/uhdm.h"
#include "frontends/ast/ast.h"
#include "UhdmAst.h"
#include "vpi_user.h"

YOSYS_NAMESPACE_BEGIN

void UhdmAst::visit_one_to_many (const std::vector<int> childrenNodeTypes,
		vpiHandle parentHandle, std::set<const UHDM::BaseClass*> visited,
		std::map<std::string, AST::AstNode*>* top_nodes,
		const std::function<void(AST::AstNode*)> &f) {
	for (auto child : childrenNodeTypes) {
		vpiHandle itr = vpi_iterate(child, parentHandle);
		while (vpiHandle vpi_child_obj = vpi_scan(itr) ) {
			auto *childNode = visit_object(vpi_child_obj, visited, top_nodes);
			f(childNode);
			vpi_free_object(vpi_child_obj);
		}
		vpi_free_object(itr);
	}
}

void UhdmAst::visit_one_to_one (const std::vector<int> childrenNodeTypes,
		vpiHandle parentHandle, std::set<const UHDM::BaseClass*> visited,
		std::map<std::string, AST::AstNode*>* top_nodes,
		const std::function<void(AST::AstNode*)> &f) {
	for (auto child : childrenNodeTypes) {
		vpiHandle itr = vpi_handle(child, parentHandle);
		if (itr) {
			auto *childNode = visit_object(itr, visited, top_nodes);
			f(childNode);
		}
		vpi_free_object(itr);
	}
}

void sanitize_symbol_name(std::string &name) {
		// symbol names must begin with '\'
		name.insert(0, "\\");
		std::replace(name.begin(), name.end(), '@','_');
}

int parse_int_string(const char* int_str) {
	const char* bin_str = std::strchr(int_str, 'b');
	const char* dec_str = std::strchr(int_str, 'd');
	const char* hex_str = std::strchr(int_str, 'h');
	if (bin_str) {
		return std::stoi(bin_str + 1, nullptr, 2);
	} else if (dec_str) {
		return std::stoi(dec_str + 1, nullptr, 10);
	} else if (hex_str) {
		return std::stoi(hex_str + 1, nullptr, 16);
	} else {
		return std::stoi(int_str);
	}
}

void UhdmAst::make_cell(vpiHandle obj_h, AST::AstNode* current_node, const std::string& type) {
	current_node->type = AST::AST_CELL;
	auto typeNode = new AST::AstNode(AST::AST_CELLTYPE);
	typeNode->str = type;
	current_node->children.push_back(typeNode);
	// Add port connections as arguments
	vpiHandle port_itr = vpi_iterate(vpiPort, obj_h);
	while (vpiHandle port_h = vpi_scan(port_itr) ) {
		auto highConn_h = vpi_handle(vpiHighConn, port_h);
		std::string argumentName, identifierName;
		if (auto s = vpi_get_str(vpiName, highConn_h)) {
			identifierName = s;
			sanitize_symbol_name(identifierName);
		}
		if (auto s = vpi_get_str(vpiName, port_h)) {
			argumentName = s;
			sanitize_symbol_name(argumentName);
		}
		auto argNode = new AST::AstNode(AST::AST_ARGUMENT);
		argNode->str = argumentName;
		argNode->filename = current_node->filename;
		argNode->location = current_node->location;
		auto identifierNode = new AST::AstNode(AST::AST_IDENTIFIER);
		identifierNode->filename = current_node->filename;
		identifierNode->location = current_node->location;
		identifierNode->str = identifierName;
		argNode->children.push_back(identifierNode);
		current_node->children.push_back(argNode);
		report.mark_handled(highConn_h);
		report.mark_handled(port_h);
		vpi_free_object(port_h);
	}
	vpi_free_object(port_itr);
}

void UhdmAst::add_typedef(AST::AstNode* current_node, AST::AstNode* type_node) {
	auto typedef_node = new AST::AstNode(AST::AST_TYPEDEF);
	typedef_node->location = type_node->location;
	typedef_node->filename = type_node->filename;
	typedef_node->str = type_node->str;
	type_node->str = "";
	if (type_node->type == AST::AST_STRUCT) {
		typedef_node->children.push_back(type_node);
		current_node->children.push_back(typedef_node);
	} else if (type_node->type == AST::AST_ENUM) {
		auto wire_node = new AST::AstNode(AST::AST_WIRE);
		wire_node->attributes["\\enum_type"] = AST::AstNode::mkconst_str(type_node->str);
		typedef_node->children.push_back(wire_node);
		type_node->str = "$enum" + std::to_string(enum_count++);
		for (auto* enum_item : type_node->children) {
			enum_item->attributes["\\enum_base_type"] = AST::AstNode::mkconst_str(type_node->str);
		}
		current_node->children.push_back(type_node);
		current_node->children.push_back(typedef_node);
	}
}

AST::AstNode* UhdmAst::visit_object (
		vpiHandle obj_h,
		std::set<const UHDM::BaseClass*> visited,
		std::map<std::string, AST::AstNode*>* top_nodes) {

	// Current object data
	std::string objectName = "";
	const uhdm_handle* const handle = (const uhdm_handle*) obj_h;
	const UHDM::BaseClass* const object = (const UHDM::BaseClass*) handle->object;
	bool alreadyVisited = false;
	if (visited.find(object) != visited.end()) {
		alreadyVisited = true;
	}
	visited.insert(object);

	auto *current_node = new AST::AstNode(AST::AST_NONE);

	if (auto s = vpi_get_str(vpiName, obj_h)) {
		objectName = s;
	} else if (auto s = vpi_get_str(vpiDefName, obj_h)) {
		objectName = s;
	}
	if (objectName != "") {
		sanitize_symbol_name(objectName);
		current_node->str = objectName;
	}
	if (unsigned int l = vpi_get(vpiLineNo, obj_h)) {
		current_node->linenum = l;
	}
	if (auto filename = vpi_get_str(vpiFile, obj_h)) {
		current_node->filename = filename;
	}

	const unsigned int objectType = vpi_get(vpiType, obj_h);
	std::cout << "Object: " << objectName
		<< " of type " << objectType
		<< std::endl;

	if (alreadyVisited) {
		report.mark_handled(object);
		return current_node;
	}
	switch(objectType) {
		case vpiDesign: {
			current_node->type = AST::AST_DESIGN;
			std::map<std::string, AST::AstNode*> top_nodes;
			visit_one_to_many({
					UHDM::uhdmallInterfaces,
					UHDM::uhdmtopModules,
					UHDM::uhdmallModules,
					UHDM::uhdmallPackages
					},
					obj_h,
					visited,
					&top_nodes,  // Will be used to add module definitions
					[&](AST::AstNode* object) {
						if (object != nullptr) {
							top_nodes[object->str] = object;
						}
					});
			// Once we walked everything, unroll that as children of this node
			for (auto pair : top_nodes) {
				current_node->children.push_back(pair.second);
			}

			// Unhandled relationships: will visit (and print) the object
			//visit_one_to_many({
			//		UHDM::uhdmallPrograms,
			//		UHDM::uhdmallClasses,
			//		UHDM::uhdmallUdps},
			//		obj_h,
			//		visited,
			//		[](AST::AstNode*){});
			break;
		}
		case vpiParameter: {
			current_node->type = AST::AST_PARAMETER;
			s_vpi_value val;
			vpi_get_value(obj_h, &val);
			AST::AstNode* constant_node = nullptr;
			switch (val.format) {
				case 0: {
					vpiHandle typespec_h = vpi_handle(vpiTypespec, obj_h);
					if (typespec_h) {
						int typespec_type = vpi_get(vpiType, typespec_h);
						switch (typespec_type) {
							case vpiLogicTypespec: {
								current_node->is_logic = true;
								visit_one_to_many({vpiRange},
										typespec_h,
										visited,
										top_nodes,
										[&](AST::AstNode* node){
											current_node->children.push_back(node);
										});
								report.mark_handled(typespec_h);
								break;
							}
							case vpiIntTypespec: {
								report.mark_handled(typespec_h);
								break;
							}
							default: {
								error("Encountered unhandled typespec: %d\n", typespec_type);
							}
						}
					}
					break;
				}
				case vpiScalarVal: {
					constant_node = AST::AstNode::mkconst_int(val.value.scalar, true);
					break;
				}
				case vpiIntVal: {
					constant_node = AST::AstNode::mkconst_int(val.value.integer, true);
					break;
				}
				default: {
					error("Encountered unhandled parameter format: %d\n", val.format);
				}
			}
			if (constant_node) {
				constant_node->filename = current_node->filename;
				constant_node->location = current_node->location;
				current_node->children.push_back(constant_node);
			}
			break;
		}
		case vpiPort: {
			current_node->type = AST::AST_WIRE;
			static int portId = 1;
			current_node->port_id = portId++;

			vpiHandle lowConn_h = vpi_handle(vpiLowConn, obj_h);
			if (lowConn_h != nullptr) {
				vpiHandle actual_h = vpi_handle(vpiActual, lowConn_h);
				auto actual_type = vpi_get(vpiType, actual_h);
				switch (actual_type) {
				case vpiModport: {
						vpiHandle iface_h = vpi_handle(vpiInterface, actual_h);
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
						current_node->str = objectName;
						auto typeNode = new AST::AstNode(AST::AST_INTERFACEPORTTYPE);
						// Skip '\' in cellName
						typeNode->str = ifaceName + '.' + cellName.substr(1, cellName.length());
						current_node->children.push_back(typeNode);
						report.mark_handled(actual_h);
						report.mark_handled(iface_h);
						break;
					}
					case vpiInterface: {
						auto typeNode = new AST::AstNode(AST::AST_INTERFACEPORTTYPE);
						if (auto s = vpi_get_str(vpiDefName, actual_h)) {
							typeNode->str = s;
							sanitize_symbol_name(typeNode->str);
						}
						current_node->type = AST::AST_INTERFACEPORT;
						current_node->str = objectName;
						current_node->children.push_back(typeNode);
						report.mark_handled(actual_h);
						break;
					}
					case vpiLogicNet: {
						visit_one_to_many({vpiRange},
							actual_h,
							visited,
							top_nodes,
							[&](AST::AstNode* node){
								current_node->children.push_back(node);
							});
						report.mark_handled(actual_h);
					}
				}
				report.mark_handled(lowConn_h);
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

			// Unhandled relationships: will visit (and print) the object
			//visit_one_to_many({vpiBit},
			//		obj_h,
			//		visited,
			//		[](AST::AstNode*){});
			//visit_one_to_one({vpiTypedef,
			//		vpiInstance,
			//		vpiModule,
			//		vpiHighConn,
			//		vpiLowConn},
			//		obj_h,
			//		visited,
			//		[](AST::AstNode*){});
			break;
		}
		case vpiModule: {
			std::string type = vpi_get_str(vpiDefName, obj_h);
			if (type != "") {
				sanitize_symbol_name(type);
			}

			AST::AstNode* elaboratedModule;
			// Check if we have encountered this object before
			auto it = top_nodes->find(type);
			if (it != top_nodes->end()) {
				// Was created before, fill missing
				elaboratedModule = it->second;
				current_module = elaboratedModule;

				visit_one_to_many({vpiTypedef},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node){
						add_typedef(elaboratedModule, node);
					});
				visit_one_to_many({
					vpiInterface,
					vpiModule,
					vpiContAssign,
					vpiProcess,
					vpiGenScopeArray
					},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node){
					if (node != nullptr)
						elaboratedModule->children.push_back(node);
					});
			} else {
				// Encountered for the first time
				elaboratedModule = new AST::AstNode(AST::AST_MODULE);
				elaboratedModule->str = type;
				current_module = elaboratedModule;

				visit_one_to_many({vpiTypedef},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node){
						add_typedef(elaboratedModule, node);
					});
				visit_one_to_many({
					vpiInterface,
					vpiPort,
					vpiModule,
					vpiVariables,
					vpiContAssign,
					vpiProcess,
					vpiGenScopeArray
					},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node){
					if (node != nullptr)
						elaboratedModule->children.push_back(node);
					});
			}
			(*top_nodes)[elaboratedModule->str] = elaboratedModule;
			report.mark_handled(object);
			if (objectName != type) {
				// Not a top module, create instance
				make_cell(obj_h, current_node, type);
			}

			visit_one_to_many({vpiParameter,
					vpiParamAssign,
					vpiNet,
					vpiTaskFunc
			// Unhandled relationships:
			//		vpiProcess,
			//		vpiPrimitive,
			//		vpiPrimitiveArray,
			//		vpiInterfaceArray,
			//		//vpiModule,
			//		vpiModuleArray,
			//		vpiModPath,
			//		vpiTchk,
			//		vpiDefParam,
			//		vpiIODecl,
			//		vpiAliasStmt,
			//		vpiClockingBlock,
			//		vpiArrayNet,
			//		vpiAssertion,
			//		vpiClassDefn,
			//		vpiProgram,
			//		vpiProgramArray,
			//		vpiSpecParam,
			//		vpiConcurrentAssertions,
			//		vpiInternalScope,
			//		vpiPropertyDecl,
			//		vpiSequenceDecl,
			//		vpiNamedEvent,
			//		vpiNamedEventArray,
			//		vpiVirtualInterfaceVar,
			//		vpiReg,
			//		vpiRegArray,
			//		vpiMemory,
			//		vpiLetDecl,
			//		vpiImport
					},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node){
						if (node != nullptr) {
							if (node->type == AST::AST_PARAMETER) {
								// If we already have this parameter, replace it
								for (auto& child : elaboratedModule->children) {
									if (child->str == node->str) {
										std::swap(child, node);
										delete node;
										return;
									}
								}
							} else if (node->type == AST::AST_WIRE && elaboratedModule->find_child(AST::AST_WIRE, node->str)) {
								// If we already have this wire, do not add it again
								delete node;
								return;
							}
							elaboratedModule->children.push_back(node);
						}
					});
			//visit_one_to_one({vpiDefaultDisableIff,
			//		vpiInstanceArray,
			//		vpiGlobalClocking,
			//		vpiDefaultClocking,
			//		vpiModuleArray,
			//		vpiInstance,
			//		vpiModule  // TODO: Both here and in one-to-many?
			//		},
			//		obj_h,
			//		visited,
			//		[](AST::AstNode*){});
			break;
		}
		case vpiStructTypespec: {
			current_node->type = AST::AST_STRUCT;
			visit_one_to_many({vpiTypespecMember},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node){
						current_node->children.push_back(node);
					});
			break;
		}
		case vpiTypespecMember: {
			current_node->type = AST::AST_STRUCT_ITEM;
			current_node->str = current_node->str.substr(1);
			vpiHandle typespec_h = vpi_handle(vpiTypespec, obj_h);
			int typespec_type = vpi_get(vpiType, typespec_h);
			switch (typespec_type) {
				case vpiStructTypespec: {
					auto struct_node = visit_object(typespec_h, visited, top_nodes);
					struct_node->str = current_node->str;
					delete current_node;
					current_node = struct_node;
					report.mark_handled(typespec_h);
					break;
				}
				case vpiEnumTypespec: {
					current_node->children.push_back(visit_object(typespec_h, visited, top_nodes));
					report.mark_handled(typespec_h);
					break;
				}
				case vpiLogicTypespec: {
					current_node->is_logic = true;
					visit_one_to_many({vpiRange},
							typespec_h,
							visited,
							top_nodes,
							[&](AST::AstNode* node){
								current_node->children.push_back(node);
							});
					report.mark_handled(typespec_h);
					break;
				}
				default: {
					error("Encountered unhandled typespec: %d\n", typespec_type);
					break;
				}
			}
			break;
		}
		case vpiEnumTypespec: {
			current_node->type = AST::AST_ENUM;
			visit_one_to_many({vpiEnumConst},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node){
						current_node->children.push_back(node);
					});
			vpiHandle typespec_h = vpi_handle(vpiBaseTypespec, obj_h);
			int typespec_type = vpi_get(vpiType, typespec_h);
			if (typespec_type == vpiLogicTypespec) {
				current_node->is_logic = true;
				visit_one_to_many({vpiRange},
						typespec_h,
						visited,
						top_nodes,
						[&](AST::AstNode* node){
							for (auto child : current_node->children) {
								child->children.push_back(node->clone());
							}
							delete node;
						});
				report.mark_handled(typespec_h);
			} else {
				error("Encountered unhandled typespec: %d\n", typespec_type);
				break;
			}
			break;
		}
		case vpiEnumConst: {
			current_node->type = AST::AST_ENUM_ITEM;
			s_vpi_value val;
			vpi_get_value(obj_h, &val);
			switch (val.format) {
				case vpiIntVal: {
					current_node->children.push_back(AST::AstNode::mkconst_int(val.value.integer, false));
					break;
				}
				default: {
					error("Encountered unhandled constant format: %d\n", val.format);
				}
			}
			break;
		}
		case vpiStructVar: {
			current_node->type = AST::AST_WIRE;
			vpiHandle typespec_h = vpi_handle(vpiTypespec, obj_h);
			std::string name = vpi_get_str(vpiName, typespec_h);
			sanitize_symbol_name(name);
			auto wiretype_node = new AST::AstNode(AST::AST_WIRETYPE);
			wiretype_node->str = name;
			current_node->children.push_back(wiretype_node);
			break;
		}
		case vpiArrayVar: {
			current_node->type = AST::AST_MEMORY;
			vpiHandle itr = vpi_iterate(vpiReg, obj_h);
			while (vpiHandle reg_h = vpi_scan(itr)) {
				if (vpi_get(vpiType, reg_h) == vpiStructVar) {
					vpiHandle typespec_h = vpi_handle(vpiTypespec, reg_h);
					std::string name = vpi_get_str(vpiName, typespec_h);
					sanitize_symbol_name(name);
					auto wiretype_node = new AST::AstNode(AST::AST_WIRETYPE);
					wiretype_node->str = name;
					current_node->children.push_back(wiretype_node);
					report.mark_handled(reg_h);
					report.mark_handled(typespec_h);
				}
				vpi_free_object(reg_h);
			}
			vpi_free_object(itr);
			visit_one_to_many({vpiRange},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node){
						current_node->children.push_back(node);
					});
			break;
		}
		case vpiParamAssign: {
			current_node->type = AST::AST_PARAMETER;
			visit_one_to_one({vpiLhs, vpiRhs},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node){
						if (node->type == AST::AST_PARAMETER) {
							current_node->str = node->str;
							delete node;
						} else {
							current_node->children.push_back(node);
						}
					});
			break;
		}
		case vpiContAssign: {
			current_node->type = AST::AST_ASSIGN;
			visit_one_to_one({vpiRhs, vpiLhs},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node){
						if (node) {
							current_node->children.push_back(node);
						}
					});

			// Unhandled relationships: will visit (and print) the object
			//visit_one_to_one({vpiDelay},
			//		obj_h,
			//		visited,
			//		[](AST::AstNode*){});
			//visit_one_to_many({vpiBit},
			//		obj_h,
			//		visited,
			//		[](AST::AstNode*){});
			break;
		}
		case vpiAssignStmt:
		case vpiAssignment: {
			current_node->type = AST::AST_ASSIGN_EQ;
			(*top_nodes)["assign_node"] = current_node;
			visit_one_to_one({vpiLhs, vpiRhs},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node){
						if (node) {
							current_node->children.push_back(node);
						}
					});
			break;
		}
		case vpiRefObj: {
			// Unhandled relationships: will visit (and print) the object
			visit_one_to_one({vpiInstance,
					vpiTaskFunc,
					vpiActual,
					vpiTypespec},
					obj_h,
					visited,
					top_nodes,
					[](AST::AstNode*){});
			visit_one_to_many({vpiPortInst},
					obj_h,
					visited,
					top_nodes,
					[](AST::AstNode*){});
			current_node->type = AST::AST_IDENTIFIER;

			break;
		}
		case vpiNet: {
			current_node->type = AST::AST_WIRE;
			auto net_type = vpi_get(vpiNetType, obj_h);
			current_node->is_reg = net_type == vpiReg;
			current_node->is_output = net_type == vpiOutput;
			visit_one_to_many({vpiRange
			//		vpiBit,
			//		vpiPortInst,
			//		vpiDriver,
			//		vpiLoad,
			//		vpiLocalDriver,
			//		vpiLocalLoad,
			//		vpiPrimTerm,
			//		vpiContAssign,
			//		vpiPathTerm,
			//		vpiTchkTerm
					},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node){
						current_node->children.push_back(node);
					});
			// Unhandled relationships: will visit (and print) the object
			//visit_one_to_one({vpiLeftRange,
			//		vpiRightRange,
			//		vpiSimNet,
			//		vpiModule,
			//		vpiTypespec
			//		},
			//		obj_h,
			//		visited,
			//		[](AST::AstNode*){});
			break;
		}
		case vpiClassDefn: {
			if (const char* s = vpi_get_str(vpiFullName, obj_h)) {
				std::cout << "|vpiFullName: " << s << std::endl;
			}

			// Unhandled relationships: will visit (and print) the object
			//visit_one_to_many({vpiConcurrentAssertions,
			//		vpiVariables,
			//		vpiParameter,
			//		vpiInternalScope,
			//		vpiTypedef,
			//		vpiPropertyDecl,
			//		vpiSequenceDecl,
			//		vpiNamedEvent,
			//		vpiNamedEventArray,
			//		vpiVirtualInterfaceVar,
			//		vpiReg,
			//		vpiRegArray,
			//		vpiMemory,
			//		vpiLetDecl,
			//		vpiImport},
			//		obj_h,
			//		visited,
			//		[](AST::AstNode*){});
			break;
		}
		case vpiPackage: {
			current_node->type = AST::AST_PACKAGE;
			visit_one_to_many({vpiParameter,
							   vpiParamAssign,
							   vpiTypedef},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node){
						if (node->type == AST::AST_PARAMETER) {
							// If we already have this parameter, replace it
							for (auto& child : current_node->children) {
								if (child->str == node->str) {
									std::swap(child, node);
									delete node;
									return;
								}
							}
							current_node->children.push_back(node);
						} else {
							add_typedef(current_node, node);
						}
					});
			break;
		}
		case vpiInterface: {
			std::string type = vpi_get_str(vpiDefName, obj_h);
			if (type != "") {
				sanitize_symbol_name(type);
			}

			AST::AstNode* elaboratedInterface;
			// Check if we have encountered this object before
			auto it = top_nodes->find(type);
			if (it != top_nodes->end()) {
				// Was created before, fill missing
				elaboratedInterface = it->second;
				visit_one_to_many({
					vpiPort},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node){
					if (node != nullptr)
						std::replace_if(elaboratedInterface->children.begin(), elaboratedInterface->children.end(),
										[&node](AST::AstNode* x) {return x->str == node->str;}, node);
					});
			} else {
				// Encountered for the first time
				elaboratedInterface = new AST::AstNode(AST::AST_INTERFACE);
				elaboratedInterface->str = objectName;
				visit_one_to_many({
					vpiNet,
					vpiModport
					},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node){
					if (node != nullptr)
						elaboratedInterface->children.push_back(node);
					});
			}
			(*top_nodes)[elaboratedInterface->str] = elaboratedInterface;

			if (objectName != type) {
				// Not a top module, create instance
				make_cell(obj_h, current_node, type);
				break;
			}

			// Unhandled relationships: will visit (and print) the object
			//visit_one_to_one({
			//		vpiParent,
			//		vpiInstanceArray,
			//		vpiGlobalClocking,
			//		vpiDefaultClocking,
			//		vpiDefaultDisableIff,
			//		vpiInstance,
			//		vpiModule
			//		},
			//		obj_h,
			//		visited,
			//		[](AST::AstNode*){});
			//visit_one_to_many({
			//		vpiProcess,
			//		vpiInterfaceTfDecl,
			//		vpiModPath,
			//		vpiContAssign,
			//		vpiInterface,
			//		vpiInterfaceArray,
			//		vpiPort,
			//		vpiTaskFunc,
			//		vpiArrayNet,
			//		vpiAssertion,
			//		vpiClassDefn,
			//		vpiProgram,
			//		vpiProgramArray,
			//		vpiSpecParam,
			//		vpiConcurrentAssertions,
			//		vpiVariables,
			//		vpiParameter,
			//		vpiInternalScope,
			//		vpiTypedef,
			//		vpiPropertyDecl,
			//		vpiSequenceDecl,
			//		vpiNamedEvent,
			//		vpiNamedEventArray,
			//		vpiVirtualInterfaceVar,
			//		vpiReg,
			//		vpiRegArray,
			//		vpiMemory,
			//		vpiLetDecl,
			//		vpiImport,
			//		},
			//		obj_h,
			//		visited,
			//		[](AST::AstNode*){});
			break;
		}
		case vpiModport: {
			current_node->type = AST::AST_MODPORT;
			visit_one_to_many({vpiIODecl},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* net){
				if(net) {
					current_node->children.push_back(net);
				}
			});
			break;
		}
		case vpiIODecl: {
			current_node->type = AST::AST_MODPORTMEMBER;
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
			visit_one_to_many({vpiRange},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node) {
						current_node->children.push_back(node);
					});
			break;
		}
		case vpiAlways: {
			current_node->type = AST::AST_ALWAYS;
			// Some shenanigans to allow writing multiple conditions without intermediary node
			std::map<std::string, AST::AstNode*> nodes;
			nodes["process_node"] = current_node;

			switch (vpi_get(vpiAlwaysType, obj_h)) {
				case vpiAlwaysComb:
				case vpiAlwaysFF: {
					visit_one_to_one({
						vpiStmt,
						},
						obj_h,
						visited,
						&nodes,
						[&](AST::AstNode* node) {
							current_node->children.push_back(node);
						});
					break;
				}
				default: {
					visit_one_to_one({
						vpiStmt,
						},
						obj_h,
						visited,
						&nodes,
						[&](AST::AstNode* node) {
							if (node) {
								current_node->children.push_back(node);
							}
						});
					break;
				}
			}
			break;
		}
		case vpiEventControl: {
			current_node->type = AST::AST_BLOCK;
			visit_one_to_one({vpiCondition}, obj_h, visited, top_nodes,
					[&](AST::AstNode* node) {
						if (node) {
							top_nodes->at("process_node")->children.push_back(node);
						}
						// is added inside vpiOperation
					});
			visit_one_to_one({
					vpiStmt,
					},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node) {
						if (node) {
							current_node->children.push_back(node);
						}
					});
			break;
		}
		case vpiInitial: {
			current_node->type = AST::AST_INITIAL;
			visit_one_to_one({vpiStmt},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node) {
						if (node) {
							current_node->children.push_back(node);
						}
					});
			break;
		}
		case vpiBegin: {
			current_node->type = AST::AST_BLOCK;
			visit_one_to_many({
				vpiStmt,
				},
				obj_h,
				visited,
				top_nodes,
				[&](AST::AstNode* node){
					current_node->children.push_back(node);
				});
			break;
		}
		case vpiIntVar: {
			current_node->type = AST::AST_IDENTIFIER;
			break;
		}
		case vpiCondition:
		case vpiOperation: {
			auto operation = vpi_get(vpiOpType, obj_h);
			switch (operation) {
				case vpiAssignmentPatternOp: {
					auto block_node = (*top_nodes)["assign_node"];
					block_node->type = AST::AST_BLOCK;
					auto lhs_node = block_node->children.front();
					block_node->children.clear();
					auto type_name = current_module->find_child(AST::AST_WIRE, lhs_node->str)->children[0]->str;
					auto type_node = current_module->find_child(AST::AST_TYPEDEF, type_name)->children[0];
					std::unordered_set<std::string> visited_fields;
					visit_one_to_many({vpiOperand}, obj_h, visited, top_nodes,
						[&](AST::AstNode* rhs_node){
							size_t next_index = block_node->children.size();
							if (next_index < type_node->children.size()) {
								auto field_name = type_node->children[next_index]->str;
								if (visited_fields.find(field_name) == visited_fields.end()) {
									visited_fields.insert(field_name);
									auto assign_node = new AST::AstNode(AST::AST_ASSIGN_EQ);
									auto lhs_field_node = lhs_node->clone();
									lhs_field_node->str += '.' + field_name;
									assign_node->children.push_back(lhs_field_node);
									assign_node->children.push_back(rhs_node);
									block_node->children.push_back(assign_node);
								}
							}
						});
					delete current_node;
					delete lhs_node;
					return nullptr;
				}
				case vpiNotOp: {
					current_node->type = AST::AST_REDUCE_BOOL;
					visit_one_to_many({vpiOperand}, obj_h, visited, top_nodes,
						[&](AST::AstNode* node){
							auto *negate = new AST::AstNode(AST::AST_LOGIC_NOT);
							negate->children.push_back(node);
							current_node->children.push_back(negate);
						});
					break;
				}
				case vpiEqOp: {
					current_node->type = AST::AST_REDUCE_BOOL;
					auto eq_node = new AST::AstNode(AST::AST_EQ);
					current_node->children.push_back(eq_node);
					visit_one_to_many({vpiOperand}, obj_h, visited, top_nodes,
						[&](AST::AstNode* node){
							eq_node->children.push_back(node);
						});
					break;
				}
				case vpiNeqOp: {
					current_node->type = AST::AST_REDUCE_BOOL;
					auto *not_node = new AST::AstNode(AST::AST_LOGIC_NOT);
					current_node->children.push_back(not_node);
					auto eq_node = new AST::AstNode(AST::AST_EQ);
					not_node->children.push_back(eq_node);
					visit_one_to_many({vpiOperand}, obj_h, visited, top_nodes,
						[&](AST::AstNode* node){
							eq_node->children.push_back(node);
						});
					break;
				}
				case vpiLtOp: {
					current_node->type = AST::AST_REDUCE_BOOL;
					auto lt_node = new AST::AstNode(AST::AST_LT);
					current_node->children.push_back(lt_node);
					visit_one_to_many({vpiOperand}, obj_h, visited, top_nodes,
						[&](AST::AstNode* node){
							lt_node->children.push_back(node);
						});
					break;
				}
				case vpiGtOp: {
					current_node->type = AST::AST_REDUCE_BOOL;
					auto lt_node = new AST::AstNode(AST::AST_GT);
					current_node->children.push_back(lt_node);
					visit_one_to_many({vpiOperand}, obj_h, visited, top_nodes,
						[&](AST::AstNode* node){
							lt_node->children.push_back(node);
						});
					break;
				}
				case vpiEventOrOp: {
					// Add all operands as children of process node
					auto it = top_nodes->find("process_node");
					if (it != top_nodes->end()) {
						AST::AstNode* processNode = it->second;
						visit_one_to_many({vpiOperand}, obj_h, visited, top_nodes,
							[&](AST::AstNode* node){
								// add directly to process node
								if (node) {
									processNode->children.push_back(node);
								}
							});
					}
					// Do not return a node
					// Parent should not use returned value
					report.mark_handled(object);
					return nullptr;
				}
				case vpiCastOp: {
					visit_one_to_many({vpiOperand}, obj_h, visited, top_nodes,
						[&](AST::AstNode* node){
							delete current_node;
							current_node = node;
						});
					break;
				}
				case vpiInsideOp: {
					current_node->type = AST::AST_EQ;
					AST::AstNode* lhs = nullptr;
					visit_one_to_many({vpiOperand}, obj_h, visited, top_nodes,
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
					visit_one_to_many({vpiOperand}, obj_h, visited, top_nodes,
						[&](AST::AstNode* node){
							current_node->children.push_back(node);
						});
					switch(operation) {
						case vpiMinusOp: current_node->type = AST::AST_SUB; break;
						case vpiPlusOp: current_node->type = AST::AST_ADD; break;
						case vpiPosedgeOp: current_node->type = AST::AST_POSEDGE; break;
						case vpiNegedgeOp: current_node->type = AST::AST_NEGEDGE; break;
						case vpiUnaryOrOp: current_node->type = AST::AST_REDUCE_OR; break;
						case vpiBitNegOp: current_node->type = AST::AST_BIT_NOT; break;
						case vpiBitAndOp: current_node->type = AST::AST_BIT_AND; break;
						case vpiBitOrOp: current_node->type = AST::AST_BIT_OR; break;
						case vpiBitXorOp: current_node->type = AST::AST_BIT_XOR; break;
						case vpiBitXnorOp: current_node->type = AST::AST_BIT_XNOR; break;
						case vpiLShiftOp: current_node->type = AST::AST_SHIFT_LEFT; break;
						case vpiLogAndOp: current_node->type = AST::AST_LOGIC_AND; break;
						case vpiLogOrOp: current_node->type = AST::AST_LOGIC_OR; break;
						case vpiSubOp: current_node->type = AST::AST_SUB; break;
						case vpiAddOp: current_node->type = AST::AST_ADD; break;
						case vpiArithRShiftOp: current_node->type = AST::AST_SHIFT_SRIGHT; break;
						case vpiPostIncOp: {
							// TODO: Make this an actual post-increment op (currently it's a pre-increment)
							current_node->type = AST::AST_ASSIGN_EQ;
							auto id = current_node->children[0]->clone();
							auto add_node = new AST::AstNode(AST::AST_ADD, id, AST::AstNode::mkconst_int(1, true));
							add_node->filename = current_node->filename;
							add_node->location = current_node->location;
							current_node->children.push_back(add_node);
							break;
						}
						case vpiConditionOp: current_node->type = AST::AST_TERNARY; break;
						case vpiConcatOp: {
							current_node->type = AST::AST_CONCAT;
							std::reverse(current_node->children.begin(), current_node->children.end());
							break;
						}
						case vpiMultiConcatOp: current_node->type = AST::AST_REPLICATE; break;
						default: {
							error("Encountered unhandled operation: %d\n", operation);
						}
					}
					break;
				}
			}
			break;
		}
		case vpiBitSelect: {
			current_node->type = AST::AST_IDENTIFIER;
			visit_one_to_one({vpiIndex},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node) {
						auto range_node = new AST::AstNode(AST::AST_RANGE, node);
						range_node->filename = current_node->filename;
						range_node->location = current_node->location;
						current_node->children.push_back(range_node);
					});
			break;
		}
		case vpiPartSelect: {
			current_node->type = AST::AST_IDENTIFIER;

			visit_one_to_one({vpiParent},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node) {
						current_node->str = node->str;
						delete node;
					});

			auto range_node = new AST::AstNode(AST::AST_RANGE);
			range_node->filename = current_node->filename;
			range_node->location = current_node->location;
			visit_one_to_one({vpiLeftRange, vpiRightRange},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node) {
						range_node->children.push_back(node);
					});
			current_node->children.push_back(range_node);
			break;
		}
		case vpiNamedBegin: {
			current_node->type = AST::AST_BLOCK;
			visit_one_to_many({
				vpiStmt,
				},
				obj_h,
				visited,
				top_nodes,
				[&](AST::AstNode* node) {
					if (node) {
						current_node->children.push_back(node);
					}
				});
			break;
		}
		case vpiIf:
		case vpiIfElse: {
			current_node->type = AST::AST_BLOCK;
			auto* case_node = new AST::AstNode(AST::AST_CASE);
			visit_one_to_one({vpiCondition}, obj_h, visited, top_nodes,
				[&](AST::AstNode* node){
					case_node->children.push_back(node);
				});
			// If true:
			auto *condition = new AST::AstNode(AST::AST_COND);
			auto *constant = AST::AstNode::mkconst_int(1, false, 1);
			condition->children.push_back(constant);
			visit_one_to_one({vpiStmt}, obj_h, visited, top_nodes,
				[&](AST::AstNode* node){
					auto *statements = new AST::AstNode(AST::AST_BLOCK);
					statements->children.push_back(node);
					condition->children.push_back(statements);
				});
			case_node->children.push_back(condition);
			if (objectType == vpiIfElse) {
				auto *condition = new AST::AstNode(AST::AST_COND);
				auto *elseBlock = new AST::AstNode(AST::AST_DEFAULT);
				condition->children.push_back(elseBlock);
				visit_one_to_one({vpiElseStmt}, obj_h, visited, top_nodes,
					[&](AST::AstNode* node){
						auto *statements = new AST::AstNode(AST::AST_BLOCK);
						statements->children.push_back(node);
						condition->children.push_back(statements);
					});
				case_node->children.push_back(condition);
			}
			current_node->children.push_back(case_node);
			break;
		}
		case vpiFor: {
			current_node->type = AST::AST_BLOCK;
			auto for_node = new AST::AstNode(AST::AST_FOR);
			visit_one_to_many({vpiForInitStmt}, obj_h, visited, top_nodes,
				[&](AST::AstNode* node){
					for_node->children.push_back(node);
				});
			visit_one_to_one({vpiCondition}, obj_h, visited, top_nodes,
				[&](AST::AstNode* node){
					for_node->children.push_back(node);
				});
			visit_one_to_many({vpiForIncStmt}, obj_h, visited, top_nodes,
				[&](AST::AstNode* node){
					for_node->children.push_back(node);
				});
			visit_one_to_one({vpiStmt}, obj_h, visited, top_nodes,
				[&](AST::AstNode* node){
					auto *statements = new AST::AstNode(AST::AST_BLOCK);
					statements->children.push_back(node);
					for_node->children.push_back(statements);
				});
			current_node->children.push_back(for_node);
			break;
		}
		case vpiGenScopeArray: {
			current_node->type = AST::AST_GENBLOCK;
			visit_one_to_many({vpiGenScope}, obj_h, visited, top_nodes,
				[&](AST::AstNode* node){
					if (node->type == AST::AST_GENBLOCK) {
						current_node->children.insert(current_node->children.end(),
													  node->children.begin(), node->children.end());
					} else {
						current_node->children.push_back(node);
					}
				});
			break;
		}
		case vpiGenScope: {
			current_node->type = AST::AST_GENBLOCK;
			visit_one_to_many({vpiParameter,
							   vpiContAssign},
				obj_h, visited, top_nodes,
				[&](AST::AstNode* node){
					if (node) {
						current_node->children.push_back(node);
					}
				});
			break;
		}
		case vpiCase: {
			current_node->type = AST::AST_CASE;
			visit_one_to_one({vpiCondition}, obj_h, visited, top_nodes,
				[&](AST::AstNode* node){
					current_node->children.push_back(node);
				});
			visit_one_to_many({vpiCaseItem
				},
				obj_h,
				visited,
				top_nodes,
				[&](AST::AstNode* node) {
					current_node->children.push_back(node);
				});
			break;
		}
		case vpiCaseItem: {
			current_node->type = AST::AST_COND;
			visit_one_to_many({vpiExpr
				},
				obj_h,
				visited,
				top_nodes,
				[&](AST::AstNode* node) {
					current_node->children.push_back(node);
				});
			if (current_node->children.empty()) {
				current_node->children.push_back(new AST::AstNode(AST::AST_DEFAULT));
			}
			visit_one_to_one({vpiStmt
				},
				obj_h,
				visited,
				top_nodes,
				[&](AST::AstNode* node) {
					if (node->type != AST::AST_BLOCK) {
						auto block_node = new AST::AstNode(AST::AST_BLOCK);
						block_node->children.push_back(node);
						node = block_node;
					}
					current_node->children.push_back(node);
				});
			break;
		}
		case vpiConstant: {
			s_vpi_value val;
			vpi_get_value(obj_h, &val);
			AST::AstNode* constant_node = nullptr;
			switch (val.format) {
				case vpiScalarVal: {
					constant_node = AST::AstNode::mkconst_int(val.value.scalar, false);
					break;
				}
				case vpiBinStrVal: {
					int int_val = parse_int_string(val.value.str);
					constant_node = AST::AstNode::mkconst_int(int_val, false);
					break;
				}
				case vpiHexStrVal: {
					int int_val = parse_int_string(val.value.str);
					constant_node = AST::AstNode::mkconst_int(int_val, false);
					break;
				}
				case vpiIntVal: {
					constant_node = AST::AstNode::mkconst_int(val.value.integer, false);
					break;
				}
				case vpiRealVal: {
					constant_node = AST::AstNode::mkconst_real(val.value.real);
					break;
				}
				case vpiStringVal: {
					constant_node = AST::AstNode::mkconst_str(val.value.str);
					break;
				}
				default: {
					error("Encountered unhandled constant format: %d\n", val.format);
				}
			}
			if (constant_node) {
				constant_node->filename = current_node->filename;
				constant_node->location = current_node->location;
				delete current_node;
				current_node = constant_node;
			}
			break;
		}
		case vpiRange: {
			current_node->type = AST::AST_RANGE;
			visit_one_to_one({vpiLeftRange, vpiRightRange},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node) {
						current_node->children.push_back(node);
					});
			break;
		}
		case vpiFunction: {
			current_node->type = AST::AST_FUNCTION;
			visit_one_to_one({vpiReturn},
				obj_h,
				visited,
				top_nodes,
				[&](AST::AstNode* node) {
					current_node->children.push_back(node);
					node->str = current_node->str;
				});
			visit_one_to_one({vpiStmt},
				obj_h,
				visited,
				top_nodes,
				[&](AST::AstNode* node) {
					current_node->children.push_back(node);
				});
			visit_one_to_many({vpiIODecl},
				obj_h,
				visited,
				top_nodes,
				[&](AST::AstNode* node) {
					node->type = AST::AST_WIRE;
					current_node->children.push_back(node);
				});
			break;
		}
		case vpiLogicVar: {
			current_node->type = AST::AST_WIRE;
			visit_one_to_many({vpiRange},
				obj_h,
				visited,
				top_nodes,
				[&](AST::AstNode* node) {
					current_node->children.push_back(node);
				});
			break;
		}
		case vpiSysFuncCall: {
			if (current_node->str == "\\$signed") {
				current_node->type = AST::AST_TO_SIGNED;
			} else if (current_node->str == "\\$unsigned") {
				current_node->type = AST::AST_TO_UNSIGNED;
			} else if (current_node->str == "\\$display" || current_node->str == "\\$time") {
				current_node->type = AST::AST_TCALL;
				current_node->str = current_node->str.substr(1);
			} else {
				current_node->type = AST::AST_FCALL;
			}
			visit_one_to_many({vpiArgument},
				obj_h,
				visited,
				top_nodes,
				[&](AST::AstNode* node){
					if (node) {
						current_node->children.push_back(node);
					}
				});
			break;
		}
		case vpiFuncCall: {
			current_node->type = AST::AST_FCALL;
			visit_one_to_many({vpiArgument},
				obj_h,
				visited,
				top_nodes,
				[&](AST::AstNode* node){
					if (node) {
						current_node->children.push_back(node);
					}
				});
			break;
		}
		// Explicitly unsupported
		case vpiProgram:
		default: {
			error("Encountered unhandled object type: %d\n", objectType);
		}
	}

	// Check if we initialized the node in switch-case
	if (current_node->type != AST::AST_NONE) {
		report.mark_handled(object);
		return current_node;
	} else {
		return nullptr;
	}
}

AST::AstNode* UhdmAst::visit_designs (const std::vector<vpiHandle>& designs) {
	auto *top_design = new AST::AstNode(AST::AST_DESIGN);

	for (auto design : designs) {
		std::set<const UHDM::BaseClass*> visited;
		auto *nodes = visit_object(design, visited, nullptr);
		// Flatten multiple designs into one
		for (auto child : nodes->children) {
		  top_design->children.push_back(child);
		}
	}
	return top_design;
}

void UhdmAst::error(const char* message, unsigned object_type) const {
	if (stop_on_error) {
		log_error(message, object_type);
	} else {
		log_warning(message, object_type);
	}
}


YOSYS_NAMESPACE_END

