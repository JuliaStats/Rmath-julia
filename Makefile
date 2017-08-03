RVERSION := $(shell cat RVERSION)


all:
	$(MAKE) -C src

clean:
	$(MAKE) -C src clean
	rm -rf R-*

R-$(RVERSION).tar.gz:
	wget https://cran.r-project.org/src/base/R-3/$@

update: R-$(RVERSION).tar.gz
	tar -xzvf $< --strip-components 3 -C src --include '*/src/nmath/*.[ch]' \
	--exclude '*/sexp.c' --exclude '*/snorm.c' --exclude '*/standalone/*'
	find include/R_ext -name '*.h' | xargs -I {} tar -xzvf $< --strip-components 2 '*/src/{}'
	tar -xzvf $< --strip-components 2 '*/src/include/Rmath.h0.in'
	mv -f include/Rmath.h0.in include/Rmath.h

.PHONY: all clean update
