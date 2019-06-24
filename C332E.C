#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "..\common\include\types.h"
#include "..\common\include\debug.h"
#include "..\common\include\fileio.h"
#include "..\common\include\doublell.h"
#include "..\common\include\bqueue.h"
#include "..\common\include\jscser.h"
#include "..\common\include\modem.h"
#include "..\common\include\keybrd.h"
#include "..\common\include\chrgraph.h"
#include "..\common\include\window.h"
#include "..\common\include\intrface.h"
#include "..\common\include\mem.h"
#include "include\termobj.h"
#include "include\ilink.h"

term_obj *Initc332e(void);
void c332eAddConfigMenuItems(term_obj *, s_menu *, int);
void c332eCollectArgs(term_obj *, BYTE);
void c332eCopyScreenToSlave(term_obj *);
void c332eEndTerm(term_obj *);
BOOLN c332eEvaluateKey(term_obj *, WORD);
void c332eInterpretControlCh(term_obj *, BYTE);
void c332eOutChar(term_obj *, BYTE);
void c332eSaveSetup(term_obj *);
void c332eShowChar(term_obj *, BYTE);

extern char colors_str[];
extern s_choice_items colors[];
extern BOOLN slave_port_online;
extern s_com_port *ports[2];

#define c332e_obj struct s_c332e_obj
struct s_c332e_obj
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
  s_xlate_table *default_xlate_table;
  s_xlate_table *graphics_xlate_table;
};

char *c332eClearScreenString = "\x1b"
                               "2J"
                               "\x1b"
                               "H";

/******************************************************************************\

  Routine: Initc332e

 Function: Initialize values for c332e routines

     Pass: Nothing

   Return: Terminal routines structure

\******************************************************************************/

term_obj *Initc332e(void)
{
  c332e_obj *new_c332e;
  term_obj *new_term;
  FILE *config_file;
  int x;

#ifdef DEBUG
  OutDebugText("JA");
#endif

  new_term = NewTerm(TRUE, "c332e", "c332e", "c332e");
  if (new_term != NULL)
  {
    if ((new_c332e = (c332e_obj *)malloc(sizeof(c332e_obj))) != NULL)
    {
      new_c332e->attributes[0][0] = 2;
      new_c332e->attributes[1][0] = 0;
      new_c332e->attributes[0][1] = 10;
      new_c332e->attributes[1][1] = 0;
      new_c332e->attributes[0][2] = 4;
      new_c332e->attributes[1][2] = 0;
      new_c332e->attributes[0][3] = 12;
      new_c332e->attributes[1][3] = 0;
      new_c332e->attributes[0][4] = 2;
      new_c332e->attributes[1][4] = 0;
      new_c332e->attributes[0][5] = 10;
      new_c332e->attributes[1][5] = 0;
      new_c332e->attributes[0][6] = 4;
      new_c332e->attributes[1][6] = 0;
      new_c332e->attributes[0][7] = 12;
      new_c332e->attributes[1][7] = 0;
      new_c332e->attributes[0][8] = 0;
      new_c332e->attributes[1][8] = 2;
      new_c332e->attributes[0][9] = 0;
      new_c332e->attributes[1][9] = 5;
      new_c332e->attributes[0][10] = 0;
      new_c332e->attributes[1][10] = 4;
      new_c332e->attributes[0][11] = 0;
      new_c332e->attributes[1][11] = 7;
      new_c332e->attributes[0][12] = 0;
      new_c332e->attributes[1][12] = 2;
      new_c332e->attributes[0][13] = 0;
      new_c332e->attributes[1][13] = 5;
      new_c332e->attributes[0][14] = 0;
      new_c332e->attributes[1][14] = 4;
      new_c332e->attributes[0][15] = 0;
      new_c332e->attributes[1][15] = 6;
      new_c332e->curr_attrib = 0;
      new_c332e->blink = 0x00;
      new_c332e->scroll_row1 = 0;
      new_c332e->scroll_row2 = 23;
      new_c332e->save_cursor_y = 0;
      new_c332e->save_cursor_x = 0;
      new_c332e->save_cursor_attrib = new_c332e->attributes[0][0];
      new_c332e->curr_arg = 0;
      new_c332e->default_xlate_table = new_term->translation_table_p;
      new_c332e->graphics_xlate_table = LoadTranslationTable("c332eg");

      for (x = 0; x < 10; x++)
        new_c332e->arg_vals[x] = 0;

      if ((config_file = fopen("c332e.cfg", "rb")) != NULL)
      {
        int x;

        SetCurrentFile(config_file);
        for (x = 0; x < 16; x++)
        {
          new_c332e->attributes[0][x] = ReadWordFromFile();
          new_c332e->attributes[1][x] = ReadWordFromFile();
        }
        fclose(config_file);
      }

      new_term->CharHandler = c332eOutChar;
      new_term->SaveTerminalSetup = c332eSaveSetup;
      new_term->AddConfigMenuItems = c332eAddConfigMenuItems;
      new_term->CopyScreenToSlave = c332eCopyScreenToSlave;
      new_term->EndTerm = c332eEndTerm;
      new_term->EvaluateKey = c332eEvaluateKey;
      new_term->emulator = new_c332e;
      new_term->num_menu_items = 1;
      new_term->ClearScreenString = c332eClearScreenString;
    }
    else
      new_term->EndTerm(new_term);
  }
  return new_term;
}

