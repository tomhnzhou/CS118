if [ "$#" -eq 1 ]; then
	if [ "$1" == '-r' ]; then
		randids=$(shuf -e A B C D E F)
		echo "Starting routers in this sequence: $randids"
		IFS=$'\n'
		for id in $randids
		do
			if [ -f "routing-output$id.txt" ]; then
				rm "routing-output$id.txt"
			fi
			gnome-terminal -x sh -c "./my-router $id"
			sleep 2
		done
	fi
elif [ "$#" -eq 6 ]; then
	for id in "$@"
	do
			if [ -f "routing-output$id.txt" ]; then
				rm "routing-output$id.txt"
			fi
			gnome-terminal -x sh -c "./my-router $id"
			sleep 2
	done
else
	echo "Usage: "
	echo "1) Start routers in random order: ./test-my-router.sh -r"
	echo "2) Start routers in a given order: ./test-my-router.sh <sequence of IDs>"
fi
