OBJS += techlibs/quicklogic/pp3_braminit.o
OBJS += techlibs/quicklogic/ql-dsp.o
OBJS += techlibs/quicklogic/ql-edif.o
OBJS += techlibs/quicklogic/quicklogic_eqn.o
OBJS += techlibs/quicklogic/synth_quicklogic.o

COMMON          = common
QLF_K4N8_DIR    = qlf_k4n8
QLF_K6N10_DIR   = qlf_k6n10
QLF_K6N10F_DIR  = qlf_k6n10f
PP3_DIR         = pp3

VERILOG_MODULES = $(COMMON)/cells_sim.v         \
                  $(QLF_K4N8_DIR)/arith_map.v   \
                  $(QLF_K4N8_DIR)/cells_sim.v   \
                  $(QLF_K4N8_DIR)/ffs_map.v     \
                  $(QLF_K6N10_DIR)/arith_map.v \
                  $(QLF_K6N10_DIR)/brams_map.v \
                  $(QLF_K6N10_DIR)/brams.txt   \
                  $(QLF_K6N10_DIR)/cells_sim.v \
                  $(QLF_K6N10_DIR)/ffs_map.v   \
                  $(QLF_K6N10_DIR)/dsp_map.v   \
                  $(QLF_K6N10_DIR)/lut_map.v   \
                  $(QLF_K6N10F_DIR)/arith_map.v \
                  $(QLF_K6N10F_DIR)/brams_map.v \
                  $(QLF_K6N10F_DIR)/brams.txt   \
                  $(QLF_K6N10F_DIR)/cells_sim.v \
                  $(QLF_K6N10F_DIR)/ffs_map.v   \
                  $(QLF_K6N10F_DIR)/dsp_map.v   \
                  $(PP3_DIR)/abc9_map.v    \
                  $(PP3_DIR)/abc9_model.v  \
                  $(PP3_DIR)/abc9_unmap.v  \
                  $(PP3_DIR)/cells_map.v   \
                  $(PP3_DIR)/cells_sim.v   \
                  $(PP3_DIR)/ffs_map.v     \
                  $(PP3_DIR)/latches_map.v \
                  $(PP3_DIR)/lut_map.v     \
                  $(PP3_DIR)/lutdefs.txt   \
                  $(PP3_DIR)/brams_sim.v   \
                  $(PP3_DIR)/brams_map.v   \
                  $(PP3_DIR)/brams.txt     \
                  $(PP3_DIR)/bram_init_8_16.vh \
                  $(PP3_DIR)/bram_init_32.vh   \
                  $(PP3_DIR)/qlal4s3b_sim.v    \
                  $(PP3_DIR)/mult_sim.v        \
                  $(PP3_DIR)/qlal3_sim.v       \

$(foreach f,$(VERILOG_MODULES),$(eval $(call add_share_file,share/quicklogic/$(dir $f),techlibs/quicklogic/$f)))
