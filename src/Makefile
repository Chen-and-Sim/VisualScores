# Project: VisualScores

INCLUDES = -I../include/
C_FLAGS = ${INCLUDES} -W -std=c11
LIBS = -L../lib/ \
-lavformat -lavcodec -lavdevice -lavfilter -lavutil -lswresample -lswscale \
-lbcrypt -lgdi32 -liconv -lm -lmfplat -lole32 -lpthread -lrt -lsecur32 -lstrmiids -lws2_32 -lz

CC = gcc.exe
RM = rm.exe -f
OBJ = tracks.o partition.o visualscores.o avinfo.o vslog.o
VS_INCLUDE_PATH = ../include/vslog.h ../include/visualscores.h
BIN = ../VisualScores.exe

.PHONY: all all-before all-after clean clean-custom

all: all-before $(BIN) all-after

clean: clean-custom
	${RM} $(OBJ) $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o ../VisualScores.exe $(LIBS)

vslog.o: vslog.c ../include/vslog.h
	$(CC) -c vslog.c -o vslog.o $(C_FLAGS)

avinfo.o: avinfo.c ../include/avinfo.h
	$(CC) -c avinfo.c -o avinfo.o $(C_FLAGS)

tracks.o: tracks.c $(VS_INCLUDE_PATH)
	$(CC) -c tracks.c -o tracks.o $(C_FLAGS)

partition.o: partition.c $(VS_INCLUDE_PATH)
	$(CC) -c partition.c -o partition.o $(C_FLAGS)

visualscores.o: visualscores.c $(VS_INCLUDE_PATH)
	$(CC) -c visualscores.c -o visualscores.o $(C_FLAGS)
