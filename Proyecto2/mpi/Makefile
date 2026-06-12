CC = mpicc
CXX = mpic++

TARGET = mpi_processor

# Codigo distribuido en C puro.
MPI_C_SRC = \
	main.c \
	distribucion.c \
	grafo.c \
	cnc_traduccion.c

MPI_C_OBJ = $(MPI_C_SRC:.c=.o)

# Modulo de procesamiento de imagen en C++ (unica capa OpenCV).
IMG_CPP_OBJ = ../img/procesamiento_imagen.o

OBJ = $(MPI_C_OBJ) $(IMG_CPP_OBJ)

OPENCV_CFLAGS := $(shell pkg-config --cflags opencv4)
OPENCV_LIBS := $(shell pkg-config --libs opencv4)

CNC_ROOT = ../biblioteca
CNC_CFLAGS = -I$(CNC_ROOT)/include
CNC_LIBS = -L$(CNC_ROOT)/bin -lcnc

CFLAGS = -Wall -Wextra -O2
CXXFLAGS = -Wall -Wextra -O2

all: $(TARGET)

# El enlace final usa mpic++ para resolver OpenCV y el runtime de C++.
$(TARGET): $(OBJ) $(CNC_ROOT)/bin/libcnc.a
	$(CXX) \
	$(OBJ) \
	-o $(TARGET) \
	$(OPENCV_LIBS) \
	$(CNC_LIBS)

$(CNC_ROOT)/bin/libcnc.a:
	$(MAKE) -C $(CNC_ROOT) all

# Reglas de compilacion: C con mpicc, C++ (OpenCV) con mpic++.
%.o: %.c
	$(CC) \
	$(CFLAGS) \
	-I. -I../img -I$(CNC_ROOT)/include \
	-c $< \
	-o $@

../img/%.o: ../img/%.cpp
	$(CXX) \
	$(CXXFLAGS) \
	$(OPENCV_CFLAGS) \
	-I../mpi \
	-c $< \
	-o $@

clean:
	rm -f \
	$(MPI_C_OBJ) \
	$(IMG_CPP_OBJ) \
	$(TARGET)

rebuild: clean all

.PHONY: all clean rebuild