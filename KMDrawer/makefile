all: release debug

release: kmdrawer
debug: kmdrawer_dbg

kmdrawer_dbg: KMDrawer.cpp
	clang++  -D_GLIBCXX_DEBUG -g -O0 -I/usr/local/include/graphviz/ -lrt -ldl -ltinfo -lz -lm -lstdc++ -lgvc -lcgraph -lgvpr -o kmdrawer_dbg ./KMDrawer.cpp

kmdrawer: KMDrawer.cpp
	clang++ -I/usr/local/include/graphviz/ -lrt -ldl -ltinfo -lz -lm -lstdc++ -lgvc -lcgraph -lgvpr -o kmdrawer ./KMDrawer.cpp

install:
	sudo ln -sf $(shell pwd)/kmdrawer /usr/local/bin/
	sudo ln -sf $(shell pwd)/kmdrawer_dbg /usr/local/bin/

clean:
	rm kmdrawer