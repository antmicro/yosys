//                FZ        FS
(* abc9_lut=1, lib_whitebox *)
module LUT1 (
  output O,
  input I0
);
  parameter [1:0] INIT = 0;
  parameter EQN = "(I0)";

  assign O = I0 ? INIT[1] : INIT[0];
endmodule

//               TZ        TSL TAB
(* abc9_lut=1, lib_whitebox *)
module LUT2 (
  output O,
  input I0, I1
);
  parameter [3:0] INIT = 4'h0;
  parameter EQN = "(I0)";

  wire [1:0] s1 = I1 ? INIT[3:2] : INIT[1:0];
  assign O = I0 ? s1[1] : s1[0];
endmodule

// O:  TZ
// I0: TA1 TA2 TB1 TB2
// I1: TSL
// I2: TAB
(* abc9_lut=1, lib_whitebox *)
module LUT3 (
  output O,
  input I0, I1, I2
);
  parameter [7:0] INIT = 8'h0;
  parameter EQN = "(I0)";

  wire [3:0] s2 = I2 ? INIT[7:4] : INIT[3:0];
  wire [1:0] s1 = I1 ? s2[3:2] : s2[1:0];
  assign O = I0 ? s1[1] : s1[0];
endmodule

// O:  CZ
// I0: TA1 TA2 TB1 TB2 BA1 BA2 BB1 BB2
// I1: TSL BSL
// I2: TAB BAB
// I3: TBS
(* abc9_lut=1, lib_whitebox *)
module LUT4 (
  output O,
  input I0, I1, I2, I3
);
  parameter [15:0] INIT = 16'h0;
  parameter EQN = "(I0)";

  wire [7:0] s3 = I3 ? INIT[15:8] : INIT[7:0];
  wire [3:0] s2 = I2 ? s3[7:4] : s3[3:0];
  wire [1:0] s1 = I1 ? s2[3:2] : s2[1:0];
  assign O = I0 ? s1[1] : s1[0];
endmodule

(* abc9_lut=1, lib_whitebox *)
module LUT5 (
  output O,
  input I0, I1, I2, I3, I4
);
  parameter [31:0] INIT = 32'h0;
  parameter EQN = "(I0)";

  wire [15:0] s4 = I4 ? INIT[31:16] : INIT[15:0];
  wire [ 7:0] s3 = I3 ? s4[15:8] : s4[7:0];
  wire [ 3:0] s2 = I2 ? s3[7:4] : s3[3:0];
  wire [ 1:0] s1 = I1 ? s2[3:2] : s2[1:0];
  assign O = I0 ? s1[1] : s1[0];
endmodule
