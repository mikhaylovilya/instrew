set history save
set breakpoint pending on
b connection.cc:197
run
del
n
set follow-exec-mode new
b dispatch.c:77
c
s
finish
b rtld.c:296
del 2

