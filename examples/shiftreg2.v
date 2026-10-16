module dff(
    input clk,
    input rst,
    input d,
    output q
);
    reg q;
    always @(posedge clk)
        if (rst)
            q <= 0;
        else
            q <= d;
endmodule

module shiftreg2(
    input clk,
    input rst,
    input d,
    output q0,
    output q1
);
    dff u0(.clk(clk), .rst(rst), .d(d), .q(q0));
    dff u1(.clk(clk), .rst(rst), .d(q0), .q(q1));
endmodule
