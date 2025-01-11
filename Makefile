HEADERS := $(wildcard src/*.h)
SOURCES := $(wildcard src/*.c)
PROJECT := opus

LIBBRR := brrUtils/build/libbrr.a

all : $(PROJECT)

$(PROJECT) : $(HEADERS) $(SOURCES) Makefile $(LIBBRR)
	gcc -o $(PROJECT) $(SOURCES) -lm `pkg-config --cflags --libs ncursesw opus ao` -IbrrUtils $(LIBBRR) -g -Wall -Werror -Wextra -O1 -flto

$(LIBBRR) : $(wildcard brrUtils/*.c brrUtils/*.h)
	sed -i 's/^\(add_executable\|target_link_libraries\)/# \1/' brrUtils/CMakeLists.txt
	cd brrUtils; mkdir build; cd build; cmake ..; make
