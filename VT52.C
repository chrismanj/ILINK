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

term_obj *Initvt52(void);
void vt52AddConfigMenuItems(term_obj *, s_menu *, int);
void vt52CollectArg(term_obj *, BYTE);
void vt52CopyScreenToSlave(term_obj *);
void vt52EndTerm(term_obj *);
void vt52InterpretControlCh(term_obj *, int);
void vt52InterpretEscape(term_obj *, BYTE);
void vt52OutChar(term_obj *, BYTE);
void vt52SaveSetup(term_obj *);
void vt52ShowChar(term_obj *, int);
void vt52SetCursorY(term_obj *, BYTE);
void vt52SetCursorX(term_obj *, BYTE);

extern char colors_str[];
extern s_choice_items colors[];
extern BOOLN slave_port_online;
extern s_com_port *ports[2];

#define vt52_obj struct s_vt52
struct s_vt52
{
  WORD fore_color; /* Value of foreground color */
  WORD back_color; /* Value of background color */
  BOOLN line_wrap;
  char arg_str[2];
  s_xlate_table *default_xlate_table;
  s_xlate_table *graphics_xlate_table;
};

char *vt52ClearScreenString = "\x1bH\x1bJ";
char vt52graphchars[] = "±    øñ  Ù¿ÚÀÅ  Ä  Ã´ÁÂ³óò";
char vt52revgraphchars[] = "abcdefghijklmnopqrstuvwxyz";

/******************************************************************************\

  Routine: Initvt52

 Function: Initialize values for vt52 routines

     Pass: Nothing

   Return: Terminal routines structure

\******************************************************************************/

term_obj *Initvt52(void)
{
  vt52_obj *new_vt52;
  term_obj *new_term;
  FILE *config_file;

  new_term = NewTerm(TRUE, "vt52", "vt52", "vt52");
  if (new_term != NULL)
  {
    if ((new_vt52 = (vt52_obj *)malloc(sizeof(vt52_obj))) != NULL)
    {
      new_vt52->fore_color = 2;
      new_vt52->back_color = 0;
      new_vt52->line_wrap = FALSE;
      new_vt52->arg_str[1] = 0;
      new_vt52->default_xlate_table = new_term->translation_table_p;
      new_vt52->graphics_xlate_table = LoadTranslationTable("vt52g");

      if ((config_file = fopen("vt52.cfg", "rb")) != NULL)
      {
        SetCurrentFile(config_file);
        new_vt52->fore_color = ReadWordFromFile();
        new_vt52->back_color = ReadWordFromFile();
        new_vt52->line_wrap = ReadWordFromFile();
        fclose(config_file);
      }

      WindowSetLineWrap(new_term->window, new_vt52->line_wrap);
      WindowResetAttribute(new_term->window, new_vt52->back_color << 4 | new_vt52->fore_color);

      new_term->CharHandler = vt52OutChar;
      new_term->SaveTerminalSetup = vt52SaveSetup;
      new_term->AddConfigMenuItems = vt52AddConfigMenuItems;
      new_term->CopyScreenToSlave = vt52CopyScreenToSlave;
      new_term->EndTerm = vt52EndTerm;
      new_term->emulator = new_vt52;
      new_term->num_menu_items = 2;
      new_term->ClearScreenString = vt52ClearScreenString;
    }
    else
      new_term->EndTerm(new_term);
  }
  return new_term;
}

/******************************************************************************\

  Routine: vt52SaveSetup

 Function:

     Pass:

   Return:

\******************************************************************************/

void vt52SaveSetup(term_obj *term)
{
  FILE *config_file;
  vt52_obj *vt52 = term->emulator;

  if ((config_file = fopen("vt52.cfg", "wb")) != NULL)
  {
    SetCurrentFile(config_file);
    WriteWordToFile(vt52->fore_color);
    WriteWordToFile(vt52->back_color);
    WriteWordToFile(vt52->line_wrap);
    fclose(config_file);
    MessageBoxPause("VT52 Configuration Saved");
  }
  else
    MessageBoxPause("Error trying to save VT52 config file.");
}

/******************************************************************************\

  Routine: vt52SetColors

 Function:

     Pass:

   Return:

\******************************************************************************/

