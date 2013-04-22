CC = gcc

VERSION = 0.4

INCS = -I/usr/include -I/usr/local/include
LIBS = -L/usr/lib -L/usr/local/lib -lX11 -lxpb
DEFS = -D_BSD_SOURCE -DVERSION=\"${VERSION}\"

CFLAGS = -std=c99 -Wall -Werror ${INCS} ${DEFS}
LDFLAGS = ${LIBS}

SRC = $(wildcard *.c)
OBJS = ${SRC:.c=.o}
EXEC = xbbar

INSTALL_PATH = /usr/local/bin

.PHONY: all
all: ${EXEC}

%.o:%.c
	${CC} ${CFLAGS} -c -o $@ $<

${EXEC}: ${OBJS}
	${CC} ${LDFLAGS} -o ${EXEC} ${OBJS} 

.PHONY: debug
debug: CFLAGS += -DDEBUG -g
debug: ${EXEC}

.PHONY: install
install: all
	@echo Installing xbbar to ${INSTALL_PATH}
	@mkdir -p ${INSTALL_PATH}
	@cp -f ${EXEC} ${INSTALL_PATH}
	@chmod 755 ${INSTALL_PATH}/${EXEC}

.PHONY: clean
clean:
	@rm -f ${EXEC} ${OBJS}
