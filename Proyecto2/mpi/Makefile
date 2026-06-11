CXX = mpic++

TARGET = mpi_processor

SRC = \
	main.cpp \
	distribucion.cpp \
	procesamiento_imagen.cpp \
	esqueleto.cpp \
	grafo.cpp \
	cnc_traduccion.cpp

OBJ = $(SRC:.cpp=.o)

OPENCV_CFLAGS := $(shell pkg-config --cflags opencv4)
OPENCV_LIBS := $(shell pkg-config --libs opencv4)

CNC_ROOT = ../biblioteca
CNC_CFLAGS = -I$(CNC_ROOT)/include
CNC_LIBS = -L$(CNC_ROOT)/bin -lcnc

CXXFLAGS = -Wall -Wextra -O2

all: $(TARGET)

$(TARGET): $(OBJ) $(CNC_ROOT)/bin/libcnc.a
	$(CXX) \
	$(OBJ) \
	-o $(TARGET) \
	$(OPENCV_LIBS) \
	$(CNC_LIBS)

$(CNC_ROOT)/bin/libcnc.a:
	$(MAKE) -C $(CNC_ROOT) all

%.o: %.cpp
	$(CXX) \
	$(CXXFLAGS) \
	$(OPENCV_CFLAGS) \
	$(CNC_CFLAGS) \
	-c $< \
	-o $@

clean:
	rm -f \
	$(OBJ) \
	$(TARGET)

rebuild: clean all

.PHONY: all clean rebuild