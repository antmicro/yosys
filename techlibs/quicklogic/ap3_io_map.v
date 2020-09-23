module \$__out_buff (Q, A);
	output Q;
    input A;

    parameter _TECHMAP_CONSTMSK_A_ = 0;
    parameter _TECHMAP_CONSTVAL_A_ = 0;

    if(_TECHMAP_CONSTMSK_A_ == 1) begin
        D_BUFF #(.DSEL(_TECHMAP_CONSTVAL_A_)) _TECHMAP_REPLACE_ (.Q(Q));
    end
    else begin
        OUT_BUFF _TECHMAP_REPLACE_ (.Q(Q), .A(A));
    end
endmodule

module \$__in_buff (Q, A);
	output Q;
    input A;

    IN_BUFF _TECHMAP_REPLACE_ (.Q(Q), .A(A));

endmodule
