#include <dos.h>
#include <bios.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <stdio.h>
#include <ctype.h>

#include "..\common\include\types.h"
#include "..\common\include\debug.h"
#include "..\common\include\chrgraph.h"
#include "..\common\include\window.h"
#include "..\common\include\bqueue.h"
#include "..\common\include\jscser.h"
#include "..\common\include\modem.h"
#include "..\common\include\intrface.h"
#include "..\common\include\jscio.h"
#include "..\common\include\fileio.h"
#include "..\common\include\jsctime.h"
#include "..\common\include\speaker.h"
#include "..\common\include\keybrd.h"
#include "..\common\include\doublell.h"
#include "..\common\include\mem.h"
#include "..\filexfer\include\thread.h"
#include "..\filexfer\include\filexfer.h"
#include "include\termobj.h"
#include "include\ilink.h"

void ShowStrOnStatusBar(int, const char *);

void c332AttribFill(term_obj *, BYTE _far *, BYTE);
void c332AddConfigMenuItems(term_obj *, s_menu *, int);
void c332CharHandler(term_obj *, BYTE);
void c332ClearTabStop(term_obj *);
void c332ClearAllTabs(term_obj *);
void c332CollectArgs(term_obj *, BYTE);
void c332CopyScreenToSlave(term_obj *);
void c332CursorAt(term_obj *, int, int);
void c332CursorDown(term_obj *);
void c332CursorLeft(term_obj *);
void c332CursorRight(term_obj *);
void c332CursorUp(term_obj *);
void c332DeleteLine(term_obj *, int);
void c332DeleteChar(term_obj *);
void c332DispFnKeyLabels(term_obj *);
void c332InsertLine(term_obj *, int);
void c332EndTerm(term_obj *);
BOOLN c332EvaluateKey(term_obj *, WORD);
void c332DisplayStatusMessage(term_obj *);
BYTE c332EvalAttrib(term_obj *, BYTE);
void c332InsertChar(term_obj *);
void c332InterpretEscape(term_obj *, BYTE);
void InterpretControlCh(term_obj *, int);
BOOLN c332KeyboardLocked(term_obj *);
void LoadFunctionKeyLabel(term_obj *, BYTE);
void LoadFunctionKey(term_obj *, BYTE);
void c332OutChar(term_obj *, BYTE);
void c332Reset(term_obj *);
void c332RestoreTerm(term_obj *);
void c332SaveSetup(term_obj *);
void c332SaveTerm(term_obj *);
void c332SaveTerminalSetup(term_obj *);
void c332ScrollScreenUp(term_obj *, int, int);
void c332SetColors(term_obj *);
void c332SetCursorCol(term_obj *, BYTE);
void c332SetCursorRow(term_obj *, BYTE);
void c332SetScreenBase(term_obj *);
void c332SetTabStop(term_obj *);
void c332ShowStatus(term_obj *);
void c332ShowChar(term_obj *, int);
void c332SlavePrtOptionsDB(term_obj *);
BOOLN c332StatBarChanged(term_obj *);
BOOLN c332StatBar(term_obj *);
void SlavePrint(term_obj *, BYTE);
void EscCursorAt(term_obj *, BYTE);
void SetAttr(term_obj *, BYTE);
void DeleteAttrib(term_obj *, WORD);
void DepositStatusMessage(term_obj *, BYTE);
void ThrowAwayChars(term_obj *, BYTE);
void FunctionKeyorLabelLoad(term_obj *, BYTE);
void ShowFnKeyLabels(term_obj *);
void CloseSlaveFile(term_obj *);
void ReleaseStatusBar(term_obj *);
void c332UpdateCursorPos(term_obj *);
void SlavePrintSet(term_obj *, BYTE);

/* Declare external variables defined in the main module */

extern union REGS inregs, outregs;
extern BOOLN sound_on;
extern s_choice_items colors[];
extern char colors_str[];
extern BYTE _far *screen_base;
extern int slave_port_online;
extern BOOLN add_lf_on;
extern WORD menu_frame_attrib;
extern WORD menu_text_attrib;
extern void RefreshStatusBar(void);
extern s_com_port *ports[2];

/* Characters used when graphics mode is on */
const char graphchars[] = "Ä³ÅÁÂÀÚ¿ÙÃ´Ä³ÅÁÂÀÚ¿ÙÃ´          ";

/* These characters are sent to the slave port to redraw the graphics characters
   on the screen. Each one corresponds to a character in the graphchars array
   above */
const char revgraphchars[] = " !\"#$%&'()*";

const char slvprt_str[] = "SlvPrt";
const char slvshw_str[] = "SlvShw";
const char slvcap_str[] = "SlvCap";

const char *slvprtstrings[] =
    {slvprt_str, slvprt_str, slvprt_str, slvshw_str, slvcap_str};

s_choice_items slave_destinations[] = {{"LPT1", '1'},
                                       {"LPT2", '2'},
                                       {"LPT3", '3'},
                                       {"Screen", 'S'},
                                       {"File", 'F'}};

#define c332_obj struct s_c332_obj
struct s_c332_obj
{
  int cursor_x;                           /* c332 Cursor X position */
  int cursor_y;                           /* c332 Cursor Y position */
  BYTE _far *cursor_loc;                  /* Ptr to cursor location within screen memory */
  WORD attribpos;                         /* Ptr to cursor location within attribute buffer */
  void (*pCharHandler)(term_obj *, BYTE); /* Pointer to current routine evaluating characters received */
  BOOLN graph_on;                         /* Flag indicating whether or not graphics mode is on */
  BYTE attribbuff[1920];                  /* Attribute buffer */
  int protected_line[24];                 /* Flags for each row of the screen indicating whether or not that row contains a protected attribute */
  BOOLN keyboard_locked;                  /* Flag indicating keyboard locked or not */
  BOOLN stat_bar;                         /* Whether status bar is being used by c332 module or not */
  BOOLN stat_string;
  char stat_message[79];
  BOOLN tab_stops[80];
  BOOLN stat_bar_changed; /* Whether or not this module has taken control of or released control of the status bar recently */
  BOOLN slave_message;    /* Whether or not there is currently a slave print message displayed on the status bar */
  WORD normal_fore;       /* Value of normal foreground color */
  WORD normal_back;       /* Value of normal background color */
  BYTE normal_attrib;
  WORD half_intens_fore; /* Value of half-intensity color */
  WORD half_intens_back;
  WORD inverse_fore;
  WORD inverse_back;
  WORD half_inverse_fore;
  WORD half_inverse_back;
  BOOLN old_add_lf_on;
  int slave_destination;
  BOOLN slave_show_input;
  char slave_filename[161];
  FILE *slave_file;
  char *function_keys[40];
  BYTE func_key_labels[40][8];
  BOOLN fn_key_labels_displayed;
  char args_str[4];
  int curr_arg;
  int curr_arg_char;
  int arg_vals[2];
  BOOLN esc_lbracket_question_mark;
  WORD num_chars_to_throw_away;
  int slave_esc;
  int state;
  WORD block_len;
  WORD label_num;
  WORD curr_char;
  WORD checksum;
  WORD chars_received;
  WORD remote_checksum;
  WORD num_chars;
  WORD fn_key_num;
  char run_filename[161];
};

char *c332ClearScreenString = "\x1bK";

BYTE _far *screen_end;

#define attrib_protected_bit 0x02

/******************************************************************************\

  Routine: Initc332

 Function: Initialize values for c332 routines

     Pass: Nothing

   Return: Terminal routines structure

\******************************************************************************/

