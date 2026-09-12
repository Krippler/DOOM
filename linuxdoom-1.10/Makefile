################################################################
#
# $Id:$
#
# $Log:$
#
CC?= gcc  # gcc or g++

CFLAGS?=-O2 -g
CFLAGS+=-Wall -DNORMALUNIX -DLINUX # -DUSEASM 
LDFLAGS?=
# -lnsl is gone on modern glibc: those symbols now live in libc itself.
LIBS=-lXext -lX11 -lm

# Music: "fluidsynth" (default) or "none".
#
# The WADs hold music as MUS lumps, which mus2mid.c converts to Standard
# MIDI and FluidSynth renders against a General MIDI soundfont. "none"
# restores the silent dummies the original release shipped.
MUSIC?=fluidsynth

ifeq ($(MUSIC),fluidsynth)
CFLAGS+=-DMUSIC_FLUIDSYNTH
MUSIC_OBJ=$(O)/mus2mid.o
MUSIC_LIBS=-lfluidsynth -lpthread
else ifeq ($(MUSIC),none)
MUSIC_OBJ=
MUSIC_LIBS=
else
$(error MUSIC must be "fluidsynth" or "none", got "$(MUSIC)")
endif

# subdirectory for objects
O=linux

# not too sophisticated dependency
OBJS=				\
		$(O)/doomdef.o		\
		$(O)/doomstat.o		\
		$(O)/dstrings.o		\
		$(O)/i_system.o		\
		$(O)/i_sound.o		\
		$(O)/i_video.o		\
		$(O)/i_net.o			\
		$(O)/tables.o			\
		$(O)/f_finale.o		\
		$(O)/f_wipe.o 		\
		$(O)/d_main.o			\
		$(O)/d_net.o			\
		$(O)/d_items.o		\
		$(O)/g_game.o			\
		$(O)/m_menu.o			\
		$(O)/m_misc.o			\
		$(O)/m_argv.o  		\
		$(O)/m_bbox.o			\
		$(O)/m_fixed.o		\
		$(O)/m_swap.o			\
		$(O)/m_cheat.o		\
		$(O)/m_random.o		\
		$(O)/am_map.o			\
		$(O)/p_ceilng.o		\
		$(O)/p_doors.o		\
		$(O)/p_enemy.o		\
		$(O)/p_floor.o		\
		$(O)/p_inter.o		\
		$(O)/p_lights.o		\
		$(O)/p_map.o			\
		$(O)/p_maputl.o		\
		$(O)/p_plats.o		\
		$(O)/p_pspr.o			\
		$(O)/p_setup.o		\
		$(O)/p_sight.o		\
		$(O)/p_spec.o			\
		$(O)/p_switch.o		\
		$(O)/p_mobj.o			\
		$(O)/p_telept.o		\
		$(O)/p_tick.o			\
		$(O)/p_saveg.o		\
		$(O)/p_user.o			\
		$(O)/r_bsp.o			\
		$(O)/r_data.o			\
		$(O)/r_draw.o			\
		$(O)/r_main.o			\
		$(O)/r_plane.o		\
		$(O)/r_segs.o			\
		$(O)/r_sky.o			\
		$(O)/r_things.o		\
		$(O)/w_wad.o			\
		$(O)/wi_stuff.o		\
		$(O)/v_video.o		\
		$(O)/st_lib.o			\
		$(O)/st_stuff.o		\
		$(O)/hu_stuff.o		\
		$(O)/hu_lib.o			\
		$(O)/s_sound.o		\
		$(O)/z_zone.o			\
		$(O)/info.o				\
		$(O)/sounds.o			\
		$(MUSIC_OBJ)

all:	 $(O)/linuxxdoom

$(O):
	mkdir -p $(O)

clean:
	rm -f *.o *~ *.flc
	rm -f linux/*

$(O)/linuxxdoom:	$(O) $(OBJS) $(O)/i_main.o
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(O)/i_main.o \
	-o $(O)/linuxxdoom $(LIBS) $(MUSIC_LIBS)

$(O)/%.o:	%.c | $(O)
	$(CC) $(CFLAGS) -c $< -o $@

#############################################################
#
#############################################################