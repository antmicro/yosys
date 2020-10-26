/*
 *  yosys -- Yosys Open SYnthesis Suite
 *
 *  Copyright (C) 2012  Clifford Wolf <clifford@clifford.at>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "kernel/yosys.h"
#include "kernel/satgen.h"
#include "kernel/sigtools.h"
#include "kernel/modtools.h"
#include "kernel/mem.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct MemoryShareWorker
{
	RTLIL::Design *design;
	RTLIL::Module *module;
	SigMap sigmap, sigmap_xmux;
	ModWalker modwalker;
	CellTypes cone_ct;
	bool flag_widen;


	// ------------------------------------------------------
	// Consolidate write ports that write to the same address
	// (or close enough to be merged to wide ports)
	// ------------------------------------------------------

	bool consolidate_wr_by_addr(Mem &mem)
	{
		if (GetSize(mem.wr_ports) <= 1)
			return false;

		log("Consolidating write ports of memory %s.%s by address:\n", log_id(module), log_id(mem.memid));

		bool changed = false;
		for (int i = 0; i < GetSize(mem.wr_ports); i++)
		{
			auto &port1 = mem.wr_ports[i];
			if (port1.removed)
				continue;
			if (!port1.clk_enable)
				continue;
			for (int j = i + 1; j < GetSize(mem.wr_ports); j++)
			{
				auto &port2 = mem.wr_ports[j];
				if (port2.removed)
					continue;
				if (!port2.clk_enable)
					continue;
				if (port1.clk != port2.clk)
					continue;
				if (port1.clk_polarity != port2.clk_polarity)
					continue;
				// If the width of the ports doesn't match, they can still be
				// merged by widening the narrow one.  Check if the conditions
				// hold for that.
				int wide_log2 = std::max(port1.wide_log2, port2.wide_log2);
				if (GetSize(port1.addr) <= wide_log2)
					continue;
				if (GetSize(port2.addr) <= wide_log2)
					continue;
				if (!port1.addr.extract(0, wide_log2).is_fully_const())
					continue;
				if (!port2.addr.extract(0, wide_log2).is_fully_const())
					continue;
				if (sigmap_xmux(port1.addr.extract_end(wide_log2)) != sigmap_xmux(port2.addr.extract_end(wide_log2))) {
					// Incompatible addresses after widening.  Last chance — widen both
					// ports by one more bit to merge them.
					if (!flag_widen)
						continue;
					wide_log2++;
					if (sigmap_xmux(port1.addr.extract_end(wide_log2)) != sigmap_xmux(port2.addr.extract_end(wide_log2)))
						continue;
					if (!port1.addr.extract(0, wide_log2).is_fully_const())
						continue;
					if (!port2.addr.extract(0, wide_log2).is_fully_const())
						continue;
				}
				log("  Merging ports %d, %d (address %s).\n", i, j, log_signal(port1.addr));
				mem.prepare_wr_merge(i, j);
				port1.addr = sigmap_xmux(port1.addr);
				port2.addr = sigmap_xmux(port2.addr);
				mem.widen_wr_port(i, wide_log2);
				mem.widen_wr_port(j, wide_log2);
				int pos = 0;
				while (pos < GetSize(port1.data)) {
					int epos = pos;
					while (epos < GetSize(port1.data) && port1.en[epos] == port1.en[pos] && port2.en[epos] == port2.en[pos])
						epos++;
					int width = epos - pos;
					SigBit new_en;
					if (port2.en[pos] == State::S0) {
						new_en = port1.en[pos];
					} else if (port1.en[pos] == State::S0) {
						port1.data.replace(pos, port2.data.extract(pos, width));
						new_en = port2.en[pos];
					} else {
						port1.data.replace(pos, module->Mux(NEW_ID, port1.data.extract(pos, width), port2.data.extract(pos, width), port2.en[pos]));
						new_en = module->Or(NEW_ID, port1.en[pos], port2.en[pos]);
					}
					for (int k = pos; k < epos; k++)
						port1.en[k] = new_en;
					pos = epos;
				}
				changed = true;
				port2.removed = true;
			}
		}

		if (changed)
			mem.emit();

		return changed;
	}


	// --------------------------------------------------------
	// Consolidate write ports using sat-based resource sharing
	// --------------------------------------------------------

	void consolidate_wr_using_sat(Mem &mem)
	{
		if (GetSize(mem.wr_ports) <= 1)
			return;

		// Get a list of ports that have any chance of being mergeable.

		pool<int> eligible_ports;

		for (int i = 0; i < GetSize(mem.wr_ports); i++) {
			auto &port = mem.wr_ports[i];
			std::vector<RTLIL::SigBit> bits = modwalker.sigmap(port.en);
			for (auto bit : bits)
				if (bit == RTLIL::State::S1)
					goto port_is_always_active;
			if (modwalker.has_drivers(bits))
				eligible_ports.insert(i);
		port_is_always_active:;
		}

		if (eligible_ports.size() <= 1)
			return;

		log("Consolidating write ports of memory %s.%s using sat-based resource sharing:\n", log_id(module), log_id(mem.memid));

		// Group eligible ports by clock domain and width.

		pool<int> checked_ports;
		std::vector<std::vector<int>> groups;
		for (int i = 0; i < GetSize(mem.wr_ports); i++)
		{
			auto &port1 = mem.wr_ports[i];
			if (!eligible_ports.count(i))
				continue;
			if (checked_ports.count(i))
				continue;


			std::vector<int> group;
			group.push_back(i);

			for (int j = i + 1; j < GetSize(mem.wr_ports); j++)
			{
				auto &port2 = mem.wr_ports[j];
				if (!eligible_ports.count(j))
					continue;
				if (checked_ports.count(j))
					continue;
				if (port1.clk_enable != port2.clk_enable)
					continue;
				if (port1.clk_enable) {
					if (port1.clk != port2.clk)
						continue;
					if (port1.clk_polarity != port2.clk_polarity)
						continue;
				}
				if (port1.wide_log2 != port2.wide_log2)
					continue;
				group.push_back(j);
			}

			for (auto j : group)
				checked_ports.insert(j);

			if (group.size() <= 1)
				continue;

			groups.push_back(group);
		}

		bool changed = false;
		for (auto &group : groups) {
			auto &some_port = mem.wr_ports[group[0]];
			string ports;
			for (auto idx : group) {
				if (idx != group[0])
					ports += ", ";
				ports += std::to_string(idx);
			}
			if (!some_port.clk_enable) {
				log("  Checking unclocked group, width %d: ports %s.\n", mem.width << some_port.wide_log2, ports.c_str());
			} else {
				log("  Checking group clocked with %sedge %s, width %d: ports %s.\n", some_port.clk_polarity ? "pos" : "neg", log_signal(some_port.clk), mem.width << some_port.wide_log2, ports.c_str());
			}

			// Okay, time to actually run the SAT solver.

			ezSatPtr ez;
			SatGen satgen(ez.get(), &modwalker.sigmap);

			// create SAT representation of common input cone of all considered EN signals

			pool<Wire*> one_hot_wires;
			std::set<RTLIL::Cell*> sat_cells;
			std::set<RTLIL::SigBit> bits_queue;
			dict<int, int> port_to_sat_variable;

			for (auto idx : group) {
				RTLIL::SigSpec sig = modwalker.sigmap(mem.wr_ports[idx].en);
				port_to_sat_variable[idx] = ez->expression(ez->OpOr, satgen.importSigSpec(sig));

				std::vector<RTLIL::SigBit> bits = sig;
				bits_queue.insert(bits.begin(), bits.end());
			}

			while (!bits_queue.empty())
			{
				for (auto bit : bits_queue)
					if (bit.wire && bit.wire->get_bool_attribute(ID::onehot))
						one_hot_wires.insert(bit.wire);

				pool<ModWalker::PortBit> portbits;
				modwalker.get_drivers(portbits, bits_queue);
				bits_queue.clear();

				for (auto &pbit : portbits)
					if (sat_cells.count(pbit.cell) == 0 && cone_ct.cell_known(pbit.cell->type)) {
						pool<RTLIL::SigBit> &cell_inputs = modwalker.cell_inputs[pbit.cell];
						bits_queue.insert(cell_inputs.begin(), cell_inputs.end());
						sat_cells.insert(pbit.cell);
					}
			}

			for (auto wire : one_hot_wires) {
				log("  Adding one-hot constraint for wire %s.\n", log_id(wire));
				vector<int> ez_wire_bits = satgen.importSigSpec(wire);
				for (int i : ez_wire_bits)
				for (int j : ez_wire_bits)
					if (i != j) ez->assume(ez->NOT(i), j);
			}

			log("  Common input cone for all EN signals: %d cells.\n", int(sat_cells.size()));

			for (auto cell : sat_cells)
				satgen.importCell(cell);

			log("  Size of unconstrained SAT problem: %d variables, %d clauses\n", ez->numCnfVariables(), ez->numCnfClauses());

			// now try merging the ports.

			for (int ii = 0; ii < GetSize(group); ii++) {
				int idx1 = group[ii];
				auto &port1 = mem.wr_ports[idx1];
				if (port1.removed)
					continue;
				for (int jj = ii + 1; jj < GetSize(group); jj++) {
					int idx2 = group[jj];
					auto &port2 = mem.wr_ports[idx2];
					if (port2.removed)
						continue;

					if (ez->solve(port_to_sat_variable.at(idx1), port_to_sat_variable.at(idx2))) {
						log("  According to SAT solver sharing of port %d with port %d is not possible.\n", idx1, idx2);
						continue;
					}

					log("  Merging port %d into port %d.\n", idx2, idx1);
					mem.prepare_wr_merge(idx1, idx2);
					port_to_sat_variable.at(idx1) = ez->OR(port_to_sat_variable.at(idx1), port_to_sat_variable.at(idx2));

					RTLIL::SigSpec last_addr = port1.addr;
					RTLIL::SigSpec last_data = port1.data;
					std::vector<RTLIL::SigBit> last_en = modwalker.sigmap(port1.en);

					RTLIL::SigSpec this_addr = port2.addr;
					RTLIL::SigSpec this_data = port2.data;
					std::vector<RTLIL::SigBit> this_en = modwalker.sigmap(port2.en);

					RTLIL::SigBit this_en_active = module->ReduceOr(NEW_ID, this_en);

					if (GetSize(last_addr) < GetSize(this_addr))
						last_addr.extend_u0(GetSize(this_addr));
					else
						this_addr.extend_u0(GetSize(last_addr));

					port1.addr = module->Mux(NEW_ID, last_addr, this_addr, this_en_active);
					port1.data = module->Mux(NEW_ID, last_data, this_data, this_en_active);

					std::map<std::pair<RTLIL::SigBit, RTLIL::SigBit>, int> groups_en;
					RTLIL::SigSpec grouped_last_en, grouped_this_en, en;
					RTLIL::Wire *grouped_en = module->addWire(NEW_ID, 0);

					for (int j = 0; j < int(this_en.size()); j++) {
						std::pair<RTLIL::SigBit, RTLIL::SigBit> key(last_en[j], this_en[j]);
						if (!groups_en.count(key)) {
							grouped_last_en.append(last_en[j]);
							grouped_this_en.append(this_en[j]);
							groups_en[key] = grouped_en->width;
							grouped_en->width++;
						}
						en.append(RTLIL::SigSpec(grouped_en, groups_en[key]));
					}

					module->addMux(NEW_ID, grouped_last_en, grouped_this_en, this_en_active, grouped_en);
					port1.en = en;

					port2.removed = true;
					changed = true;
				}
			}
		}

		if (changed)
			mem.emit();
	}


	// -------------
	// Setup and run
	// -------------

	MemoryShareWorker(RTLIL::Design *design, bool flag_widen) : design(design), modwalker(design), flag_widen(flag_widen) {}

	void operator()(RTLIL::Module* module)
	{
		std::vector<Mem> memories = Mem::get_selected_memories(module);

		this->module = module;
		sigmap.set(module);

		sigmap_xmux = sigmap;
		for (auto cell : module->cells())
		{
			if (cell->type == ID($mux))
			{
				RTLIL::SigSpec sig_a = sigmap_xmux(cell->getPort(ID::A));
				RTLIL::SigSpec sig_b = sigmap_xmux(cell->getPort(ID::B));

				if (sig_a.is_fully_undef())
					sigmap_xmux.add(cell->getPort(ID::Y), sig_b);
				else if (sig_b.is_fully_undef())
					sigmap_xmux.add(cell->getPort(ID::Y), sig_a);
			}
		}

		for (auto &mem : memories) {
			while (consolidate_wr_by_addr(mem));
		}

		cone_ct.setup_internals();
		cone_ct.cell_types.erase(ID($mul));
		cone_ct.cell_types.erase(ID($mod));
		cone_ct.cell_types.erase(ID($div));
		cone_ct.cell_types.erase(ID($modfloor));
		cone_ct.cell_types.erase(ID($divfloor));
		cone_ct.cell_types.erase(ID($pow));
		cone_ct.cell_types.erase(ID($shl));
		cone_ct.cell_types.erase(ID($shr));
		cone_ct.cell_types.erase(ID($sshl));
		cone_ct.cell_types.erase(ID($sshr));
		cone_ct.cell_types.erase(ID($shift));
		cone_ct.cell_types.erase(ID($shiftx));

		modwalker.setup(module, &cone_ct);

		for (auto &mem : memories)
			consolidate_wr_using_sat(mem);
	}
};

struct MemorySharePass : public Pass {
	MemorySharePass() : Pass("memory_share", "consolidate memory ports") { }
	void help() override
	{
		//   |---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|---v---|
		log("\n");
		log("    memory_share [selection]\n");
		log("\n");
		log("This pass merges share-able memory ports into single memory ports.\n");
		log("\n");
		log("The following methods are used to consolidate the number of memory ports:\n");
		log("\n");
		log("  - When multiple write ports access the same address then this is converted\n");
		log("    to a single write port with a more complex data and/or enable logic path.\n");
		log("\n");
		log("  - When multiple write ports are never accessed at the same time (a SAT\n");
		log("    solver is used to determine this), then the ports are merged into a single\n");
		log("    write port.\n");
		log("\n");
		log("Note that in addition to the algorithms implemented in this pass, the $memrd\n");
		log("and $memwr cells are also subject to generic resource sharing passes (and other\n");
		log("optimizations) such as \"share\" and \"opt_merge\".\n");
		log("\n");
	}
	void execute(std::vector<std::string> args, RTLIL::Design *design) override {
		log_header(design, "Executing MEMORY_SHARE pass (consolidating $memrd/$memwr cells).\n");
		// TODO: expose when wide ports are actually supported.
		bool flag_widen = false;
		extra_args(args, 1, design);
		MemoryShareWorker msw(design, flag_widen);

		for (auto module : design->selected_modules())
			msw(module);
	}
} MemorySharePass;

PRIVATE_NAMESPACE_END