/******************************************************************************\

  Routine: c332eEndTerm

 Function:

     Pass:

   Return:

\******************************************************************************/

void c332eEndTerm(term_obj *term)
{
  c332e_obj *c332e = term->emulator;

#ifdef DEBUG
  OutDebugText("JB");
#endif

  free(c332e->graphics_xlate_table);
  DefDestroyTerm(term);
}

/******************************************************************************\

  Routine: c332eEvaluateKey

 Function: Evaluate extended keypresses which the main program could not identify

     Pass: The extended portion of the keycode

   Return: Nothing

\******************************************************************************/

BOOLN c332eEvaluateKey(term_obj *term, WORD key)
{
  BOOLN evaluated = FALSE;
  s_fkey_def *fkeydef;

#ifdef DEBUG
  OutDebugText("JC");
#endif

  if ((fkeydef = DoublyLinkedListGetItemWKeyOf(term->fn_keys->fn_key_defs, key)) != NULL)
  {
    SendStringToModem(ports[0], fkeydef->definition, FALSE);
    evaluated = TRUE;
  }
  return evaluated;
}

/******************************************************************************\

  Routine: c332eSaveSetup

 Function:

     Pass:

   Return:

\******************************************************************************/

void c332eSaveSetup(term_obj *term)
{
  FILE *config_file;
  c332e_obj *c332e = term->emulator;

#ifdef DEBUG
  OutDebugText("JD");
#endif

  if ((config_file = fopen("c332e.cfg", "wb")) != NULL)
  {
    int x;

    SetCurrentFile(config_file);

    for (x = 0; x < 16; x++)
    {
      WriteWordToFile(c332e->attributes[0][x]);
      WriteWordToFile(c332e->attributes[1][x]);
    }
    fclose(config_file);
    MessageBoxPause("c332e Configuration Saved");
  }
  else
    MessageBoxPause("Error trying to save c332e config file.");
}

/******************************************************************************\

  Routine: c332eSetColors

 Function:

     Pass:

   Return:

\******************************************************************************/

