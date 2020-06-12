#ifndef _UHDM_AST_REPORT_H_
#define _UHDM_AST_REPORT_H_ 1

#include <map>
#include <string>
#include <unordered_map>
#include "kernel/yosys.h"

YOSYS_NAMESPACE_BEGIN

class UhdmAstReport {
	private:
		// Maps a filename and a line number to a flag that says if the line is being handled by the frontend
		std::unordered_map<std::string, std::map<unsigned, bool>> lines_handled;

		// Acquires the map of line statuses for the specified file
		std::map<unsigned, bool>& get_for_file(const std::string& filename);

	public:
		// Add the specified source file to the report
		void add_file(const std::string& filename);

		// Marks the specified source file and line number as being handled by the frontend
		void mark_handled(const std::string& filename, unsigned line);

		// Marks the specified source file and line number as not being handled by the frontend
		void mark_unhandled(const std::string& filename, unsigned line);

		// Write the HTML report to the specified path
		void write(const std::string& filename);
};

YOSYS_NAMESPACE_END

#endif
