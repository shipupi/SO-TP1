
echo "Starting compilation..."

gcc -Wall  -g -c slave.c view.c hashfiles.c
gcc -g  -Wall slave.o -o slave.fl
gcc -g  -Wall view.o -o view.fl
gcc -g  -Wall hashfiles.o -o hashfiles.fl
printf "Compilation Done!\n\n"
rm -f *.o

if [[ $* == *--dev* ]]; then

	# DEV COMANDS
	cppcheck slave.c
	read -p "Press any key to continue... "
	cppcheck view.c
	read -p "Press any key to continue... "
	cppcheck hashfiles.c
	read -p "Press any key to continue... "

	printf "Checking Valgrind for slave"
	valgrind ./slave.fl | echo -n "slave.fl"
else 
	# Regular Commands
	printf "Running program \n\n"
	./hashfiles.fl
fi

