#include <vector>
#include <functional>
#include <algorithm>

#include "headers/uhdm.h"
#include "frontends/ast/ast.h"
#include "UhdmAst.h"

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

	const unsigned int objectType = vpi_get(vpiType, obj_h);
	std::cout << "Object: " << objectName
		<< " of type " << objectType
		<< std::endl;

	if (alreadyVisited) {
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
			//		UHDM::uhdmallPackages,
			//		UHDM::uhdmallClasses,
			//		UHDM::uhdmallUdps},
			//		obj_h,
			//		visited,
			//		[](AST::AstNode*){});
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
				if (actual_type == vpiModport) {
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
					break;
				}
			}

			// For non-interface ports
			current_node->type = AST::AST_WIRE;
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
				visit_one_to_many({
					vpiInterface,
					vpiModule,
					vpiContAssign,
					vpiProcess,
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
				visit_one_to_many({
					vpiInterface,
					vpiPort,
					vpiModule,
					vpiContAssign,
					vpiProcess,
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
			if (objectName != type) {
				// Not a top module, create instance
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
					auto identifierNode = new AST::AstNode(AST::AST_IDENTIFIER);
					argNode->str = argumentName;
					identifierNode->str = identifierName;
					argNode->children.push_back(identifierNode);
					current_node->children.push_back(argNode);
					vpi_free_object(port_h);
				}
				vpi_free_object(port_itr);
			}

			// Unhandled relationships: will visit (and print) the object
			//visit_one_to_many({vpiProcess,
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
			//		vpiTaskFunc,
			//		vpiNet,
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
			//		vpiImport
			//		},
			//		obj_h,
			//		visited,
			//		[](AST::AstNode*){});
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
		case vpiAssignment: {
			current_node->type = AST::AST_ASSIGN_LE;
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

			//TODO: Check type
			current_node->is_logic = true;

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
			//visit_one_to_many({vpiRange,
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
					},
					obj_h,
					visited,
					top_nodes,
					[&](AST::AstNode* node){
					if (node != nullptr)
						elaboratedInterface->children.push_back(node);
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
				current_node->type = AST::AST_CELL;
				auto typeNode = new AST::AstNode(AST::AST_CELLTYPE);
				typeNode->str = type;
				current_node->children.push_back(typeNode);
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
			break;
		}
		case vpiAlways: {
			current_node->type = AST::AST_ALWAYS;
			auto event_control_h = vpi_handle(vpiStmt, obj_h);

			// Some shenanigans to allow writing multiple conditions without intermediary node
			std::map<std::string, AST::AstNode*> nodes;
			nodes["process_node"] = current_node;
			visit_one_to_one({vpiCondition}, event_control_h, visited, &nodes,
				[](AST::AstNode*){
					// is added inside vpiOperation
				});

			visit_one_to_one({
				vpiStmt,
				},
				event_control_h,
				visited,
				top_nodes,
				[&](AST::AstNode* node){
					current_node->children.push_back(node);
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
		case vpiOperation: {
			auto operation = vpi_get(vpiOpType, obj_h);
			switch (operation) {
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
				case vpiEventOrOp: {
					// Add all operands as children of process node
					auto it = top_nodes->find("process_node");
					if (it != top_nodes->end()) {
						AST::AstNode* processNode = it->second;
						visit_one_to_many({vpiOperand}, obj_h, visited, top_nodes,
							[&](AST::AstNode* node){
								// add directly to process node
								processNode->children.push_back(node);
							});
					}
					// Do not return a node
					// Parent should not use returned value
					return nullptr;
				}
				case vpiPosedgeOp: {
					current_node->type = AST::AST_POSEDGE;
					visit_one_to_many({vpiOperand}, obj_h, visited, top_nodes,
						[&](AST::AstNode* node){
							current_node->children.push_back(node);
						});
					break;
				}
				case vpiNegedgeOp: {
					current_node->type = AST::AST_NEGEDGE;
					visit_one_to_many({vpiOperand}, obj_h, visited, top_nodes,
						[&](AST::AstNode* node){
							current_node->children.push_back(node);
						});
					break;
				}
				default: {
					break;
				}
			}
			break;
		}
		case vpiIfElse: {
			current_node->type = AST::AST_CASE;
			visit_one_to_one({vpiCondition}, obj_h, visited, top_nodes,
				[&](AST::AstNode* node){
					current_node->children.push_back(node);
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
			current_node->children.push_back(condition);
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
				current_node->children.push_back(condition);
			}
			break;
		}
		case vpiConstant: {
			current_node->type = AST::AST_CONSTANT;
			s_vpi_value val;
			vpi_get_value(obj_h, &val);
			switch (val.format) {
				case vpiIntVal: {
					current_node = AST::AstNode::mkconst_int(val.value.integer, false, 1);
					break;
				}
				default: {
					break;
				}
			}
			break;
		}
		// Explicitly unsupported
		case vpiProgram:
		default: {
			break;
		}
	}

	// Check if we initialized the node in switch-case
	if (current_node->type != AST::AST_NONE) {
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

YOSYS_NAMESPACE_END

