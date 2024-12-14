
The image below shows the compiler's performance.
The compiler was compiled by the MSVC compiler with /O2 optimization.
The compiler is about 3 times slower without the optimization flag.

![](/docs/img/perf-2023-08-06.png)

You can find more measurements in [perf-2023-08-06-O2.txt](/docs/stats/perf-2023-08-06-O2.txt).
rdtsc was used. 2.9 GHz is the clock speed of my laptop. The power cable was plugged in
my laptop. I get slower results when it's not plugged in.
The measurement is probably not very accurate but it gives you an idea
of the compiler's speed and moving forward we will know whether it
became faster or slower because I will test it again on the same computer with same
settings (power cable plugged in). In the future, I will write a program
with 2000 to 6000 lines of code which should also give more accurate
results when compiled. Then we will know if the correlation between time
and lines of code is linear or exponential (or something else).

I want to emphasize that I don't know how to measure a program's performance
correctly. Take all measurements with a large quantity of salt just in case I have
made a mistake.
