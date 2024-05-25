all: QRclient QRserver

client: QRclient.c
	gcc -Wall -o QRclient QRclient.c 

server: QRserver.c
	gcc -Wall -o QRserver QRserver.c -lrt

clean:
	rm QRclient QRserver