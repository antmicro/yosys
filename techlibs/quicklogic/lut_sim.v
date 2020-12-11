(* abc9_lut=1, lib_whitebox *)
module LUT1 (
  output O,
  input I0
);
  parameter [1:0] INIT = 0;
  parameter EQN = "(I0)";

  // These timings are for PolarPro 3E; other families will need updating.
  specify
    (I0 => O) = (231 + 217, 202 + 198); // FS -> FZ
  endspecify

  assign O = I0 ? INIT[1] : INIT[0];
endmodule

//               TZ        TSL TAB
(* abc9_lut=2, lib_whitebox *)
module LUT2 (
  output O,
  input I0, I1
);
  parameter [3:0] INIT = 4'h0;
  parameter EQN = "(I0)";

  // These timings are for PolarPro 3E; other families will need updating.
  specify
    (I0 => O) = (329 + 332, 349 + 403); // TAB -> TZ
    (I1 => O) = (329 + 380, 350 + 460); // TSL -> TZ
  endspecify

  wire [1:0] s1 = I1 ? INIT[3:2] : INIT[1:0];
  assign O = I0 ? s1[1] : s1[0];
endmodule

(* abc9_lut=2, lib_whitebox *)
module LUT3 (
  output O,
  input I0, I1, I2
);
  parameter [7:0] INIT = 8'h0;
  parameter EQN = "(I0)";

  // These timings are for PolarPro 3E; other families will need updating.
  specify
    (I0 => O) = (278 + 442, 295 + 473); // TA1 -> TZ (positive unate)
    (I1 => O) = (329 + 380, 350 + 460); // TSL -> TZ
    (I2 => O) = (329 + 332, 349 + 403); // TAB -> TZ
  endspecify

  wire [3:0] s2 = I2 ? INIT[7:4] : INIT[3:0];
  wire [1:0] s1 = I1 ? s2[3:2] : s2[1:0];
  assign O = I0 ? s1[1] : s1[0];
endmodule

(* abc9_lut=4, lib_whitebox *)
module LUT4 (
  output O,
  input I0, I1, I2, I3
);
  parameter [15:0] INIT = 16'h0;
  parameter EQN = "(I0)";

  // These timings are for PolarPro 3E; other families will need updating.
  specify
    (I0 => O) = (416 + 580, 434 + 657); // TB1 -> CZ (positive unate)
    (I1 => O) = (415 + 440, 430 + 536); // TSL -> CZ
    (I2 => O) = (415 + 392, 429 + 479); // TAB -> CZ
    (I3 => O) = (549 + 465, 548 + 480); // TBS -> CZ (negative unate)
  endspecify

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

  // These timings are for PolarPro 3E; other families will need updating.
  specify
    (I0 => O) = (416 + 580, 434 + 657); // TB1 -> CZ (positive unate)
    (I1 => O) = (415 + 440, 430 + 536); // TSL -> CZ
    (I2 => O) = (415 + 392, 429 + 479); // TAB -> CZ
    (I3 => O) = (549 + 465, 548 + 480); // TBS -> CZ (negative unate)
    (I4 => O) = (549 + 465, 548 + 480); // TBS -> CZ (negative unate)
  endspecify

  wire [15:0] s4 = I4 ? INIT[31:16] : INIT[15:0];
  wire [ 7:0] s3 = I3 ? s4[15:8] : s4[7:0];
  wire [ 3:0] s2 = I2 ? s3[7:4] : s3[3:0];
  wire [ 1:0] s1 = I1 ? s2[3:2] : s2[1:0];
  assign O = I0 ? s1[1] : s1[0];
endmodule
