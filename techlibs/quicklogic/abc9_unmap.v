module \$__PP3_DFFEPC_SYNCONLY (
  output reg Q,
  input D,
  input CLK,
  input EN,
);

dffepc _TECHMAP_REPLACE_ (.Q(Q), .D(D), .CLK(CLK), .EN(EN), .PRE(1'b0), .CLR(1'b0));

endmodule
