module counter(
    input clk,
    input rst,
    output [3:0] count
);
    reg [3:0] count;
    always @(posedge clk) begin
        if (rst)
            count <= 0;
        else
            count <= count + 1;
    end
endmodule
