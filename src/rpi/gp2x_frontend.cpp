#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "minimal.h"
#include "gp2x_frontend_list.h"
#include <SDL.h>
#include "allegro.h"

int game_num_avail=0;
static int last_game_selected=0;
char playemu[16] = "mame\0";
char playgame[16] = "builtinn\0";

int gp2x_freq=200;
int gp2x_video_depth=8;
int gp2x_video_aspect=0;
int gp2x_video_sync=0;
int gp2x_frameskip=-1;
int gp2x_sound = 14;
int gp2x_volume = 3;
int gp2x_clock_cpu=80;
int gp2x_clock_sound=80;
int gp2x_cpu_cores=1;
int gp2x_ramtweaks=1;
int gp2x_cheat=0;

static unsigned char *gp2xmenu_bmp;
static unsigned char *gp2xsplash_bmp;

static void gp2x_exit(void);

static void load_bmp_8bpp(unsigned char *out, unsigned char *in)
{
	int i,x,y;
	unsigned char r,g,b,c;
	in+=14; /* Skip HEADER */
	in+=40; /* Skip INFOHD */
	/* Set Palette */
	for (i=0;i<256;i++) {
		b=*in++;
		g=*in++;
		r=*in++;
		c=*in++;
		gp2x_video_color8(i,r,g,b);
	}
	gp2x_video_setpalette();
	/* Set Bitmap */	
	for (y=479;y!=-1;y--) {
		for (x=0;x<640;x++) {
			*out++=gp2x_palette[in[x+y*640]];
		}
	}
}

static void gp2x_intro_screen(int first_run) {
	char name[256];
	FILE *f;
	gp2x_video_flip();
	sprintf(name,"skins/rpisplash.bmp");
	f=fopen(name,"rb");
	if (f) {
		fread(gp2xsplash_bmp,1,350000,f);
		fclose(f);
	} 	
	else {
		printf("\nERROR: Splash screen missing from skins directory\n");
		gp2x_exit();
	}

	if(first_run) {
		load_bmp_8bpp(gp2x_screen8,gp2xsplash_bmp);
		gp2x_video_flip();
		sleep(1);
	}
	
	sprintf(name,"skins/rpimenu.bmp");
	f=fopen(name,"rb");
	if (f) {
		fread(gp2xmenu_bmp,1,328017,f);
		fclose(f);
	} 
	else {
		printf("\nERROR: Menu screen missing from skins directory\n");
		gp2x_exit();
	}
}

static void game_list_init_nocache(void)
{
	int i;
	FILE *f;
	DIR *d=opendir("roms");
	char game[32];
	if (d)
	{
		struct dirent *actual=readdir(d);
		while(actual)
		{
			for (i=0;i<NUMGAMES;i++)
			{
				if (fe_drivers[i].available==0)
				{
					sprintf(game,"%s.zip",fe_drivers[i].name);
					if (strcmp(actual->d_name,game)==0)
					{
						fe_drivers[i].available=1;
						game_num_avail++;
						break;
					}
				}
			}
			actual=readdir(d);
		}
		closedir(d);
	}
	
	if (game_num_avail)
	{
		remove("frontend/mame.lst");
		sync();
		f=fopen("frontend/mame.lst","w");
		if (f)
		{
			for (i=0;i<NUMGAMES;i++)
			{
				fputc(fe_drivers[i].available,f);
			}
			fclose(f);
			sync();
		}
	}
}

static void game_list_init_cache(void)
{
	FILE *f;
	int i;
	f=fopen("frontend/mame.lst","r");
	if (f)
	{
		for (i=0;i<NUMGAMES;i++)
		{
			fe_drivers[i].available=fgetc(f);
			if (fe_drivers[i].available)
				game_num_avail++;
		}
		fclose(f);
	}
	else
		game_list_init_nocache();
}

static void game_list_init(int argc)
{
	if (argc==1)
		game_list_init_nocache();
	else
		game_list_init_cache();
}

