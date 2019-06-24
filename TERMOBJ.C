#include <stdlib.h>
#include <string.h>

#include "..\common\include\types.h"
#include "..\common\include\debug.h"
#include "..\common\include\doublell.h"
#include "..\common\include\intrface.h"
#include "..\common\include\window.h"
#include "..\common\include\bqueue.h"
#include "..\common\include\jscser.h"
#include "..\common\include\modem.h"
#include "..\common\include\mem.h"
#include "include\termobj.h"
#include "include\ilink.h"

extern s_com_port *ports[2];

void DefCharHandler(term_obj *term, BYTE ch)
{
  WindowPrintChar(term->window, ch);
}

void DefShowStatus(term_obj *term)
{
}

BOOLN DefKeyboardLocked(term_obj *term)
{
  return FALSE;
}

BOOLN DefEvaluateKey(term_obj *term, WORD key)
{
  BOOLN evaluated;
  s_fkey_def *fkeydef;

  evaluated = FALSE;

  if ((fkeydef = DoublyLinkedListGetItemWKeyOf(term->fn_keys->fn_key_defs, key)) != NULL)
  {
    SendStringToModem(ports[0], fkeydef->definition, FALSE);
    evaluated = TRUE;
  }
  return evaluated;
}

BOOLN DefStatBarChanged(term_obj *term)
{
  return FALSE;
}

BOOLN DefStatBar(term_obj *term)
{
  return FALSE;
}

void DefDisplayStatusMessage(term_obj *term)
{
}

void DefSaveTerminalSetup(term_obj *term)
{
}

void DefReleaseStatusBar(term_obj *term)
{
}

void DefAddConfigMenuItems(term_obj *term, s_menu *menu, int row)
{
}

void DefCopyScreenToSlave(term_obj *term)
{
}

void DefSetScreenBase(term_obj *term)
{
}

void DefSaveTerm(term_obj *term)
{
  WindowSave(term->window);
}

void DefRestoreTerm(term_obj *term)
{
  WindowRestore(term->window);
}

void DefDestroyTerm(term_obj *term)
{
  if (term->window != NULL)
    DestroyWindow(term->window);
  free(term->translation_table_p);
  DestroyFNKeyTable(&term->fn_keys);
  free(term->emulator);
  free(term);
}

term_obj *NewTerm(BOOLN default_window, char *translation_table, char *name, char *fn_key_filename)
{
  term_obj *new_term;

  new_term = malloc(sizeof(term_obj));
  if (new_term != NULL)
  {
    new_term->CharHandler = DefCharHandler;
    new_term->ShowStatus = DefShowStatus;
    new_term->KeyBoardLocked = DefKeyboardLocked;
    new_term->EvaluateKey = DefEvaluateKey;
    new_term->StatBarChanged = DefStatBarChanged;
    new_term->StatBar = DefStatBar;
    new_term->DisplayStatusMessage = DefDisplayStatusMessage;
    new_term->SaveTerminalSetup = DefSaveTerminalSetup;
    new_term->ReleaseStatusBar = DefReleaseStatusBar;
    new_term->AddConfigMenuItems = DefAddConfigMenuItems;
    new_term->CopyScreenToSlave = DefCopyScreenToSlave;
    new_term->SetScreenBase = DefSetScreenBase;
    new_term->SaveTerm = DefSaveTerm;
    new_term->RestoreTerm = DefRestoreTerm;
    new_term->EndTerm = DefDestroyTerm;
    new_term->fn_keys = NULL;
    if (strlen(fn_key_filename) > 0)
      LoadFNKeyTable(fn_key_filename, &new_term->fn_keys);
    new_term->translation_table_p = LoadTranslationTable(translation_table);
    new_term->name = name;
    new_term->ClearScreenString = "";
    new_term->num_menu_items = 0;
    if (default_window == TRUE)
      new_term->window = CreateWindow("", 0, 0, 24, 80, 0x07, CLEAR);
    else
      new_term->window = NULL;
    new_term->save_win = NULL;
    new_term->emulator = NULL;
  }
  return new_term;
}
