CC = gcc
CFLAGS = -Wall -Wextra -pthread -std=c11 -O2

TARGET = sensormesh
SRC = task3.c

all: $(TARGET)

$(TARGET):
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET) a.out