term_obj *Initc332(void)
{
  c332_obj *new_c332;
  term_obj *new_term;
  FILE *config_file;
  int x, y;

  screen_end = screen_base + 0xf00l;
  new_term = NewTerm(FALSE, "c332", "c332", "c332");
  if (new_term != NULL)
  {
    if ((new_c332 = (c332_obj *)malloc(sizeof(c332_obj))) != NULL)
    {
      new_c332->graph_on = FALSE;
      new_c332->normal_fore = 2; /* Set screen colors */
      new_c332->normal_back = 0;
      new_c332->normal_attrib = 2;
      new_c332->half_intens_fore = 9;
      new_c332->half_intens_back = 0;
      new_c332->inverse_fore = 15;
      new_c332->inverse_back = 2;
      new_c332->half_inverse_fore = 14;
      new_c332->half_inverse_back = 3;
      new_c332->slave_destination = 0;
      new_c332->slave_show_input = FALSE;
      new_c332->slave_message = FALSE;
      new_c332->slave_file = NULL;
      new_c332->pCharHandler = c332OutChar;
      new_c332->keyboard_locked = FALSE;
      new_c332->fn_key_labels_displayed = FALSE;
      new_c332->num_chars_to_throw_away = 0;
      new_c332->slave_esc = 0;
      new_c332->state = 0;
      new_c332->checksum = 0;
      new_c332->chars_received = 0;
      new_c332->curr_arg_char = 0; /* TODO: Fix this in other modules ?*/
      strcpy(new_c332->run_filename, "/c ");

      for (x = 0; x < 40; x++)
      {
        new_c332->function_keys[x] = NULL;
        new_c332->func_key_labels[x][0] = 0x0c;
        for (y = 1; y < 7; y++)
          new_c332->func_key_labels[x][y] = ' ';
        new_c332->func_key_labels[x][7] = 0;
      }
      new_c332->curr_arg = 0;
      new_c332->arg_vals[0] = new_c332->arg_vals[1] = 0xff;
      new_c332->esc_lbracket_question_mark = FALSE;
      new_c332->slave_filename[0] = 0;
      if ((config_file = fopen("c332.cfg", "rb")) != NULL)
      {
        SetCurrentFile(config_file);

        new_c332->normal_fore = ReadWordFromFile();
        new_c332->normal_back = ReadWordFromFile();
        new_c332->normal_attrib = new_c332->normal_back << 4 | new_c332->normal_fore;
        new_c332->half_intens_fore = ReadWordFromFile();
        new_c332->half_intens_back = ReadWordFromFile();
        new_c332->inverse_fore = ReadWordFromFile();
        new_c332->inverse_back = ReadWordFromFile();
        new_c332->half_inverse_fore = ReadWordFromFile();
        new_c332->half_inverse_back = ReadWordFromFile();
        new_c332->slave_destination = ReadWordFromFile();
        new_c332->slave_show_input = ReadWordFromFile();
        ReadStringFromFile(new_c332->slave_filename);
        fclose(config_file);
      }
      new_term->CharHandler = c332CharHandler;
      new_term->ShowStatus = c332ShowStatus;
      new_term->KeyBoardLocked = c332KeyboardLocked;
      new_term->EvaluateKey = c332EvaluateKey;
      new_term->StatBarChanged = c332StatBarChanged;
      new_term->StatBar = c332StatBar;
      new_term->DisplayStatusMessage = c332DisplayStatusMessage;
      new_term->SaveTerminalSetup = c332SaveSetup;
      new_term->ReleaseStatusBar = ReleaseStatusBar;
      new_term->AddConfigMenuItems = c332AddConfigMenuItems;
      new_term->CopyScreenToSlave = c332CopyScreenToSlave;
      new_term->SetScreenBase = c332SetScreenBase;
      new_term->ClearScreenString = c332ClearScreenString;
      new_term->SaveTerm = c332SaveTerm;
      new_term->RestoreTerm = c332RestoreTerm;
      new_term->EndTerm = c332EndTerm;
      new_term->num_menu_items = 4;
      new_term->emulator = new_c332;
      c332Reset(new_term);
    }
    else
      new_term->EndTerm(new_term);
  }
  return new_term;
}

/******************************************************************************\

  Routine: DeInitc332

 Function: Initialize values for c332 routines

     Pass: Nothing

   Return: Terminal routines structure

\******************************************************************************/

void c332EndTerm(term_obj *term)
{
  int x;
  c332_obj *c332 = term->emulator;

  for (x = 0; x < 40; x++)
    if (c332->function_keys[x] != NULL)
      free(c332->function_keys[x]);
  if (c332->slave_file != NULL)
    fclose(c332->slave_file);
  if (term->save_win != NULL)
    free(term->save_win);
  free(term->translation_table_p);
  DestroyFNKeyTable(&term->fn_keys);
  free(c332);
  free(term);
}

/******************************************************************************\

  Routine: c332SaveSetup

 Function:

     Pass:

   Return:

\******************************************************************************/

void c332SaveSetup(term_obj *term)
{
  FILE *config_file;
  c332_obj *c332 = term->emulator;

  config_file = fopen("c332.cfg", "wb");
  SetCurrentFile(config_file);

  WriteWordToFile(c332->normal_fore);
  WriteWordToFile(c332->normal_back);
  WriteWordToFile(c332->half_intens_fore);
  WriteWordToFile(c332->half_intens_back);
  WriteWordToFile(c332->inverse_fore);
  WriteWordToFile(c332->inverse_back);
  WriteWordToFile(c332->half_inverse_fore);
  WriteWordToFile(c332->half_inverse_back);
  WriteWordToFile(c332->slave_destination);
  WriteWordToFile(c332->slave_show_input);
  WriteStringToFile(c332->slave_filename);

  fclose(config_file);
}

/******************************************************************************\

  Routine: c332TerminalSetup

 Function:

     Pass:

   Return:

\******************************************************************************/

void c332AddConfigMenuItems(term_obj *term, s_menu *configmenu, int row)
{
  AddMenuItem(configmenu, row++, 2, FUNCTION, "c332 Colors", '3', "", NULL,
              c332SetColors, (void *)term);
  AddMenuItem(configmenu, row++, 2, FUNCTION, "c332 Slave Print Options", 'r',
              "", NULL, c332SlavePrtOptionsDB, (void *)term);
  AddMenuItem(configmenu, row++, 2, FUNCTION, "Close c332 Slave Print File", 'l',
              "", NULL, CloseSlaveFile, (void *)term);
  AddMenuItem(configmenu, row, 2, FUNCTION, "Display Function Key Labels", 'D',
              "", NULL, c332DispFnKeyLabels, (void *)term);
}

/******************************************************************************\

  Routine: ThrowAwayChars

 Function:

     Pass:

   Return:

\******************************************************************************/

void ThrowAwayChars(term_obj *term, BYTE num_chars)
{
  c332_obj *c332 = term->emulator;

  if (c332->num_chars_to_throw_away == 0)
    c332->num_chars_to_throw_away = num_chars;
  else
    c332->num_chars_to_throw_away--;
  if (c332->num_chars_to_throw_away == 0)
    c332->pCharHandler = c332OutChar;
}

/******************************************************************************\

  Routine: c332SetColors

 Function:

     Pass:

   Return:

\******************************************************************************/

void c332SetColors(term_obj *term)
{
  c332_obj *c332 = term->emulator;
  s_dialog_box *db = CreateDialogBox("c332 Colors", 6, 13, 12, 54);

  AddDBItem(db, CHOICE_GADGET, 'F', 2, 2, "Normal Foreground...................",
            2, 39, &colors_str[0], 16, 2, 33, 20, 14, &colors[0], &c332->normal_fore);
  AddDBItem(db, CHOICE_GADGET, 'B', 3, 2, "Normal Background...................",
            3, 39, &colors_str[0], 8, 6, 33, 12, 14, &colors[0], &c332->normal_back);
  AddDBItem(db, CHOICE_GADGET, 'H', 4, 2, "Half-Intensity Foreground...........",
            4, 39, &colors_str[0], 16, 2, 33, 20, 14, &colors[0], &c332->half_intens_fore);
  AddDBItem(db, CHOICE_GADGET, 'I', 5, 2, "Half-Intensity Background...........",
            5, 39, &colors_str[0], 8, 6, 33, 12, 14, &colors[0], &c332->half_intens_back);
  AddDBItem(db, CHOICE_GADGET, 'n', 6, 2, "Inverse Foreground..................",
            6, 39, &colors_str[0], 16, 2, 33, 20, 14, &colors[0], &c332->inverse_fore);
  AddDBItem(db, CHOICE_GADGET, 'v', 7, 2, "Inverse Background..................",
            7, 39, &colors_str[0], 8, 6, 33, 12, 14, &colors[0], &c332->inverse_back);
  AddDBItem(db, CHOICE_GADGET, 'o', 8, 2, "Half-Intensity & Inverse Foreground.",
            8, 39, &colors_str[0], 16, 2, 33, 20, 14, &colors[0], &c332->half_inverse_fore);
  AddDBItem(db, CHOICE_GADGET, 'a', 9, 2, "Half-Intensity & Inverse Background.",
            9, 39, &colors_str[0], 8, 6, 33, 12, 14, &colors[0], &c332->half_inverse_back);
  DoDialogBox(db);
  DestroyDialogBox(db);
  c332->normal_attrib = c332->normal_back << 4 | c332->normal_fore;
}

