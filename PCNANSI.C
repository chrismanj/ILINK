#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "..\common\include\types.h"
#include "..\common\include\debug.h"
#include "..\common\include\fileio.h"
#include "..\common\include\doublell.h"
#include "..\common\include\bqueue.h"
#include "..\common\include\jscser.h"
#include "..\common\include\keybrd.h"
#include "..\common\include\chrgraph.h"
#include "..\common\include\window.h"
#include "..\common\include\intrface.h"
#include "..\common\include\mem.h"
#include "include\termobj.h"
#include "include\ilink.h"

term_obj *InitPCNANSI(void);
void PCNANSISaveSetup(term_obj *);
void PCNANSICopyScreenToSlave(term_obj *);
void PCNANSIInterpretControlCh(term_obj *, BYTE);
void PCNANSIShowChar(term_obj *, BYTE);
BOOLN PCNANSIStatBar(term_obj *);
void PCNANSIOutChar(term_obj *, BYTE);
void PCNANSICharHandler(term_obj *, BYTE);

extern s_com_port *ports[2];

#define PCNANSI_obj struct s_PCNANSI_obj
struct s_PCNANSI_obj
{
  char args_str[4];
  int curr_arg;
  int curr_arg_char;
  int arg_vals[10];
  WORD foreground;
  WORD background;
  BYTE blink;
  BYTE high_bit;
  WORD high_intensity;
  WORD saved_cursor_y;
  WORD saved_cursor_x;
};

BYTE pcnansi_colors[] = {0, 4, 2, 6, 1, 5, 3, 7};
char *PCNANSIClearScreenString = "\x1b"
                                 "2J";

/******************************************************************************\

  Routine: InitPCNANSI

 Function: Initialize values for PCNANSI routines

     Pass: Nothing

   Return: Terminal routines structure

\******************************************************************************/

term_obj *InitPCNANSI(void)
{
  PCNANSI_obj *new_ansi;
  term_obj *new_term;
  FILE *config_file;
  int x;

  new_term = NewTerm(FALSE, "PCNANSI", "PCNANSI", "PCNANSI");
  if (new_term != NULL)
  {
    if ((new_ansi = (PCNANSI_obj *)malloc(sizeof(PCNANSI_obj))) != NULL)
    {

      new_ansi->background = 0x00;
      new_ansi->foreground = 0x07;
      new_ansi->blink = 0x00;
      new_ansi->high_bit = 0x00;
      new_ansi->high_intensity = 0x00;
      new_ansi->saved_cursor_y = 0;
      new_ansi->saved_cursor_x = 0;

      for (x = 0; x < 10; x++)
        new_ansi->arg_vals[x] = 0;
      new_ansi->curr_arg = 0;

      if ((config_file = fopen("PCNANSI.cfg", "rb")) != NULL)
      {
        SetCurrentFile(config_file);
        fclose(config_file);
      }

      new_term->window = CreateWindow("", 0, 0, 25, 80, 0x07, CLEAR);
      new_term->CharHandler = PCNANSIOutChar;
      new_term->SaveTerminalSetup = PCNANSISaveSetup;
      new_term->CopyScreenToSlave = PCNANSICopyScreenToSlave;
      new_term->StatBar = PCNANSIStatBar;
      new_term->emulator = new_ansi;
      new_term->ClearScreenString = PCNANSIClearScreenString;
    }
  }
  return new_term;
}

/******************************************************************************\

  Routine: PCNANSISaveSetup

 Function:

     Pass:

   Return:

\******************************************************************************/

void PCNANSISaveSetup(term_obj *term)
{
  FILE *config_file;

  if ((config_file = fopen("PCNANSI.cfg", "wb")) != NULL)
  {
    SetCurrentFile(config_file);
    fclose(config_file);
    MessageBoxPause("PCNANSI Configuration Saved");
  }
  else
    MessageBoxPause("Error trying to save PCNANSI config file.");
}

/******************************************************************************\

  Routine: PCNANSICopyScreenToSlave

 Function:

     Pass:

   Return:

\******************************************************************************/

void PCNANSICopyScreenToSlave(term_obj *term)
{
}

/******************************************************************************\

  Routine: PCNANSIStatBar

 Function: Notifies the calling program whether or not this subprogram has
	   control of the status bar.

     Pass: Nothing

   Return:

\******************************************************************************/

BOOLN PCNANSIStatBar(term_obj *term)
{
  return TRUE;
}

/******************************************************************************\

 Routine: PCNANSICollectArgs

    Pass:

  Return: Nothing

\******************************************************************************/

