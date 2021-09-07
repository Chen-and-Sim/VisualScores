# Project: VisualScores

INCLUDES = -I../include/
C_FLAGS = ${INCLUDES} -W -std=c11
LIBS = -L../lib/ \
-lavformat -lavcodec -lavdevice -lavfilter -lavutil -lswresample -lswscale \
-lbcrypt -lgdi32 -liconv -lm -lmfplat -lole32 -lpthread -lrt -lsecur32 -lstrmiids -lwinmm -lws2_32 -lz

CC = gcc.exe
WINDRES  = windres.exe
RES = ../resource/resource.res
RM = rm.exe -f
OBJ = tracks.o partition.o video.o visualscores.o codec.o avinfo.o vslog.o $(RES)
AV_INCLUDE_PATH = ../include/vslog.h ../include/avinfo.h
VS_INCLUDE_PATH = ../include/vslog.h ../include/visualscores.h
BIN = ../VisualScores.exe

.PHONY: all all-before all-after clean clean-custom

all: all-before $(BIN) all-after

clean: clean-custom
	${RM} $(OBJ) $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $(BIN) $(LIBS)

vslog.o: vslog.c ../include/vslog.h
	$(CC) -c vslog.c -o vslog.o $(C_FLAGS)

codec.o: codec.c $(AV_INCLUDE_PATH)
	$(CC) -c codec.c -o codec.o $(C_FLAGS)

avinfo.o: avinfo.c $(AV_INCLUDE_PATH)
	$(CC) -c avinfo.c -o avinfo.o $(C_FLAGS)

tracks.o: tracks.c $(VS_INCLUDE_PATH)
	$(CC) -c tracks.c -o tracks.o $(C_FLAGS)

partition.o: partition.c $(VS_INCLUDE_PATH)
	$(CC) -c partition.c -o partition.o $(C_FLAGS)

video.o: video.c $(VS_INCLUDE_PATH)
	$(CC) -c video.c -o video.o $(C_FLAGS)

visualscores.o: visualscores.c $(VS_INCLUDE_PATH)
	$(CC) -c visualscores.c -o visualscores.o $(C_FLAGS)
	
$(RES): ../resource/resource.rc
	$(WINDRES) -i ../resource/resource.rc --input-format=rc -o $(RES) -O coff 
