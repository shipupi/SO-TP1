
echo "Starting compilation..."

gcc -Wall  -g -c slave.c view.c hashfiles.c
gcc -g  -Wall slave.o -lpthread -o slave.fl
gcc -g  -Wall view.o -lpthread -o view.fl
gcc -g  -Wall hashfiles.o -lpthread -o hashfiles.fl
printf "Compilation Done!\n\n"
rm -f *.o


if [[ $* == *--dev* ]]; then
	echo "-------------------------------------------------------"
	printf "RUNNING CPPCHECK\n\n"
	# DEV COMANDS
	cppcheck --enable=all --suppress=missingIncludeSystem --force --inconclusive --quiet slave.c
	read -p "Press any key to continue... "
	cppcheck --enable=all --suppress=missingIncludeSystem --force --inconclusive --quiet view.c
	read -p "Press any key to continue... "
	cppcheck --enable=all --suppress=missingIncludeSystem --force --inconclusive --quiet hashfiles.c
	read -p "Press any key to continue... "
	echo "-------------------------------------------------------"

	printf "RUNNING Valgrind\n\n"
	valgrind --leak-check=full --show-leak-kinds=all ./hashfiles.fl * | ./view.fl
	printf "\n"


elif [[ $* == *--test* ]]; then
	rm outputs/* > /dev/null
	./hashfiles.fl * > /dev/null
	sort outputs/hashFilesOutput > outputs/sortedOutput1
	md5sum * > outputs/md5output
	sort outputs/md5output > outputs/sortedOutput2
	printf "Showing differences: \n"
	DIFF=$(diff outputs/sortedOutput1 outputs/sortedOutput2)
	if [ "$DIFF" == "" ] 
	then
		echo "[TEST SUCCESSFUL] No differences were found! :-)"
	else 
		echo $DIFF
	fi
	rm outputs/* > /dev/null
else 

	# Regular Commands
	# printf "Running program \n\n"
	./hashfiles.fl * | ./view.fl 
fi

