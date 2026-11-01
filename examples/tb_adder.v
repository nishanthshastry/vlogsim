module tb_adder;
    reg [7:0] a;
    reg [7:0] sum;

    initial begin
        a = 1;
        sum = 0;
        #1 sum = sum + a;
        $display("t=%d a=%d sum=%d", $time, a, sum);

        a = 2;
        #1 sum = sum + a;
        $display("t=%d a=%d sum=%d", $time, a, sum);

        a = 3;
        #1 sum = sum + a;
        $display("t=%d a=%d sum=%d", $time, a, sum);

        $finish;
    end
endmodule
