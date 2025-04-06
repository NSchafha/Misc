#export LD_LIBRARY_PATH=/home/socs/workdir/assign3/bin/:$LD_LIBRARY_PATH
#make the parser target.
#Does the write card match create card?

CC = gcc
CFLAGS = -Wall -std=c11 -g -fPIC
LDFLAGS = -L$(BIN)
LIBS = -L$(BIN) -llist -lvcparser
INC = include/
SRC = src/
BIN = bin/

all: $(BIN)libvcparser.so $(BIN)liblist.so



parser: $(BIN)libvcparser.so $(BIN)liblist.so

$(BIN)VCHelpers.o: $(SRC)VCHelpers.c $(INC)VCHelpers.h
	$(CC) $(CFLAGS) -I$(INC) -c $(SRC)VCHelpers.c -o $(BIN)VCHelpers.o

$(BIN)VCValidate.o: $(SRC)VCValidate.c $(INC)VCValidate.h
	$(CC) $(CFLAGS) -I$(INC) -c $(SRC)VCValidate.c -o $(BIN)VCValidate.o

$(BIN)VCAPIHelpers.o: $(SRC)VCAPIHelpers.c $(INC)VCAPIHelpers.h
	$(CC) $(CFLAGS) -I$(INC) -c $(SRC)VCAPIHelpers.c -o $(BIN)VCAPIHelpers.o

$(BIN)VCParser.o: $(SRC)VCParser.c $(INC)VCParser.h
	$(CC) $(CFLAGS) -I$(INC) -c $(SRC)VCParser.c -o $(BIN)VCParser.o

$(BIN)libvcparser.so: $(BIN)VCHelpers.o $(BIN)VCValidate.o $(BIN)VCAPIHelpers.o $(BIN)VCParser.o $(BIN)LinkedListAPI.o 
	$(CC) -shared -o $(BIN)libvcparser.so $(BIN)VCHelpers.o $(BIN)VCValidate.o $(BIN)VCAPIHelpers.o $(BIN)VCParser.o $(BIN)LinkedListAPI.o 



$(BIN)LinkedListAPI.o: $(SRC)LinkedListAPI.c $(INC)LinkedListAPI.h
	$(CC) $(CFLAGS) -I$(INC) -c $(SRC)LinkedListAPI.c -o $(BIN)LinkedListAPI.o

$(BIN)liblist.so: $(BIN)LinkedListAPI.o
	$(CC) -shared -o $(BIN)liblist.so $(BIN)LinkedListAPI.o



$(BIN)UnitTests.o: $(SRC)UnitTests.c
	$(CC) $(CFLAGS) -I$(INC) -c $(SRC)UnitTests.c -o $(BIN)UnitTests.o

unitTests: $(BIN)UnitTests.o $(BIN)libvcparser.so $(BIN)liblist.so
	$(CC) $(CFLAGS) $(LDFLAGS) -o unitTests $(BIN)UnitTests.o $(LIBS)


clean:
	rm -f $(BIN)*.o $(BIN)*.so unitTests testOut.vcf