void vt52SetColors(term_obj *term)
{
  int attribute;
  s_dialog_box *db;
  vt52_obj *vt52 = term->emulator;

  db = CreateDialogBox("vt52 Colors", 6, 28, 6, 25);
  AddDBItem(db, CHOICE_GADGET, 'F', 2, 2, "Foreground",
            2, 13, &colors_str[0], 16, 2, 33, 20, 14, &colors[0], &vt52->fore_color);
  AddDBItem(db, CHOICE_GADGET, 'B', 3, 2, "Background",
            3, 13, &colors_str[0], 8, 6, 33, 12, 14, &colors[0], &vt52->back_color);
  DoDialogBox(db);
  DestroyDialogBox(db);
  attribute = vt52->back_color << 4 | vt52->fore_color;
  WindowResetAttribute(term->window, attribute);
}

/******************************************************************************\

  Routine: vt52SetLineWrap

 Function:

     Pass:

   Return:

\******************************************************************************/

void vt52SetLineWrap(term_obj *term)
{
  vt52_obj *vt52;

  vt52 = term->emulator;
  WindowSetLineWrap(term->window, vt52->line_wrap);
}

/******************************************************************************\

  Routine: vt52AddConfigMenuItems

 Function:

     Pass:

   Return:

\******************************************************************************/

void vt52AddConfigMenuItems(term_obj *term, s_menu *configmenu, int row)
{
  vt52_obj *vt52;

  vt52 = term->emulator;
  AddMenuItem(configmenu, row++, 2, FUNCTION, "vt52 Colors", 'v', "", NULL,
              (void *)vt52SetColors, (void *)term);
  AddMenuItem(configmenu, row, 2, CHECKED, "vt52 Line Wrap", 'L', "", &vt52->line_wrap, vt52SetLineWrap, (void *)term);
}

/******************************************************************************\

  Routine: vt52CopyScreenToSlave

 Function:

     Pass:

   Return:

\******************************************************************************/

void vt52CopyScreenToSlave(term_obj *term)
{
  int x, y, ch, revchar;
  winhdl *window = term->window;
  vt52_obj *vt52 = term->emulator;
  BOOLN graphics_mode = FALSE;
  BOOLN line_wrap = vt52->line_wrap;

  /* Reset remote screen */

  SerialStringWrite(ports[1], vt52ClearScreenString);

  /* Redraw remote screen */

  for (y = 0; y < 24; y++)
  {
    for (x = 0; x < 80; x++)
    {
      if ((line_wrap == TRUE && y == 23 && x == 79) == FALSE)
      {
        ch = WindowGetCharAt(window, x, y);
        if (ch < 128)
        {
          if (graphics_mode == TRUE)
          {
            SerialStringWrite(ports[1], "\0x1bG");
            graphics_mode = FALSE;
          }
          SerialWrite(ports[1], ch);
        }
        else
        {
          revchar = 0;

          if (graphics_mode == FALSE)
          {
            SerialStringWrite(ports[1], "\0x1bF");
            graphics_mode = TRUE;
          }
          while (ch != vt52graphchars[revchar] && revchar < 26)
            revchar++;
          SerialWrite(ports[1], vt52revgraphchars[revchar]);
        }
      }
    }
    if (line_wrap == FALSE && y < 23)
      SerialStringWrite(ports[1], "\n\r");
  }

  /* Reposition cursor to correct location on remote */

  SerialStringWrite(ports[1], "\x1bY");
  SerialWrite(ports[1], WindowGetCursorY(window) + 32);
  SerialWrite(ports[1], WindowGetCursorX(window) + 32);
}

/******************************************************************************\

  Routine: DeInitvt52

 Function: Initialize values for vt52 routines

     Pass: Nothing

   Return: Terminal routines structure

\******************************************************************************/

void vt52EndTerm(term_obj *term)
{
  vt52_obj *vt52 = term->emulator;

  free(vt52->graphics_xlate_table);
  DefDestroyTerm(term);
}

/******************************************************************************\

 Routine: vt52SetCursorY

    Pass:

  Return: Nothing

\******************************************************************************/

void vt52SetCursorY(term_obj *term, BYTE ch)
{
  WindowSetCursorRow(term->window, ch - 32);
  term->CharHandler = vt52SetCursorX;
}