/******************************************************************************\

  Routine: c332SlavePrtOptionsDB

 Function:

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

void c332SlavePrtOptionsDB(term_obj *term)
{
  c332_obj *c332 = term->emulator;
  s_dialog_box *db = CreateDialogBox("Slave Print Options", 9, 5, 8, 70);

  AddDBItem(db, CHOICE_GADGET, 'D', 2, 2, "Slave Print Destination......",
            2, 32, "Destinations", 5, 12, 31, 9, 20, &slave_destinations[0],
            &c332->slave_destination);
  AddDBItem(db, ON_OFF_GADGET, 'S', 3, 2, "Show Incoming Data on Screen.",
            3, 32, &c332->slave_show_input);
  AddDBItem(db, STRING_GADGET, 'F', 4, 2, "File Name for Slave File.....",
            5, 2, 66, c332->slave_filename, 160);
  DoDialogBox(db);
  DestroyDialogBox(db);
}

/******************************************************************************\

  Routine: c332EvaluateKey

 Function: Evaluate extended keypresses which the main program could not identify

     Pass: The extended portion of the keycode

   Return: Nothing

\******************************************************************************/

BOOLN c332EvaluateKey(term_obj *term, WORD key)
{

  c332_obj *c332 = term->emulator;
  WORD keyval = key & 0xff;
  BOOLN evaluated = FALSE;
  s_fkey_def *fkeydef;

  if (IsNotASCIIKey(key) && (keyval >= 0x3b) && (keyval <= 0x44))
  {
    WORD x = 0;
    WORD fn_key_num;

    if (IsShifted(key))
      x = 1;
    else if (IsCtrled(key))
      x = 2;
    else if (IsAlted(key))
      x = 3;
    fn_key_num = ((keyval - 0x3a) + x * 10) - 1;
    if (c332->function_keys[fn_key_num] != NULL)
    {
      SerialStringWrite(ports[0], c332->function_keys[fn_key_num]);
      evaluated = TRUE;
    }
  }
  else if ((fkeydef = DoublyLinkedListGetItemWKeyOf(term->fn_keys->fn_key_defs, key)) != NULL)
  {
    SendStringToModem(ports[0], fkeydef->definition, FALSE);
    evaluated = TRUE;
  }
  else
    switch (key)
    {
    case KEY_ALT_R:
      c332->graph_on = FALSE;
      break;

    case KEY_ALT_K:
      c332->keyboard_locked = FALSE;
      if (c332->fn_key_labels_displayed == FALSE)
        HChar(24, StatBarLEFT + 17, 6, ' ');
      break;
    }
  return evaluated;
}

/******************************************************************************\

  Routine: ShowFnKeyLabels

 Function: Clears the screen, clears the attribute buffer, sets all lines to
	   show they do not contain a protected attribute, places the cursor in
	   the upper left hand corner

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

void ShowFnKeyLabels(term_obj *term)
{
  c332_obj *c332 = term->emulator;
  int x;

  CursorAt(24, 0);
  for (x = 0; x < 10; x++)
  {
    SetTextAttrib(c332EvalAttrib(term, c332->func_key_labels[x][0]));
    OutTextC(&c332->func_key_labels[x][1]);
    OutChar(' ');
  }
}

/******************************************************************************\

  Routine: c332DispFnKeyLabels

 Function: Clears the screen, clears the attribute buffer, sets all lines to
	   show they do not contain a protected attribute, places the cursor in
	   the upper left hand corner

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

void c332DispFnKeyLabels(term_obj *term)
{
  c332_obj *c332 = term->emulator;

  if (c332->fn_key_labels_displayed == FALSE)
  {
    ShowFnKeyLabels(term);
    c332->stat_bar = TRUE;
    c332->fn_key_labels_displayed = TRUE;
  }
  else
  {
    c332->stat_bar = c332->stat_string;
    c332->fn_key_labels_displayed = FALSE;
    RefreshStatusBar();
  }
}

/******************************************************************************\

  Routine: c332SaveTerm

 Function:

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

void c332SaveTerm(term_obj *term)
{
  term->save_win = SaveRect(0, 0, 24, 80);
}

/******************************************************************************\

  Routine: c332RestoreTerm

 Function:

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

void c332RestoreTerm(term_obj *term)
{
  RestoreRect(term->save_win);
  term->save_win = NULL;
  c332UpdateCursorPos(term);
}

/******************************************************************************\

  Routine: c332Reset

 Function: Clears the screen, clears the attribute buffer, sets all lines to
	   show they do not contain a protected attribute, places the cursor in
	   the upper left hand corner

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

void c332Reset(term_obj *term)
{
  c332_obj *c332 = term->emulator;
  WORD _far *loc = (WORD _far *)screen_base;
  WORD value = c332->normal_attrib << 8 | 32;
  WORD x = 0;

  while (loc < (WORD _far *)(screen_end))
  {
    *loc++ = value;
    c332->attribbuff[x++] = 0;
  }
  for (x = 0; x < 24; x++)
    c332->protected_line[x] = 0;

  for (x = 0; x < 80; x++)
    c332->tab_stops[x] = FALSE;
  c332CursorAt(term, 0, 0);
  c332->stat_bar = c332->fn_key_labels_displayed;
  c332->stat_string = FALSE;
  c332->stat_bar_changed = TRUE;
}

/******************************************************************************\

  Routine: c332CopyScreenToSlave

 Function:

     Pass:

   Return:

\******************************************************************************/

void c332CopyScreenToSlave(term_obj *term)
{
  c332_obj *c332 = term->emulator;
  BYTE _far *loc = (BYTE _far *)screen_base;
  WORD x = 0;
  BOOLN graphics_mode = NEUTRAL;

  /* Reset remote screen */

  SerialStringWrite(ports[1], c332ClearScreenString);

  /* Redraw remote screen */

  while (loc < (BYTE _far *)(screen_base + 0xefeL))
  {
    if (c332->attribbuff[x])
    {
      SerialStringWrite(ports[1], "\x1b!");
      SerialWrite(ports[1], c332->attribbuff[x]);
    }
    else
    {
      if (*loc < (BYTE)128)
      {
        if (graphics_mode != FALSE)
        {
          SerialWrite(ports[1], 15);
          graphics_mode = FALSE;
        }
        SerialWrite(ports[1], *loc);
      }
      else
      {
        int y = 0;

        while (*loc != (BYTE)graphchars[y] && y < 10)
          y++;
        if (graphics_mode != TRUE)
        {
          SerialWrite(ports[1], 14);
          graphics_mode = TRUE;
        }
        SerialWrite(ports[1], revgraphchars[y]);
      }
    }
    loc += 2;
    x++;
  }

  /* Set graphics mode back to what it should be on the remote */
  if (c332->graph_on == FALSE)
    SerialWrite(ports[1], 15);
  else
    SerialWrite(ports[1], 14);

  /* Reposition cursor to correct location on remote */

  SerialStringWrite(ports[1], "\x1b@");
  SerialWrite(ports[1], c332->cursor_x + 32);
  SerialWrite(ports[1], c332->cursor_y + 32);
}

/******************************************************************************\

  Routine: c332ShowStatus

 Function:

     Pass:

   Return:

\******************************************************************************/

void c332ShowStatus(term_obj *term)
{
  c332_obj *c332 = term->emulator;

  if (c332->slave_message == TRUE)
    ShowStrOnStatusBar(StatBarLEFT + 10, slvprtstrings[c332->slave_destination]);
}

