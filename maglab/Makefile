API_DIR = ../tmp/opt/redpitaya

CROSS_COMPILE=arm-linux-gnueabihf-
CC = $(CROSS_COMPILE)gcc
CFLAGS  = -O3 -march=armv7-a -mtune=cortex-a9 -mfpu=neon -mfloat-abi=hard -D_GNU_SOURCE
CFLAGS += -I$(API_DIR)/include
LDFLAGS = -L$(API_DIR)/lib
LDLIBS = -static -lrp-hw-calib -lrp-hw-profiles -lm

export CC
export CROSS_COMPILE

CUR_DIR = $(PWD)


# List of compiled object files (not yet linked to executable)

PRGS =  maglab_rp_info

OBJS := $(patsubst %,%.o,$(PRGS))
SRC := $(patsubst %,%.c,$(PRGS))

all: $(PRGS)

$(PRGS): %: %.c
	$(CC) $< $(CFLAGS) $(LDFLAGS) $(LDLIBS) -o $@

clean:
	$(RM) *.o
	$(RM) $(OBJS)

clean_all: clean
	$(RM) $(PRGS)

