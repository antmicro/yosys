#include "kernel/register.h"
#include "kernel/celltypes.h"
#include "kernel/rtlil.h"
#include "kernel/log.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

struct SynthQuickLogicPass : public ScriptPass
{
    SynthQuickLogicPass() : ScriptPass("synth_quicklogic", "Synthesis for QuickLogic FPGAs") { }

    void help() override
    {
        log("\n");
        log("   synth_quicklogic [options]\n");
        log("This command runs synthesis for QuickLogic FPGAs\n");
        log("\n");
        log("    -top <module>\n");
        log("         use the specified module as top module\n");
        log("\n");
        log("    -family <family>\n");
        log("        run synthesis for the specified QuickLogic architecture\n");
        log("        generate the synthesis netlist for the specified family.\n");
        log("        supported values:\n");
        log("        - pp3: PolarPro 3 \n");
        log("        - ap3: ArcticPro 3 \n");
        log("\n");
        log("    -edif <file>\n");
        log("        write the design to the specified edif file. writing of an output file\n");
        log("        is omitted if this parameter is not specified.\n");
        log("\n");
        log("    -adder\n");
        log("        use adder cells in output netlist\n");
        log("\n");
        log("    -blif <file>\n");
        log("        write the design to the specified BLIF file. writing of an output file\n");
        log("        is omitted if this parameter is not specified.\n");
        log("\n");
        help_script();
        log("\n");
    }

    std::string top_opt = "";
    std::string edif_file = "";
    std::string blif_file = "";
    std::string currmodule = "";
    std::string family = "";
    bool inferAdder;
    
    void clear_flags() override
    {
        top_opt = "-auto-top";
        family = "pp3";
        inferAdder = false;
        edif_file.clear();
        blif_file.clear();
    }

    void execute(std::vector<std::string> args, RTLIL::Design *design) override
    {
        std::string run_from, run_to;
        clear_flags();

        size_t argidx;
        for (argidx = 1; argidx < args.size(); argidx++)
        {
            if (args[argidx] == "-top" && argidx+1 < args.size()) {
                top_opt = "-top " + args[++argidx];
                continue;
            }
            if (args[argidx] == "-family" && argidx+1 < args.size()) {
                family = args[++argidx];
                continue;
            }
            if (args[argidx] == "-edif" && argidx+1 < args.size()) {
                edif_file = args[++argidx];
                continue;
            }
            if (args[argidx] == "-adder") {
                inferAdder = true;
                continue;
            }
            if (args[argidx] == "-blif" && argidx+1 < args.size()) {
                blif_file = args[++argidx];
                continue;
            }
            break;
        }
        extra_args(args, argidx, design);

        if (!design->full_selection())
            log_cmd_error("This command only operates on fully selected designs!\n");

        log_header(design, "Executing SYNTH_QUICKLOGIC pass.\n");
        log_push();

        run_script(design, run_from, run_to);

        log_pop();
    }

    void script() override
    {
        if (check_label("begin")) {
            std::string readVelArgs = "+/quicklogic/" + family + "_cells_sim.v";
            run("read_verilog -lib -specify " + readVelArgs);

            run("read_verilog -lib +/quicklogic/cells_sim.v");

            run(stringf("hierarchy -check %s", top_opt.c_str()));
        }

        if (check_label("prepare")) {
            run("proc");
            run("flatten");
            run("wreduce -keepdc");
            run("muxpack");
        }

        if (check_label("coarse")) {
            run("tribuf -logic");
            run("deminout");
            run("opt");
            run("opt_clean");
            run("peepopt");
            run("techmap");
            std::string abc_opts;

            if (family == "pp3") {
                run("muxcover -mux8 -mux4");
                abc_opts += " -luts 1,2,2,4";
            } else if (family == "ap3") {
                // Prefer LUT4 over any other size
                abc_opts += " -luts 3,2,1,0";
            }
            run("abc" + abc_opts);
            run("opt");
            run("check");
        }

        if (check_label("map")) {
            std::string techMapArgs = " -map +/quicklogic/cells_map.v";
            techMapArgs += " -map +/quicklogic/" + family + "_cells_map.v";
            if (inferAdder) {
                techMapArgs += " -map +/quicklogic/" + family + "_arith_map.v";
                run("techmap" + techMapArgs);
            } else {
                run("techmap" + techMapArgs);
            }
            run("opt_clean");
            run("check");
            run("autoname");
        }

        if (check_label("iomap")) {
            if (family == "pp3") {
                run("clkbufmap -buf $_BUF_ Y:A -inpad ckpad Q:P");
                run("iopadmap -bits -outpad outpad A:P -inpad inpad Q:P -tinoutpad bipad EN:Q:A:P A:top");
            } else if (family == "ap3") {
                run("clkbufmap -buf $_BUF_ Y:A -inpad ck_buff Q:A");
                run("iopadmap -bits -outpad out_buff A:Q -inpad in_buff Q:A -toutpad EN:A:Q A:top");
            }
        }

        if (check_label("finalize")) {
            run("splitnets -ports -format ()");
            run("setundef -zero -params -undriven");
            run("hilomap -hicell logic_1 a -locell logic_0 a -singleton A:top");
            run("opt_clean");
            run("check");
        }

        if (check_label("edif")) {
            if (!edif_file.empty() || help_mode) {
                run(stringf("write_edif -nogndvcc -attrprop -pvector par %s %s", this->currmodule.c_str(), edif_file.c_str()));
            }
        }

        if (check_label("blif")) {
            if (!blif_file.empty() || help_mode)
                run(stringf("write_blif %s %s", top_opt.c_str(), blif_file.c_str()));
        }
    }
} SynthQuickLogicPass;

PRIVATE_NAMESPACE_END