void c332eSetColors(term_obj *term)
{
  c332e_obj *c332e = term->emulator;
  s_dialog_box *db;

#ifdef DEBUG
  OutDebugText("JE");
#endif

  db = CreateDialogBox("c332e Colors", 2, 4, 20, 70);
  AddDBItem(db, CHOICE_GADGET, 'A', 2, 2, "A) Normal - FG", 2, 44, colors_str, 16, 2, 33, 20, 14, colors, &c332e->attributes[0][0]);
  AddDBItem(db, CHOICE_GADGET, 'B', 2, 55, "B) BG", 2, 61, colors_str, 8, 6, 33, 12, 14, colors, &c332e->attributes[1][0]);
  AddDBItem(db, CHOICE_GADGET, 'C', 3, 2, "C) Bold - FG", 3, 44, colors_str, 16, 2, 33, 20, 14, colors, &c332e->attributes[0][1]);
  AddDBItem(db, CHOICE_GADGET, 'D', 3, 55, "D) BG", 3, 61, colors_str, 8, 6, 33, 12, 14, colors, &c332e->attributes[1][1]);
  AddDBItem(db, CHOICE_GADGET, 'E', 4, 2, "E) Underscore - FG", 4, 44, colors_str, 16, 2, 33, 20, 14, colors, &c332e->attributes[0][2]);
  AddDBItem(db, CHOICE_GADGET, 'F', 4, 55, "F) BG", 4, 61, colors_str, 8, 6, 33, 12, 14, colors, &c332e->attributes[1][2]);
  AddDBItem(db, CHOICE_GADGET, 'G', 5, 2, "G) Bold & Underscore - FG", 5, 44, colors_str, 16, 2, 33, 20, 14, colors, &c332e->attributes[0][3]);
  AddDBItem(db, CHOICE_GADGET, 'H', 5, 55, "H) BG", 5, 61, colors_str, 8, 6, 33, 12, 14, colors, &c332e->attributes[1][3]);
  AddDBItem(db, CHOICE_GADGET, 'I', 6, 2, "I) Blank - FG", 6, 44, colors_str, 16, 2, 33, 20, 14, colors, &c332e->attributes[0][4]);
  AddDBItem(db, CHOICE_GADGET, 'J', 6, 55, "J) BG", 6, 61, colors_str, 8, 6, 33, 12, 14, colors, &c332e->attributes[1][4]);
  AddDBItem(db, CHOICE_GADGET, 'K', 7, 2, "K) Bold & Blank - FG", 7, 44, colors_str, 16, 2, 33, 20, 14, colors, &c332e->attributes[0][5]);
  AddDBItem(db, CHOICE_GADGET, 'L', 7, 55, "L) BG", 7, 61, colors_str, 8, 6, 33, 12, 14, colors, &c332e->attributes[1][5]);
  AddDBItem(db, CHOICE_GADGET, 'M', 8, 2, "M) Underscore & Blank - FG", 8, 44, colors_str, 16, 2, 33, 20, 14, colors, &c332e->attributes[0][6]);
  AddDBItem(db, CHOICE_GADGET, 'N', 8, 55, "N) BG", 8, 61, colors_str, 8, 6, 33, 12, 14, colors, &c332e->attributes[1][6]);
  AddDBItem(db, CHOICE_GADGET, 'O', 9, 2, "O) Bold, Underscore & Blank - FG", 9, 44, colors_str, 16, 2, 33, 20, 14, colors, &c332e->attributes[0][7]);
  AddDBItem(db, CHOICE_GADGET, 'P', 9, 55, "P) BG", 9, 61, colors_str, 8, 6, 33, 12, 14, colors, &c332e->attributes[1][7]);
  AddDBItem(db, CHOICE_GADGET, 'Q', 10, 2, "Q) Reverse - FG", 10, 44, colors_str, 16, 2, 33, 20, 14, colors, &c332e->attributes[0][8]);
  AddDBItem(db, CHOICE_GADGET, 'R', 10, 55, "R) BG", 10, 61, colors_str, 8, 6, 33, 12, 14, colors, &c332e->attributes[1][8]);
  AddDBItem(db, CHOICE_GADGET, 'S', 11, 2, "S) Bold & Reverse - FG", 11, 44, colors_str, 16, 2, 33, 20, 14, colors, &c332e->attributes[0][9]);
  AddDBItem(db, CHOICE_GADGET, 'T', 11, 55, "T) BG", 11, 61, colors_str, 8, 6, 33, 12, 14, colors, &c332e->attributes[1][9]);
  AddDBItem(db, CHOICE_GADGET, 'U', 12, 2, "U) Underscore & Reverse - FG", 12, 44, colors_str, 16, 2, 33, 20, 14, colors, &c332e->attributes[0][10]);
  AddDBItem(db, CHOICE_GADGET, 'V', 12, 55, "V) BG", 12, 61, colors_str, 8, 6, 33, 12, 14, colors, &c332e->attributes[1][10]);
  AddDBItem(db, CHOICE_GADGET, 'W', 13, 2, "W) Bold, Underscore & Reverse - FG", 13, 44, colors_str, 16, 2, 33, 20, 14, colors, &c332e->attributes[0][11]);
  AddDBItem(db, CHOICE_GADGET, 'X', 13, 55, "X) BG", 13, 61, colors_str, 8, 6, 33, 12, 14, colors, &c332e->attributes[1][11]);
  AddDBItem(db, CHOICE_GADGET, 'Y', 14, 2, "Y) Blank & Reverse - FG", 14, 44, colors_str, 16, 2, 33, 20, 14, colors, &c332e->attributes[0][12]);
  AddDBItem(db, CHOICE_GADGET, 'Z', 14, 55, "Z) BG", 14, 61, colors_str, 8, 6, 33, 12, 14, colors, &c332e->attributes[1][12]);
  AddDBItem(db, CHOICE_GADGET, '0', 15, 2, "0) Bold, Blank & Reverse - FG", 15, 44, colors_str, 16, 2, 33, 20, 14, colors, &c332e->attributes[0][13]);
  AddDBItem(db, CHOICE_GADGET, '1', 15, 55, "1) BG", 15, 61, colors_str, 8, 6, 33, 12, 14, colors, &c332e->attributes[1][13]);
  AddDBItem(db, CHOICE_GADGET, '2', 16, 2, "2) Underscore, Blank & Reverse - FG", 16, 44, colors_str, 16, 2, 33, 20, 14, colors, &c332e->attributes[0][14]);
  AddDBItem(db, CHOICE_GADGET, '3', 16, 55, "3) BG", 16, 61, colors_str, 8, 6, 33, 12, 14, colors, &c332e->attributes[1][14]);
  AddDBItem(db, CHOICE_GADGET, '4', 17, 2, "4) Bold, Underscore, Blank & Reverse - FG", 17, 44, colors_str, 16, 2, 33, 20, 14, colors, &c332e->attributes[0][15]);
  AddDBItem(db, CHOICE_GADGET, '5', 17, 55, "5) BG", 17, 61, colors_str, 8, 6, 33, 12, 14, colors, &c332e->attributes[1][15]);

  DoDialogBox(db);
  DestroyDialogBox(db);
  WindowSetAttribute(term->window, c332e->attributes[1][c332e->curr_attrib] << 4 | c332e->attributes[0][c332e->curr_attrib] | c332e->blink);
}

