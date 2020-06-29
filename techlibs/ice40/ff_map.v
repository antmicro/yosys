module  \$_DFF_N_ (input D, C, output Q); SB_DFFN _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C)); endmodule
module  \$_DFF_P_ (input D, C, output Q); SB_DFF  _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C)); endmodule

module  \$_DFFE_NN_ (input D, C, E, output Q); SB_DFFNE _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .E(!E)); endmodule
module  \$_DFFE_PN_ (input D, C, E, output Q); SB_DFFE  _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .E(!E)); endmodule

module  \$_DFFE_NP_ (input D, C, E, output Q); SB_DFFNE _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .E(E)); endmodule
module  \$_DFFE_PP_ (input D, C, E, output Q); SB_DFFE  _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .E(E)); endmodule

module  \$_DFF_NN0_ (input D, C, R, output Q); SB_DFFNR _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .R(!R)); endmodule
module  \$_DFF_NN1_ (input D, C, R, output Q); SB_DFFNS _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .S(!R)); endmodule
module  \$_DFF_PN0_ (input D, C, R, output Q); SB_DFFR  _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .R(!R)); endmodule
module  \$_DFF_PN1_ (input D, C, R, output Q); SB_DFFS  _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .S(!R)); endmodule

module  \$_DFF_NP0_ (input D, C, R, output Q); SB_DFFNR _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .R(R)); endmodule
module  \$_DFF_NP1_ (input D, C, R, output Q); SB_DFFNS _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .S(R)); endmodule
module  \$_DFF_PP0_ (input D, C, R, output Q); SB_DFFR  _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .R(R)); endmodule
module  \$_DFF_PP1_ (input D, C, R, output Q); SB_DFFS  _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .S(R)); endmodule

module  \$_DFFE_NN0P_ (input D, C, E, R, output Q); SB_DFFNER _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .E(E), .R(!R)); endmodule
module  \$_DFFE_NN1P_ (input D, C, E, R, output Q); SB_DFFNES _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .E(E), .S(!R)); endmodule
module  \$_DFFE_PN0P_ (input D, C, E, R, output Q); SB_DFFER  _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .E(E), .R(!R)); endmodule
module  \$_DFFE_PN1P_ (input D, C, E, R, output Q); SB_DFFES  _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .E(E), .S(!R)); endmodule

module  \$_DFFE_NP0P_ (input D, C, E, R, output Q); SB_DFFNER _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .E(E), .R(R)); endmodule
module  \$_DFFE_NP1P_ (input D, C, E, R, output Q); SB_DFFNES _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .E(E), .S(R)); endmodule
module  \$_DFFE_PP0P_ (input D, C, E, R, output Q); SB_DFFER  _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .E(E), .R(R)); endmodule
module  \$_DFFE_PP1P_ (input D, C, E, R, output Q); SB_DFFES  _TECHMAP_REPLACE_ (.D(D), .Q(Q), .C(C), .E(E), .S(R)); endmodule
