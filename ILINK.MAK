PROJ	=ILINK
DEBUG	=1
CC	=qcl
CFLAGS_G	= /AL /W4 /Za /G2 /FPi87 
CFLAGS_D	= /Zd /Ox 
CFLAGS_R	= /O /Ol /Gs 
CFLAGS	=$(CFLAGS_G) $(CFLAGS_D)
LFLAGS_G	= /CP:0xfff /NOI /NOE /SE:0x80 /ST:0x800 
LFLAGS_D	= 
LFLAGS_R	= 
LFLAGS	=$(LFLAGS_G) $(LFLAGS_D)
RUNFLAGS	=
OBJS_EXT = 	
LIBS_EXT = 	

.asm.obj: ; $(AS) $(AFLAGS) -c $*.asm

all:	$(PROJ).EXE

ilink.obj:	ilink.c $(H)

c332.obj:	c332.c $(H)

chrgraph.obj:	..\common\chrgraph.c $(H)

bqueue.obj:	..\common\bqueue.c $(H)

debug.obj:	..\common\debug.c $(H)

doublell.obj:	..\common\doublell.c $(H)

fileio.obj:	..\common\fileio.c $(H)

intrface.obj:	..\common\intrface.c $(H)

jscio.obj:	..\common\jscio.c $(H)

jscser.obj:	..\common\jscser.c $(H)

jsctime.obj:	..\common\jsctime.c $(H)

keybrd.obj:	..\common\keybrd.c $(H)

speaker.obj:	..\common\speaker.c $(H)

filexfer.obj:	..\filexfer\filexfer.c $(H)

kermit.obj:	..\filexfer\kermit.c $(H)

xmodem.obj:	..\filexfer\xmodem.c $(H)

vt52.obj:	vt52.c $(H)

window.obj:	..\common\window.c $(H)

modem.obj:	..\common\modem.c $(H)

video.obj:	..\common\video.c $(H)

termobj.obj:	termobj.c $(H)

thread.obj:	..\filexfer\thread.c $(H)

pcnansi.obj:	pcnansi.c $(H)

vt100.obj:	vt100.c $(H)

ansi.obj:	ansi.c $(H)

c332e.obj:	c332e.c $(H)

$(PROJ).EXE:	ilink.obj c332.obj chrgraph.obj bqueue.obj debug.obj doublell.obj fileio.obj \
	intrface.obj jscio.obj jscser.obj jsctime.obj keybrd.obj speaker.obj filexfer.obj \
	kermit.obj xmodem.obj vt52.obj window.obj modem.obj video.obj termobj.obj thread.obj \
	pcnansi.obj vt100.obj ansi.obj c332e.obj $(OBJS_EXT)
	echo >NUL @<<$(PROJ).crf
ilink.obj +
c332.obj +
chrgraph.obj +
bqueue.obj +
debug.obj +
doublell.obj +
fileio.obj +
intrface.obj +
jscio.obj +
jscser.obj +
jsctime.obj +
keybrd.obj +
speaker.obj +
filexfer.obj +
kermit.obj +
xmodem.obj +
vt52.obj +
window.obj +
modem.obj +
video.obj +
termobj.obj +
thread.obj +
pcnansi.obj +
vt100.obj +
ansi.obj +
c332e.obj +
$(OBJS_EXT)
$(PROJ).EXE

$(LIBS_EXT);
<<
	ilink -a -e "qlink $(LFLAGS) @$(PROJ).crf" $(PROJ)

run: $(PROJ).EXE
	$(PROJ) $(RUNFLAGS)

