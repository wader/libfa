# Copyright (c) 2015 Waystream AB
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the NetBSD license.  See the LICENSE file for details.

CC = cc
CFLAGS = -Wall -Wstrict-prototypes -Wmissing-prototypes -O0 -ggdb -O3 --coverage -pg
LDLIBS = --coverage -pg
COMMON_OBJS = \
	fa.o \
	fa_state_set.o \
	fa_state_set_hash.o \
	fa_state_group.o \
	fa_regexp_yacc.o \
	fa_regexp_lex.o \
	fa_regexp.o \
	fa_regexp_bin.o \
	fa_regexp_class.o \
	fa_misc.o

all: fatool faregress fagrep faexample

fagrep: fagrep.o \
	fa_sim.o \
	$(COMMON_OBJS)

fatool: fatool.o \
	fa_graphviz.o \
	fa_graphviz_tikz.o \
	fa_sim.o \
	fa_text.o \
	$(COMMON_OBJS)

faregress.o: CFLAGS += $(shell pcre-config --cflags)
faregress: LDLIBS += $(shell pcre-config --libs)
faregress: faregress.o \
	fa_sim.o \
	fa_sim_bitcomp.o \
	$(COMMON_OBJS)

faexample: faexample.o \
	fa_graphviz.o \
	fa_sim.o \
	$(COMMON_OBJS)

clean:
	rm -f *.o *.gcno *.gcda *.gcov fatool faregress fagrep faexample fa_regexp_lex.[ch] fa_regexp_yacc.[ch]

test: faregress
	./faregress --dir test
	which ruby > /dev/null && ruby gcovstats

%.c %.h: %.y
	yacc -p yylibfa -o $@ $<

%.c %.h: %.l
	lex -P yylibfa -o $@ $<

doc: faexample fatool
	./faexample
	for i in union dfa mdfa ; do dot -Gdpi=70 -Tpng -odoc/$$i.png $$i.dot ; done
	./fatool --in 're:^[df]{2}a+$$' --in 're:^(dfa)*$$' --out dot:- --dfa --min | dot -Gdpi=70 -Tpng -odoc/fatool.png /dev/stdin
