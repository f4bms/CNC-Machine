CXX = mpic++

TARGET = mpi_processor

SRC = \
	main.cpp \
	distribucion.cpp \
	procesamiento_imagen.cpp \
	contornos.cpp \
	vectorizacion.cpp \
	esqueleto.cpp

OBJ = $(SRC:.cpp=.o)

OPENCV_CFLAGS := $(shell pkg-config --cflags opencv4)
OPENCV_LIBS := $(shell pkg-config --libs opencv4)

CXXFLAGS = -Wall -Wextra -O2

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(OBJ) -o $(TARGET) $(OPENCV_LIBS)

%.o: %.cpp
	$(CXX) \
	$(CXXFLAGS) \
	$(OPENCV_CFLAGS) \
	-c $< \
	-o $@

clean:
	rm -f $(OBJ) $(TARGET)

rebuild: clean all

.PHONY: all clean rebuild