OUTPUT  = req ns req_slow
BIN     = $(foreach B,$(OUTPUT),$(B).bin)

TEST_FILES = $(wildcard tests/*.c)
TEST_OBJS  = $(patsubst %.c,%.o,$(TEST_FILES))
TEST_DEPS  = $(patsubst %.c,%.d,$(TEST_FILES))
TEST_BIN   = $(sort $(patsubst %.c,%.test,$(TEST_FILES)))
TEST_TMP   = $(TEST_OBJS) $(TEST_DEPS) $(TEST_BIN)

SRV_FILES = $(wildcard srv/*.c)
SRV_OBJS  = $(patsubst %.c,%.o,$(SRV_FILES))
SRV_DEPS  = $(patsubst %.c,%.d,$(SRV_FILES))
SRV_BIN   = $(sort $(patsubst %.c,%.bin,$(SRV_FILES)))
SRV_TMP   = $(SRV_OBJS) $(SRV_DEPS) $(SRV_BIN)

LIBDIR     = libs
INCDIRS    = . $(LIBDIR) util

CC       = gcc
# generate files that encode make rules for the .h dependencies
DEPFLAGS = -MP -MD
CFLAGS   = -Wall -Wextra -Werror -Wno-unused-function -g $(foreach D,$(INCDIRS),-I$(D)) -O0 $(DEPFLAGS)
LD       = gcc
LDFLAGS  = -L. -L$(LIBDIR) # -lptrie

CFILE    = $(wildcard *.c)
# regular expression replacement
OBJECT   = $(patsubst %.c,%.o,$(CFILE))
DEPFILE  = $(patsubst %.c,%.d,$(CFILE))
TMPFILE  = $(OBJECT) $(DEPFILE)

DOC_OUT  = README.pdf

UTIL     = util
UTIL_URL = https://github.com/gwu-cs-sysprog/utils/raw/main/util.tgz

all: clean_output $(UTIL) $(OUTPUT) $(SRV_BIN) # $(TEST_BIN)

$(UTIL):
	wget $(UTIL_URL)
	tar zxvf util.tgz
	rm util.tgz

%.test: %.o
	$(LD) -o $@ $< $(LDFLAGS)

%.o:%.c
	$(CC) $(CFLAGS) -c -o $@ $<

### Products ###

$(OUTPUT): $(BIN)
	$(foreach B,$@,cp $(B).bin $(B))

%.bin: %.o
	$(LD) -o $@ $< $(LDFLAGS)

test: all $(UTIL)
	@echo "Running tests..."
	$(foreach T, $(TEST_BIN), ./$(T);)
	$(foreach T, $(SHTESTS), sh tests/shell_check.sh $(T);)
	@echo "\nRunning valgrind tests..."
	$(foreach T, $(TEST_BIN), sh util/valgrind_test.sh ./$(T);)
	$(foreach T, $(SHTESTS), sh tests/shell_check_valgrind.sh $(T);)
## 	@echo "\nRunning symbol visibility test..."
## 	sh tests/assess_visibility.sh "ptrie_add\|ptrie_allocate\|ptrie_autocomplete\|ptrie_free\|ptrie_print\|ptrie_test_eval" $(LIB)

%.pdf: %.md
	pandoc -V geometry:margin=1in $^ -o $@

doc: $(DOC_OUT)

clean: clean_output
	rm -rf $(TEST_TMP) $(TMPFILE) $(DOC_OUT) $(BIN) $(SRV_TMP)

clean_output:
	rm -rf $(OUTPUT) ns_domain_socket

.PHONY: all test clean doc prebin clean_output

# include the dependencies
-include $(DEPFILE) $(TEST_DEPS) $(LIBDEPS)
