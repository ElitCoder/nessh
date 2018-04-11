CXX			:= g++

CPP_FILES	:= $(wildcard src/*.cpp)
OBJ_FILES	:= $(addprefix obj/,$(notdir $(CPP_FILES:.cpp=.o)))

CC_FLAGS	:= -std=c++11 -Wall -Wextra -pedantic-errors
CC_FLAGS	+= -O3
CC_FLAGS	+= -fPIC
CC_FLAGS	+= -I./include/
LD_LIBS		:= -lssh -lssh_threads

LIB_TYPE	:= -shared
TARGET		:= libnessh.so

all: build

clean:
	rm -f lib/* obj/*

install:
	chmod 644 lib/$(TARGET)
	mkdir -p /usr/include/libnessh/
	cp -r include/* /usr/include/libnessh/
	cp -r lib/$(TARGET) /usr/lib/

build: $(OBJ_FILES)
	$(CXX) $(FLAGS) $^ -o lib/$(TARGET) $(LD_LIBS) $(LIB_TYPE)

obj/%.o: src/%.cpp
	g++ $(CC_FLAGS) -c -o $@ $<
	
CC_FLAGS += -MMD
-include $(OBJFILES:.o=.d)