/******************************************************************************\

  Routine: c332SetScreenBase

 Function:

     Pass:

   Return:

\******************************************************************************/

void c332SetScreenBase(term_obj *term)
{
  c332_obj *c332 = term->emulator;

  c332->cursor_loc = screen_base + c332->cursor_y * 160 + c332->cursor_x * 2;
  screen_end = screen_base + 0xf00l;
}

/******************************************************************************\

  Routine: c332StatBarChanged

 Function: When the c332 subprogram grabs control of or releases control of the
	   status bar the variable stat_bar_changed is set to 1. This function
	   returns the value of this variable to the calling program and resets
	   it to zero.

     Pass: Nothing

   Return: 0 = This subprogram has not taken control or released control of the
	       status bar since this function was last called.
	   1 = This subprogram has taken conrtol or released control of the
	       status bar since this function was last called.

\******************************************************************************/

BOOLN c332StatBarChanged(term_obj *term)
{
  c332_obj *c332 = term->emulator;
  BOOLN x = c332->stat_bar_changed;

  c332->stat_bar_changed = FALSE;
  return x;
}

/******************************************************************************\

  Routine: c332StatBar

 Function: Notifies the calling program whether or not this subprogram has
	   control of the status bar.

     Pass: Nothing

   Return: 0 = This subprogram does not have control of the status bar.
	   1 = This subprogram has control of the status bar.

\******************************************************************************/

BOOLN c332StatBar(term_obj *term)
{
  c332_obj *c332 = term->emulator;
  return c332->stat_bar;
}

/******************************************************************************\

  Routine: ReleaseStatusBar

 Function:

     Pass:

   Return:

\******************************************************************************/

void ReleaseStatusBar(term_obj *term)
{
  c332_obj *c332 = term->emulator;

  c332->stat_bar = c332->fn_key_labels_displayed;
  c332->stat_string = FALSE;
  c332->stat_bar_changed = TRUE;
}

/******************************************************************************\

  Routine: c332KeyboardLocked

 Function: Notifies the calling program whether or not this subprogram has
	   locked the keyboard out.

     Pass: Nothing

   Return: 0 = This subprogram has not locked out the keyboard.
	   1 = This subprogram has locked out the keyboard.

\******************************************************************************/

BOOLN c332KeyboardLocked(term_obj *term)
{
  c332_obj *c332 = term->emulator;
  return c332->keyboard_locked;
}

/******************************************************************************\

  Routine: c332ScrollScreenIUp

 Function:

     Pass:

   Return: Nothing

\******************************************************************************/

void c332ScrollScreenUp(term_obj *term, int line_no, int last_row)
{
  c332_obj *c332 = term->emulator;
  WORD _far *dest;
  WORD _far *source;
  WORD _far *last_row_p;
  WORD _far *row_after_last;
  WORD value;
  WORD del_attrib_pos;

  dest = (WORD _far *)(screen_base + (line_no << 7) + (line_no << 5));
  source = dest + 80L;
  last_row_p = (WORD _far *)(screen_base + last_row * 160);
  row_after_last = last_row_p + 80L;
  value = *(screen_base + last_row * 160 - 1) << 8 | 32;
  del_attrib_pos = (WORD)(line_no * 80);

  while (dest < last_row_p)
  {
    *dest++ = *source++;
    c332->attribbuff[del_attrib_pos++] = c332->attribbuff[del_attrib_pos + 80];
  }

  while (dest < row_after_last)
  {
    *dest++ = value;
    c332->attribbuff[del_attrib_pos++] = 0;
  }

  for (value = line_no; value < last_row; value++)
    c332->protected_line[value] = c332->protected_line[value + 1];
  c332->protected_line[last_row] = 0;
}

/******************************************************************************\

  Routine: c332DeleteLine

 Function: Deletes the line passed to the function and moves all lines below it
	   up one line until it reaches a line with a protected attribute on it
	   or the bottom of the screen.

     Pass: The line number to be deleted (Line 0 is top line)

   Return: Nothing

\******************************************************************************/

void c332DeleteLine(term_obj *term, int line_no)
{
  c332_obj *c332 = term->emulator;
  int last_row = line_no;

  if (c332->protected_line[line_no])
    return;

  while (c332->protected_line[last_row + 1] == 0 && last_row < 23)
    last_row++;

  c332ScrollScreenUp(term, line_no, last_row);
}

/******************************************************************************\

  Routine: c332InsertLine

 Function: Inserts a line at the line number passed to the function.

     Pass: The line number to insert a line (Line 0 is top line)

   Return: Nothing

\******************************************************************************/

void c332InsertLine(term_obj *term, int line_no)
{
  c332_obj *c332 = term->emulator;
  WORD _far *dest;
  WORD _far *source;
  WORD _far *line_1;
  WORD _far *line_2;
  WORD value;
  WORD del_attrib_pos;
  int last_row = line_no;

  if ((WORD)line_no > (WORD)23)
    return;

  if (c332->protected_line[line_no] > 0)
    return;

  while (c332->protected_line[last_row + 1] == 0 && last_row < 23)
    last_row++;

  dest = (WORD _far *)(screen_base + last_row * 160 + 158);
  source = dest - 80;
  line_1 = (WORD _far *)(screen_base + line_no * 160);
  line_2 = line_1 + 80;
  value = *(screen_base + line_no * 160 + 1) << 8 | 32;
  del_attrib_pos = (WORD)((last_row + 1) * 80 - 1);

  while (dest >= line_2)
  {
    *dest-- = *source--;
    c332->attribbuff[del_attrib_pos--] = c332->attribbuff[del_attrib_pos - 80];
  }

  /* Fill in the new inserted line with nothing */
  while (dest >= line_1)
  {
    *dest-- = value;
    c332->attribbuff[del_attrib_pos--] = 0;
  }
}

/******************************************************************************\

  Routine: DeleteAttrib

 Function: Deletes an attribute from the attribute buffer at the position passed
	   to the function and sets all characters on the screen which were
	   using that attribute to the correct attribute.

     Pass: The position (0 - 1919) of the attribute within the buffer

   Return: Nothing

\******************************************************************************/

void DeleteAttrib(term_obj *term, WORD delete_attrib_pos)
{
  c332_obj *c332 = term->emulator;

  if (c332->attribbuff[delete_attrib_pos] & 2)
    c332->protected_line[c332->cursor_y]--;
  c332->attribbuff[delete_attrib_pos] = 0;
  while (!c332->attribbuff[delete_attrib_pos] && delete_attrib_pos)
    delete_attrib_pos--;
  if (c332->attribbuff[delete_attrib_pos])
    c332AttribFill(term, screen_base + (delete_attrib_pos << 1) + 2, c332EvalAttrib(term, c332->attribbuff[delete_attrib_pos]));
  else
    c332AttribFill(term, screen_base, c332->normal_attrib);
}

/******************************************************************************\

  Routine: ClearToEOL

 Function: Clears all characters and attributes from the cursor position to the
	   end of the line. If an attribute is deleted it ensures all characters
	   which were using that attribute are reset to the proper attribute.

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

void ClearToEOL(term_obj *term)
{
  c332_obj *c332 = term->emulator;
  int x;
  WORD clear_attrib_pos = c332->attribpos;

  for (x = 0; x < 80 - c332->cursor_x; x++)
  {
    *(c332->cursor_loc + (x << 1)) = 32;
    if (c332->attribbuff[clear_attrib_pos])
      DeleteAttrib(term, clear_attrib_pos);
    clear_attrib_pos++;
  }
}

/******************************************************************************\

  Routine: IsProtected

 Function:

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

BOOLN IsProtected(term_obj *term, WORD position)
{
  c332_obj *c332 = term->emulator;

  while (!c332->attribbuff[position] && position > 0)
    position--;
  return (c332->attribbuff[position] & attrib_protected_bit) ? FALSE : TRUE;
}

/******************************************************************************\

  Routine: ClearUnprotected

 Function:

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

void ClearUnprotected(term_obj *term)
{
  c332_obj *c332 = term->emulator;
  BYTE _far *position = screen_base + c332->cursor_y * 160 + c332->cursor_x * 2;
  WORD clear_attrib_pos = c332->attribpos;
  BOOLN clear; /* If clear is false it means we are in a protected area and
		   the characters shouldn't be cleared */

  clear = IsProtected(term, clear_attrib_pos);

  while (position < screen_end)
  {
    if (c332->attribbuff[clear_attrib_pos])
      clear = (c332->attribbuff[clear_attrib_pos] & attrib_protected_bit) ? FALSE : TRUE;
    else if (clear == TRUE)
      *position = ' ';
    position += 2;
    clear_attrib_pos++;
  }
}

