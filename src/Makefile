CXX := g++
CXXFLAGS := -std=c++17 -O2 -fopenmp -I../include

TARGET := code
OBJDIR := obj

SRCS := main.cpp common.cpp decoder.cpp \
        decoder/base.cpp \
        decoder/classic.cpp \
        decoder/dp.cpp \
        decoder/random_dp.cpp \
        decoder/dp_rebuild.cpp \
        decoder/new_heuristic.cpp \
        decoder/local_search.cpp

OBJS := $(addprefix $(OBJDIR)/,$(notdir $(SRCS:.cpp=.o)))
HDRS := $(wildcard ../include/*.h) $(wildcard ../include/decoder/*.h)

# ---- Profiling (gprof) ----
# A "flag do profile" e apenas -pg adicionada ao build REAL (mesmos -O3 -fopenmp
# do fluxo normal). Nao muda o codigo nem o numero de threads: voce roda
# ./code_prof exatamente como roda ./code (ex.: nohup ./code_prof 6).
PROF_TARGET := code_prof
PROFDIR := ../profile

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(TARGET)

$(OBJDIR)/%.o: %.cpp $(HDRS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/%.o: decoder/%.cpp $(HDRS) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

# 1) Compila o binario real com a flag -pg do gprof (mesmo -O3 do seu comando).
$(PROF_TARGET): $(SRCS) $(HDRS)
	$(CXX) -std=c++17 -O3 -fopenmp -pg -I../include $(SRCS) -o $(PROF_TARGET)

# 2) Rode ./$(PROF_TARGET) <type> do jeito natural (ex.: nohup ./code_prof 6).
#    Ao terminar, ele deixa um gmon.out aqui em src/. Entao gere o relatorio:
profile-report:
	mkdir -p $(PROFDIR)
	gprof $(PROF_TARGET) gmon.out > $(PROFDIR)/gprof.txt
	cd .. && python3 plot_profile.py profile/gprof.txt
	@echo "Profile gerado em $(PROFDIR)/ (gprof.txt + PNGs)"

clean:
	rm -rf $(OBJDIR) $(TARGET) $(PROF_TARGET) gmon.out

.PHONY: all clean profile-report
