## Copyright 2025 Andrea Bernardi
## All rights reserved. Distributed under the terms of the MIT License.

NAME = HaikuBench

SRCS = \
	WattHaiku.cpp \
	MainWindow.cpp \
	HeaderView.cpp \
	SplashWindow.cpp \
	Settings.cpp \
	BenchDeckWindow.cpp \
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
LDFLAGS = -lbe -llocalestub -lGL -lGLU -lglut -lgame

# Localization (Haiku Locale Kit)
SIGNATURE = application/x-vnd.HaikuBench
CATALOG_SIG = x-vnd.HaikuBench
LANGUAGES = it

.PHONY: all clean catkeys catalogs

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $(NAME)
	@# Embed the app signature + version resources so a locally built binary is
	@# a first-class citizen (Deskbar/Tracker), not just the recipe-built one.
	@if [ -f $(NAME).rdef ]; then \
		rc -o $(NAME).rsrc $(NAME).rdef && xres -o $(NAME) $(NAME).rsrc; fi
	@if [ -f icon.hvif ]; then addattr -t icon -f icon.hvif BEOS:ICON $(NAME); fi
	@mimeset -f $(NAME) 2>/dev/null || true

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(NAME)

# Scan the sources for B_TRANSLATE() keys and (re)generate the reference
# English catkeys. Preprocess with -E -P (no line markers, which collectcatkeys
# cannot parse) and B_COLLECTING_CATKEYS so B_TRANSLATE expands to B_CATKEY.
catkeys:
	@mkdir -p locales
	$(CC) -DB_COLLECTING_CATKEYS=1 -E -P $(CFLAGS) $(SRCS) > locales/_pp.cpp 2>/dev/null
	collectcatkeys -s $(SIGNATURE) -o locales/en.catkeys locales/_pp.cpp
	@rm -f locales/_pp.cpp
	@echo "Wrote locales/en.catkeys — translate it into locales/<lang>.catkeys"

# Compile each locales/<lang>.catkeys into a binary <lang>.catalog.
catalogs:
	@for lang in $(LANGUAGES); do \
		if [ -f locales/$$lang.catkeys ]; then \
			linkcatkeys -o locales/$$lang.catalog -s $(SIGNATURE) \
				-l $$lang locales/$$lang.catkeys && \
			echo "Built locales/$$lang.catalog"; \
		else \
			echo "Skip $$lang: locales/$$lang.catkeys not found"; \
		fi; \
	done

# Dependencies
WattHaiku.o: MainWindow.h GpuBenchWindow.h SplashWindow.h
MainWindow.o: MainWindow.h HeaderView.h BenchDeckWindow.h SysBenchmark.h CpuDatabase.h TeapotWindow.h Bench2DWindow.h GpuBenchWindow.h VulkanBenchWindow.h BenchStats.h Settings.h Version.h
HeaderView.o: HeaderView.h IconData.h
SplashWindow.o: SplashWindow.h Settings.h Version.h
Settings.o: Settings.h
BenchDeckWindow.o: BenchDeckWindow.h HeaderView.h Bench2DWindow.h GpuBenchWindow.h TeapotWindow.h VulkanBenchWindow.h
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
