CXX = mpic++

TARGET = mpi_processor

SRC = \
	main.cpp \
	distribucion.cpp \
	procesamiento_imagen.cpp

OBJ = $(SRC:.cpp=.o)

OPENCV_FLAGS := $(shell pkg-config --cflags --libs opencv4)

CXXFLAGS = -Wall -Wextra -O2

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGET) $(OPENCV_FLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ $(shell pkg-config --cflags opencv4)

clean:
	rm -f $(OBJ) $(TARGET)

rebuild: clean all

.PHONY: all clean rebuild