(* abc9_lut=1, lib_whitebox *)
module LUT4 (
  output O,
  input I0,
  input I1,
  input I2,
  input I3
);
<<<<<<< HEAD
    parameter [15:0] INIT = 16'h0;
    parameter EQN = "(I0)";
    
    assign O = INIT[{I3, I2, I1, I0}];
endmodule

(* abc9_flop, lib_whitebox *)
module ff(
    output reg CQZ,
    input D,
    (* clkbuf_sink *)
    input QCK,
    input QEN,
    input QRT,
    input QST
=======
  parameter [15:0] INIT = 16'h0;
  parameter EQN = "(I0)";

  wire [7:0] s3 = I3 ? INIT[15:8] : INIT[7:0];
  wire [3:0] s2 = I2 ? s3[7:4] : s3[3:0];
  wire [1:0] s1 = I1 ? s2[3:2] : s2[1:0];
  assign O = I0 ? s1[1] : s1[0];
endmodule

(* abc9_box, lib_whitebox *)
module FF (
  output reg CQZ,
  input D,
  //(* clkbuf_sink *)
  input QCK,
  input QEN,
  //(* clkbuf_sink *)
  input QRT,
  //(* clkbuf_sink *)
  input QST
>>>>>>> 1b3212e63 (ql: Format verilog files)
);
  parameter [0:0] INIT = 1'b0;
  initial CQZ = INIT;

  always @(posedge QCK or posedge QRT or posedge QST)
    if (QRT) CQZ <= 1'b0;
    else if (QST) CQZ <= 1'b1;
    else if (QEN) CQZ <= D;
endmodule

<<<<<<< HEAD
(* abc9_flop, lib_whitebox *)
module openfpga_ff(
    output reg CQZ,
    input D,
    input QCK
);
    parameter [0:0] INIT = 1'b0;
    initial CQZ = INIT;

    always @(posedge QCK)
        CQZ <= D;
endmodule

module full_adder(
   output S,
   output CO,
   input A,
   input B,
   input CI
=======
module FULL_ADDER (
  output S,
  output CO,
  input A,
  input B,
  input CI
>>>>>>> 1b3212e63 (ql: Format verilog files)
);

  assign {CO, S} = A + B + CI;
endmodule

(* lib_whitebox *)
module QL_CARRY (
  output CO,
  input I0,
  input I1,
  input CI
);
  assign CO = ((I0 ^ I1) & CI) | (~(I0 ^ I1) & (I0 & I1));
endmodule

<<<<<<< HEAD
module ck_buff ( 
	output Q,
    (* iopad_external_pin *)
	input A
=======
module CK_BUFF (
  output Q,
  (* iopad_external_pin *)
  input A
>>>>>>> 1b3212e63 (ql: Format verilog files)
);

  assign Q = A;

<<<<<<< HEAD
module in_buff ( 
	output Q,
    (* iopad_external_pin *)
	input A
=======
endmodule  /* ck buff */

module IN_BUFF (
  output Q,
  (* iopad_external_pin *)
  input A
>>>>>>> 1b3212e63 (ql: Format verilog files)
);

  assign Q = A;

endmodule  /* in buff */

<<<<<<< HEAD
module out_buff ( 
    (* iopad_external_pin *)
	output Q,
	input A
=======
module OUT_BUFF (
  (* iopad_external_pin *)
  output Q,
  input A
>>>>>>> 1b3212e63 (ql: Format verilog files)
);

  assign Q = A;

endmodule  /* out buff */

<<<<<<< HEAD
module d_buff ( 
    (* iopad_external_pin *)
	output Q
=======
module D_BUFF (
  (* iopad_external_pin *)
  output Q
>>>>>>> 1b3212e63 (ql: Format verilog files)
);
  parameter DSEL = 1'b0;
  assign Q = DSEL ? 1'b1 : 1'b0;

endmodule  /* d buff */

<<<<<<< HEAD
module in_reg (
	output dataOut,
    (* clkbuf_sink *)
	input clk, 
	input rst, 
	(* iopad_external_pin *)
	input dataIn
=======
module IN_REG (
  output dataOut,
  (* clkbuf_inhibit *)
  input clk,
  (* iopad_external_pin *)
  input rst,
  (* iopad_external_pin *)
  input dataIn
>>>>>>> 1b3212e63 (ql: Format verilog files)
);

  parameter ISEL = 0;
  parameter FIXHOLD = 0;

  wire dataIn_reg_int, dataIn_reg_int_buff;
  wire fixhold_mux_op;

  reg  iqz_reg;

  assign dataIn_reg_int = dataIn;
  assign dataIn_reg_int_buff = dataIn_reg_int;
  assign fixhold_mux_op = (FIXHOLD) ? dataIn_reg_int_buff : dataIn_reg_int;

  always @(posedge clk or posedge rst) begin
    if (rst) iqz_reg <= 1'b0;
    else iqz_reg <= fixhold_mux_op;
  end

  assign dataOut = (ISEL) ? dataIn_reg_int : iqz_reg;

endmodule  /* in_reg*/

<<<<<<< HEAD
module out_reg (
	(* iopad_external_pin *)
	output dataOut,
    (* clkbuf_sink *)
	input clk, 
	input rst, 
	input dataIn
=======
module OUT_REG (
  (* iopad_external_pin *)
  output dataOut,
  (* clkbuf_inhibit *)
  input clk,
  (* iopad_external_pin *)
  input rst,
  input dataIn
>>>>>>> 1b3212e63 (ql: Format verilog files)
);

  parameter OSEL = 0;
  wire sel_mux_op;

  reg  dataOut_reg;

  always @(posedge clk or posedge rst) begin
    if (rst) dataOut_reg <= 1'b0;
    else dataOut_reg <= dataIn;
  end

  assign sel_mux_op = (OSEL) ? dataIn : dataOut_reg;
  assign dataOut = sel_mux_op;

endmodule  /* out_reg*/

(* blackbox *)
<<<<<<< HEAD
module RAM (RADDR,RRLSEL,REN,RMODE,
	    WADDR,WDATA,WEN,WMODE,
	    FMODE,FFLUSH,RCLK,WCLK,RDATA,
	    FFLAGS,FIFO_DEPTH,ENDIAN,POWERDN,PROTECT,
	    UPAE,UPAF,SBOG);

   input [10:0] RADDR,WADDR;
   input [1:0] 	RRLSEL,RMODE,WMODE;
   input 	REN,WEN,FFLUSH;
   (* clkbuf_sink *)
   input RCLK, WCLK;
   input [31:0] WDATA;
   input [1:0] 	SBOG, ENDIAN, UPAF, UPAE;
   output [31:0] RDATA;
   output [3:0]  FFLAGS;
   input [2:0] 	 FIFO_DEPTH;
   input 	 FMODE, POWERDN, PROTECT;
   

     DPRAM_FIFO U1(
		 .RCLK(RCLK), .REN(REN), .WCLK(WCLK), .WEN(WEN),
		 .WADDR(WADDR),.WDATA(WDATA),.RADDR(RADDR), .RDATA(RDATA),
		 .RMODE(RMODE), .WMODE(WMODE),
		 .TLD(), .TRD(),          // SideBand bus outputs
		 .FRD(32'h0), .FLD(32'h0),  // SideBand bus inputs
		 .FFLAGS(FFLAGS), // unused FIFO flags
		 .FMODE(FMODE), .FFLUSH(FFLUSH),.RRLSEL(RRLSEL),
		 .PROTECT(PROTECT),
		 .PL_INIT(1'b0), .PL_ENA(1'b0), .PL_CLK(1'b0),
		 .PL_REN(1'b0), .PL_WEN(1'b0),
		 .PL_ADDR(20'h0), .PL_DATA_IN(32'h0), .PL_DATA_OUT(),
		 .ENDIAN(ENDIAN), .FIFO_DEPTH(FIFO_DEPTH), .RAM_ID(8'h00),
		 .POWERDN(POWERDN), .SBOG(SBOG),
		 .UPAE(UPAE), .UPAF(UPAF),
		 .DFT_SCAN_CLK_DAISYIN(1'b0), .DFT_SCAN_RST_DAISYIN(1'b0),
		 .DFT_SCAN_MODE_DAISYIN(1'b0), .DFT_SCAN_EN_DAISYIN(1'b0),
		 .DFT_SCAN_IN_DAISYIN(1'b0), .dft_FFB_scan_out()
		 );
endmodule 

(* blackbox *)
module DSP (MODE_SEL,COEF_DATA,OPER_DATA,OUT_SEL,ENABLE,CLR,RND,SAT,CLOCK,MAC_OUT,CSEL,OSEL,SBOG);

input [1:0] MODE_SEL,OUT_SEL;
input [1:0] CSEL;
input [1:0] OSEL;
input [31:0] COEF_DATA,OPER_DATA;
input ENABLE,CLR,RND,SAT;
(* clkbuf_sink *)
input CLOCK;
input [1:0]SBOG;
output [63:0] MAC_OUT;


dsp_top U1 (
	       .oper(OPER_DATA), .coef(COEF_DATA),
	       .outsel(OUT_SEL),
	       .mode(MODE),
	       .clk(CLOCK), .clr(CLR), .ena(ENABLE),
	       .fld(32'h0), .frd(32'h0), .sbog(SBOG),
	       .rnd(RND), .sat(SAT),
	       .o_sel(OSEL), .c_sel(CSEL),
	       .mac_out(MAC_OUT),
	       .tld(), .trd(),
	       .DFT_SCAN_CLK_DAISYIN(1'b0),
	       .DFT_SCAN_RST_DAISYIN(1'b0),
	       .DFT_SCAN_MODE_DAISYIN(1'b0),
	       .DFT_SCAN_EN_DAISYIN(1'b0),
	       .DFT_SCAN_IN_DAISYIN(1'b0),
	       .dft_FFB_scan_out()
	       );
endmodule 
=======
module RAM (
  RADDR,
  RRLSEL,
  REN,
  RMODE,
  WADDR,
  WDATA,
  WEN,
  WMODE,
  FMODE,
  FFLUSH,
  RCLK,
  WCLK,
  RDATA,
  FFLAGS,
  FIFO_DEPTH,
  ENDIAN,
  POWERDN,
  PROTECT,
  UPAE,
  UPAF,
  SBOG
);

  input [10:0] RADDR, WADDR;
  input [1:0] RRLSEL, RMODE, WMODE;
  input REN, WEN, FFLUSH, RCLK, WCLK;
  input [31:0] WDATA;
  input [1:0] SBOG, ENDIAN, UPAF, UPAE;
  output [31:0] RDATA;
  output [3:0] FFLAGS;
  input [2:0] FIFO_DEPTH;
  input FMODE, POWERDN, PROTECT;

  DPRAM_FIFO U1 (
    .RCLK(RCLK),
    .REN(REN),
    .WCLK(WCLK),
    .WEN(WEN),
    .WADDR(WADDR),
    .WDATA(WDATA),
    .RADDR(RADDR),
    .RDATA(RDATA),
    .RMODE(RMODE),
    .WMODE(WMODE),
    .TLD(),
    .TRD(),  // SideBand bus outputs
    .FRD(32'h0),
    .FLD(32'h0),  // SideBand bus inputs
    .FFLAGS(FFLAGS),  // unused FIFO flags
    .FMODE(FMODE),
    .FFLUSH(FFLUSH),
    .RRLSEL(RRLSEL),
    .PROTECT(PROTECT),
    .PL_INIT(1'b0),
    .PL_ENA(1'b0),
    .PL_CLK(1'b0),
    .PL_REN(1'b0),
    .PL_WEN(1'b0),
    .PL_ADDR(20'h0),
    .PL_DATA_IN(32'h0),
    .PL_DATA_OUT(),
    .ENDIAN(ENDIAN),
    .FIFO_DEPTH(FIFO_DEPTH),
    .RAM_ID(8'h00),
    .POWERDN(POWERDN),
    .SBOG(SBOG),
    .UPAE(UPAE),
    .UPAF(UPAF),
    .DFT_SCAN_CLK_DAISYIN(1'b0),
    .DFT_SCAN_RST_DAISYIN(1'b0),
    .DFT_SCAN_MODE_DAISYIN(1'b0),
    .DFT_SCAN_EN_DAISYIN(1'b0),
    .DFT_SCAN_IN_DAISYIN(1'b0),
    .dft_FFB_scan_out()
  );
endmodule

(* blackbox *)
module DSP (
  MODE_SEL,
  COEF_DATA,
  OPER_DATA,
  OUT_SEL,
  ENABLE,
  CLR,
  RND,
  SAT,
  CLOCK,
  MAC_OUT,
  CSEL,
  OSEL,
  SBOG
);

  input [1:0] MODE_SEL, OUT_SEL;
  input [1:0] CSEL;
  input [1:0] OSEL;
  input [31:0] COEF_DATA, OPER_DATA;
  input ENABLE, CLR, RND, SAT, CLOCK;
  input [1:0] SBOG;
  output [63:0] MAC_OUT;

  dsp_top U1 (
    .oper(OPER_DATA),
    .coef(COEF_DATA),
    .outsel(OUT_SEL),
    .mode(MODE),
    .clk(CLOCK),
    .clr(CLR),
    .ena(ENABLE),
    .fld(32'h0),
    .frd(32'h0),
    .sbog(SBOG),
    .rnd(RND),
    .sat(SAT),
    .o_sel(OSEL),
    .c_sel(CSEL),
    .mac_out(MAC_OUT),
    .tld(),
    .trd(),
    .DFT_SCAN_CLK_DAISYIN(1'b0),
    .DFT_SCAN_RST_DAISYIN(1'b0),
    .DFT_SCAN_MODE_DAISYIN(1'b0),
    .DFT_SCAN_EN_DAISYIN(1'b0),
    .DFT_SCAN_IN_DAISYIN(1'b0),
    .dft_FFB_scan_out()
  );
endmodule
>>>>>>> 1b3212e63 (ql: Format verilog files)