void PCNANSICollectArgs(term_obj *term, BYTE ch)
{
  int x;
  PCNANSI_obj *pcnansi = term->emulator;

  if (isdigit(ch))
    pcnansi->args_str[pcnansi->curr_arg_char++] = (char)ch;
  else if (ch == ';')
  {
    pcnansi->args_str[pcnansi->curr_arg_char] = 0;
    pcnansi->curr_arg_char = 0;
    pcnansi->arg_vals[pcnansi->curr_arg++] = atoi(pcnansi->args_str);
  }
  else
  {
    pcnansi->args_str[pcnansi->curr_arg_char] = 0;
    pcnansi->curr_arg_char = 0;
    pcnansi->arg_vals[pcnansi->curr_arg++] = atoi(pcnansi->args_str);

    switch (ch)
    {
    case 'A': /*Cursor Up*/
      if (pcnansi->arg_vals[0] == 0)
        pcnansi->arg_vals[0]++;
      for (x = 0; x < pcnansi->arg_vals[0]; x++)
        WindowCursorUp(term->window);
      break;

    case 'B': /*Cursor Down*/
      if (pcnansi->arg_vals[0] == 0)
        pcnansi->arg_vals[0]++;
      for (x = 0; x < pcnansi->arg_vals[0]; x++)
        WindowCursorDown(term->window);
      break;

    case 'C': /*Cursor Right*/
      if (pcnansi->arg_vals[0] == 0)
        pcnansi->arg_vals[0]++;
      for (x = 0; x < pcnansi->arg_vals[0]; x++)
        WindowCursorRight(term->window);
      break;

    case 'D': /*Cursor Left*/
      if (pcnansi->arg_vals[0] == 0)
        pcnansi->arg_vals[0]++;
      for (x = 0; x < pcnansi->arg_vals[0]; x++)
        WindowCursorLeft(term->window);
      break;

    case 'H': /*Direct Cursor Addressing (Line, then column)*/
    case 'f':
      if (pcnansi->arg_vals[0] > 0)
        pcnansi->arg_vals[0]--;
      if (pcnansi->arg_vals[1] > 0)
        pcnansi->arg_vals[1]--;
      WindowCursorAt(term->window, pcnansi->arg_vals[1], pcnansi->arg_vals[0]);
      break;

    case 'J': /* Clear screen */
      if (pcnansi->arg_vals[0] == 1 || pcnansi->arg_vals[0] == 2)
      {
        WindowClear(term->window);
        WindowCursorAt(term->window, 0, 0);
      }
      break;

    case 'K':
      WindowEraseToEOL(term->window);
      break;

    case 'm': /* Set attribute */
      for (x = 0; x < pcnansi->curr_arg; x++)
        if (pcnansi->arg_vals[x] < 30)
        {
          switch (pcnansi->arg_vals[x])
          {
          case 0:
            pcnansi->foreground = 0x07;
            pcnansi->background = 0x00;
            pcnansi->blink = 0x00;
            pcnansi->high_bit = 0x00;
            pcnansi->high_intensity = 0x00;
            break;

          case 1:
            pcnansi->high_intensity = 0x08;
            break;

          case 4:
            break;

          case 5:
            pcnansi->blink = 0x80;
            break;

          case 7:
            pcnansi->foreground = 0x00;
            pcnansi->background = 0x07;
            pcnansi->high_intensity = 0x00;
            break;

          case 8:
            pcnansi->foreground = pcnansi->background;
            break;

          case 10:
            pcnansi->high_bit = 0x00;
            break;

          case 12:
            pcnansi->high_bit = 0x80;
            break;
          }
        }
        else
        {
          if (pcnansi->arg_vals[x] < 40)
            pcnansi->foreground = pcnansi_colors[pcnansi->arg_vals[x] - 30];
          else
            pcnansi->background = pcnansi_colors[pcnansi->arg_vals[x] - 40];
        }
      WindowSetAttribute(term->window, pcnansi->background << 4 | pcnansi->foreground | pcnansi->blink | pcnansi->high_intensity);
      WindowSetDefAttrib(term->window, pcnansi->background << 4 | pcnansi->foreground | pcnansi->blink | pcnansi->high_intensity);
      break;

    case 's':
      pcnansi->saved_cursor_y = WindowGetCursorY(term->window);
      pcnansi->saved_cursor_x = WindowGetCursorX(term->window);
      break;

    case 'u':
      WindowCursorAt(term->window, pcnansi->saved_cursor_x, pcnansi->saved_cursor_y);
      break;

    case 'n':
      if (pcnansi->arg_vals[0] == 6)
      {
        SerialStringWrite(ports[0], "\0x1b[");
        SerialStringWrite(ports[0], itoa(WindowGetCursorY(term->window) + 1, "  ", 10));
        SerialWrite(ports[0], ';');
        SerialStringWrite(ports[0], itoa(WindowGetCursorX(term->window) + 1, "  ", 10));
        SerialWrite(ports[0], 'R');
      }
      break;

#ifdef DEBUG
    default:
      WindowPrintChar(term->window, 0x1b);
      WindowPrintChar(term->window, '[');
      WindowPrintChar(term->window, ch);
      break;
#endif
    }
    for (x = 0; x < pcnansi->curr_arg; x++)
      pcnansi->arg_vals[x] = 0;
    pcnansi->curr_arg = 0;
    term->CharHandler = PCNANSIOutChar;
  }
}

/******************************************************************************\

 Routine: PCNANSIInterpretEscape

    Pass:

  Return: Nothing

\******************************************************************************/

void PCNANSIInterpretEscape(term_obj *term, BYTE ch)
{
  term->CharHandler = PCNANSIOutChar;

  switch (ch)
  {
  case '[':
    term->CharHandler = PCNANSICollectArgs;
    break;

  case 'K':
    WindowClear(term->window);
    WindowCursorAt(term->window, 0, 0);
    break;

#ifdef DEBUG
  default:
    WindowPrintChar(term->window, ch);
    break;
#endif
  }
}

/******************************************************************************\

 Routine: PCNANSIInterpretControlCh

    Pass:

  Return: Nothing

\******************************************************************************/

void PCNANSIInterpretControlCh(term_obj *term, BYTE ch)
{
  switch (ch)
  {
  case 0x1b:
    term->CharHandler = PCNANSIInterpretEscape;
    break;

  case 0x0c:
    WindowClear(term->window);
    WindowCursorAt(term->window, 0, 0);
    break;

  default:
    WindowPrintChar(term->window, ch);
  }
}

/******************************************************************************\

 Routine: PCNANSIOutChar

    Pass:

  Return: Nothing

\******************************************************************************/

void PCNANSIOutChar(term_obj *term, BYTE ch)
{
  PCNANSI_obj *pcnansi = term->emulator;

  if (ch < 32)
    PCNANSIInterpretControlCh(term, ch);
  else
    WindowPrintChar(term->window, ch | pcnansi->high_bit);
}
