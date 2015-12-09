
# Restricted Remote Shell (rrsh) Server

## Emory Hufbauer
## Project for CS485: Systems Programming

### Usage:

Running `make` in the main directory should build the project on most Unix OSs. It will generate a single executable file, called `rrshServer`. When run, it allows remote users to login to and execute commands on the host computer, with security measures in place to prevent unauthorized user access.

	`./rrshServer <port>`

### Purpose:

To build a simple remote shell server as a class project. For basic security, the server only allows the users in the rrshusers.txt file to login, and then they can only execute the commands listed in rrshcommands.txt

### Testing:

Although originally intended to work with a client program provided by another student, this program can be tested using telnet. Simply telnet onto the host's address and the server's port, login to the server, and enter commands to be executed on the host.

### Limitations:
- Supports a maximum of 256 characters in a single comand line.
- Supports a maximum of 32 words in a single command line.
- Neither usernames nor passwords in the rrshusers.txt file may exceed 64 characters in length.
- Allowed commands in the rrshcommands.txt file may not exceed 64 characters in length.
