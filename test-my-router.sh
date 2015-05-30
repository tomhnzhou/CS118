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
fi
