package pkg;
parameter int unsigned Num  = 4;
endpackage

module mdia(
  input logic [pkg::Num-1:0][pkg::Num-1:0] var1_i,
  input logic [3:0][3:0][7:0] var2_i,
  output logic [3:0][3:0][7:0] var3_o
);
  logic [3:0][3:0][7:0] out;
  assign out[1][1][2] = 1'b1;
  assign out[2][1][2] = 1'b0;
  assign var3_o = out;

always_comb begin
  assert(var1_i[0][0] == 1'b1);
  assert(var1_i[3][2] == 1'b0);
  assert(var2_i[0][0][0] == 1'b0);
  assert(var2_i[2][1][3] == 1'b1);
end
endmodule

module dut();
parameter [3:0][3:0][7:0] PackedParam = 8'b11000011;
localparam [3:0][3:0][7:0] LocalPackedParam = 8'b11000011;

logic [pkg::Num-1:0][pkg::Num-1:0] var1;
logic [3:0][3:0][7:0] var2;
logic [3:0][3:0][7:0] var3;

logic [3:0][3:0][7:0] var4;
logic [3:0][3:0][7:0] var5;

assign var1[0][0] = 1'b1;
assign var1[3][2] = 1'b0;

assign var2[0][0][0] = 1'b0;
assign var2[2][1][3] = 1'b1;

assign var4 = PackedParam;
assign var5 = LocalPackedParam;

mdia md(var1, var2, var3);

always_comb begin
  assert(var3[1][1][2] == 1'b1);
  assert(var3[2][1][2] == 1'b0);
  assert(var4 == PackedParam);
  assert(var5 == LocalPackedParam);
end

endmodule
