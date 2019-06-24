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

term_obj *Initvt100(void);
void vt100AddConfigMenuItems(term_obj *, s_menu *, int);
void vt100CollectArgs(term_obj *, BYTE);
void vt100CopyScreenToSlave(term_obj *);
void vt100InterpretControlCh(term_obj *, BYTE);
void vt100OutChar(term_obj *, BYTE);
void vt100SaveSetup(term_obj *);
void vt100ShowChar(term_obj *, BYTE);

extern char colors_str[];
extern s_choice_items colors[];
extern BOOLN slave_port_online;
extern s_com_port *ports[2];

#define vt100_obj struct s_vt100_obj
struct s_vt100_obj
{
  char args_str[4];
  int curr_arg;
  int curr_arg_char;
  int arg_vals[10];
  int curr_attrib;
  WORD blink;
  BOOLN esc_lbrack_question;
  int scroll_row1;
  int scroll_row2;
  WORD attributes[2][16];
  WORD save_cursor_y;
  WORD save_cursor_x;
  WORD save_cursor_attrib;
};

char *vt100ClearScreenString = "\x1b"
                               "2J"
                               "\x1b"
                               "H";

/******************************************************************************\

  Routine: Initvt100

 Function: Initialize values for vt100 routines

     Pass: Nothing

   Return: Terminal routines structure

\******************************************************************************/

term_obj *Initvt100(void)
{
  vt100_obj *new_vt100;
  term_obj *new_term;
  FILE *config_file;
  int x;

  new_term = NewTerm(TRUE, "vt100", "vt100", "vt100");
  if (new_term != NULL)
  {
    if ((new_vt100 = (vt100_obj *)malloc(sizeof(vt100_obj))) != NULL)
    {
      new_vt100->attributes[0][0] = 2;
      new_vt100->attributes[1][0] = 0;
      new_vt100->attributes[0][1] = 10;
      new_vt100->attributes[1][1] = 0;
      new_vt100->attributes[0][2] = 4;
      new_vt100->attributes[1][2] = 0;
      new_vt100->attributes[0][3] = 12;
      new_vt100->attributes[1][3] = 0;
      new_vt100->attributes[0][4] = 2;
      new_vt100->attributes[1][4] = 0;
      new_vt100->attributes[0][5] = 10;
      new_vt100->attributes[1][5] = 0;
      new_vt100->attributes[0][6] = 4;
      new_vt100->attributes[1][6] = 0;
      new_vt100->attributes[0][7] = 12;
      new_vt100->attributes[1][7] = 0;
      new_vt100->attributes[0][8] = 0;
      new_vt100->attributes[1][8] = 2;
      new_vt100->attributes[0][9] = 0;
      new_vt100->attributes[1][9] = 5;
      new_vt100->attributes[0][10] = 0;
      new_vt100->attributes[1][10] = 4;
      new_vt100->attributes[0][11] = 0;
      new_vt100->attributes[1][11] = 7;
      new_vt100->attributes[0][12] = 0;
      new_vt100->attributes[1][12] = 2;
      new_vt100->attributes[0][13] = 0;
      new_vt100->attributes[1][13] = 5;
      new_vt100->attributes[0][14] = 0;
      new_vt100->attributes[1][14] = 4;
      new_vt100->attributes[0][15] = 0;
      new_vt100->attributes[1][15] = 6;
      new_vt100->curr_attrib = 0;
      new_vt100->blink = 0x00;
      new_vt100->scroll_row1 = 0;
      new_vt100->scroll_row2 = 23;
      new_vt100->save_cursor_y = 0;
      new_vt100->save_cursor_x = 0;
      new_vt100->save_cursor_attrib = new_vt100->attributes[0][0];
      new_vt100->curr_arg = 0;

      for (x = 0; x < 10; x++)
        new_vt100->arg_vals[x] = 0;

      if ((config_file = fopen("vt100.cfg", "rb")) != NULL)
      {
        int x;

        SetCurrentFile(config_file);
        for (x = 0; x < 16; x++)
        {
          new_vt100->attributes[0][x] = ReadWordFromFile();
          new_vt100->attributes[1][x] = ReadWordFromFile();
        }
        fclose(config_file);
      }

      new_term->CharHandler = vt100OutChar;
      new_term->SaveTerminalSetup = vt100SaveSetup;
      new_term->AddConfigMenuItems = vt100AddConfigMenuItems;
      new_term->CopyScreenToSlave = vt100CopyScreenToSlave;
      new_term->emulator = new_vt100;
      new_term->num_menu_items = 1;
      new_term->ClearScreenString = vt100ClearScreenString;
    }
    else
      new_term->EndTerm(new_term);
  }
  return new_term;
}

