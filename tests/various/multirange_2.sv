package pkg;
parameter int unsigned Num  = 4;
endpackage

module mdia(
  input logic [pkg::Num-1:0][pkg::Num-1:0] var1_i,
  input logic [3:0][3:0][7:0] var2_i
);

always_comb begin
  assert(var1_i[0][0] == 1'b1);
  assert(var1_i[3][2] == 1'b0);
  assert(var2_i[0][0][0] == 1'b0);
  assert(var2_i[2][1][3] == 1'b1);
end
endmodule

module dut();

logic [pkg::Num-1:0][pkg::Num-1:0] var1;
logic [3:0][3:0][7:0] var2;

assign var1[0][0] = 1'b1;
assign var1[3][2] = 1'b0;

assign var2[0][0][0] = 1'b0;
assign var2[2][1][3] = 1'b1;

mdia md(var1, var2);

endmodule
