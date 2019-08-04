SRCS=client.cpp server.cpp main.cpp common_funcs.cpp group_serv.cpp
OBJS=main.o server.o client.o common_funcs.o group_serv.o


a.out: main.o
	g++ -o a.out main.o client.o server.o common_funcs.o group_serv.o

main.o: main.cpp server.o client.o group_serv.o main.h
	g++ -c  main.cpp main.h server.o client.o group_serv.o

group_serv.o: common_funcs.o group_serv.cpp main.h
	g++ -c common_funcs.o group_serv.cpp main.h

server.o: common_funcs.o server.cpp main.h
	g++ -c common_funcs.o server.cpp main.h

client.o: common_funcs.o client.cpp main.h
	g++ -c common_funcs.o client.cpp main.h

common_funcs.o: common_funcs.cpp
	g++ -c common_funcs.cpp

clean:
	rm -f $(OBJS)
	rm -f *.gch