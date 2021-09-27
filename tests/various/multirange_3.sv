typedef struct packed {
  logic a_p;
  logic a_n;
} pinout_t;


typedef struct packed {
   logic [1:0] s1;
   logic [2:0] s2;
   logic       s3;
} parity_t;

typedef struct packed {
   logic                  [2:0]   var1;
   logic                  [2:0]   var2;
   parity_t                       var3;
} complex_struct_t;

parameter int Size = 4;

module dut();

logic [3:0][3:0][7:0] multirange_logic;
logic [15:0] multirange_logic_cpy;

pinout_t [Size*2-1:0] pinout = 16'b1011010110110010;
logic [5:0] pinout_part_cpy;

complex_struct_t complex_hw [3];
complex_struct_t complex_hw_cpy;

pinout_t[1:0][1:0] complex_hw_multirange[2:0][1:0];
pinout_t[1:0][1:0] hw_multirange_cpy[2:0][1:0];

localparam int N_HARTS  = 2;
localparam int N_TIMERS = 1;
logic [3:0] mtime            [N_HARTS];
logic [3:0] mtimecmp         [N_HARTS][N_TIMERS];
logic       mtimecmp_update  [1][1];

assign multirange_logic[3][3:2] = 16'b1010101010101010;
assign multirange_logic[3][1:0] = 16'b0000000000000000;

assign multirange_logic_cpy = multirange_logic[3][2:1];

assign pinout_part_cpy = pinout[4:2];

assign complex_hw[1] = 12'b110010011100;
assign complex_hw_cpy = complex_hw[1];

assign complex_hw_multirange = 48'b100101100101100101100101100101100101100101100101;
assign hw_multirange_cpy = complex_hw_multirange;

assign mtime[0] = 4'b1100;
assign mtime[1] = 4'b0011;

assign mtimecmp_update[0][0] = 1;

assign mtimecmp[0][0][3:2] = mtime[0][3:2];
assign mtimecmp[1][0][1:0] = mtime[1][1:0];

always_comb begin
  assert(multirange_logic_cpy == 16'b1010101000000000);

  assert(pinout_part_cpy == 6'b011011);

  assert(complex_hw_cpy == 12'b110010011100);
  assert(complex_hw_cpy == complex_hw[1]);

  assert(complex_hw_cpy[3] == 1);

  assert(mtimecmp[0][0][3:2] == mtimecmp[1][0][1:0]);
  assert(mtimecmp_update[0][0] == 1);

  assert(complex_hw_multirange == hw_multirange_cpy);
  // TODO fix mem access
  assert(hw_multirange_cpy[0][1] == 8'b01011001);

end

endmodule
