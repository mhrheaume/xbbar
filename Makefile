CC = gcc
CFLAGS = -Wall -Werror -lX11 -lm

SRC = xbbar.c
OBJS = ${SRC:.c=.o}
EXEC = xbbar

INSTALL_PATH = /usr/local/bin

.PHONY: all
all : ${EXEC}

${OBJS} : version.h

${EXEC} : ${OBJS}
	${CC} ${CFLAGS} -o ${EXEC} ${OBJS}

.PHONY : debug
debug : CFLAGS += -DDEBUG -g
debug : ${EXEC}

.PHONY : install
install: all
	@echo Installing xbbar to ${INSTALL_PATH}
	@mkdir -p ${INSTALL_PATH}
	@cp -f ${EXEC} ${INSTALL_PATH}
	@chmod 755 ${INSTALL_PATH}/${EXEC}

.PHONY : clean
clean:
	@rm -f ${EXEC} ${OBJS}
