/*
 *  yosys -- Yosys Open SYnthesis Suite
 *
 *  Copyright (C) 2012  Clifford Wolf <clifford@clifford.at>
 *  Copyright (C) 2020  The Symbiflow Authors
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

#include "kernel/register.h"
#include "kernel/rtlil.h"
#include "kernel/sigtools.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct NexusDsp : public Pass {

    /// A structure identifying specific pin in a cell instance
    struct CellPin {
        RTLIL::Cell*    cell;   /// Cell pointer (nullptr for top-level ports)
        RTLIL::IdString port;   /// Port name
        int             bit;    /// Bit index

        CellPin (RTLIL::Cell* _cell,
                 const RTLIL::IdString& _port,
                 int _bit = 0) : 
            cell(_cell),
            port(_port),
            bit (_bit)
        {}

        CellPin (const CellPin& ref) = default;
        CellPin (CellPin&& ref) = default;

        unsigned int hash () const {
            unsigned int h = 0;
            if (cell != nullptr) {
                h = mkhash_add(h, cell->hash());
            }
            h = mkhash_add(h, port.hash());
            h = mkhash_add(h, bit);
            return h;
        }

        bool operator == (const CellPin& ref) const {
            return  (cell == ref.cell) &&
                    (port == ref.port) &&
                    (bit  == ref.bit);
        }
    };

    // A structure describing unique Flip-flop properties and connections
    // that cannot be mixed together.
    struct FlopData {
        RTLIL::IdString type;   /// Flip-flop type
        RTLIL::SigBit   clk;    /// Clock connection (CK)
        RTLIL::SigBit   ena;    /// Enable connection (SP)
        RTLIL::SigBit   rst;    /// Reset/set connection (PD/CD)
        RTLIL::Const    gsr;    /// GSR setting

        FlopData (const RTLIL::IdString& _type,
                  RTLIL::SigBit _clk,
                  RTLIL::SigBit _ena,
                  RTLIL::SigBit _rst,
                  RTLIL::Const  _gsr) :
            type (_type),
            clk  (_clk),
            ena  (_ena),
            rst  (_rst),
            gsr  (_gsr)
        {}

        FlopData (const FlopData& ref) = default;
        FlopData (FlopData&& ref) = default;

        unsigned int hash () const {
            unsigned int h = 0;
            h = mkhash_add(h, type.hash());
            h = mkhash_add(h, clk.hash());
            h = mkhash_add(h, ena.hash());
            h = mkhash_add(h, rst.hash());
            h = mkhash_add(h, gsr.hash());
            return h;
        }

        bool operator == (const FlopData& ref) const {
            return  (type == ref.type) &&
                    (clk  == ref.clk ) &&
                    (ena  == ref.ena ) &&
                    (rst  == ref.rst ) &&
                    (gsr  == ref.gsr );
        }
    };

    /// Temporary SigBit to SigBit helper map.
    SigMap m_SigMap;

    /// Known DSP cell types
    const pool<RTLIL::IdString> m_DspTypes;
    /// Known flip-flop cell types
    const pool<RTLIL::IdString> m_FlopTypes;

    // ..........................................

    NexusDsp() :
        Pass("nexus_dsp", "Integrates flip-flop into DSP blocks of the Nexus arch"),

        m_DspTypes({
            RTLIL::escape_id("MULT36X36"),
            RTLIL::escape_id("MULT36X18"),
            RTLIL::escape_id("MULT18X18"),
            RTLIL::escape_id("MULT9X9")
        }),

        m_FlopTypes({
            RTLIL::escape_id("FD1P3DX"),
            RTLIL::escape_id("FD1P3IX"),

            // FIXME: Allow integration of sync/async set FFs only when their
            // PD port is not connected to an active net
            //RTLIL::escape_id("FD1P3BX"),
            //RTLIL::escape_id("FD1P3JX")
        })
    {}

    void help () override {
        log("\n");
        log("    nexus_dsp [selection]\n");
        log("\n");
        log("Integrates flip-flops with DSP blocks in the Nexus architecture\n");
        log("and enables their internal registers. The pass takes care not to\n");
        log("mix conflicting flip-flop types/configurations.\n");
        log("\n");
        log("Recognized DSP cell types:\n");

        for (const auto& name : m_DspTypes) {
            log("    %s\n", name.c_str());
        }
        log("Recognized flip-flop cell types:\n");

        for (const auto& name : m_FlopTypes) {
            log("    %s\n", name.c_str());
        }
    }

    void execute(std::vector<std::string> a_Args, RTLIL::Design *a_Design) override
    {
        log_header(a_Design, "Executing NEXUS_DSP pass.\n");

        extra_args(a_Args, 1, a_Design);

        // Process modules
        for (auto module : a_Design->selected_modules()) {

            // Setup the SigMap
            m_SigMap.clear();
            m_SigMap.set(module);

            // Integrate output flip-flops of DSP cells
            integrateOutputFlipFlops(module);
        }

        // Clear maps
        m_SigMap.clear();
    }

    pool<CellPin> getSinks (const CellPin& a_Driver) {

        auto module = a_Driver.cell->module;
        pool<CellPin> sinks;

        // The driver has to be an output pin
        if (!a_Driver.cell->output(a_Driver.port)) {
            return sinks;
        }

        // Get the driver sigbit
        auto driverSigspec = a_Driver.cell->getPort(a_Driver.port);
        auto driverSigbit = m_SigMap(driverSigspec.bits().at(a_Driver.bit));

        // Look for connected sinks
        for (auto cell : module->cells()) {
            for (auto conn : cell->connections()) {
                auto port = conn.first;
                auto sigspec = conn.second;

                // Consider only sinks (inputs)
                if (!cell->input(port)) {
                    continue;
                }

                // Check all sigbits
                auto sigbits = sigspec.bits();
                for (size_t bit = 0; bit < sigbits.size(); ++bit) {

                    auto sigbit = sigbits[bit];
                    if (!sigbit.wire) {
                        continue;
                    }

                    // Got a sink pin of another cell
                    sigbit = m_SigMap(sigbit);
                    if (sigbit == driverSigbit) {
                        sinks.insert(CellPin(cell, port, bit));
                    }
                }
            }
        }

        // Look for connected top-level output ports
        for (auto conn : module->connections()) {
            auto dst = conn.first;
            auto src = conn.second;

            auto sigbits = dst.bits();
            for (size_t bit = 0; bit < sigbits.size(); ++bit) {

                auto sigbit = sigbits[bit];
                if (!sigbit.wire) {
                    continue;
                }

                if (!sigbit.wire->port_output) {
                    continue;
                }

                sigbit = m_SigMap(sigbit);
                if (sigbit == driverSigbit) {
                    sinks.insert(CellPin(nullptr, sigbit.wire->name, bit));
                }
            }
        }

        return sinks;
    }

    // Retrieves Flip-flop data needed for the FlopData struct
    FlopData getFlopData (RTLIL::Cell* a_Cell) {

        // Double check if this is a flip-flop
        log_assert(m_FlopTypes.count(a_Cell->type) != 0);

        // Get set/reset connection
        RTLIL::SigBit rst;
        if (a_Cell->hasPort(RTLIL::escape_id("CD"))) {
            rst = m_SigMap(a_Cell->getPort(RTLIL::escape_id("CD")).bits().at(0));
        }
        else if (a_Cell->hasPort(RTLIL::escape_id("PD"))) {
            rst = m_SigMap(a_Cell->getPort(RTLIL::escape_id("PD")).bits().at(0));
        }
        else {
            log_error("The flip-flop of type '%s' does not have neither 'CD' nor 'PD' port", a_Cell->type.c_str());
        }
        
        // Fill the data
        return FlopData (
            a_Cell->type,
            m_SigMap(a_Cell->getPort(RTLIL::escape_id("CK")).bits().at(0)),
            m_SigMap(a_Cell->getPort(RTLIL::escape_id("SP")).bits().at(0)),
            rst,
            a_Cell->parameters.at(RTLIL::escape_id("GSR"))
        );
    }

    // Integrates flip-flops into DSP cell output registers
    void integrateOutputFlipFlops (RTLIL::Module* a_Module) {

        pool<RTLIL::Cell*> flopsToRemove;

        log("Integrating flip-flops with outputs of DSP cells...\n");
        for (auto cell : a_Module->cells()) {

            // Skip non-DSP cells
            if (!m_DspTypes.count(cell->type)) {
                continue;
            }

            // Get output port connection
            auto outSigspec = cell->getPort(RTLIL::escape_id("Z"));
            auto outSigbits = outSigspec.bits();

            // Collect output flip-flops
            std::vector<RTLIL::Cell*> flops (outSigbits.size(), nullptr);
            pool<FlopData> types;

            for (size_t i=0; i<outSigbits.size(); ++i) {
                auto sigbit = outSigbits[i];

                if (!sigbit.wire) {
                    continue;
                }

                // Get sinks(s)
                auto sinks = getSinks(CellPin(cell, RTLIL::escape_id("Z"), i));

                // More than one sink, abort
                // TODO: Possible handle case when a DSP drivers two or more
                // parallel flip-flops
                if (sinks.size() > 1) {
                    flops.clear();
                    types.clear();
                    break;
                }

                // No sinks - output unconnected
                if (sinks.empty()) {
                    continue;
                }

                // Get the sink, check if this is a flip-flop
                auto* flop = (*sinks.begin()).cell;
                if (flop == nullptr || !m_FlopTypes.count(flop->type)) {
                    continue;
                }

                // Store the connection and add the type data
                flops[i] = flop;
                types.insert(getFlopData(flop));
            }

            // Check if all of the flip-flops are of the same type and
            // settings+connectivity
            if (flops.empty() || types.size() != 1) {
                continue;
            }
            auto flopData = (*types.begin());

            // Log / Debug
            log(" %s %s\n", cell->type.c_str(), cell->name.c_str());
            for (size_t i=0; i<flops.size(); ++i) {
                if (flops[i] != nullptr) {
                    log_debug("  %2zu. %s %s\n", i,
                        flops[i]->type.c_str(), flops[i]->name.c_str());
                }
                else {
                    log_debug("  %2zu. None\n", i);
                }
            }

            // Re-connect data, mark the flip-flop for removal
            for (size_t i=0; i<flops.size(); ++i) {
                if (flops[i] == nullptr) {
                    continue;
                }

                auto sigbit = flops[i]->getPort(RTLIL::escape_id("Q")).bits().at(0);
                outSigbits[i] = sigbit;

                flopsToRemove.insert(flops[i]);
            }
            cell->setPort(RTLIL::escape_id("Z"), RTLIL::SigSpec(outSigbits));

            // Connect control signals
            cell->setPort(RTLIL::escape_id("CLK"),    RTLIL::SigSpec({flopData.clk}));
            cell->setPort(RTLIL::escape_id("CEOUT"),  RTLIL::SigSpec({flopData.ena}));
            cell->setPort(RTLIL::escape_id("RSTOUT"), RTLIL::SigSpec({flopData.rst}));

            // Set control parameters
            cell->setParam(RTLIL::escape_id("GSR"), flopData.gsr);
            cell->setParam(RTLIL::escape_id("REGOUTPUT"), RTLIL::Const("REGISTER"));

            if (cell->type == RTLIL::escape_id("FD1P3DX")) {
                cell->setParam(RTLIL::escape_id("GSR"), RTLIL::Const("ASYNC"));
            }
            if (cell->type == RTLIL::escape_id("FD1P3IX")) {
                cell->setParam(RTLIL::escape_id("GSR"), RTLIL::Const("SYNC"));
            }
        }

        // Remove the flip-flops
        for (const auto& flop : flopsToRemove) {
            a_Module->remove(flop);
        }
    }

} NexusDsp;

PRIVATE_NAMESPACE_END
