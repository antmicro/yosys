module dut();
logic [3:0][3:0][7:0] multirange_logic;
logic [15:0] multirange_logic_cpy;

assign multirange_logic[3][3:2] = 16'b1010101010101010;
assign multirange_logic[3][1:0] = 16'b0000000000000000;

assign multirange_logic_cpy = multirange_logic[3][2:1];

always_comb begin
  assert(multirange_logic_cpy == 16'b1010101000000000);
end

endmodule
