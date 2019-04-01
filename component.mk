#COMPONENT_OWNBUILDTARGET = 1
COMPONENT_SRCDIRS := src/aes src/hal src/lmic
COMPONENT_ADD_INCLUDEDIRS := include

#AES_OBJS = src/aes/lmic.o src/aes/other.o
#HAL_OBJS = src/hal/hal.o
#LMIC_OBJS = $(patsubst %.c,%.o,$(wildcard src/lmic/*.c))

#%.o : %.c
#	$(CC) -c $(CCFLAGS) $(CFLAGS) $< -o $@

#aes: $(AES_OBJS)

#hal: $(HAL_OBJS)

#lmic: $(LMIC_OBJS)

#build: aes hal lmic
#	$(CC) $(CCFLAGS) $(CFLAGS) $(AES_OBJS) $(HAL_OBJS) $(LMIC_OBJS) -o $(COMPONENT_LIBRARY)

#clean:
#	rm -f $(AES_OBJS) $(HAL_OBJS) $(LMIC_OBJS)