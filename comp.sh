
echo "Starting compilation..."

gcc -Wall  -g -c slave.c view.c hashfiles.c
gcc -g  -Wall slave.o -lpthread -o slave.fl
gcc -g  -Wall view.o -o view.fl
gcc -g  -Wall hashfiles.o -lpthread -o hashfiles.fl
printf "Compilation Done!\n\n"
rm -f *.o

if [[ $* == *--dev* ]]; then
	echo "-------------------------------------------------------"
	printf "RUNNING CPPCHECK\n\n"
	# DEV COMANDS
	cppcheck slave.c
	read -p "Press any key to continue... "
	cppcheck view.c
	read -p "Press any key to continue... "
	cppcheck hashfiles.c
	read -p "Press any key to continue... "
	echo "-------------------------------------------------------"

	printf "RUNNING Valgrind\n\n"
	printf "Checking Valgrind for slave\n"
	echo -n "slave.fl" | valgrind --track-origins=yes ./slave.fl

	read -p "Press any key to continue... "

	NUMBER=$((1 + RANDOM % 255))
	echo $NUMBER > test/$NUMBER

	echo "-------------------------------------------------------"
	echo "CHECKING SLAVE FILE"
	# Hash with MD5
	printf "Hashing random number with md5sum\n"
	MD5CHECK=$(md5sum test/$NUMBER)
	echo $MD5CHECK > test/md5check
	echo $MD5CHECK
	printf "\n"
	# Hash with Slave
	printf "Hashing random number with Slave file\n"
	SLAVECHECK=$(echo -n "test/$NUMBER" | ./slave.fl)
	echo $SLAVECHECK > test/slavecheck
	echo $SLAVECHECK
	printf "\n"

	# Diff between hashes
	printf "Checking for differences between files..\n"
	DIFF=$(diff test/slavecheck test/md5check)
	echo $DIFF
	if [ "$DIFF" != "" ] 
	then
		echo "[ERROR] ERROR IN SLAVE"
	else 
	    echo "[OK] The results are the same "
	fi

	# Cleanup
	rm test/$NUMBER
	rm test/slavecheck
	rm test/md5check
	printf "\n"



else 

	# Regular Commands
	printf "Running program \n\n"
	./hashfiles.fl *
fi

