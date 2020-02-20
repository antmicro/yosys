#include <vector>
#include <functional>
#include <algorithm>

#include "headers/uhdm.h"
#include "frontends/ast/ast.h"
#include "UhdmAst.h"

YOSYS_NAMESPACE_BEGIN

void UhdmAst::visit_one_to_many (const std::vector<int> childrenNodeTypes,
		vpiHandle parentHandle,
		const std::function<void(AST::AstNode*)> &f) {
	for (auto child : childrenNodeTypes) {
		vpiHandle itr = vpi_iterate(child, parentHandle);
		while (vpiHandle vpi_child_obj = vpi_scan(itr) ) {
			auto *childNode = visit_object(vpi_child_obj);
			f(childNode);
			vpi_free_object(vpi_child_obj);
		}
		vpi_free_object(itr);
	}
}

void UhdmAst::visit_one_to_one (const std::vector<int> childrenNodeTypes,
		vpiHandle parentHandle,
		const std::function<void(AST::AstNode*)> &f) {
	for (auto child : childrenNodeTypes) {
		vpiHandle itr = vpi_handle(child, parentHandle);
		if (itr) {
			auto *childNode = visit_object(itr);
			f(childNode);
		}
		vpi_free_object(itr);
	}
}

AST::AstNode* UhdmAst::visit_object (vpiHandle obj_h) {

	// Current object data
	int lineNo = 0;
	std::string objectName = "";

	if (const char* s = vpi_get_str(vpiName, obj_h)) {
		objectName = s;
		// symbol names must begin with '\'
		objectName.insert(0, "\\");
		std::replace(objectName.begin(), objectName.end(), '@','_');
	}
	if (unsigned int l = vpi_get(vpiLineNo, obj_h)) {
		lineNo = l;
	}

	const unsigned int objectType = vpi_get(vpiType, obj_h);
	std::cout << "Object: " << objectName
		<< " of type " << objectType
		<< std::endl;

	switch(objectType) {
		case vpiDesign: {

			auto *design = new AST::AstNode(AST::AST_DESIGN);
			// Unhandled relationships: will visit (and print) the object
			visit_one_to_many({UHDM::uhdmtopModules,
					UHDM::uhdmallPrograms,
					UHDM::uhdmallPackages,
					UHDM::uhdmallClasses,
					UHDM::uhdmallInterfaces,
					UHDM::uhdmallUdps},
					obj_h,
					[](AST::AstNode*){});

			visit_one_to_many({UHDM::uhdmallModules},
					obj_h,
					[&](AST::AstNode* module) {
						if (module != nullptr) {
							design->children.push_back(module);
							design = module;
						}
					});
			return design;

			break;
		}
		case vpiPort: {
			// Unhandled relationships: will visit (and print) the object
			visit_one_to_many({vpiBit},
					obj_h,
					[](AST::AstNode*){});
			visit_one_to_one({vpiTypedef,
					vpiInstance,
					vpiModule,
					vpiHighConn,
					vpiLowConn},
					obj_h,
					[](AST::AstNode*){});
			auto *port = new AST::AstNode(AST::AST_WIRE);

			if (const int n = vpi_get(vpiDirection, obj_h)) {
				if (n == vpiInput) {
					port->is_input = true;
				} else if (n == vpiOutput) {
					port->is_output = true;
				} else if (n == vpiInout) {
					port->is_input = true;
					port->is_output = true;
				}
			}
			port->str = objectName;
			return port;

			break;
		}
		case vpiModule: {
			// Unhandled relationships: will visit (and print) the object
			visit_one_to_many({vpiProcess,
					vpiPrimitive,
					vpiPrimitiveArray,
					vpiInterface,
					vpiInterfaceArray,
					//vpiModule,
					vpiModuleArray,
					vpiModPath,
					vpiTchk,
					vpiDefParam,
					vpiIODecl,
					vpiAliasStmt,
					vpiClockingBlock,
					vpiTaskFunc,
					vpiNet,
					vpiArrayNet,
					vpiAssertion,
					vpiClassDefn,
					vpiProgram,
					vpiProgramArray,
					vpiSpecParam,
					vpiConcurrentAssertions,
					vpiVariables,
					vpiParameter,
					vpiInternalScope,
					vpiTypedef,
					vpiPropertyDecl,
					vpiSequenceDecl,
					vpiNamedEvent,
					vpiNamedEventArray,
					vpiVirtualInterfaceVar,
					vpiReg,
					vpiRegArray,
					vpiMemory,
					vpiLetDecl,
					vpiImport
					},
					obj_h,
					[](AST::AstNode*){});
			visit_one_to_one({vpiDefaultDisableIff,
					vpiInstanceArray,
					vpiGlobalClocking,
					vpiDefaultClocking,
					vpiModuleArray,
					vpiInstance,
					vpiModule  // TODO: Both here and in one-to-many?
					},
					obj_h,
					[](AST::AstNode*){});

			auto *module = new AST::AstNode(AST::AST_MODULE);
			module->str = objectName;
			visit_one_to_many({vpiPort, vpiContAssign}, obj_h, [&](AST::AstNode* port){
				if(port) {
					module->children.push_back(port);
				}
			});
			return module;
			break;
		}
		case vpiContAssign: {
			auto *assign = new AST::AstNode(AST::AST_ASSIGN);
			// Unhandled relationships: will visit (and print) the object
			visit_one_to_one({vpiDelay},
					obj_h,
					[](AST::AstNode*){});
			visit_one_to_many({vpiBit},
					obj_h,
					[](AST::AstNode*){});

			visit_one_to_one({vpiRhs, vpiLhs},
					obj_h,
					[&](AST::AstNode* node){
						if (node) {
							assign->children.push_back(node);
							}
					});

			return assign;

			break;
		}
		case vpiRefObj: {
			// Unhandled relationships: will visit (and print) the object
			visit_one_to_one({vpiInstance,
					vpiTaskFunc,
					vpiActual,
					vpiTypespec},
					obj_h,
					[](AST::AstNode*){});
			visit_one_to_many({vpiPortInst},
					obj_h,
					[](AST::AstNode*){});
			auto *reference = new AST::AstNode(AST::AST_IDENTIFIER);
			reference->str = objectName;
			return reference;

			break;
		}
		case vpiLogicNet: {
			// Handling of this node is not functional yet
			break;
			// Unhandled relationships: will visit (and print) the object
			visit_one_to_one({vpiLeftRange,
					vpiRightRange,
					vpiSimNet,
					vpiModule,
					vpiTypespec
					},
					obj_h,
					[](AST::AstNode*){});
			visit_one_to_many({vpiRange,
					vpiBit,
					vpiPortInst,
					vpiDriver,
					vpiLoad,
					vpiLocalDriver,
					vpiLocalLoad,
					vpiPrimTerm,
					vpiContAssign,
					vpiPathTerm,
					vpiTchkTerm
					},
					obj_h,
					[](AST::AstNode*){});

			break;
		}
		case vpiClassDefn: {
			if (const char* s = vpi_get_str(vpiFullName, obj_h)) {
				std::cout << "|vpiFullName: " << s << std::endl;
			}

			// Unhandled relationships: will visit (and print) the object
			visit_one_to_many({vpiConcurrentAssertions,
					vpiVariables,
					vpiParameter,
					vpiInternalScope,
					vpiTypedef,
					vpiPropertyDecl,
					vpiSequenceDecl,
					vpiNamedEvent,
					vpiNamedEventArray,
					vpiVirtualInterfaceVar,
					vpiReg,
					vpiRegArray,
					vpiMemory,
					vpiLetDecl,
					vpiImport},
					obj_h,
					[](AST::AstNode*){});

			break;
		}
		// Explicitly unsupported
		case vpiProgram:
		default: {
			break;
		}
	}

	return nullptr;
}

AST::AstNode* UhdmAst::visit_designs (const std::vector<vpiHandle>& designs) {
	auto *top_design = new AST::AstNode(AST::AST_DESIGN);

	for (auto design : designs) {
		top_design->children.push_back(visit_object(design));
	}
	return top_design;
}

YOSYS_NAMESPACE_END

