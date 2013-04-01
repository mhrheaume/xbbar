CC = gcc
CFLAGS = -Wall -Werror -lX11 -lm

EXEC = xbbar

OBJS := $(patsubst %.c,%.o,$(wildcard *.c))

.PHONY: all
all : ${EXEC}

${EXEC} : ${OBJS}
	${CC} ${CFLAGS} -o ${EXEC} ${OBJS}

.PHONY : debug
debug : CFLAGS += -DDEBUG -g
debug : ${EXEC}

.PHONY : clean
clean:
	@rm -f ${EXEC} ${OBJS}
