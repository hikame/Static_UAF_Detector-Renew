# kmdrawer: KMDrawer.o
# 	clang++ -g3 -O0 -lrt -ldl -ltinfo -lz -lm -o kmdrawer ./KMDrawer.bc -lstdc++ -lgvc -lcgraph -lgvpr
	
# KMDrawer.o: KMDrawer.cpp
	# clang++ -I/usr/local/include/graphviz/ -O0 -emit-llvm -g3 -Wall -c -fmessage-length=0 -fPIC -fno-rtti -std=c++0x -o KMDrawer.bc KMDrawer.cpp

kmdrawer: KMDrawer.cpp
	# clang++ -D_GLIBCXX_DEBUG -I/usr/local/include/graphviz/ -g -O0 -lrt -ldl -ltinfo -lz -lm -lstdc++ -lgvc -lcgraph -lgvpr -o kmdrawer ./KMDrawer.cpp
	clang++  -D_GLIBCXX_DEBUG -g -O0 -I/usr/local/include/graphviz/ -lrt -ldl -ltinfo -lz -lm -lstdc++ -lgvc -lcgraph -lgvpr -o kmdrawer ./KMDrawer.cpp
	# -fsanitize=address
	# g++ -I/usr/local/include/graphviz/ -g -O0 -lrt -ldl -ltinfo -lz -lm -lstdc++ -lgvc -lcgraph -lgvpr -o kmdrawer ./KMDrawer.cpp

test:
	rm ./tmp/*.png
	kmdrawer -e /home/kame/workspace/openssl-1.1.1d ./tmp
	code ./tmp/*.png
clean:
	rm kmdrawer