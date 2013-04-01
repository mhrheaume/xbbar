CC = gcc
CFLAGS = -Wall -Werror -lX11 -lm

EXEC = xbbar

src = xbbar.c

.PHONY: all
all : ${EXEC}

${EXEC} :
	${CC} ${CFLAGS} -o ${EXEC} ${src}

.PHONY : clean
clean:
	@rm -f  ${EXEC}
