CC=gcc
CFLAGS=-g
TARGET:tcpstack.exe CommandParser/libcli.a pkt_gen.exe
LIBS=-lpthread -L ./CommandParser -lcli 
OBJS=gl_thread/gl_thread.o \
            graph.o \
            topologies.o \
            net.o \
	    comm.o \
	    Layer2/layer2.o \
	    Layer3/layer3.o \
	    layer5/layer5.o \
	    layer5/ping.o \
            utils.o \
	    nwcli.o \
	    Layer2/l2switch.o \
	    layer5/spf_algo/spf.o \
	    pkt_dump.o 


pkt_gen.exe:pkt_gen.o utils.o
	${CC} ${CFLAGS} -I tcp_public.h pkt_gen.o utils.o -o pkt_gen.exe

pkt_gen.o:pkt_gen.c
	${CC} ${CFLAGS} -c pkt_gen.c -o pkt_gen.o


tcpstack.exe:main.o ${OBJS} CommandParser/libcli.a
	${CC} ${CFLAGS} main.o ${OBJS} -o tcpstack.exe ${LIBS}
    
    
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

comm.o:comm.c
	${CC} ${CFLAGS} -c -I . comm.c -o comm.o

pkt_dump.o:pkt_dump.c
	${CC} ${CFLAGS} -c -I . pkt_dump.c -o pkt_dump.o

Layer2/layer2.o:Layer2/layer2.c
	${CC} ${CFLAGS} -c -I . Layer2/layer2.c -o Layer2/layer2.o

Layer2/l2switch.o:Layer2/l2switch.c
	${CC} ${CFLAGS} -c -I . Layer2/l2switch.c -o Layer2/l2switch.o

Layer3/layer3.o:Layer3/layer3.c
	${CC} ${CFLAGS} -c -I . Layer3/layer3.c -o Layer3/layer3.o

layer5/layer5.o:layer5/layer5.c
	${CC} ${CFLAGS} -c -I . layer5/layer5.c -o layer5/layer5.o

layer5/ping.o:layer5/ping.c
	${CC} ${CFLAGS} -c -I . layer5/ping.c -o layer5/ping.o

layer5/spf_algo/spf.o:layer5/spf_algo/spf.c
	${CC} ${CFLAGS} -c -I . layer5/spf_algo/spf.c -o layer5/spf_algo/spf.o


CommandParser/libcli.a:
	(cd CommandParser; make)



clean:
	rm -f *.o
	rm -f gl_thread/gl_thread.o
	rm -f *exe
	rm -f Layer2/*.o
	rm -f Layer3/*.o
	rm -f Layer5/*.o
	(cd CommandParser; make clean)
    
all:
	make
	(cd CommandParser; make)

