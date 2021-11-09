BUILD      := obj
PATH_FF	:= /home/ffmpeg/output
CFLAGS := -Wall
CFLAGS += -g -O0
DFLAGS := -MD

INC := -I./
INC += -I $(PATH_FF)/include
LIBS := -L$(PATH_FF)/lib 
LIBS += -lavformat -lavcodec -lavutil
LIBS += -lswresample -lswscale
LIBS += -lx264
LIBS += -lm -lpthread -ldl -lz

# streaming
SRCS_STRM = streaming.c
OBJS_STRM = $(addprefix $(BUILD)/, $(addsuffix .o,$(basename $(SRCS_STRM))))

# muxing
SRCS_MUX = muxing.c
OBJS_MUX = $(addprefix $(BUILD)/, $(addsuffix .o,$(basename $(SRCS_MUX))))

all: stream mux
clean:
	@rm -rf stream mux $(BUILD) log

$(BUILD): Makefile
	@mkdir -p $(BUILD)

stream: $(OBJS_STRM)
	$(CC) $(LDFLAGS) -o $@ $^  $(LIBS)

mux: $(OBJS_MUX)
	$(CC) $(LDFLAGS) -o $@ $^  $(LIBS)

$(BUILD)/%.o: %.c Makefile | $(BUILD)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@ $(DFLAGS)

##################################################
-include $(OBJS_STRM:.o=.d)
-include $(OBJS_MUX:.o=.d)