/******************************************************************************\

  Routine: c332DeleteChar

 Function:

     Pass:

   Return:

\******************************************************************************/

void c332DeleteChar(term_obj *term)
{
  c332_obj *c332 = term->emulator;

  WORD _far *position = (WORD _far *)c332->cursor_loc;
  WORD clear_attrib_pos = c332->attribpos;
  BOOLN clear; /* If clear is false it means we are in a protected area and
		   the characters shouldn't be cleared */

  clear = IsProtected(term, clear_attrib_pos);

  while (clear == TRUE && position < (WORD _far *)screen_end)
  {
    if (c332->attribbuff[clear_attrib_pos + 1] & attrib_protected_bit)
      clear = FALSE;
    else
    {
      if (*((BYTE _far *)position + 2) == ' ' && *((BYTE _far *)position + 4) == ' ')
        clear = FALSE;
      else
      {
        *position = *(position + 1);
        c332->attribbuff[clear_attrib_pos] = c332->attribbuff[clear_attrib_pos + 1];
        position++;
        clear_attrib_pos++;
      }
    }
  }
  *((BYTE _far *)position) = ' ';
}

/******************************************************************************\

  Routine: c332InsertChar

 Function:

     Pass:

   Return:

\******************************************************************************/

void c332InsertChar(term_obj *term)
{
  c332_obj *c332 = term->emulator;
  WORD _far *start_position = (WORD _far *)c332->cursor_loc;
  WORD _far *position = (WORD _far *)c332->cursor_loc;
  WORD clear_attrib_pos = c332->attribpos;
  BOOLN clear; /* If clear is false it means we are in a protected area and
		   the characters shouldn't be cleared */

  clear = IsProtected(term, clear_attrib_pos);

  while (clear == TRUE && position < (WORD _far *)screen_end)
  {
    if ((c332->attribbuff[clear_attrib_pos + 1] & attrib_protected_bit))
      clear = FALSE;
    else
    {
      if (*((BYTE _far *)position + 2) == ' ' && *((BYTE _far *)position + 4) == ' ')
        clear = FALSE;
      position++;
      clear_attrib_pos++;
    }
  }

  while (position > start_position)
  {
    *position = *(position - 1);
    c332->attribbuff[clear_attrib_pos] = c332->attribbuff[clear_attrib_pos - 1];
    position--;
    clear_attrib_pos--;
  }
  *((BYTE _far *)position) = ' ';
}

/******************************************************************************\

  Routine: DepositStatusMessage

 Function:

     Pass:

   Return:

\******************************************************************************/

void DepositStatusMessage(term_obj *term, BYTE ch)
{
  c332_obj *c332 = term->emulator;
  char cha[2];

  cha[0] = (char)ch;
  cha[1] = 0;
  if (ch != 29)
    strcat(c332->stat_message, cha);
  if (strlen(c332->stat_message) == 78 || ch == 29)
    c332->pCharHandler = c332OutChar;
}

/******************************************************************************\

  Routine: c332SetTabStop

 Function:

     Pass:

   Return:

\******************************************************************************/

void c332SetTabStop(term_obj *term)
{
  c332_obj *c332 = term->emulator;
  c332->tab_stops[c332->cursor_x] = TRUE;
}

/******************************************************************************\

  Routine: c332ClearTabStop

 Function:

     Pass:

   Return:

\******************************************************************************/

void c332ClearTabStop(term_obj *term)
{
  c332_obj *c332 = term->emulator;
  c332->tab_stops[c332->cursor_x] = FALSE;
}

/******************************************************************************\

  Routine: c332ClearAllTabs

 Function:

     Pass:

   Return:

\******************************************************************************/

void c332ClearAllTabs(term_obj *term)
{
  c332_obj *c332 = term->emulator;
  int x;

  for (x = 0; x < 80; x++)
    c332->tab_stops[x] = FALSE;
}

/******************************************************************************\

  Routine: c332DisplayStatusMessage

 Function:

     Pass:

   Return:

\******************************************************************************/

void c332DisplayStatusMessage(term_obj *term)
{
  c332_obj *c332 = term->emulator;

  if (c332->fn_key_labels_displayed == TRUE)
    ShowFnKeyLabels(term);
  else
    ShowStrOnStatusBar(0, &c332->stat_message[0]);
}

/******************************************************************************\

  Routine: c332UpdateCursorPos

 Function:

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

void c332UpdateCursorPos(term_obj *term)
{
  c332_obj *c332 = term->emulator;

  inregs.h.ah = 0x02;
  inregs.h.bh = 0x00;
  inregs.h.dh = (BYTE)(c332->cursor_y);
  inregs.h.dl = (BYTE)(c332->cursor_x);
  int86(0x10, &inregs, &outregs);
}

/******************************************************************************\

  Routine: c332AttribFill

 Function:

     Pass:

   Return: Nothing

\******************************************************************************/

void c332AttribFill(term_obj *term, BYTE _far *screenpos, BYTE attrib)
{
  c332_obj *c332 = term->emulator;
  WORD fill_attrib_pos = (WORD)((screenpos - screen_base) >> 1);

  while (!c332->attribbuff[fill_attrib_pos++] && screenpos < screen_end)
  {
    screenpos++;
    *screenpos = (BYTE)attrib;
    screenpos++;
  }
}

/******************************************************************************\

 Routine:

    Pass:

  Return: Nothing

\******************************************************************************/

void c332CursorAt(term_obj *term, int row, int column)
{
  c332_obj *c332 = term->emulator;

  c332->cursor_x = column;
  c332->cursor_y = row;
  c332->cursor_loc = screen_base + row * 160 + column * 2;
  c332->attribpos = c332->cursor_y * 80 + c332->cursor_x;
  c332UpdateCursorPos(term);
}

/******************************************************************************\

 Routine:

    Pass:

  Return: Nothing

\******************************************************************************/

void SetAttr(term_obj *term, BYTE ch)
{
  c332_obj *c332 = term->emulator;
  ch = ch | (BYTE)0x80;

  *c332->cursor_loc = ' ';
  *(c332->cursor_loc + 1) = (BYTE)c332->normal_attrib;
  if (c332->attribbuff[c332->attribpos] & 0x02)
    c332->protected_line[c332->cursor_y]--;
  if (ch & attrib_protected_bit)
    c332->protected_line[c332->cursor_y]++;
  c332->attribbuff[c332->attribpos] = (char)ch;
  c332CursorRight(term);
  c332AttribFill(term, c332->cursor_loc, c332EvalAttrib(term, ch));
  c332->pCharHandler = c332OutChar;
}

/******************************************************************************\

 Routine: c332SetCursorRow

    Pass:

  Return: Nothing

\******************************************************************************/

void c332SetCursorRow(term_obj *term, BYTE ch)
{
  c332_obj *c332 = term->emulator;

  if (ch < (BYTE)56)
    c332CursorAt(term, ch - 32, c332->cursor_x);
  c332->pCharHandler = c332OutChar;
}

/******************************************************************************\

 Routine: c332SetCursorCol

    Pass:

  Return: Nothing

\******************************************************************************/

void c332SetCursorCol(term_obj *term, BYTE ch)
{
  c332_obj *c332 = term->emulator;

  if (ch < (BYTE)112)
    c332CursorAt(term, c332->cursor_y, ch - 32);
  c332->pCharHandler = c332OutChar;
}

/******************************************************************************\

  Routine: EscCursorAt

 Function:

     Pass:

   Return:

\******************************************************************************/

void EscCursorAt(term_obj *term, BYTE ch)
{
  c332_obj *c332 = term->emulator;

  c332SetCursorCol(term, ch);
  c332->pCharHandler = c332SetCursorRow;
}