/******************************************************************************\

  Routine: vt100SaveSetup

 Function:

     Pass:

   Return:

\******************************************************************************/

void vt100SaveSetup(term_obj *term)
{
  FILE *config_file;
  vt100_obj *vt100 = term->emulator;

  if ((config_file = fopen("vt100.cfg", "wb")) != NULL)
  {
    int x;

    SetCurrentFile(config_file);

    for (x = 0; x < 16; x++)
    {
      WriteWordToFile(vt100->attributes[0][x]);
      WriteWordToFile(vt100->attributes[1][x]);
    }
    fclose(config_file);
    MessageBoxPause("vt100 Configuration Saved");
  }
  else
    MessageBoxPause("Error trying to save vt100 config file.");
}

/******************************************************************************\

  Routine: vt100SetColors

 Function:

     Pass:

   Return:

\******************************************************************************/

void vt100SetColors(term_obj *term)
{
  vt100_obj *vt100 = term->emulator;
  s_dialog_box *db = CreateDialogBox("vt100 Colors", 2, 4, 20, 70);

  AddDBItem(db, CHOICE_GADGET, 'A', 2, 2, "A) Normal - FG", 2, 44, colors_str, 16, 2, 33, 20, 14, colors, &vt100->attributes[0][0]);
  AddDBItem(db, CHOICE_GADGET, 'B', 2, 55, "B) BG", 2, 61, colors_str, 8, 6, 33, 12, 14, colors, &vt100->attributes[1][0]);
  AddDBItem(db, CHOICE_GADGET, 'C', 3, 2, "C) Bold - FG", 3, 44, colors_str, 16, 2, 33, 20, 14, colors, &vt100->attributes[0][1]);
  AddDBItem(db, CHOICE_GADGET, 'D', 3, 55, "D) BG", 3, 61, colors_str, 8, 6, 33, 12, 14, colors, &vt100->attributes[1][1]);
  AddDBItem(db, CHOICE_GADGET, 'E', 4, 2, "E) Underscore - FG", 4, 44, colors_str, 16, 2, 33, 20, 14, colors, &vt100->attributes[0][2]);
  AddDBItem(db, CHOICE_GADGET, 'F', 4, 55, "F) BG", 4, 61, colors_str, 8, 6, 33, 12, 14, colors, &vt100->attributes[1][2]);
  AddDBItem(db, CHOICE_GADGET, 'G', 5, 2, "G) Bold & Underscore - FG", 5, 44, colors_str, 16, 2, 33, 20, 14, colors, &vt100->attributes[0][3]);
  AddDBItem(db, CHOICE_GADGET, 'H', 5, 55, "H) BG", 5, 61, colors_str, 8, 6, 33, 12, 14, colors, &vt100->attributes[1][3]);
  AddDBItem(db, CHOICE_GADGET, 'I', 6, 2, "I) Blank - FG", 6, 44, colors_str, 16, 2, 33, 20, 14, colors, &vt100->attributes[0][4]);
  AddDBItem(db, CHOICE_GADGET, 'J', 6, 55, "J) BG", 6, 61, colors_str, 8, 6, 33, 12, 14, colors, &vt100->attributes[1][4]);
  AddDBItem(db, CHOICE_GADGET, 'K', 7, 2, "K) Bold & Blank - FG", 7, 44, colors_str, 16, 2, 33, 20, 14, colors, &vt100->attributes[0][5]);
  AddDBItem(db, CHOICE_GADGET, 'L', 7, 55, "L) BG", 7, 61, colors_str, 8, 6, 33, 12, 14, colors, &vt100->attributes[1][5]);
  AddDBItem(db, CHOICE_GADGET, 'M', 8, 2, "M) Underscore & Blank - FG", 8, 44, colors_str, 16, 2, 33, 20, 14, colors, &vt100->attributes[0][6]);
  AddDBItem(db, CHOICE_GADGET, 'N', 8, 55, "N) BG", 8, 61, colors_str, 8, 6, 33, 12, 14, colors, &vt100->attributes[1][6]);
  AddDBItem(db, CHOICE_GADGET, 'O', 9, 2, "O) Bold, Underscore & Blank - FG", 9, 44, colors_str, 16, 2, 33, 20, 14, colors, &vt100->attributes[0][7]);
  AddDBItem(db, CHOICE_GADGET, 'P', 9, 55, "P) BG", 9, 61, colors_str, 8, 6, 33, 12, 14, colors, &vt100->attributes[1][7]);
  AddDBItem(db, CHOICE_GADGET, 'Q', 10, 2, "Q) Reverse - FG", 10, 44, colors_str, 16, 2, 33, 20, 14, colors, &vt100->attributes[0][8]);
  AddDBItem(db, CHOICE_GADGET, 'R', 10, 55, "R) BG", 10, 61, colors_str, 8, 6, 33, 12, 14, colors, &vt100->attributes[1][8]);
  AddDBItem(db, CHOICE_GADGET, 'S', 11, 2, "S) Bold & Reverse - FG", 11, 44, colors_str, 16, 2, 33, 20, 14, colors, &vt100->attributes[0][9]);
  AddDBItem(db, CHOICE_GADGET, 'T', 11, 55, "T) BG", 11, 61, colors_str, 8, 6, 33, 12, 14, colors, &vt100->attributes[1][9]);
  AddDBItem(db, CHOICE_GADGET, 'U', 12, 2, "U) Underscore & Reverse - FG", 12, 44, colors_str, 16, 2, 33, 20, 14, colors, &vt100->attributes[0][10]);
  AddDBItem(db, CHOICE_GADGET, 'V', 12, 55, "V) BG", 12, 61, colors_str, 8, 6, 33, 12, 14, colors, &vt100->attributes[1][10]);
  AddDBItem(db, CHOICE_GADGET, 'W', 13, 2, "W) Bold, Underscore & Reverse - FG", 13, 44, colors_str, 16, 2, 33, 20, 14, colors, &vt100->attributes[0][11]);
  AddDBItem(db, CHOICE_GADGET, 'X', 13, 55, "X) BG", 13, 61, colors_str, 8, 6, 33, 12, 14, colors, &vt100->attributes[1][11]);
  AddDBItem(db, CHOICE_GADGET, 'Y', 14, 2, "Y) Blank & Reverse - FG", 14, 44, colors_str, 16, 2, 33, 20, 14, colors, &vt100->attributes[0][12]);
  AddDBItem(db, CHOICE_GADGET, 'Z', 14, 55, "Z) BG", 14, 61, colors_str, 8, 6, 33, 12, 14, colors, &vt100->attributes[1][12]);
  AddDBItem(db, CHOICE_GADGET, '0', 15, 2, "0) Bold, Blank & Reverse - FG", 15, 44, colors_str, 16, 2, 33, 20, 14, colors, &vt100->attributes[0][13]);
  AddDBItem(db, CHOICE_GADGET, '1', 15, 55, "1) BG", 15, 61, colors_str, 8, 6, 33, 12, 14, colors, &vt100->attributes[1][13]);
  AddDBItem(db, CHOICE_GADGET, '2', 16, 2, "2) Underscore, Blank & Reverse - FG", 16, 44, colors_str, 16, 2, 33, 20, 14, colors, &vt100->attributes[0][14]);
  AddDBItem(db, CHOICE_GADGET, '3', 16, 55, "3) BG", 16, 61, colors_str, 8, 6, 33, 12, 14, colors, &vt100->attributes[1][14]);
  AddDBItem(db, CHOICE_GADGET, '4', 17, 2, "4) Bold, Underscore, Blank & Reverse - FG", 17, 44, colors_str, 16, 2, 33, 20, 14, colors, &vt100->attributes[0][15]);
  AddDBItem(db, CHOICE_GADGET, '5', 17, 55, "5) BG", 17, 61, colors_str, 8, 6, 33, 12, 14, colors, &vt100->attributes[1][15]);

  DoDialogBox(db);
  DestroyDialogBox(db);
  WindowSetAttribute(term->window, vt100->attributes[1][vt100->curr_attrib] << 4 | vt100->attributes[0][vt100->curr_attrib] | vt100->blink);
}

