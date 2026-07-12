## Copyright 2025 Andrea Bernardi
## All rights reserved. Distributed under the terms of the MIT License.

NAME = HaikuBench

SRCS = \
	WattHaiku.cpp \
	MainWindow.cpp \
	CpuDatabase.cpp \
	SysBenchmark.cpp \
	BenchStats.cpp \
	AdaptiveRunner.cpp \
	BenchWarmup.cpp \
	TeapotWindow.cpp \
	Bench2DWindow.cpp \
	GpuBenchWindow.cpp \
	VulkanBenchWindow.cpp \
	TempOverlay.cpp

OBJS = $(SRCS:.cpp=.o)

CC = g++
CFLAGS = -Wall -Wextra -std=c++14 -O2
LDFLAGS = -lbe -lGL -lGLU -lglut -lgame

.PHONY: all clean

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(NAME)
	@if [ -f icon.hvif ]; then addattr -t icon -f icon.hvif BEOS:ICON $(NAME); fi

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(NAME)

# Dependencies
WattHaiku.o: MainWindow.h GpuBenchWindow.h
MainWindow.o: MainWindow.h SysBenchmark.h CpuDatabase.h TeapotWindow.h Bench2DWindow.h GpuBenchWindow.h VulkanBenchWindow.h BenchStats.h Version.h
SysBenchmark.o: SysBenchmark.h CpuDatabase.h BenchStats.h AdaptiveRunner.h BenchWarmup.h
BenchStats.o: BenchStats.h
AdaptiveRunner.o: AdaptiveRunner.h BenchStats.h
BenchWarmup.o: BenchWarmup.h
Bench2DWindow.o: Bench2DWindow.h
CpuDatabase.o: CpuDatabase.h
TeapotWindow.o: TeapotWindow.h TempOverlay.h
GpuBenchWindow.o: GpuBenchWindow.h TempOverlay.h
VulkanBenchWindow.o: VulkanBenchWindow.h
TempOverlay.o: TempOverlay.h
