NAME = notc

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
db_vmint_pasm.o \
osint_propgcc.o \
simple_vsnprintf.o \
notc_vm.o

OBJS += db_vmdebug.o

HDRS = \
db_compiler.h \
db_symbols.h \
db_types.h \
db_vmdebug.h \
db_image.h \
db_system.h \
db_vm.h

vpath %.h ..
vpath %.c ..

ifndef MODEL
MODEL = cmm
endif

CFLAGS = -m$(MODEL) -Wall -Os -DPROPELLER_GCC -Dvsnprintf=__simple_vsnprintf
#CFLAGS += -DUSE_FDS

LDFLAGS += $(CFLAGS) -fno-exceptions -fno-rtti

all:	$(NAME).elf $(NAME).binary

$(NAME).elf: $(OBJS)
	propeller-elf-gcc $(LDFLAGS) -o $@ $(OBJS)

%.binary: %.elf
	propeller-load -s $<

%.o: %.c
	propeller-elf-gcc $(CFLAGS) -o $@ -c $<

%.dat:	%.spin
	spin2cpp --dat -o $@ $<
	
%.o: %.dat
	propeller-elf-objcopy -I binary -B propeller -O propeller-elf-gcc --rename-section .data=.text $< $@
	
run: $(NAME).elf
	propeller-load $(NAME).elf -r -t

clean:
	rm -f *.o *.elf *.binary

$(OBJS):	$(HDRS)