/******************************************************************************\

  Routine: vt100AddConfigMenuItems

 Function:

     Pass:

   Return:

\******************************************************************************/

void vt100AddConfigMenuItems(term_obj *term, s_menu *configmenu, int row)
{
  AddMenuItem(configmenu, row++, 2, FUNCTION, "vt100 Colors", 'v', "", NULL,
              (void *)vt100SetColors, term);
}

/******************************************************************************\

  Routine: vt100CopyScreenToSlave

 Function:

     Pass:

   Return:

\******************************************************************************/

void vt100CopyScreenToSlave(term_obj *term)
{
}

/******************************************************************************\

 Routine: vt100CollectArgs

    Pass:

  Return: Nothing

\******************************************************************************/

void vt100CollectArgs(term_obj *term, BYTE ch)
{
  int x;
  vt100_obj *vt100 = term->emulator;

  if (isdigit(ch))
    vt100->args_str[vt100->curr_arg_char++] = ch;
  else if (ch == ';')
  {
    vt100->args_str[vt100->curr_arg_char] = 0;
    vt100->curr_arg_char = 0;
    vt100->arg_vals[vt100->curr_arg++] = atoi(vt100->args_str);
  }
  else if (ch == '?')
    vt100->esc_lbrack_question = TRUE;
  else
  {
    vt100->args_str[vt100->curr_arg_char] = 0;
    vt100->curr_arg_char = 0;
    vt100->arg_vals[vt100->curr_arg++] = atoi(vt100->args_str);

    if (vt100->esc_lbrack_question == FALSE)
    {
      switch (ch)
      {
      case 'A': /*Cursor Up*/
        if (vt100->arg_vals[0] == 0)
          vt100->arg_vals[0]++;
        for (x = 0; x < vt100->arg_vals[0]; x++)
          WindowCursorUp(term->window);
        break;

      case 'B': /*Cursor Down*/
        if (vt100->arg_vals[0] == 0)
          vt100->arg_vals[0]++;
        for (x = 0; x < vt100->arg_vals[0]; x++)
          WindowCursorDown(term->window);
        break;

      case 'C': /*Cursor Right*/
        if (vt100->arg_vals[0] == 0)
          vt100->arg_vals[0]++;
        for (x = 0; x < vt100->arg_vals[0]; x++)
          WindowCursorRight(term->window);
        break;

      case 'D': /*Cursor Left*/
        if (vt100->arg_vals[0] == 0)
          vt100->arg_vals[0]++;
        for (x = 0; x < vt100->arg_vals[0]; x++)
          WindowCursorLeft(term->window);
        break;

      case 'H': /*Direct Cursor Addressing (Line, then column)*/
      case 'I':
        if (vt100->arg_vals[0] > 0)
          vt100->arg_vals[0]--;
        if (vt100->arg_vals[1] > 0)
          vt100->arg_vals[1]--;
        WindowCursorAt(term->window, vt100->arg_vals[1], vt100->arg_vals[0]);
        break;

      case 'J': /* Clear screen */
        if (vt100->arg_vals[0] == 1 || vt100->arg_vals[0] == 2)
          WindowClear(term->window);
        break;

      case 'K':
        switch (vt100->arg_vals[0])
        {
        case 0: /*Erase from cursor to EOL */
          WindowEraseToEOL(term->window);
          break;

        case 1: /*Erase from beginning of line to cursor*/
          WindowEraseToBOL(term->window);
          break;

        case 2: /*Erase entire line*/
          WindowEraseLine(term->window);
          break;
        }
        break;

      case 'L': /*Insert line at cursor*/
        WindowScrollDown(term->window, WindowGetCursorY(term->window), 0, 23, 79);
        WindowPrintChar(term->window, 0x0d);
        break;

      case 'M': /*Remove line the cursor is on*/
        WindowScrollUp(term->window, WindowGetCursorY(term->window), 0, 23, 79);
        WindowPrintChar(term->window, 0x0d);
        break;

      case 'm': /* Set attribute */
        for (x = 0; x < vt100->curr_arg; x++)
          switch (vt100->arg_vals[x])
          {
          case 0:
            vt100->curr_attrib = 0;
            vt100->blink = 0x00;
            break;

          case 1:
            vt100->curr_attrib |= 0x01;
            break;

          case 4:
            vt100->curr_attrib |= 0x02;
            break;

          case 5:
            vt100->blink = 0x80;
            break;

          case 6:
            vt100->curr_attrib |= 0x04;
            break;

          case 7:
            vt100->curr_attrib |= 0x08;
            break;

          case 22:
            vt100->curr_attrib &= 0xfe;
            break;

          case 24:
            vt100->curr_attrib &= 0xfd;
            break;

          case 25:
            vt100->blink = 0x00;
            break;

          case 26:
            vt100->curr_attrib &= 0xfb;
            break;

          case 27:
            vt100->curr_attrib &= 0xf7;
            break;
          }
        WindowSetAttribute(term->window, vt100->attributes[1][vt100->curr_attrib] << 4 | vt100->attributes[0][vt100->curr_attrib] | vt100->blink);
        break;

      case 'r':
        if (vt100->arg_vals[0] > 0)
          vt100->arg_vals[0]--;
        if (vt100->arg_vals[1] > 0)
          vt100->arg_vals[1]--;
        vt100->scroll_row1 = vt100->arg_vals[0];
        vt100->scroll_row2 = vt100->arg_vals[1];
        break;

#ifdef DEBUG
      default:
        WindowPrintChar(term->window, 0x1b);
        WindowPrintChar(term->window, '[');
        WindowPrintChar(term->window, ch);
        break;
#endif
      }
    }
    else
    {
      switch (ch)
      {
      case 'h':
        switch (vt100->arg_vals[0])
        {
        case 7:
          WindowSetLineWrap(term->window, TRUE);
          break;

        case 25:
          WindowSetCursorShape(term->window, BLOCKCURSOR);
          break;
        }
        break;

      case 'l':
        switch (vt100->arg_vals[0])
        {
        case 7:
          WindowSetLineWrap(term->window, FALSE);
          break;

        case 25:
          WindowSetCursorShape(term->window, HIDECURSOR);
          break;
        }
        break;
      }
    }
    for (x = 0; x < vt100->curr_arg; x++)
      vt100->arg_vals[x] = 0;
    vt100->curr_arg = 0;
    vt100->esc_lbrack_question = FALSE;
    term->CharHandler = vt100OutChar;
  }
}

