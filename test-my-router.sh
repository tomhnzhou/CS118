print_err () {
	echo "Usage: "
	echo "1) Start routers in random order: ./test-my-router.sh -r"
	echo "2) Start routers in a given order: ./test-my-router.sh <sequence of IDs>"
}
if [ "$#" -eq 1 ]; then
	if [ "$1" == '-r' ]; then
		randids=$(shuf -e A B C D E F | tr '\n' ' ')
		echo "Starting routers in this order: $randids"
		IFS=$' '
		for id in $randids
		do
			if [ -f "routing-output$id.txt" ]; then
				rm "routing-output$id.txt"
			fi
			if [ -f "routing-log$id.txt" ]; then
				rm "routing-log$id.txt"
			fi
			./my-router "$id" &
			echo "Running router $id with PID: $!"
			sleep 2;
		done
		wait ${!}
	else
		print_err
	fi
elif [ "$#" -eq 6 ]; then
	echo "Starting routers in this order: $1 $2 $3 $4 $5 $6"
	for id in "$@"
	do
			if [ -f "routing-output$id.txt" ]; then
				rm "routing-output$id.txt"
			fi
			if [ -f "routing-log$id.txt" ]; then
				rm "routing-log$id.txt"
			fi
			./my-router "$id" &
			echo "Running router $id with PID: $!"
			sleep 2;
	done
	wait ${!}
else
	print_err
fi
