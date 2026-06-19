CC = g++
CFLAGS = -std=c++17 -O2 -Iinclude
SRC = main.cpp Instancia.cpp HeuristicasConstructivas.cpp BusquedaLocal.cpp Metaheuristicas.cpp
OBJ = $(SRC:.cpp=.o)
EXEC = gap_simulator

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $(EXEC)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# suite de tests y experimentacion (no incluye main.o)
TESTOBJ = tests.o Instancia.o HeuristicasConstructivas.o BusquedaLocal.o Metaheuristicas.o
tests: $(TESTOBJ)
	$(CC) $(TESTOBJ) -o tests

clean:
	rm -f $(OBJ) $(EXEC) tests.o tests