/******************************************************************************\

 Routine: vt100LParenthesis

    Pass:

  Return: Nothing

\******************************************************************************/

void vt100LParenthesis(term_obj *term, BYTE ch)
{
#ifdef DEBUG
  if (ch != 'B')
  {
    WindowPrintChar(term->window, '(');
    WindowPrintChar(term->window, ch);
  }
#endif
  term->CharHandler = vt100OutChar;
}

/******************************************************************************\

 Routine: vt100InterpretEscape

    Pass:

  Return: Nothing

\******************************************************************************/

void vt100InterpretEscape(term_obj *term, BYTE ch)
{
  vt100_obj *vt100 = term->emulator;
  term->CharHandler = vt100OutChar;

  switch (ch)
  {
  case '[':
    term->CharHandler = vt100CollectArgs;
    break;

  case '(':
    term->CharHandler = vt100LParenthesis;
    break;

  case '=': /* TODO: What does this do? */
    break;

  case '>': /* TODO: What does this do? */
    break;

  case '7':
    vt100->save_cursor_y = WindowGetCursorY(term->window);
    vt100->save_cursor_x = WindowGetCursorX(term->window);
    vt100->save_cursor_attrib = WindowGetAttribute(term->window);
    break;

  case '8':
    WindowCursorAt(term->window, vt100->save_cursor_x, vt100->save_cursor_y);
    WindowSetAttribute(term->window, vt100->save_cursor_attrib);
    break;

  case 'M':
    WindowScrollDown(term->window, vt100->scroll_row1, 0, vt100->scroll_row2, 79);
    break;

  case 'D':
    WindowScrollUp(term->window, vt100->scroll_row1, 0, vt100->scroll_row2, 79);
    break;

  case 'y': /* Enable paging */
    if (!slave_port_online)
      SerialWrite(ports[0], 27);
    break;

#ifdef DEBUG
  default:
    WindowPrintChar(term->window, ch);
    break;
#endif
  }
}

/******************************************************************************\

 Routine: vt100InterpretControlCh

    Pass:

  Return: Nothing

\******************************************************************************/

void vt100InterpretControlCh(term_obj *term, BYTE ch)
{
  switch (ch)
  {
  case 0x1b:
    term->CharHandler = vt100InterpretEscape;
    break;

  default:
    WindowPrintChar(term->window, ch);
  }
}

/******************************************************************************\

 Routine: vt100OutChar

    Pass:

  Return: Nothing

\******************************************************************************/

void vt100OutChar(term_obj *term, BYTE ch)
{
  if (ch < 32)
    vt100InterpretControlCh(term, ch);
  else
    WindowPrintChar(term->window, ch);
}
