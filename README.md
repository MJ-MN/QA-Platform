# Q&A Platform
Q&A Platform is a question and answer platform for students and teaching assistants based on CLI using Socket Programming and developed by C.
![screenshot](./Q&A.png?raw=true)
## Tutorial
### Building
1. Open a new terminal console
2. Create a new destination folder at a place of your choice e.g. at `~/git`: `mkdir $HOME/git`
3. Change to this directory: `cd ~/git`
4. Fetch the project source files by running `git clone https://github.com/MJ-MN/Q&A-Platform.git`
1. Change into the project source directory: `cd Q&A-Platform`
2. Run `make clean`
3. Run `make all` to compile and build bin file.
### How to Run
1. Open a terminal and run `bin/server.out <port>` to run the server.
2. Open another terminal and run `bin/server.out <port>` to connect to the server.