static void game_list_view(int *pos) {

	int i;
	int view_pos;
	int aux_pos=0;
	int screen_y = 45;
	int screen_x = 38;

	/* Draw background image */
	load_bmp_8bpp(gp2x_screen8,gp2xmenu_bmp);

	/* Check Limits */
	if (*pos<0)
		*pos=game_num_avail-1;
	if (*pos>(game_num_avail-1))
		*pos=0;
					   
	/* Set View Pos */
	if (*pos<10) {
		view_pos=0;
	} else {
		if (*pos>game_num_avail-11) {
			view_pos=game_num_avail-21;
			view_pos=(view_pos<0?0:view_pos);
		} else {
			view_pos=*pos-10;
		}
	}

	/* Show List */
	for (i=0;i<NUMGAMES;i++) {
		if (fe_drivers[i].available==1) {
			if (aux_pos>=view_pos && aux_pos<=view_pos+20) {
				if (aux_pos==*pos) {
					gp2x_gamelist_text_out( screen_x, screen_y, fe_drivers[i].description, 29);
					gp2x_gamelist_text_out( screen_x-10, screen_y,">",255 );
					gp2x_gamelist_text_out( screen_x-13, screen_y-1,"-",255 );
				}
				else {
					gp2x_gamelist_text_out( screen_x, screen_y, fe_drivers[i].description, 255);
				}
				
				screen_y+=8;
			}
			aux_pos++;
		}
	}
}

static void game_list_select (int index, char *game, char *emu) {
	int i;
	int aux_pos=0;
	for (i=0;i<NUMGAMES;i++)
	{
		if (fe_drivers[i].available==1)
		{
			if(aux_pos==index)
			{
				strcpy(game,fe_drivers[i].name);
				strcpy(emu,fe_drivers[i].exe);
				gp2x_cpu_cores=fe_drivers[i].cores;
				break;
			}
			aux_pos++;
		}
	}
}

static char *game_list_description (int index)
{
	int i;
	int aux_pos=0;
	for (i=0;i<NUMGAMES;i++) {
		if (fe_drivers[i].available==1) {
			if(aux_pos==index) {
				return(fe_drivers[i].description);
			}
			aux_pos++;
		   }
	}
	return ((char *)0);
}


static void gp2x_exit(void)
{
	remove("frontend/mame.lst");
	sync();
	gp2x_deinit();
    deinit_SDL();
	exit(0);
}

extern unsigned long ExKey1;

extern int osd_is_key_pressed(int keycode);
extern int is_joy_axis_pressed (int axis, int dir, int joynum);

