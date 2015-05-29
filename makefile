my-router: main.cpp my-router.h my-router.cpp
	c++ -std=gnu++0x -I /usr/local/boost_1_58_0 main.cpp my-router.cpp -o my-router -L/usr/lib/ -lboost_system -lboost_date_time
