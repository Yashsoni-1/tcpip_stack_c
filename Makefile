CC=gcc
CFLAGS=-g
TARGET:test.exe CommandParser/libcli.a 
LIBS:-lpthread -L ./CommandParser -lcli -lrt

OBJS=gl_thread/gl_thread.o \
            graph.o \
            topologies.o \
            net.o \
            utils.o \
	    nwcli.o

test.exe:main.o ${OBJS} CommandParser/libcli.a
	${CC} ${CFLAGS} main.o ${OBJS} -o test.exe ${LIBS}
    
    
main.o:main.c
	${CC} ${CFLAGS} -c main.c -o main.o
    
gl_thread/gl_thread.o:gl_thread/gl_thread.c
	${CC} ${CFLAGS} -c -I gl_thread gl_thread/gl_thread.c -o gl_thread/gl_thread.o
    
graph.o:graph.c
	${CC} ${CFLAGS} -c -I . graph.c -o graph.o
topologies.o:topologies.c
	${CC} ${CFLAGS} -c -I . topologies.c -o topologies.o
net.o:net.c
	${CC} ${CFLAGS} -c -I . net.c -o net.o
utils.o:utils.c
	${CC} ${CFLAGS} -c -I . utils.c -o utils.o
nwcli.o:nwcli.c
	${CC} ${CFLAGS} -c -I . nwcli.c -o nwcli.o
CommandParser/libcli.a:
	(cd CommandParser; make)



clean:
	rm *.o
	rm gl_thread/gl_thread.o
	rm *exe
	(cd CommandParser; make clean)
    
all:
	make
	(cd CommandParser; make)