/******************************************************************************\

  Routine: SlavePrint

 Function:

     Pass:

   Return:

\******************************************************************************/

void SlavePrint(term_obj *term, BYTE ch)
{
  c332_obj *c332 = term->emulator;

  switch (c332->slave_esc)
  {
  case 0:
    if (ch != 27)
    {
      if (!slave_port_online)
      {
        switch (c332->slave_destination)
        {
        case 4:
          fputc(ch, c332->slave_file);
          break;

        case 3:
          break;

        default:
          _bios_printer(_PRINTER_WRITE, c332->slave_destination, (unsigned)ch);
        }
        if (c332->slave_show_input == TRUE || c332->slave_destination == 3)
          c332OutChar(term, ch);
      }
    }
    else
      c332->slave_esc = 1;
    break;

  case 1:
    if (ch == ';')
      c332->slave_esc = 2;
    else
    {
      if (!slave_port_online)
      {
        switch (c332->slave_destination)
        {
        case 4:
          fputc(27, c332->slave_file);
          fputc(ch, c332->slave_file);
          break;

        case 3:
          break;

        default:
          _bios_printer(_PRINTER_WRITE, c332->slave_destination, (unsigned)27);
          _bios_printer(_PRINTER_WRITE, c332->slave_destination, (unsigned)ch);
        }

        if (c332->slave_show_input == TRUE)
        {
          c332OutChar(term, 27);
          c332OutChar(term, ch);
        }
        c332->slave_esc = 0;
      }
    }
    break;

  case 2:
    c332->slave_esc = 0;
    if (ch == '1')
      CloseSlaveFile(term);
    else
    {
      if (!slave_port_online)
      {
        switch (c332->slave_destination)
        {
        case 4:
          fputc(27, c332->slave_file);
          fputc(';', c332->slave_file);
          fputc(ch, c332->slave_file);
          break;

        case 3:
          _bios_printer(_PRINTER_WRITE, c332->slave_destination, (unsigned)27);
          _bios_printer(_PRINTER_WRITE, c332->slave_destination, (unsigned)';');
          _bios_printer(_PRINTER_WRITE, c332->slave_destination, (unsigned)ch);
          break;
        }

        if (c332->slave_show_input == TRUE || c332->slave_destination == 3)
        {
          c332OutChar(term, 27);
          c332OutChar(term, ';');
          c332OutChar(term, ch);
        }
      }
    }
  }
}

/******************************************************************************\

  Routine: SlavePrintSet

 Function:

     Pass:

   Return:

\******************************************************************************/

/* TODO: This routine only handles character '6'. Fix it so it handles all the
   characters in the following table.

   '1' = Cancel
   '2' = Print screen from home
   '5' = Simulprint with display
   '6' = Simulprint non-display
   Any other char cancels ESC
*/
void SlavePrintSet(term_obj *term, BYTE ch)
{
  c332_obj *c332 = term->emulator;

  if (ch == '6')
  {

    c332->pCharHandler = SlavePrint;
    if (c332->slave_destination == 4)
      c332->slave_file = fopen(c332->slave_filename, "ab");
    c332->old_add_lf_on = add_lf_on;
    add_lf_on = FALSE;
    c332->slave_message = TRUE;
  }
  else
    c332->pCharHandler = c332OutChar;
}

/******************************************************************************\

  Routine: CloseSlaveFile

 Function:

     Pass:

   Return:

\******************************************************************************/

void CloseSlaveFile(term_obj *term)
{
  c332_obj *c332 = term->emulator;

  if (c332->slave_message == TRUE)
  {
    c332->slave_message = FALSE;
    add_lf_on = c332->old_add_lf_on;
    if (c332->slave_destination == 4)
    {
      fclose(c332->slave_file);
      c332->slave_file = NULL;
    }
    if (c332->slave_show_input == TRUE)
      c332UpdateCursorPos(term);
    c332->pCharHandler = c332OutChar;
  }
}

/******************************************************************************\

  Routine: GetDOSCommand

 Function:

     Pass:

   Return:

\******************************************************************************/

void GetDOSCommand(term_obj *term, BYTE ch)
{
  c332_obj *c332 = term->emulator;
  char temp_string[2] = {0, 0};
  temp_string[0] = ch;

  if (ch != 13)
    strcat(c332->run_filename, temp_string);
  else
  {
    RunProg(c332->run_filename);
    strcpy(c332->run_filename, "/c ");
    c332->pCharHandler = c332OutChar;
  }
}

/******************************************************************************\

  Routine: FunctionKeyorLabelLoad

 Function:

     Pass:

   Return:

\******************************************************************************/

void FunctionKeyorLabelLoad(term_obj *term, BYTE ch)
{
  c332_obj *c332 = term->emulator;

  if (ch == 'K')
    c332->pCharHandler = LoadFunctionKey;
  else
    c332->pCharHandler = LoadFunctionKeyLabel;
}

/******************************************************************************\

  Routine: LoadFunctionKeyLabel

 Function:

     Pass:

   Return:

\******************************************************************************/

void LoadFunctionKeyLabel(term_obj *term, BYTE ch)
{
  c332_obj *c332 = term->emulator;

  ch = ch - 0x30;

  switch (c332->state)
  {
  case 0: /* High nybble of lsb of c332->block_len */
    c332->block_len = ch << 4;
    c332->state = 1;
    break;

  case 1: /* Low nybble of lsb of c332->block_len */
    c332->block_len |= ch;
    c332->state = 2;
    break;

  case 2:
    c332->block_len |= ch << 12; /* High nybble of msb of c332->block_len */
    c332->state = 3;
    break;

  case 3: /* Low nybble of msb of c332->block_len */
    c332->block_len |= ch << 8;
    c332->state = 4;
    break;

  case 4: /* High nybble of c332->label_num */
    c332->label_num = ch << 4;
    c332->state = 5;
    break;

  case 5: /* Low nybble of c332->label_num */
    c332->label_num |= ch;
    c332->checksum += c332->label_num;
    if (c332->label_num != 0)
    {
      c332->label_num--;
      c332->state = 6;
    }
    else
      c332->state = 10;
    break;

  case 6:
    if (ch == 0x01)
      c332->func_key_labels[c332->label_num][0] = 0x08;
    c332->state = 7;
    break;

  case 7:
    if (ch == 0x01)
      c332->func_key_labels[c332->label_num][0] |= 0x04;
    c332->chars_received = 1;
    c332->state = 8;
    break;

  case 8: /* Each char */
    c332->curr_char = ch << 4;
    c332->state = 9;
    break;

  case 9:
    c332->curr_char |= ch;
    c332->checksum += c332->curr_char;
    c332->func_key_labels[c332->label_num][c332->chars_received++] = (char)c332->curr_char;
    if (c332->chars_received == 7)
    {
      c332->func_key_labels[c332->label_num][c332->chars_received] = 0;
      c332->chars_received = 0;
      c332->state = 4;
    }
    else
      c332->state = 8;
    break;

  case 10: /* High nybble of msb of c332->remote_c332->checksum */
    c332->remote_checksum = ch << 4;
    c332->state = 11;
    break;

  case 11: /* Low nybble of msb of c332->remote_c332->checksum */
    c332->remote_checksum |= ch;
    c332->state = 12;
    break;

  case 12:
    c332->remote_checksum |= ch << 12; /* High nybble of lsb of c332->remote_c332->checksum */
    c332->state = 13;
    break;

  case 13: /* Low nybble of lsb of c332->remote_c332->checksum */
    c332->remote_checksum |= ch << 8;
    SerialWrite(ports[0], 0x06);
    c332->pCharHandler = c332OutChar;
    c332->state = 0;
    break;
  }
}

/******************************************************************************\

  Routine: LoadFunctionKey

 Function:

     Pass:

   Return:

\******************************************************************************/

