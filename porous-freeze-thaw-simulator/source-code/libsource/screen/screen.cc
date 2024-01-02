/***********************************************\
* A simple interface to SDL			*
* (C) 2005 Pavel Strachota			*
* Portions (C) 1998-2001 Sam Lantinga		*
* file: screen.cc				*
\***********************************************/

/*
This file implements the SCR_Color class and the functions
for the SDL surface access.
*/

#include "screen.h"
#include <stdlib.h>

static SDL_Surface * screen=NULL;

// ------------ SCR_Color implementation ------------
// see screen.h for info about the methods and operators

void SCR_Color::RefreshColor()
{
	Color=SDL_MapRGB(screen->format,(Uint8)(r*255),(Uint8)(g*255),(Uint8)(b*255));
}

void SCR_Color::Refreshrgb()
{
	Uint8 RR,GG,BB;

	SDL_GetRGB(Color,screen->format,&RR,&GG,&BB);
	r=(float)RR/255;
	g=(float)GG/255;
	b=(float)BB/255;
}

Uint8 SCR_Color::SDL_R()
{
	Uint8 RR,GG,BB;

	SDL_GetRGB(Color,screen->format,&RR,&GG,&BB);
	return(RR);
}

Uint8 SCR_Color::SDL_G()
{
	Uint8 RR,GG,BB;

	SDL_GetRGB(Color,screen->format,&RR,&GG,&BB);
	return(GG);
}

Uint8 SCR_Color::SDL_B()
{
	Uint8 RR,GG,BB;

	SDL_GetRGB(Color,screen->format,&RR,&GG,&BB);
	return(BB);
}

void SCR_Color::SetRGB(float rr, float gg, float bb)
{
	r=Restrict(rr);
	g=Restrict(gg);
	b=Restrict(bb);
	RefreshColor();
}

void SCR_Color::SetR(float rr)
{
	r=Restrict(rr);
	RefreshColor();
}

void SCR_Color::SetG(float gg)
{
	g=Restrict(gg);
	RefreshColor();
}

void SCR_Color::SetB(float bb)
{
	b=Restrict(bb);
	RefreshColor();
}

void SCR_Color::SDL_SetRGB(Uint8 RR, Uint8 GG, Uint8 BB)
{
	Color=SDL_MapRGB(screen->format,RR,GG,BB);
	Refreshrgb();
}

void SCR_Color::SDL_SetR(Uint8 RR)
{
	Uint8 RRR,GGG,BBB;

	SDL_GetRGB(Color,screen->format,&RRR,&GGG,&BBB);
	Color=SDL_MapRGB(screen->format,RR,GGG,BBB);
	Refreshrgb();
}

void SCR_Color::SDL_SetG(Uint8 GG)
{
	Uint8 RRR,GGG,BBB;

	SDL_GetRGB(Color,screen->format,&RRR,&GGG,&BBB);
	Color=SDL_MapRGB(screen->format,RRR,GG,BBB);
	Refreshrgb();
}

void SCR_Color::SDL_SetB(Uint8 BB)
{
	Uint8 RRR,GGG,BBB;

	SDL_GetRGB(Color,screen->format,&RRR,&GGG,&BBB);
	Color=SDL_MapRGB(screen->format,RRR,GGG,BB);
	Refreshrgb();
}

float SCR_Color::Intensity()
{
	return(0.299f*r+0.587f*g+0.114f*b);
}

// In the following operators, the restriction of r,g,b to the interval [0;1]
// is managed by the SCR_Color constructor
SCR_Color SCR_Color::operator + (SCR_Color c)
{
	return SCR_Color(r+c.r,g+c.g,b+c.b);
}

SCR_Color SCR_Color::operator - (SCR_Color c)
{
	return SCR_Color(r-c.r,g-c.g,b-c.b);
}

SCR_Color SCR_Color::operator * (SCR_Color c)
{
	return SCR_Color(r*c.r,g*c.g,b*c.b);
}

SCR_Color SCR_Color::operator * (float alpha)
{
	return SCR_Color(r*alpha,g*alpha,b*alpha);
}

SCR_Color SCR_Color::operator / (float alpha)
{
	return SCR_Color(r/alpha,g/alpha,b/alpha);
}

SCR_Color operator * (float alpha, SCR_Color c)
{
	return SCR_Color(c.r*alpha,c.g*alpha,c.b*alpha);
}

// ----------

SCR_Color & SCR_Color::operator += (SCR_Color c)
{
	r=Restrict(r+c.r);
	g=Restrict(g+c.g);
	b=Restrict(b+c.b);
	RefreshColor();
	return(*this);
}

SCR_Color & SCR_Color::operator -= (SCR_Color c)
{
	r=Restrict(r-c.r);
	g=Restrict(g-c.g);
	b=Restrict(b-c.b);
	RefreshColor();
	return(*this);
}

