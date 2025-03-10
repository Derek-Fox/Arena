CFLAGS = -Wall -g -pthread

PROGRAMS = arena

arena_OBJS = arena.o util.o arena_protocol.o player.o alist.o playerlist.o queue.o duel.o

OBJS_DIR = build
BINS_DIR = bin
SRC_DIR = src

PATH_PROGS = $(PROGRAMS:%=$(BINS_DIR)/%)

.PHONY: all
all: $(OBJS_DIR) $(BINS_DIR) $(PATH_PROGS)

$(OBJS_DIR):
	@mkdir -p $(OBJS_DIR)

$(BINS_DIR):
	@mkdir -p $(BINS_DIR)

# bring in dependencies
-include $(OBJS_DIR)/*.d

define PROGRAM_template =
 $(BINS_DIR)/$(1): $$($(1)_OBJS:%.o=$$(OBJS_DIR)/%.o)
	$$(CC) -o $$@ $$(CFLAGS) $$($(1)_LDFLAGS) $$^ $$($(1)_LDLIBS)
endef

$(foreach prog,$(PROGRAMS),$(eval $(call PROGRAM_template,$(prog))))

# Note that -MMD and -MP are what allows us to handle dependencies automatically
$(OBJS_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJS_DIR)
	$(CC) -c -o $@ $(CFLAGS) -MMD -MP $< $(LDFLAGS)

.PHONY: clean
clean:
	rm -rf $(OBJS_DIR) $(BINS_DIR) *~ */*~