/******************************************************************************\

  Routine: c332eAddConfigMenuItems

 Function:

     Pass:

   Return:

\******************************************************************************/

void c332eAddConfigMenuItems(term_obj *term, s_menu *configmenu, int row)
{
#ifdef DEBUG
  OutDebugText("JF");
#endif

  AddMenuItem(configmenu, row++, 2, FUNCTION, "c332e Colors", 'v', "", NULL,
              (void *)c332eSetColors, term);
}

/******************************************************************************\

  Routine: c332eCopyScreenToSlave

 Function:

     Pass:

   Return:

\******************************************************************************/

void c332eCopyScreenToSlave(term_obj *term)
{
#ifdef DEBUG
  OutDebugText("JG");
#endif
}

/******************************************************************************\

 Routine: c332eCollectArgs

    Pass:

  Return: Nothing

\******************************************************************************/

void c332eCollectArgs(term_obj *term, BYTE ch)
{
  int x;
  c332e_obj *c332e = term->emulator;

#ifdef DEBUG
  OutDebugText("JH");
#endif

  if (isdigit(ch))
    c332e->args_str[c332e->curr_arg_char++] = ch;
  else if (ch == ';')
  {
    c332e->args_str[c332e->curr_arg_char] = 0;
    c332e->curr_arg_char = 0;
    c332e->arg_vals[c332e->curr_arg++] = atoi(c332e->args_str);
  }
  else if (ch == '?')
    c332e->esc_lbrack_question = TRUE;
  else
  {
    c332e->args_str[c332e->curr_arg_char] = 0;
    c332e->curr_arg_char = 0;
    c332e->arg_vals[c332e->curr_arg++] = atoi(c332e->args_str);

    if (c332e->esc_lbrack_question == FALSE)
    {
      switch (ch)
      {
      case 'A': /*Cursor Up*/
        if (c332e->arg_vals[0] == 0)
          c332e->arg_vals[0]++;
        for (x = 0; x < c332e->arg_vals[0]; x++)
          WindowCursorUp(term->window);
        break;

      case 'B': /*Cursor Down*/
        if (c332e->arg_vals[0] == 0)
          c332e->arg_vals[0]++;
        for (x = 0; x < c332e->arg_vals[0]; x++)
          WindowCursorDown(term->window);
        break;

      case 'C': /*Cursor Right*/
        if (c332e->arg_vals[0] == 0)
          c332e->arg_vals[0]++;
        for (x = 0; x < c332e->arg_vals[0]; x++)
          WindowCursorRight(term->window);
        break;

      case 'D': /*Cursor Left*/
        if (c332e->arg_vals[0] == 0)
          c332e->arg_vals[0]++;
        for (x = 0; x < c332e->arg_vals[0]; x++)
          WindowCursorLeft(term->window);
        break;

      case 'H': /*Direct Cursor Addressing (Line, then column)*/
      case 'I':
        if (c332e->arg_vals[0] > 0)
          c332e->arg_vals[0]--;
        if (c332e->arg_vals[1] > 0)
          c332e->arg_vals[1]--;
        WindowCursorAt(term->window, c332e->arg_vals[1], c332e->arg_vals[0]);
        break;

      case 'J': /* Clear screen */
        if (c332e->arg_vals[0] == 1 || c332e->arg_vals[0] == 2)
          WindowClear(term->window);
        if (c332e->arg_vals[0] == 0)
          WindowEraseToEnd(term->window);
        break;

      case 'K':
        switch (c332e->arg_vals[0])
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
        for (x = 0; x < c332e->curr_arg; x++)
          switch (c332e->arg_vals[x])
          {
          case 0:
            c332e->curr_attrib = 0;
            c332e->blink = 0x00;
            break;

          case 1:
            c332e->curr_attrib |= 0x01;
            break;

          case 4:
            c332e->curr_attrib |= 0x02;
            break;

          case 5:
            c332e->blink = 0x80;
            break;

          case 6:
            c332e->curr_attrib |= 0x04;
            break;

          case 7:
            c332e->curr_attrib |= 0x08;
            break;

          case 22:
            c332e->curr_attrib &= 0xfe;
            break;

          case 24:
            c332e->curr_attrib &= 0xfd;
            break;

          case 25:
            c332e->blink = 0x00;
            break;

          case 26:
            c332e->curr_attrib &= 0xfb;
            break;

          case 27:
            c332e->curr_attrib &= 0xf7;
            break;
          }
        WindowSetAttribute(term->window, c332e->attributes[1][c332e->curr_attrib] << 4 | c332e->attributes[0][c332e->curr_attrib] | c332e->blink);
        break;

      case 'r':
        if (c332e->arg_vals[0] > 0)
          c332e->arg_vals[0]--;
        if (c332e->arg_vals[1] > 0)
          c332e->arg_vals[1]--;
        c332e->scroll_row1 = c332e->arg_vals[0];
        c332e->scroll_row2 = c332e->arg_vals[1];
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
        switch (c332e->arg_vals[0])
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
        switch (c332e->arg_vals[0])
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
    for (x = 0; x < c332e->curr_arg; x++)
      c332e->arg_vals[x] = 0;
    c332e->curr_arg = 0;
    c332e->esc_lbrack_question = FALSE;
    term->CharHandler = c332eOutChar;
  }
}

/******************************************************************************\

 Routine: c332eLParenthesis

    Pass:

  Return: Nothing

\******************************************************************************/

void c332eLParenthesis(term_obj *term, BYTE ch)
{
  c332e_obj *c332e = term->emulator;

#ifdef DEBUG
  OutDebugText("JI");
#endif

  switch (ch)
  {
  case '0':
    term->translation_table_p = c332e->graphics_xlate_table;
    ;
    break;

  case 'B':
    term->translation_table_p = c332e->default_xlate_table;
    ;
    break;
  }
  term->CharHandler = c332eOutChar;
}

/******************************************************************************\

 Routine: c332eInterpretEscape

    Pass:

  Return: Nothing

\******************************************************************************/

void c332eInterpretEscape(term_obj *term, BYTE ch)
{
  c332e_obj *c332e = term->emulator;
  term->CharHandler = c332eOutChar;

#ifdef DEBUG
  OutDebugText("JJ");
#endif

  switch (ch)
  {
  case '[':
    term->CharHandler = c332eCollectArgs;
    break;

  case '(':
    term->CharHandler = c332eLParenthesis;
    break;

  case '=': /* TODO: What does this do? */
    break;

  case '>': /* TODO: What does this do? */
    break;

  case '7':
    c332e->save_cursor_y = WindowGetCursorY(term->window);
    c332e->save_cursor_x = WindowGetCursorX(term->window);
    c332e->save_cursor_attrib = WindowGetAttribute(term->window);
    break;

  case '8':
    WindowCursorAt(term->window, c332e->save_cursor_x, c332e->save_cursor_y);
    WindowSetAttribute(term->window, c332e->save_cursor_attrib);
    break;

  case 'M':
    WindowScrollDown(term->window, c332e->scroll_row1, 0, c332e->scroll_row2, 79);
    break;

  case 'D':
    WindowScrollUp(term->window, c332e->scroll_row1, 0, c332e->scroll_row2, 79);
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

 Routine: c332eInterpretControlCh

    Pass:

  Return: Nothing

\******************************************************************************/

void c332eInterpretControlCh(term_obj *term, BYTE ch)
{
  c332e_obj *c332e = term->emulator;

#ifdef DEBUG
  OutDebugText("JK");
#endif

  switch (ch)
  {
  case 0x0e:
    term->translation_table_p = c332e->graphics_xlate_table;
    ;
    break;

  case 0x0f:
    term->translation_table_p = c332e->default_xlate_table;
    break;

  case 0x1b:
    term->CharHandler = c332eInterpretEscape;
    break;

  default:
    WindowPrintChar(term->window, ch);
  }
}

/******************************************************************************\

 Routine: c332eOutChar

    Pass:

  Return: Nothing

\******************************************************************************/

void c332eOutChar(term_obj *term, BYTE ch)
{
#ifdef DEBUG
  OutDebugText("JL");
#endif

  if (ch < 32)
    c332eInterpretControlCh(term, ch);
  else
    WindowPrintChar(term->window, ch);
}