/******************************************************************************\

 Routine: vt52SetCursorX

    Pass:

  Return: Nothing

\******************************************************************************/

void vt52SetCursorX(term_obj *term, BYTE ch)
{
  WindowSetCursorCol(term->window, ch - 32);
  term->CharHandler = vt52OutChar;
}

/******************************************************************************\

 Routine: vt52InterpretEscape

    Pass:

  Return: Nothing

\******************************************************************************/

void vt52CollectArg(term_obj *term, BYTE ch)
{
  vt52_obj *vt52 = term->emulator;

  if (isdigit(ch))
    vt52->arg_str[0] = ch;
  else
  {
    switch (ch)
    {
    case 'S':
      SwitchPage(atoi(vt52->arg_str));
      term->CharHandler = vt52OutChar;
      break;
    }
  }
}

/******************************************************************************\

 Routine: vt52InterpretEscape

    Pass:

  Return: Nothing

\******************************************************************************/

void vt52InterpretEscape(term_obj *term, BYTE ch)
{
  winhdl *window = term->window;
  vt52_obj *vt52 = term->emulator;

  term->CharHandler = vt52OutChar;
  switch (ch)
  {
  case 'A': /* Cursor up */
    WindowCursorUp(window);
    break;

  case 'B': /* Cursor down */
    WindowCursorDown(window);
    break;

  case 'C': /* Cursor right */
    WindowCursorRight(window);
    break;

  case 'D': /* Cursor left */
    WindowCursorLeft(window);
    break;

  case 'H': /* Cursor home */
    WindowCursorAt(window, 0, 0);
    break;

  case 'I': /* Reverse line feed */
    if (WindowCursorUp(window) == FALSE)
      WindowScrollDown(window, 0, 0, 23, 79);
    break;

  case 'Y': /* Set cursor row & column */
    term->CharHandler = vt52SetCursorY;
    break;

  case 'F': /* Enter graphics mode */
    term->translation_table_p = vt52->graphics_xlate_table;
    break;

  case 'G': /* Exit graphics mode */
    term->translation_table_p = vt52->default_xlate_table;
    break;

  case 'J': /* Erase to end of screen */
    WindowEraseToEnd(window);
    break;

  case 'K': /* Erase to end of line */
    WindowEraseToEOL(window);
    break;

  case 'Z': /* TODO: Identify Terminal */
    SerialWrite(ports[0], 'Z');
    break;

  case 'y':                 /* Unknown: VLINK sends ESC back when it */
    if (!slave_port_online) /* receives this escape sequence */
      SerialWrite(ports[0], 27);
    break;

  case '=': /* TODO: Enter alternate keypad mode (Exited by a single escape ?)*/
            /* Procomm seems to do nothing */
    break;

  case '^': /* TODO: Enter auto print mode */
            /* Procomm seems to do nothing */
    break;

  case '_': /* TODO: Exit auto print mode */
    break;

  case 'W': /* TODO: Enter printer controller mode (slave print)*/
    break;

  case 'X': /* TODO: Exit printer controller mode */
    break;

  case ']': /* TODO: Print screen */
    break;

  case 'V': /* TODO: Print cursor line */
    break;

  case '[':
    term->CharHandler = vt52CollectArg;
    /* TODO: 9S - switches main port to auxillary port
					 0S - switched auxillary port to last main port screen */
    /* Procomm seems to do nothing */
    break;

  default:
    WindowPrintChar(window, ch);
    break;
  }
}

/******************************************************************************\

 Routine: vt52InterpretControlCh

    Pass:

  Return: Nothing

\******************************************************************************/

void vt52InterpretControlCh(term_obj *term, int ch)
{
  switch (ch)
  {
  case 0x1b:
    term->CharHandler = vt52InterpretEscape;
    break;

  default:
    WindowPrintChar(term->window, ch);
  }
}

/******************************************************************************\

 Routine: vt52OutChar

    Pass:

  Return: Nothing

\******************************************************************************/

void vt52OutChar(term_obj *term, BYTE ch)
{
  if (ch < 32)
  {
    vt52InterpretControlCh(term, ch);
    return;
  }
  else
    WindowPrintChar(term->window, ch);
}
