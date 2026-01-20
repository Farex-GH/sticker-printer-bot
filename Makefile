BIN := bot.elf

CC := g++

# Add -DDEBUG -g3 for debugging

CPPFLAGS := -Wall -Iinclude -std=gnu++23 -I/usr/local/include -lTgBot -lboost_system -lssl -lcrypto -lpthread -O2
LDFLAGS := -lm -Iinclude -std=c++23

TEST_DIR := test
CPP_SOURCES := $(wildcard src/*.cpp)
HEADERS := $(wildcard include/*.h)
CPP_OBJS := $(patsubst %.c, %.o, $(CPP_SOURCES))

$(BIN): clean $(C_OBJS) $(CU_OBJS) $(HEADERS)
	$(CC) -o $(BIN) $(CPP_OBJS) $(LDFLAGS)

test_bot:
	$(CC) -o $(BIN) $(CPP_OBJS) $(TEST_DIR)/test_bot.cpp $(LDFLAGS) $(CPPFLAGS)

test_print:
	$(CC) -o $(BIN) $(CPP_OBJS) $(TEST_DIR)/test_print.cpp $(LDFLAGS) $(CPPFLAGS)

$(CPP_OBJS): $(CPP_SOURCES) $(HEADERS)
	$(CC) -c $(CPP_SOURCES) $(CPPFLAGS)

clean:
	rm -f $(BIN) *.o

