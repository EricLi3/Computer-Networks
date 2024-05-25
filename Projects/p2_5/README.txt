To run this program, type 'make all' into your terminal. You should see two new executable files in your folder now. 

Starting the server: 

Type: './QRServer [options]', and enter.


int port = DEFAULT_PORT;
    int rate_msgs = DEFAULT_RATE_MSGS;
    int rate_time = DEFAULT_RATE_TIME;
    int max_users = DEFAULT_MAX_USERS;
    int timeout = DEFAULT_TIMEOUT;

Options are: 
 -p, --port <port number>
 -r, --rate messages <number requests>
 -t, --rate time <time (seconds)>
 -m, --max-users <number of users>
 -o, --timeout <number of seconds>

You may enter any or none of these options. If nothing, we will use the default values. DEFAULT_PORT 2012, DEFAULT_RATE_MSGS 3, DEFAULT_RATE_TIME 60, DEFAULT_MAX_USERS 3, DEFAULT_TIMEOUT 80 seconds


Starting the client:

In a seperate terminal, type './QRclient <server_ip> <port>' and enter

From here you have the option of entering a file descriptor
After entering a file name, you should recieve a message back from the server containing the return code, URL length, and the URL. 

******** Only after entering a file name, you may choose to disonnect by typing 'close' into the client. ********

**** Argunments ****
 - server_ip being the ip address on which your server is running on. I would hardcode it, but feel like it's not good form. 
 - port being the port number you specified for server. If not, defaulted to 2012. 
 
Note: server_ip if you are running the client on the same machine as your server is '127.0.0.1'. This is the loopback address.
**** If you are testing this where the server is on a different machine, please find the IP by running 'ip addr' in the terminal, And use the value as your server_ip. ****

We have a admin_log.txt file which reports user activity and system behavior. 
