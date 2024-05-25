To run this program, type 'make all' into your terminal. You should see two new executable files in your folder now. 

Starting the server: 

Type: './QRServer [options]', and enter.

Options are: 
 -p, --port <port number>
 -r, --rate <number requests> <number seconds>
 -m, --max-users <number of users>
 -t, --timeout <number of seconds>

You may enter any or none of these options. If nothing, we will use the default values. DEFAULT_PORT 2012, DEFAULT_RATE_LIMIT 3, DEFAULT_MAX_USERS 3, DEFAULT_TIMEOUT 80


Starting the client:

In a seperate terminal, type './QRclient <server_ip> <port> <QR_img>' and enter

Argunments: 
 - server_ip being the ip address on which your server is running on
 - port being the port number you specified for server. If not, defaulted to 2012. 
 - QR_img being the path to your QR code image. 

Note: server_ip if you are running the client on the same machine as your server is '127.0.0.1'. This is the loopback address.
