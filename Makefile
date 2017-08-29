
default: all

.DEFAULT:
	cd src && $(MAKE) $@

install:
	cd src && $(MAKE) $@

clean:
	cd src && $(MAKE) $@

.PHONY: install

