#include <fstream>
#include "frontends/ast/ast.h"
#include "uhdmastreport.h"

YOSYS_NAMESPACE_BEGIN

std::map<unsigned, bool>& UhdmAstReport::get_for_file(const std::string& filename) {
	auto it = lines_handled.find(filename);
	if (it == lines_handled.end()) {
		it = lines_handled.insert(std::make_pair(filename, std::map<unsigned, bool>())).first;
	}
	return it->second;
}

void UhdmAstReport::add_file(const std::string& filename) {
	get_for_file(filename);
}

void UhdmAstReport::mark_handled(const std::string& filename, unsigned line) {
	get_for_file(filename).insert(std::make_pair(line, true));
}

void UhdmAstReport::mark_unhandled(const std::string& filename, unsigned line) {
	auto insert_result = get_for_file(filename).insert(std::make_pair(line, true));
	if (!insert_result.second) {
		insert_result.first->second = false;
	}
}

void UhdmAstReport::write(const std::string& filename) {
	std::ofstream report_file(filename);
	report_file << "<!DOCTYPE html>\n<html>\n<head>\n<style>\nbody {\nbackground-color: #93B874;\n}\n</style>\n</head><body>" << std::endl;
	for (auto& handled_in_file : lines_handled) {
		if (handled_in_file.first == AST::current_filename) {
			continue;
		}
		report_file << "<pre style=\"background-color: #FFFFFF;\">" << handled_in_file.first << "</pre>" << std::endl;
		std::ifstream source_file(handled_in_file.first);
		auto handled_it = handled_in_file.second.begin();
		bool current_line_handled = true;
		unsigned line_number = 1;
		std::string line;
		while (std::getline(source_file, line)) {
			if (handled_it != handled_in_file.second.end() && line_number == handled_it->first) {
				current_line_handled = handled_it->second;
			}
			if (current_line_handled) {
				report_file << "<pre>" << line_number << "\t\t" << line << "</pre>" << std::endl;
			} else {
				report_file << "<pre style=\"background-color: #FF0000;\">" << line_number << "\t\t" << line << "</pre>" << std::endl;
			}
			if (handled_it != handled_in_file.second.end() && line_number >= handled_it->first) {
				++handled_it;
			}
			++line_number;
		}
	}
	report_file << "</body>\n</html>" << std::endl;
}

YOSYS_NAMESPACE_END
