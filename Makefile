# Define the compiler
CC = gcc

# Define the target executable
TARGET = download

# Define the source file
SRC = download.c

# Define the rule to build the target
$(TARGET): $(SRC)
	$(CC) -o $(TARGET) $(SRC)

# Define the clean rule to remove the executable
.PHONY: clean
clean:
	rm -f $(TARGET)