package pkg;

parameter int unsigned Num     = 2;

typedef enum logic[1:0] {
  MODE_11 = 2'b11,
  MODE_10 = 2'b10,
  MODE_01 = 2'b01,
  MODE_00 = 2'b00
} mode;
endpackage

module dut();

pkg::mode             mode_i           [pkg::Num];
logic [3:0][3:0][7:0] multirange_logic [2:0][3:0][3:0];
wire  [3:0][3:0][7:0] multirange_wire  [2:0][3:0][2:0];
reg   [3:0][3:0][7:0] multirange_reg   [2:0][3:0][2:0];

assign mode_i[0] = pkg::MODE_11;
assign mode_i[1] = pkg::MODE_01;

assign multirange_logic[0][0][0][0][0][0] = 1'b0;
assign multirange_logic[1][2][1][2][1][0] = 1'b1;
assign multirange_logic[1][2][1][2][1][1] = 1'b0;

assign multirange_logic[1][2][1][3][0]    = 8'b10100011;
assign multirange_logic[1][2][1][3][1]    = 8'b00110110;

assign multirange_wire[0][0][0][0][0][0] = 1'b0;
assign multirange_wire[1][2][1][2][1][0] = 1'b1;
assign multirange_wire[1][2][1][2][1][1] = 1'b0;

assign multirange_wire[1][2][1][3][0]    = 8'b10100011;
assign multirange_wire[1][2][1][3][1]    = 8'b00110110;

assign multirange_reg[0][0][0][0][0][0] = 1'b0;
assign multirange_reg[1][2][1][2][1][0] = 1'b1;
assign multirange_reg[1][2][1][2][1][1] = 1'b0;

assign multirange_reg[1][2][1][3][0]    = 8'b10100011;
assign multirange_reg[1][2][1][3][1]    = 8'b00110110;

always_comb begin
  assert(mode_i[0] == pkg::MODE_11);
  assert(mode_i[1] == pkg::MODE_01);

  assert(multirange_logic[0][0][0][0][0][0] == 1'b0);
  assert(multirange_logic[1][2][1][2][1][0] == 1'b1);
  assert(multirange_logic[1][2][1][2][1][1] == 1'b0);

  assert(multirange_logic[1][2][1][3][0] == 8'b10100011);
  assert(multirange_logic[1][2][1][3][1] == 8'b00110110);

  assert(multirange_wire[0][0][0][0][0][0] == 1'b0);
  assert(multirange_wire[1][2][1][2][1][0] == 1'b1);
  assert(multirange_wire[1][2][1][2][1][1] == 1'b0);

  assert(multirange_wire[1][2][1][3][0] == 8'b10100011);
  assert(multirange_wire[1][2][1][3][1] == 8'b00110110);

  assert(multirange_reg[0][0][0][0][0][0] == 1'b0);
  assert(multirange_reg[1][2][1][2][1][0] == 1'b1);
  assert(multirange_reg[1][2][1][2][1][1] == 1'b0);

  assert(multirange_reg[1][2][1][3][0] == 8'b10100011);
  assert(multirange_reg[1][2][1][3][1] == 8'b00110110);
end

endmodule