void LoadFunctionKey(term_obj *term, BYTE ch)
{
  c332_obj *c332 = term->emulator;
  ch = ch - 0x30;

  switch (c332->state)
  {
  case 0: /* High nybble of lsb of c332->block_len */
    c332->block_len = ch << 4;
    c332->state = 1;
    break;

  case 1: /* Low nybble of lsb of c332->block_len */
    c332->block_len |= ch;
    c332->state = 2;
    break;

  case 2:
    c332->block_len |= ch << 12; /* High nybble of msb of c332->block_len */
    c332->state = 3;
    break;

  case 3: /* Low nybble of msb of c332->block_len */
    c332->block_len |= ch << 8;
    c332->state = 4;
    break;

  case 4: /* High nybble of fn_key_num */
    c332->fn_key_num = ch << 4;
    c332->state = 5;
    break;

  case 5: /* Low nybble of c332->fn_key_num */
    c332->fn_key_num |= ch;
    c332->checksum += c332->fn_key_num;
    if (c332->fn_key_num != 0)
    {
      c332->fn_key_num--;
      c332->state = 6;
    }
    else
      c332->state = 10;
    break;

  case 6: /* High nybble of num_chars */
    c332->num_chars = ch << 4;
    c332->state = 7;
    break;

  case 7: /* Low nybble of c332->num_chars */
    c332->num_chars |= ch;
    if (c332->function_keys[c332->fn_key_num] != NULL)
      free(c332->function_keys[c332->fn_key_num]);
    if (c332->num_chars > 0)
    {
      c332->function_keys[c332->fn_key_num] = malloc((size_t)(c332->num_chars + 1));
      c332->state = 8;
    }
    else
      c332->state = 4;
    c332->checksum += c332->num_chars;
    break;

  case 8: /* Each char */
    c332->curr_char = ch << 4;
    c332->state = 9;
    break;

  case 9:
    c332->curr_char |= ch;
    c332->checksum += c332->curr_char;
    *(c332->function_keys[c332->fn_key_num] + c332->chars_received++) = (char)c332->curr_char;
    if (c332->chars_received == c332->num_chars)
    {
      *(c332->function_keys[c332->fn_key_num] + c332->chars_received) = 0;
      c332->chars_received = 0;
      c332->state = 4;
    }
    else
      c332->state = 8;
    break;

  case 10: /* High nybble of msb of remote_checksum */
    c332->remote_checksum = ch << 4;
    c332->state = 11;
    break;

  case 11: /* Low nybble of msb of c332->remote_checksum */
    c332->remote_checksum |= ch;
    c332->state = 12;
    break;

  case 12:
    c332->remote_checksum |= ch << 12; /* High nybble of lsb of c332->remote_checksum */
    c332->state = 13;
    break;

  case 13: /* Low nybble of lsb of c332->remote_checksum */
    c332->remote_checksum |= ch << 8;
    SerialWrite(ports[0], 0x06);
    c332->pCharHandler = c332OutChar;
    c332->state = 0;
    break;
  }
}

/******************************************************************************\

 Routine: c332CollectArgs

    Pass:

  Return: Nothing

\******************************************************************************/

void c332CollectArgs(term_obj *term, BYTE ch)
{
  c332_obj *c332 = term->emulator;

  if (ch == '?' && c332->arg_vals[0] == 0xff)
    c332->esc_lbracket_question_mark = TRUE;
  else if (isdigit(ch))
    c332->args_str[c332->curr_arg_char++] = (char)ch;
  else if (ch == ';')
  {
    c332->args_str[c332->curr_arg_char] = 0;
    c332->curr_arg_char = 0;
    c332->arg_vals[c332->curr_arg++] = atoi(c332->args_str);
  }
  else
  {
    c332->args_str[c332->curr_arg_char] = 0;
    c332->curr_arg_char = 0;
    c332->arg_vals[c332->curr_arg++] = atoi(c332->args_str);

    if (c332->esc_lbracket_question_mark == TRUE)
    {
      switch (ch)
      {
      case 'h':
        switch (c332->arg_vals[0])
        {
        case 60:
          CapsLockOn();
          break;

        case 61:
          NumLockOn();
          break;

        case 62:
          switch (c332->arg_vals[1])
          {
          case 0:
          case 1:
            SetCursorShape(BLOCKCURSOR);
            break;

          case 2:
          case 3:
            SetCursorShape(UNDERLINECURSOR);
            break;
          }
          break;

        case 63: /* TODO: Local echo on */
          break;

        case 64: /* TODO: Handshaking */
          break;

        case 65: /* TODO: Auto linefeed on */
          break;

        case 66: /* TODO: Status line on */
          break;

        case 67: /* TODO: Margin bell on */
          break;

        case 68: /* TODO: Screen background reverse */
          break;

        case 69: /* TODO: Main port parity */
          break;

        case 70: /* TODO: Select character set */
          break;
        }
        break;

      case 'l':
        switch (c332->arg_vals[0])
        {
        case 60:
          CapsLockOff();
          break;

        case 61:
          NumLockOff();
          break;

        case 62:
          SetCursorShape(HIDECURSOR);
          break;

        case 63: /* TODO: Local echo off */
          break;

        case 64: /* TODO: Handshaking */
          break;

        case 65: /* TODO: Auto linefeed off */
          break;

        case 66: /* TODO: Status line off */
          break;

        case 67: /* TODO: Margin bell off */
          break;

        case 68: /* TODO: Screen background reverse */
          break;

        case 69: /* TODO: Main port parity */
          break;

        case 70: /* TODO: Select character set */
          break;
        }
      }
    }
    else
    {
      switch (ch)
      {
      case 'S':
        SwitchPage(c332->arg_vals[0]);
        break;
      }
    }
    c332->curr_arg = 0;
    c332->arg_vals[0] = c332->arg_vals[1] = 0xff;
    c332->esc_lbracket_question_mark = FALSE;
    c332->pCharHandler = c332OutChar;
  }
}

/******************************************************************************\

 Routine: c332InterpretEscape

    Pass:

  Return: Nothing

\******************************************************************************/

void c332InterpretEscape(term_obj *term, BYTE ch)
{
  c332_obj *c332 = term->emulator;
  c332->pCharHandler = c332OutChar;

  switch (ch)
  {
  case 27:
    c332->pCharHandler = c332InterpretEscape;
    break;

  case '!': /* Set attribute */
    c332->pCharHandler = SetAttr;
    break;

  case '%':
    if (!slave_port_online)
      SerialWrite(ports[0], 0x42);
    break;

  case '(': /* Lock keyboard */
    c332->keyboard_locked = TRUE;
    if (c332->fn_key_labels_displayed == FALSE)
      ShowStrOnStatusBar(StatBarLEFT + 17, "KeyLck");
    break;

  case ')': /* Unlock keyboard */
    c332->keyboard_locked = FALSE;
    if (c332->fn_key_labels_displayed == FALSE)
      HChar(24, StatBarLEFT + 17, 6, ' ');
    break;

  case '1': /* Set tab at cursor position */
    c332SetTabStop(term);
    break;

  case '2': /* Clear tab at cursor position */
    c332ClearTabStop(term);
    break;

  case '3': /* Clear all tabs */
    c332ClearAllTabs(term);
    break;

  case ';': /* Slave print set */
    c332->pCharHandler = SlavePrintSet;
    break;

  case '<': /* TODO: Send key override */
            /* Format: ESC < char */
            /* char '1' = Send page (24 lines x 80 cols) */
    break;  /* char '2' = Send line (Cursor line x 80 cols) */

  case '=': /* TODO: Send line or page (per send key) */
    break;

  case '@': /* Position cursor */
    c332->pCharHandler = EscCursorAt;
    break;

  case 'A':
    c332CursorUp(term);
    break;

  case 'B':
    c332CursorDown(term);
    break;

  case 'C':
    c332CursorRight(term);
    break;

  case 'D':
    c332CursorLeft(term);
    break;

  case 'E': /* Thread run remote program */
    c332->pCharHandler = GetDOSCommand;
    break;

  case 'F': /* Load function keys */
    c332->pCharHandler = FunctionKeyorLabelLoad;
    break;

  case 'H': /* Cursor home */
    c332CursorAt(term, 0, 0);
    break;

  case 'I': /* Clear to EOL */
    ClearToEOL(term);
    break;

  case 'J': /* Clear unprotected */
    ClearUnprotected(term);
    break;

  case 'K': /* Reset screen */
    c332Reset(term);
    break;

  case 'L': /* Insert a line */
    c332InsertLine(term, c332->cursor_y);
    break;

  case 'M': /* Delete line at cursor position */
    c332DeleteLine(term, c332->cursor_y);
    break;

  case 'N': /* Insert character */
    c332InsertChar(term);
    break; /* Format: ESC N char */

  case 'O': /* Delete character */
    c332DeleteChar(term);
    break;

  case 'S': /* Thread file xfer */
    ThreadUpload(ports[0]);
    break;

  case 'T': /* Thread file xfer */
    ThreadDownload(ports[0]);
    break;

  case 'X': /* Set cursor row */
    c332->pCharHandler = c332SetCursorRow;
    break;

  case 'Y': /* Set cursor column */
    c332->pCharHandler = c332SetCursorCol;
    break;

  case 'Z': /* Cursor position report */
    if (!slave_port_online)
    {
      SerialWrite(ports[0], c332->cursor_y + 32);
      SerialWrite(ports[0], c332->cursor_x + 32);
    }
    break;

  case '[': /* Various Routines to configure terminal */
    c332->pCharHandler = c332CollectArgs;
    break;

  case 'b':
    c332->stat_bar = TRUE;
    c332->stat_bar_changed = TRUE;
    c332->stat_string = TRUE;
    break;

  case 'c':
    ReleaseStatusBar(term);
    break;

  case 'd':
    c332->pCharHandler = DepositStatusMessage;
    c332->stat_message[0] = 0;
    break;

  case 'e': /* Set time */
    ThrowAwayChars(term, 4);
    c332->pCharHandler = ThrowAwayChars;
    break;

  case 'h': /* TODO: Erase to end of screen */
    break;

  case 'l': /* TODO: Switch to local mode */
    break;

  case 'r': /* TODO: Close coupled XMODEM upload */
    break;

  case 'y':                 /* Unknown: VLINK sends ESC back when it */
    if (!slave_port_online) /* receives this escape sequence */
      SerialWrite(ports[0], 27);
    break;
  }
}

