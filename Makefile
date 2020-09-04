HEADERS := $(wildcard src/*.h)
SOURCES := $(wildcard src/*.c)
PROJECT := opus

all : $(PROJECT) $(TEST_PROJECT)

$(PROJECT) : $(HEADERS) $(SOURCES) Makefile
	gcc -o $(PROJECT) $(SOURCES) -lm -lopus -lao -g -Wall -Werror -Wextra -O1 -flto
