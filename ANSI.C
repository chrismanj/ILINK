#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <c:\progproj\c\common\include\types.h>
#include <c:\progproj\c\common\include\debug.h>
#include <c:\progproj\c\common\include\fileio.h>
#include <c:\progproj\c\common\include\doublell.h>
#include <c:\progproj\c\common\include\bqueue.h>
#include <c:\progproj\c\common\include\jscser.h>
#include <c:\progproj\c\common\include\keybrd.h>
#include <c:\progproj\c\common\include\chrgraph.h>
#include <c:\progproj\c\common\include\window.h>
#include <c:\progproj\c\common\include\intrface.h>
#include <c:\progproj\c\common\include\mem.h>
#include <c:\progproj\c\ilink\include\termobj.h>
#include <c:\progproj\c\ilink\include\ilink.h>

term_obj *InitANSI              (void);
void ANSISaveSetup		(term_obj *);
void ANSICopyScreenToSlave 	(term_obj *);
void ANSIInterpretControlCh     (term_obj *, BYTE);
void ANSIShowChar               (term_obj *, BYTE);
void ANSIOutChar                (term_obj *, BYTE);
void ANSICharHandler            (term_obj *, BYTE);

extern s_com_port *ports[2];

#define ANSI_obj struct s_ANSI_obj
struct s_ANSI_obj
{
  char args_str[4];
  int  curr_arg;
  int  curr_arg_char;
  int  arg_vals[10];
  WORD foreground;
  WORD background;
  WORD blink;
  WORD high_intensity;
  WORD saved_cursor_y;
  WORD saved_cursor_x;
};

BYTE ansi_colors[] = {0, 4, 2, 6, 1, 5, 3, 7};
char *ANSIClearScreenString = "\x1b""2J";

/******************************************************************************\

  Routine: InitANSI

 Function: Initialize values for ANSI routines

     Pass: Nothing

   Return: Terminal routines structure

\******************************************************************************/

term_obj *InitANSI (void)
{
  ANSI_obj *new_ansi;
  term_obj *new_term;
  FILE *config_file;
  int x;

  #ifdef DEBUG
    OutDebugText ("KA");
  #endif

  new_term = NewTerm(TRUE, "ANSI", "ANSI", "ANSI");
  if (new_term != NULL)
  {
    if ((new_ansi = (ANSI_obj *)malloc (sizeof(ANSI_obj))) != NULL)
    {

      new_ansi->background = 0x00;
      new_ansi->foreground = 0x07;
      new_ansi->blink = 0x00;
      new_ansi->high_intensity = 0x00;
      new_ansi->saved_cursor_y = 0;
      new_ansi->saved_cursor_x = 0;

      for (x = 0; x < 10; x++)
	new_ansi->arg_vals[x] = 0;
      new_ansi->curr_arg = 0;

      if((config_file = fopen("ANSI.cfg", "rb")) != NULL)
      {
	SetCurrentFile (config_file);
	fclose(config_file);
      }

      new_term->CharHandler          = ANSIOutChar;
      new_term->SaveTerminalSetup    = ANSISaveSetup;
      new_term->CopyScreenToSlave    = ANSICopyScreenToSlave;
      new_term->emulator             = new_ansi;
      new_term->ClearScreenString    = ANSIClearScreenString;
    }
  }
  return new_term;
}

/******************************************************************************\

  Routine: ANSISaveSetup

 Function:

     Pass:

   Return:

\******************************************************************************/

void ANSISaveSetup(term_obj *term)
{
  FILE *config_file;

  if((config_file = fopen("ANSI.cfg", "wb")) != NULL)
  {
    SetCurrentFile (config_file);
    fclose(config_file);
    MessageBoxPause ("ANSI Configuration Saved");
  }
  else
    MessageBoxPause ("Error trying to save ANSI config file.");
}

/******************************************************************************\

  Routine: ANSICopyScreenToSlave

 Function:

     Pass:

   Return:

\******************************************************************************/

void ANSICopyScreenToSlave (term_obj *term)
{
}

/******************************************************************************\

 Routine: ANSICollectArgs

    Pass:

  Return: Nothing

\******************************************************************************/