/******************************************************************************\

 Routine: c332EvalAttrib

    Pass:

  Return: Nothing

\******************************************************************************/

BYTE c332EvalAttrib(term_obj *term, BYTE ch)
{
  c332_obj *c332 = term->emulator;
  BYTE foreground = c332->normal_fore;
  BYTE background = c332->normal_back;

  if (ch & 4)
  {
    foreground = c332->half_intens_fore;
    background = c332->half_intens_back;
  }

  if (ch & 8)
  {
    foreground = c332->inverse_fore;
    background = c332->inverse_back;
  }

  if ((ch & 12) == 12)
  {
    foreground = c332->half_inverse_fore;
    background = c332->half_inverse_back;
  }

  if (ch & 64)
    background = background + 8;

  if (ch & 16)
  {
    background = c332->normal_back;
    foreground = c332->normal_back;
  }

  return (background << 4) | foreground;
}

/******************************************************************************\

 Routine: InterpretControlCh

    Pass:

  Return: Nothing

\******************************************************************************/

void InterpretControlCh(term_obj *term, int ch)
{
  c332_obj *c332 = term->emulator;

  switch (ch)
  {
  case 7: /* Bell */
    if (sound_on)
      DoNoteOnce(C4, 10);
    break;

  case 8: /* Backspace */
    c332CursorLeft(term);
    break;

  case 9: /* Tab */
  {
    int x = c332->cursor_x + 1;
    if (x > 79)
      x = 0;

    while ((c332->attribbuff[c332->attribpos] == 0 || c332->attribbuff[c332->attribpos] & 0x02) && c332->attribpos < 1918 && c332->tab_stops[x] == FALSE)
    {
      c332->attribpos++;
      x++;
      if (x > 79)
        x = 0;
    }
    c332CursorAt(term, c332->attribpos / 80, x /*c332->attribpos % 80 + 1*/);
    break;
  }

  case 10: /* Line feed */
    if (c332->cursor_y < 23)
    {
      c332->cursor_loc = c332->cursor_loc + 160;
      c332->attribpos += 80;
      c332->cursor_y++;
    }
    else
    {
      c332ScrollScreenUp(term, 0, 23);
      c332->cursor_loc = screen_base + 3680;
      c332->attribpos = 1840;
    }
    c332UpdateCursorPos(term);
    break;

  case 13: /* Carriage return */
    c332->cursor_x = 0;
    c332->cursor_loc = screen_base + c332->cursor_y * 160;
    c332->attribpos = c332->cursor_y * 80;
    c332UpdateCursorPos(term);
    break;

  case 14: /* Turn on graphics mode */
    c332->graph_on = TRUE;
    break;

  case 15: /* Turn off graphics mode */
    c332->graph_on = FALSE;
    break;

  case 27: /* Escape sequence follows */
    c332->pCharHandler = c332InterpretEscape;
    break;

  default:
    c332ShowChar(term, ch);
  }
}

/******************************************************************************\

 Routine: c332CursorUp

    Pass:

  Return: Nothing

\******************************************************************************/

void c332CursorUp(term_obj *term)
{
  c332_obj *c332 = term->emulator;

  if (c332->cursor_y)
    c332CursorAt(term, c332->cursor_y - 1, c332->cursor_x);
  else
    c332CursorAt(term, 23, c332->cursor_x);
}

/******************************************************************************\

 Routine: c332CursorDown

    Pass:

  Return: Nothing

\******************************************************************************/

void c332CursorDown(term_obj *term)
{
  c332_obj *c332 = term->emulator;

  if (c332->cursor_y < 23)
    c332CursorAt(term, c332->cursor_y + 1, c332->cursor_x);
  else
    c332CursorAt(term, 0, c332->cursor_x);
}

/******************************************************************************\

 Routine: c332CursorLeft

    Pass:

  Return: Nothing

\******************************************************************************/

void c332CursorLeft(term_obj *term)
{
  c332_obj *c332 = term->emulator;

  if (c332->cursor_loc > screen_base)
    ;
  {
    c332->cursor_x--;
    c332->cursor_loc -= 2;
    c332->attribpos--;

    if (c332->cursor_x < 0)
    {
      c332->cursor_x = 79;
      c332->cursor_y--;
    }
    c332UpdateCursorPos(term);
  }
}

/******************************************************************************\

 Routine: c332CursorRight

    Pass:

  Return: Nothing

\******************************************************************************/

void c332CursorRight(term_obj *term)
{
  c332_obj *c332 = term->emulator;

  c332->cursor_x++;
  c332->cursor_loc += 2;
  c332->attribpos++;

  if (c332->cursor_x > 79)
  {
    c332->cursor_x = 0;
    c332CursorDown(term);
  }
  c332UpdateCursorPos(term);
}

/******************************************************************************\

 Routine: c332ShowChar

    Pass:

  Return: Nothing

\******************************************************************************/

void c332ShowChar(term_obj *term, int ch)
{
  c332_obj *c332 = term->emulator;

  if (c332->graph_on == TRUE)
    ch = graphchars[ch & 31];
  if (c332->attribbuff[c332->attribpos])
    DeleteAttrib(term, c332->attribpos);
  *c332->cursor_loc = (BYTE)ch;

  c332->cursor_loc += 2;
  c332->attribpos++;

  if (++c332->cursor_x > 79)
  {
    c332->cursor_x = 0;
    if (c332->cursor_y < 23)
      c332->cursor_y++;
    else
    {
      c332DeleteLine(term, 0);
      c332->cursor_loc = screen_base + 3680;
      c332->attribpos = 1840;
    }
  }
  c332UpdateCursorPos(term);
}

/******************************************************************************\

 Routine: c332OutChar

    Pass:

  Return: Nothing

\******************************************************************************/

void c332OutChar(term_obj *term, BYTE ch)
{
  if (ch < 32)
    InterpretControlCh(term, ch);
  else
    c332ShowChar(term, ch);
}

/******************************************************************************\

 Routine: c332CharHandler

    Pass:

  Return: Nothing

\******************************************************************************/

void c332CharHandler(term_obj *term, BYTE ch)
{
  c332_obj *c332 = term->emulator;

  ch = ch & (BYTE)0x7f;
  (*c332->pCharHandler)(term, ch);
}
