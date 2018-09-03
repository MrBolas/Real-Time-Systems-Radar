#Xenomai server makefile
#had Dec 2009
#
#Assume que <xenomai instal dir>/bin está no caminho de busca

client = cliente2
#serverip = 192.168.1.37
serverip = 192.168.1.82 #dentro do nei
#serverip = 10.10.22.233 #fora do nei
#port = 3000
port = 2001

skin = native

#Interroga o script xeno-config pelos parâmetros necessários
#para compilação de acordo com man xeno-config
#
#CFLAGS := $(shell xeno-config --xeno-cflags)
#LDFLAGS := -lnative $(shell xeno-config --xeno-ldflags)

CFLAGS := $(shell xeno-config --skin=$(skin) --cflags)
LDFLAGS := $(shell xeno-config --skin=$(skin) --ldflags)

#target:
#	$(CC) $(client).c -o $(client) $(CFLAGS) $(LDFLAGS)

$(client): $(client).c
	$(CC) -o $@ $< $(CFLAGS) $(LDFLAGS) -l ncurses

#use make -i run to ignore telnet closed connection by server error	
run:
	@sleep 1
	@echo
	@echo generic client: cliente.c
	./$(client) $(serverip) $(port)

all:
	make target
	make -i run