SCR_Color & SCR_Color::operator *= (SCR_Color c)
{
	r=Restrict(r*c.r);
	g=Restrict(g*c.g);
	b=Restrict(b*c.b);
	RefreshColor();
	return(*this);
}

SCR_Color & SCR_Color::operator *= (float alpha)
{
	r=Restrict(r*alpha);
	g=Restrict(g*alpha);
	b=Restrict(b*alpha);
	RefreshColor();
	return(*this);
}

SCR_Color & SCR_Color::operator /= (float alpha)
{
	r=Restrict(r/alpha);
	g=Restrict(g/alpha);
	b=Restrict(b/alpha);
	RefreshColor();
	return(*this);
}

SCR_Color & SCR_Color::operator = (Uint32 c)
{
	Color=c;
	Refreshrgb();
	return(*this);
}

SCR_Color::SCR_Color(Uint32 c) :
	Color(c)
{
	Refreshrgb();
}

SCR_Color::SCR_Color(Uint8 RR, Uint8 GG, Uint8 BB, int)
{
	SDL_SetRGB(RR,GG,BB);
}

SCR_Color::SCR_Color(float rr, float gg, float bb)
{
	r=Restrict(rr);
	g=Restrict(gg);
	b=Restrict(bb);
	RefreshColor();
}

// --------------------------------------------------

SDL_Surface * SCR_GetScreen()
// returns the screen SDL surface pointer
// (use this for advanced operations on the screen by means
// of SDL functions)
{
	return(screen);
}

bool SCR_IsInit()
// returns true if the SDL screen is initialized
{
	return((bool)screen);
}

int SCR_InitScreen(int width, int height, int colordepth, bool fullscreen)
/*
initializes an SDL screen

return codes:
0	success
-1	could not initialize SDL video
-2	could not initialize video mode
*/
{
	if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) return(-1);
	atexit(SDL_Quit);	// in case of compilation under LINUX

	screen=SDL_SetVideoMode(width,height,colordepth,SDL_HWSURFACE|SDL_DOUBLEBUF|(fullscreen?SDL_FULLSCREEN:0));
	if ( screen == NULL ) {
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return(-2);
	}
	return(0);
}

int SCR_ResizeScreen(int new_width, int new_height)
/*
tries to resize the screen to the new width and height

return codes:
0	success
-1	could not resize the SDL screen
*/
{
	SDL_Surface * scr;
	if((scr=SDL_SetVideoMode(new_width,new_height,screen->format->BitsPerPixel,screen->flags)) == NULL) return(-1);
	screen=scr;
	return(0);
}


void SCR_Lock()
// locks the screen. The screen can only be written to when it is locked.
{
	if ( SDL_MUSTLOCK(screen) )
		if ( SDL_LockSurface(screen) < 0 )
			return;
}

void SCR_Unlock()
// unlocks the screen
{
	if ( SDL_MUSTLOCK(screen) )
		SDL_UnlockSurface(screen);
}

int SCR_Flip()
/*
performs an SDL_Flip() call on the screen. See SDL documentation
for details. In short, SCR_Flip() ensures that the changes made
to the SDL surface appear onscreen.

return codes:
0	success
-1	error
*/
{
	return(SDL_Flip(screen));
}

void SCR_putpixel(int x, int y, SCR_Color c)
{
	int bpp = screen->format->BytesPerPixel;
	/* Here p is the address to the pixel we want to set */
	Uint8 *p = (Uint8 *)screen->pixels + y * screen->pitch + x * bpp;

	Uint32 cc=c;		// here the type cast operator is used

	switch(bpp) {
		case 1:
			*p = cc;
			break;

		case 2:
			*(Uint16 *)p = cc;
			break;

		case 3:
			if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
				p[0] = (cc >> 16) & 0xff;
				p[1] = (cc >> 8) & 0xff;
				p[2] = cc & 0xff;
			} else {
				p[0] = cc & 0xff;
				p[1] = (cc >> 8) & 0xff;
				p[2] = (cc >> 16) & 0xff;
			}
		break;

		case 4:
			*(Uint32 *)p = cc;
			break;
	}
}

SCR_Color SCR_getpixel(int x, int y)
{
	int bpp = screen->format->BytesPerPixel;
	/* Here p is the address to the pixel we want to retrieve */
	Uint8 *p = (Uint8 *)screen->pixels + y * screen->pitch + x * bpp;

	switch(bpp) {
		case 1:
			return SCR_Color(*p);

		case 2:
			return SCR_Color(*(Uint16 *)p);

		case 3:
			if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
				return SCR_Color(p[0] << 16 | p[1] << 8 | p[2]);
			else
				return SCR_Color(p[0] | p[1] << 8 | p[2] << 16);

		case 4:
			return SCR_Color(*(Uint32 *)p);

		default:
			return SCR_Color(0);
	}
}