static void select_game(char *emu, char *game)
{

	unsigned long ExKey=0;

	/* No Selected game */
	strcpy(game,"builtinn");

	/* Clean screen */
	gp2x_video_flip();

	gp2x_joystick_clear();	

	/* Wait until user selects a game */
	while(1)
	{
		game_list_view(&last_game_selected);
		gp2x_video_flip();
       	gp2x_timer_delay(100000);

//sq        if( (gp2x_joystick_read()))
//sq        	gp2x_timer_delay(100000);
		while(1)
		{
            usleep(10000);
			gp2x_joystick_read();	

			//Any joy buttons pressed?
			if (ExKey1)
			{
				ExKey=ExKey1;
				break;
			}

			//Any keyboard key pressed?
			if(osd_is_key_pressed(KEY_LEFT) ||
			   osd_is_key_pressed(KEY_RIGHT) ||
			   osd_is_key_pressed(KEY_UP) ||
			   osd_is_key_pressed(KEY_DOWN) ||
			   osd_is_key_pressed(KEY_ENTER) ||
			   osd_is_key_pressed(KEY_LCONTROL) ||
			   osd_is_key_pressed(KEY_ESC)) 
			{
				break;
			}

			//Any stick direction?
			if(is_joy_axis_pressed (0, 1, 0) ||
			   is_joy_axis_pressed (0, 2, 0) ||
			   is_joy_axis_pressed (1, 1, 0) ||
			   is_joy_axis_pressed (1, 2, 0))
			{
				break;
			}

		}

//sq		if (ExKey & GP2X_UP) last_game_selected--;
//sq		if (ExKey & GP2X_DOWN) last_game_selected++;
//sq		if (ExKey & GP2X_LEFT) last_game_selected-=21;
//sq		if (ExKey & GP2X_RIGHT) last_game_selected+=21;
		int updown=0;
		if(is_joy_axis_pressed (1, 1, 0)) {last_game_selected--; updown=1;};
		if(is_joy_axis_pressed (1, 2, 0)) {last_game_selected++; updown=1;};

		if(!updown) {
			if(is_joy_axis_pressed (0, 1, 0)) last_game_selected-=21;
			if(is_joy_axis_pressed (0, 2, 0)) last_game_selected+=21;
		}

		if (ExKey & GP2X_ESCAPE) gp2x_exit();

		if (osd_is_key_pressed(KEY_UP)) last_game_selected--;
		if (osd_is_key_pressed(KEY_DOWN)) last_game_selected++;
		if (osd_is_key_pressed(KEY_LEFT)) last_game_selected-=21;
		if (osd_is_key_pressed(KEY_RIGHT)) last_game_selected+=21;
		if (osd_is_key_pressed(KEY_ESC)) gp2x_exit();

		if ((ExKey & GP2X_LCTRL) || (ExKey & GP2X_RETURN) || 
			osd_is_key_pressed(KEY_LCONTROL) || osd_is_key_pressed(KEY_ENTER))
		{
			/* Select the game */
			game_list_select(last_game_selected, game, emu);

			break;
		}
	}
}

void frontend_gui (char *gamename, int first_run)
{
	FILE *f;

	/* GP2X Initialization */
	gp2x_frontend_init();

	gp2xmenu_bmp = (unsigned char*)calloc(1, 400000);
	gp2xsplash_bmp = (unsigned char*)calloc(1, 400000);

	/* Show load bitmaps and show intro screen */
    gp2x_intro_screen(first_run);

	/* Initialize list of available games */
	game_list_init(1);
	if (game_num_avail==0)
	{
		/* Draw background image */
    	load_bmp_8bpp(gp2x_screen8,gp2xmenu_bmp);
		gp2x_gamelist_text_out(35, 110, "ERROR: NO AVAILABLE GAMES FOUND",255);
		gp2x_video_flip();
		sleep(5);
		gp2x_exit();
	}

	/* Read default configuration */
	f=fopen("frontend/mame.cfg","r");
	if (f) {
		fscanf(f,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",&gp2x_freq,&gp2x_video_depth,&gp2x_video_aspect,&gp2x_video_sync,
		&gp2x_frameskip,&gp2x_sound,&gp2x_clock_cpu,&gp2x_clock_sound,&gp2x_cpu_cores,&gp2x_ramtweaks,&last_game_selected,&gp2x_cheat,&gp2x_volume);
		fclose(f);
	}
	
	/* Select Game */
	select_game(playemu,playgame); 

	/* Write default configuration */
	f=fopen("frontend/mame.cfg","w");
	if (f) {
		fprintf(f,"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",gp2x_freq,gp2x_video_depth,gp2x_video_aspect,gp2x_video_sync,
		gp2x_frameskip,gp2x_sound,gp2x_clock_cpu,gp2x_clock_sound,gp2x_cpu_cores,gp2x_ramtweaks,last_game_selected,gp2x_cheat,gp2x_volume);
		fclose(f);
		sync();
	}
	
    strcpy(gamename, playgame);
    
    gp2x_frontend_deinit();

	free(gp2xmenu_bmp);
	free(gp2xsplash_bmp);
	
}
