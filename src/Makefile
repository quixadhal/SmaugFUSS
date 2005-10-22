CC      = gcc
#PROF    = -p

#Uncomment to compile in Cygwin
#CYGWIN = -DCYGWIN

# Uncomment the two lines below if compiling on a Solaris box
#SOLARIS_FLAG = -Dsun -DSYSV
#SOLARIS_LINK = -lnsl -lsocket

#IMC2 - Comment out to disable IMC2 support
IMC = 1

W_FLAGS = -Wall -Werror -Wshadow -Wformat-security -Wpointer-arith -Wcast-align -Wredundant-decls -Wbad-function-cast

C_FLAGS = -g2 $(W_FLAGS) $(SOLARIS_FLAG) $(PROF)
L_FLAGS = $(PROF) $(SOLARIS_LINK) -lz

C_FILES = act_comm.c act_info.c act_move.c act_obj.c act_wiz.c ban.c boards.c \
          build.c clans.c color.c comm.c comments.c const.c db.c deity.c fight.c \
          handler.c hashstr.c hotboot.c imm_host.c interp.c magic.c makeobjs.c mapout.c mccp.c \
          md5.c misc.c mpxset.c mud_comm.c mud_prog.c planes.c player.c polymorph.c \
          reset.c save.c services.c shops.c skills.c special.c tables.c \
          track.c update.c

ifdef IMC
   C_FILES := imc.c $(C_FILES)
   C_FLAGS := $(C_FLAGS) -DIMC -DIMCSMAUG
endif

O_FILES := $(patsubst %.c,o/%.o,$(C_FILES))

H_FILES = $(wildcard *.h) 

all:
	$(MAKE) -s smaug

ifdef CYGWIN
smaug: $(O_FILES)
	rm -f smaug.exe
	$(CC) -o smaug.exe $(O_FILES) $(L_FLAGS)
	echo "Done compiling mud.";
	chmod g+w smaug.exe
	chmod a+x smaug.exe
	chmod g+w $(O_FILES)

clean:
	rm -f o/*.o smaug.exe *~
else
smaug: $(O_FILES)
	rm -f smaug
	$(CC) -o smaug $(O_FILES) $(L_FLAGS)
	echo "Done compiling mud.";
	chmod g+w smaug
	chmod a+x smaug
	chmod g+w $(O_FILES)

clean:
	rm -f o/*.o smaug *~
endif

indent:
	indent -ts3 -nut -nsaf -nsai -nsaw -npcs -npsl -ncs -nbc -bls -prs -bap -cbi0 -cli3 -bli0 -l125 -lp -i3 -cdb -c1 -cd1 -sc -pmt $(C_FILES)
	indent -ts3 -nut -nsaf -nsai -nsaw -npcs -npsl -ncs -nbc -bls -prs -bap -cbi0 -cli3 -bli0 -l125 -lp -i3 -cdb -c1 -cd1 -sc -pmt $(H_FILES)

indentclean:
	rm *.c~ *.h~

o/%.o: %.c
	echo "  Compiling $@....";
	$(CC) -c $(C_FLAGS) $< -o $@

.c.o: mud.h
	$(CC) -c $(C_FLAGS) $<
