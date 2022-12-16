#Create bin directory
mkdir -p bin &

#Create log directory
mkdir -p log &

#Compile the inspection program
gcc src/inspection_console.c -lncurses -lm -o bin/inspection &

#Compile the command program
gcc src/command_console.c -lncurses -o bin/command &
#Compile the master program
gcc src/master.c -o bin/master &

#Compile the motor x program
gcc src/mx.c -o bin/mx &

#Compile the motor z program
gcc src/mz.c -o bin/mz &

#Compile the real coordinates program
gcc src/world.c -o bin/world