void ANSICollectArgs (term_obj *term, BYTE ch)
{
  int x;
  ANSI_obj *ansi = term->emulator;

  if (isdigit(ch))
    ansi->args_str[ansi->curr_arg_char++] = (char)ch;
  else if (ch == ';')
  {
    ansi->args_str[ansi->curr_arg_char] = 0;
    ansi->curr_arg_char = 0;
    ansi->arg_vals[ansi->curr_arg++] = atoi(ansi->args_str);
  }
  else
  {
    ansi->args_str[ansi->curr_arg_char] = 0;
    ansi->curr_arg_char = 0;
    ansi->arg_vals[ansi->curr_arg++] = atoi(ansi->args_str);

    switch (ch)
    {
      case 'A':		/*Cursor Up*/
	if (ansi->arg_vals[0] == 0) ansi->arg_vals[0]++;
	for (x = 0; x < ansi->arg_vals[0]; x++)
	  WindowCursorUp (term->window);
	break;

      case 'B':		/*Cursor Down*/
	if (ansi->arg_vals[0] == 0) ansi->arg_vals[0]++;
	for (x = 0; x < ansi->arg_vals[0]; x++)
	  WindowCursorDown (term->window);
	break;

      case 'C':		/*Cursor Right*/
	if (ansi->arg_vals[0] == 0) ansi->arg_vals[0]++;
	for (x = 0; x < ansi->arg_vals[0]; x++)
	  WindowCursorRight (term->window);
	break;

      case 'D':		/*Cursor Left*/
	if (ansi->arg_vals[0] == 0) ansi->arg_vals[0]++;
	for (x = 0; x < ansi->arg_vals[0]; x++)
	  WindowCursorLeft (term->window);
	break;

      case 'H':		/*Direct Cursor Addressing (Line, then column)*/
      case 'f':
	if (ansi->arg_vals[0] > 0) ansi->arg_vals[0]--;
	if (ansi->arg_vals[1] > 0) ansi->arg_vals[1]--;
	WindowCursorAt (term->window, ansi->arg_vals[1], ansi->arg_vals[0]);
	break;

      case 'J':         /* Clear screen */
	if (ansi->arg_vals[0] == 1 || ansi->arg_vals[0] == 2)
	{
	  WindowClear (term->window);
	  WindowCursorAt (term->window, 0, 0);
	}
	break;

      case 'K':
	WindowEraseToEOL(term->window);
	break;

      case 'm':         /* Set attribute */
	for (x = 0; x < ansi->curr_arg; x++)
	  if (ansi->arg_vals[x] < 30)
	  {
	    switch (ansi->arg_vals[x])
	    {
	      case 0:
		ansi->foreground = 0x07;
		ansi->background = 0x00;
		ansi->blink = 0x00;
		ansi->high_intensity = 0x00;
		break;

	      case 1:
		ansi->high_intensity = 0x08;
		break;

	      case 4:
		break;

	      case 5:
		ansi->blink = 0x80;
		break;

	      case 7:
		ansi->foreground = 0x00;
		ansi->background = 0x07;
		ansi->high_intensity = 0x00;
		break;

	      case 8:
		ansi->foreground = ansi->background;
		break;
	    }
	  }
	  else
	  {
	    if (ansi->arg_vals[x] < 40)
	      ansi->foreground = ansi_colors[ansi->arg_vals[x] - 30];
	    else
	      ansi->background = ansi_colors[ansi->arg_vals[x] - 40];
	  }
	WindowSetAttribute (term->window, ansi->background<<4 | ansi->foreground | ansi->blink | ansi->high_intensity);
	WindowSetDefAttrib (term->window, ansi->background<<4 | ansi->foreground | ansi->blink | ansi->high_intensity);
	break;

      case 's':
	ansi->saved_cursor_y = WindowGetCursorY(term->window);
	ansi->saved_cursor_x = WindowGetCursorX(term->window);
	break;

      case 'u':
	WindowCursorAt (term->window, ansi->saved_cursor_x, ansi->saved_cursor_y);
	break;

      case 'n':
	  if (ansi->arg_vals[0] == 6)
	  {
	    SerialStringWrite (ports[0], "\0x1b[");
	    SerialStringWrite (ports[0], itoa (WindowGetCursorY(term->window) + 1, "  ", 10));
	    SerialWrite (ports[0], ';');
	    SerialStringWrite (ports[0], itoa (WindowGetCursorX(term->window) + 1, "  ", 10));
	    SerialWrite (ports[0], 'R');
	  }
	break;

      #ifdef DEBUG
      default:
	WindowPrintChar (term->window, 0x1b);
	WindowPrintChar (term->window, '[');
	WindowPrintChar (term->window, ch);
	break;
      #endif
    }
    for (x = 0; x < ansi->curr_arg; x++)
      ansi->arg_vals[x] = 0;
    ansi->curr_arg = 0;
    term->CharHandler = ANSIOutChar;
  }
}

/******************************************************************************\

 Routine: ANSIInterpretEscape

    Pass:

  Return: Nothing

\******************************************************************************/

void ANSIInterpretEscape (term_obj *term, BYTE ch)
{
  term->CharHandler = ANSIOutChar;

  switch (ch)
  {
    case '[':
      term->CharHandler = ANSICollectArgs;
      break;

    #ifdef DEBUG
    default:
      WindowPrintChar (term->window, ch);
      break;
    #endif
  }
}

/******************************************************************************\

 Routine: ANSIInterpretControlCh

    Pass:

  Return: Nothing

\******************************************************************************/

void ANSIInterpretControlCh (term_obj *term, BYTE ch)
{
  switch (ch)
  {
    case 0x1b:
      term->CharHandler = ANSIInterpretEscape;
      break;

    case 0x0c:
      WindowClear (term->window);
      WindowCursorAt (term->window, 0, 0);
      break;

    default:
      WindowPrintChar (term->window, ch);
  }
}

/******************************************************************************\

 Routine: ANSIOutChar

    Pass:

  Return: Nothing

\******************************************************************************/

void ANSIOutChar (term_obj *term, BYTE ch)
{
  if (ch < 32)
    ANSIInterpretControlCh (term, ch);
  else
    WindowPrintChar (term->window, ch);
}
