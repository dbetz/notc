OBJS = \
notc.o \
db_compiler.o \
db_fun.o \
db_expr.o \
db_generate.o \
db_image.o \
db_scan.o \
db_statement.o \
db_symbols.o \
db_system.o \
db_vmint.o \
osint_posix.o

OBJS += db_vmdebug.o

HDRS = \
db_compiler.h \
db_image.h \
db_symbols.h \
db_system.h \
db_types.h \
db_vm.h \
db_vmdebug.h

all:	notc

$(OBJS):	$(HDRS)

CFLAGS = -Wall -Os -DMAC -m32
#CFLAGS = -Wall -DMAC -m32 -g

%.o:	%.c
	cc $(CFLAGS) -c -o $@ $<

notc:	$(OBJS)
	cc $(CFLAGS) -o $@ $(OBJS)

run:	notc
	./notc

debug:	notc
	lldb notc

clean:
	rm -f *.o notc
