### Maekawa's Protocol for Mutual Exclusion

This program implements a protocol running in a distributed system node. Each node in the system is part other nodes' quorum, which those nodes request frrom to enter their critical section. This protocol ensures that no node enters a critical section while another node is executing theirs. 

### Launcher script

The launcher.sh script logs into the machines specified in the configuration file and starts each
node individually. It uses UT Dallas "dcXX" distributed systems servers.

To launch, provide the configuration filename as the input argument. "launcher.sh config.txt"

### Configuration file

A configuration file is supplied to determine the distributed system quorum membership, host names, ports 
and application parameters.

### Compile the program:

make

### Run the program:

./node nodeID config_file

example: ./node 2 config_files/config.txt

### Output
Each node outputs a file called "nodexoutput" where x is the node ID. Each file contains the timestamps for when they entered their critical section and when they left, along with total messages sent and response time.

### Python Script

Use checkmutex.py to assess the system's safety. Ensure that the configuration file and ouptut files are in the same directory as the python script. use the first input argument as the configuration file name. Example: "checkmutex.py config.txt"
