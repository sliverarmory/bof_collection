# Modified by Sliver Armory contributors to exclude package helper directories.
DIRS := $(filter-out static/ scripts/ release/ .git/,$(wildcard */))

.PHONY: all $(DIRS)

all: $(DIRS)

# Run make in each subdirectory
$(DIRS):
	@echo "Entering $@"
	@$(MAKE) -C $@
