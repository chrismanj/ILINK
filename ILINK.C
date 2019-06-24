/*
   Version 6.23 - Changed buffer sizes to 4096 instead of 2048
   Version 6.24 - Fixed bug in c332e mudule. When a function key was pressed,
                  it was calling SendStringToModem and passing 0 as the port
                  instead of a port handle.
   Version 6.25 - Made Xmodem send routine transmit an XON char before
                  starting. It seems some receivers just transmit the
                  starting "C" on time then wait for a response, instead
                  of transmitting a "C" every few seconds until a response
                  is received.
   Version 6.26 - Fixed JSCSER to set the interrupt pointer back to NULL when
                  a port is closed.
                  Fixed JSCSER to not enable the THRE interrupt when a serial
                  port is opened, this caused problems on some machines.
   Version 6.27 - Fixed JSCSER to only try 1024 times to clear the RBF/THR
                  registers in RestorePortState. If the port was invalid it
                  could cause an infinite loop
   Version 6.28 - C332 module was not keeping track of the protected line
                  attributes correctly. When an attribute with the protected
                  bit set was overwritten by another attribute without the
                  protected bit set, it would not decrement the protected_line
                  variable for that line. It was also incrementing the
                  protected line variable in c332EvalAttrib instead of
                  c332SetAttr. I fixed all this. Changed protected_line from
                  a an array of bools to an array of ints so it can keep
                  track of how many protected attributes are on a line.
   Version 6.29 - Made function keys display input data while working
   Version 6.30 - Added copy function key
   Version 6.31 - Added the ability to set the Thread timeout value
*/

#include <stdlib.h>
#include <bios.h>
#include <string.h>
#include <stdarg.h>
#include <conio.h>
#include <stdio.h>
#include <process.h>
#include <dos.h>
#include <ctype.h>

#include "..\common\include\debug.h"
#include "..\common\include\types.h"
#include "..\common\include\bqueue.h"
#include "..\common\include\jscser.h"
#include "..\common\include\video.h"
#include "..\common\include\chrgraph.h"
#include "..\common\include\jscio.h"
#include "..\common\include\doublell.h"
#include "..\common\include\intrface.h"
#include "..\common\include\fileio.h"
#include "..\common\include\jsctime.h"
#include "..\common\include\keybrd.h"
#include "..\common\include\modem.h"
#include "..\common\include\window.h"
#include "..\common\include\mem.h"
#include "..\filexfer\include\xmodem.h"
#include "..\filexfer\include\kermit.h"
#include "..\ilink\include\termobj.h"
#include "..\ilink\include\c332.h"
#include "..\ilink\include\vt52.h"
#include "..\ilink\include\vt100.h"
#include "..\ilink\include\c332e.h"
#include "..\ilink\include\ilink.h"
#include "..\ilink\include\ansi.h"
#include "..\ilink\include\pcnansi.h"

#define BOTROW 24
#define COLS 80
#define NUM_TERMS 6
#define HANGUP_TIME 400L

/* Menu choices for various menus */

s_choice_items colors[16] = {{"Black", 'B'}, {"Blue", 'u'}, {"Green", 'G'}, {"Cyan", 'C'}, {"Red", 'R'}, {"Magenta", 'M'}, {"Brown", 'o'}, {"White", 'W'}, {"Gray", 'a'}, {"Lt Blue", 't'}, {"Lt Green", 'e'}, {"Lt Cyan", 'L'}, {"Lt Red", 'd'}, {"Lt Magenta", 'n'}, {"Yellow", 'Y'}, {"Brt White", 'h'}};

s_choice_items com_ports[] = {{"COM1", '1'}, {"COM2", '2'}, {"COM3", '3'}, {"COM4", '4'}};

s_choice_items parities[] = {{"None", 'N'}, {"Odd", 'O'}, {"Even", 'E'}, {"Mark", 'M'}, {"Space", 'S'}};

s_choice_items bauds[] = {{"300", '3'}, {"1200", '2'}, {"2400", '4'}, {"9600", '6'}, {"19200", '9'}, {"38400", '8'}, {"57600", '7'}, {"115200", '1'}};

s_choice_items data_lengths[] = {{"5 bits", '5'}, {"6 bits", '6'}, {"7 bits", '7'}, {"8 bits", '8'}};

s_choice_items stop_bits[] = {{"1 bit", '1'}, {"2 bits", '2'}};

s_choice_items interrupts[] = {{"IRQ 0", '0'}, {"IRQ 1", '1'}, {"IRQ 2", '2'}, {"IRQ 3", '3'}, {"IRQ 4", '4'}, {"IRQ 5", '5'}, {"IRQ 6", '6'}, {"IRQ 7", '7'}};

s_choice_items protocols[] = {{"XModem", 'X'}, {"XModem 1K", '1'}, {"YModem", 'Y'}, {"ZModem", 'Z'}, {"Kermit", 'K'}};

s_choice_items exit_choices[] = {{"Yes", 'Y'}, {"No", 'N'}, {"Yes and Hang Up", 'H'}};

s_choice_items function_key_choices[] = {{"Load Function Keys", 'L'},
                                         {"Save Function Keys", 'S'},
                                         {"Add/Edit A Function Key", 'A'},
                                         {"Delete a Function Key", 'D'},
                                         {"Unload Function Keys", 'U'},
                                         {"Copy Function Key", 'C'}};

s_fkey_names fkey_names[] = {{"F1", 0x013b},
                             {"SH+F1", 0x093b},
                             {"CTL+F1", 0x213b},
                             {"ALT+F1", 0x113b},
                             {"F2", 0x013c},
                             {"SH+F2", 0x093c},
                             {"CTL+F2", 0x213c},
                             {"ALT+F2", 0x113c},
                             {"F3", 0x013d},
                             {"SH+F3", 0x093d},
                             {"CTL+F3", 0x213d},
                             {"ALT+F3", 0x113d},
                             {"F4", 0x013e},
                             {"SH+F4", 0x093e},
                             {"CTL+F4", 0x213e},
                             {"ALT+F4", 0x113e},
                             {"F5", 0x013f},
                             {"SH+F5", 0x093f},
                             {"CTL+F5", 0x213f},
                             {"ALT+F5", 0x113f},
                             {"F6", 0x0140},
                             {"SH+F6", 0x0940},
                             {"CTL+F6", 0x2140},
                             {"ALT+F6", 0x1140},
                             {"F7", 0x0141},
                             {"SH+F7", 0x0941},
                             {"CTL+F7", 0x2141},
                             {"ALT+F7", 0x1141},
                             {"F8", 0x0142},
                             {"SH+F8", 0x0942},
                             {"CTL+F8", 0x2142},
                             {"ALT+F8", 0x1142},
                             {"F9", 0x0143},
                             {"SH+F9", 0x0943},
                             {"CTL+F9", 0x2143},
                             {"ALT+F9", 0x1143},
                             {"F10", 0x0144},
                             {"SH+F10", 0x0944},
                             {"CTL+F10", 0x2144},
                             {"ALT+F10", 0x1144},
                             {"F11", 0x0157},
                             {"SH+F11", 0x0957},
                             {"CTL+F11", 0x2157},
                             {"ALT+F11", 0x1157},
                             {"F12", 0x0158},
                             {"SH+F12", 0x0958},
                             {"CTL+F12", 0x2158},
                             {"ALT+F12", 0x1158},
                             {"INS", 0x0552},
                             {"SH+INS", 0x0d52},
                             {"CTL+INS", 0x2552},
                             {"ALT+INS", 0x1552},
                             {"HOME", 0x0547},
                             {"SH+HOME", 0x0d47},
                             {"CTL+HOME", 0x2547},
                             {"ALT+HOME", 0x1547},
                             {"PGUP", 0x0549},
                             {"SH+PGUP", 0x0d49},
                             {"CTL+PGUP", 0x2549},
                             {"ALT+PGUP", 0x1549},
                             {"DEL", 0x047f},
                             {"SH+DEL", 0x0d53},
                             {"CTL+DEL", 0x2553},
                             {"ALT+DEL", 0x1553},
                             {"END", 0x054f},
                             {"SH+END", 0x0d4f},
                             {"CTL+END", 0x254f},
                             {"ALT+END", 0x154f},
                             {"PGDN", 0x0551},
                             {"SH+PGDN", 0x0d51},
                             {"CTL+PGDN", 0x2551},
                             {"ALT+PGDN", 0x1551},
                             {"UP", 0x0548},
                             {"SH+UP", 0x0d48},
                             {"CTL+UP", 0x2548},
                             {"ALT+UP", 0x1548},
                             {"DN", 0x0550},
                             {"SH+DN", 0x0d50},
                             {"CTL+DN", 0x2550},
                             {"ALT+DN", 0x1550},
                             {"LEFT", 0x054b},
                             {"SH+LEFT", 0x0d4b},
                             {"CTL+LEFT", 0x254b},
                             {"ALT+LEFT", 0x154b},
                             {"RIGHT", 0x054d},
                             {"SH+RIGHT", 0x0d4d},
                             {"CTL+RIGHT", 0x254d},
                             {"ALT+RIGHT", 0x154d}};

/* Various constant strings used throughout the program */

char colors_str[] = "Colors";
char config_filename[161] = "default.icf";
char config_header[] = "Intralink Config";
char slave_port_str[] = "Slave Port";
char master_port_str[] = "Master Port";
char com_port_setup_str[] = "Com Port Setup";
char null_str[] = "";
char init_str[] = "Init String";
char port_str[] = "Port.......";
char baud_str[] = "Baud.......";
char parity_str[] = "Parity.....";
char data_str[] = "Data.......";
char stop_str[] = "Stop.......";
char xon_xoff_str[] = "XON/XOFF...";
char rts_cts_str[] = "RTS/CTS....";
char always_init_str[] = "Always Init";
char drop_dtr_str[] = "Drop DTR...";
char slave_port_offline_str[] = "The slave port is now offline.";
#ifdef DEBUG
char port_stat_str[] = ":\n\rRBuff Count:\n\r  RBuff Max:\n\rSBuff Count:\n\r  SBuff Max:\n\r        DTR:\n\r        RTS:\n\r        CTS:\n\r        DSR:\n\r        DCD:\n\r OP BB Xoff:\n\r  OP BB CTS:\n\r IP BB Flow:\n\r    Old IER:\n\r    Old LCR:\n\r    Old MCR:\n\r     Old DL:\n\r    Old ISR:\n\r Old Int_En:";
#endif
s_com_port *ports[2];

/* Other constants */

int data_length_vals[] = {DATA_BITS_5, DATA_BITS_6, DATA_BITS_7, DATA_BITS_8};
int parity_vals[] = {PARITY_NONE, PARITY_ODD, PARITY_EVEN, PARITY_MARK, PARITY_SPACE};
int stop_bit_vals[] = {STOP_BITS_1, STOP_BITS_2};

/* Set default colors */

WORD status_fore = 0x0a;
WORD status_back = 0x04;
WORD status_attrib = 0x4a;
WORD menu_frame_fore = 0x00;
WORD menu_frame_back = 0x05;
WORD menu_text_fore = 0x02;
WORD menu_text_back = 0x04;
WORD menu_highlight_fore = 0x01;
WORD menu_highlight_back = 0x03;
WORD menu_hotkey_fore = 0x0e;
WORD menu_hotkey_back = 0x04;

/* Set default options */

BOOLN sound_on = TRUE;
BOOLN local_echo_on = FALSE;
BOOLN add_lf_on = FALSE;
BOOLN strip_high_bit_on = FALSE;
BOOLN slave_port_in_use = FALSE;
BOOLN slave_port_online = FALSE;
BOOLN master_port_online = FALSE;
BOOLN host_display_enabled = TRUE;
BOOLN log_file_open = FALSE;
long online_time;
char remote_menu_string[10] = "";
int remote_menu_str_len;
char remote_callback_str[26] = "";
BOOLN remote_callback_enabled = FALSE;
char phone_number[26] = "";
BOOLN XonXoffEnabled = TRUE;
long thread_timeout = 300l;

char master_init_string[81] = "^MATZ^M~~~";
int master_port = 1;
int master_data_length = 3;
int master_parity = 0;
int master_stop_bits = 0;
int master_baud = 4;
BOOLN master_xon_xoff = TRUE;
BOOLN master_rts_cts = FALSE;
BOOLN master_initialized = FALSE;
BOOLN master_always_init = FALSE;
BOOLN master_drop_dtr = FALSE;

char slave_init_string[81] = "^MATZ^M~~~";
int slave_port = 0;
int slave_data_length = 3;
int slave_parity = 0;
int slave_stop_bits = 0;
int slave_baud = 4;
BOOLN slave_xon_xoff = TRUE;
BOOLN slave_rts_cts = FALSE;
BOOLN slave_initialized = FALSE;
BOOLN slave_always_init = FALSE;
BOOLN slave_drop_dtr = FALSE;

char filename_string[161];
char log_file_string[161];
WORD com_add[4] = {0x3f8, 0x2f8, 0x3e8, 0x2e8}; /* Com port base addresses */
int port_int[4] = {4, 3, 4, 3};                 /* Com port interrupts */
FILE *log_file;
term_obj *term;
term_obj *pages[3] = {NULL, NULL, NULL};
WORD page = 0;
WORD *disp_buff;
int curr_emul = 1;

char *nextFNKeyChar = NULL;
long nextFKTimer = 0;
long FKTimeOut = 10L;

BOOLN page_drawn[3] = {FALSE, FALSE, FALSE};

char temp_string[256];
s_term_emul emulations[NUM_TERMS];
s_fkey_table *fn_keys = NULL;
union REGS inregs, outregs;

/*#define FROMFILE*/

#ifdef FROMFILE
FILE *debug_input_file;
#endif

/******************************************************************************\

  Routine: NotImplemented

 Function: Does absolutely nothing.

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

static void NotImplemented(void){};

/******************************************************************************\

  Routine: ShowStrOnStatusBar

 Function: Displays a string on the status bar at the specified column.

     Pass: The column (0 - [79 | 131]) where the string is to begin & a pointer
	   to the string.

   Return: Nothing

\******************************************************************************/

void ShowStrOnStatusBar(int col, const char *text)
{
  OutTextAt(BOTROW, col, text);
}

/******************************************************************************\

  Routine: ShowChOnStatusBar

 Function: Displays a character on the status bar at the specified column.

     Pass: The column (0 - [79 | 131]) where the character is to begin & the
	   character to be displayed.

   Return: Nothing

\******************************************************************************/

void ShowChOnStatusBar(int col, int ch)
{
  OutCharAt(BOTROW, col, ch);
}

/******************************************************************************\

  Routine: ShowSerialStatus

 Function: Displays the status of a serial port on the status bar.

	   Displays a 'C' if a carrier signal is present, ' ' if it is not.
	   In the next column an 'R' is displayed if the data set ready signal
	     is present, ' ' if it is not.
	   In the next column an 'S' is displayed if the clear to send signal
	     is present, ' ' of not.

     Pass: The handle of the serial port you want the status of and the column
	   the modem status should be displayed on.

   Return: Nothing

\******************************************************************************/

static void ShowSerialStatus(s_com_port *port, int column)
{
  if (!(*term->StatBar)(term))
  {
    ShowChOnStatusBar(column++, (char)(CarrierDetected(port) ? 'C' : ' '));
    ShowChOnStatusBar(column++, (char)(DataSetReady(port) ? 'R' : ' '));
    ShowChOnStatusBar(column, (char)(ClearToSend(port) ? 'S' : ' '));
  }
}

/******************************************************************************\

  Routine: ShowStatusMaster

 Function: Displays the status of the master serial port on the status bar.

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

static void ShowStatusMaster(void)
{
  if (master_initialized)
    ShowSerialStatus(ports[0], 0);
}

/******************************************************************************\

  Routine: ShowStatusSlave

 Function: Displays the status of the slave serial port on the status bar.

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

static void ShowStatusSlave(void)
{
  ShowSerialStatus(ports[1], 17);
}

/******************************************************************************\

  Routine: ClearStatusBar

 Function: Clears all text off of the status bar and resets the colors.

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

static void ClearStatusBar(void)
{
  SetTextAttrib(status_attrib);
  HCharC(BOTROW, 0, COLS, ' ');
}

/******************************************************************************\

  Routine: RefreshStatusBar

 Function: Redraw the status bar

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

void RefreshStatusBar(void)
{
  int x;
  int vbarpos[] = {3, 10, 14, 16, 20, 27, 31, 33, 42, 51, 60};

  if (!(*term->StatBar)(term))
  {
    ClearStatusBar();
    for (x = 0; x < 11; x++)
      ShowChOnStatusBar(vbarpos[x], '³');
    ShowStatusMaster();
    ShowStrOnStatusBar(4, bauds[master_baud].text);
    ShowChOnStatusBar(11, *data_lengths[master_data_length].text);
    ShowChOnStatusBar(12, *parities[master_parity].text);
    ShowChOnStatusBar(13, *stop_bits[master_stop_bits].text);
    if (slave_initialized == TRUE)
    {
      ShowStatusSlave();
      ShowStrOnStatusBar(21, bauds[slave_baud].text);
      ShowChOnStatusBar(28, *data_lengths[slave_data_length].text);
      ShowChOnStatusBar(29, *parities[slave_parity].text);
      ShowChOnStatusBar(30, *stop_bits[slave_stop_bits].text);
    }
    ShowStrOnStatusBar(52, term->name);
    ShowStrOnStatusBar(61, "Pg");
    ShowChOnStatusBar(64, '1' + page);
    (*term->ShowStatus)(term);
  }
}

/******************************************************************************\

  Routine: OpenSerialErrChk

 Function:

     Pass: Same parameters as OpenSerial

   Return: 0 = success, anything else means error

\******************************************************************************/

int OpenSerialErrChk(WORD port, WORD address, int irq, long baud,
                     int data_length, int parity, int stop_bits)
{
  int retval = 0;

  if ((ports[port] = OpenSerial(address, irq, baud, data_length_vals[data_length], parity_vals[parity], stop_bit_vals[stop_bits], 4096, 4096)) == NULL)
    ErrorBox(port == 0 ? "Unable to configure master port!" : "Unable to configure slave port!");
  if (ports[port] != NULL)
  {
    if (port == 0)
      master_initialized = TRUE;
    else
      slave_initialized = TRUE;
  }
  else
  {
    retval = 1;
    if (port == 0)
      master_initialized = FALSE;
    else
      slave_initialized = FALSE;
  }
  return retval;
}

/******************************************************************************\

  Routine: CloseSerialErrChk

 Function:

     Pass: Same parameters as OpenSerial

   Return: 0 = success, anything else means error

\******************************************************************************/

void CloseSerialErrChk(WORD port)
{
  if (port == 0 && master_initialized)
  {
    CloseSerial(&ports[0], master_drop_dtr);
    master_initialized = FALSE;
  }
  if (port == 1 && slave_initialized)
  {
    CloseSerial(&ports[1], slave_drop_dtr);
    slave_initialized = FALSE;
  }
}

/******************************************************************************\

  Routine: CreateFNKeyTable

 Function:

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

s_fkey_table *CreateFNKeyTable(char *name)
{
  s_fkey_table *fkeys;

  if ((fkeys = malloc(sizeof(s_fkey_table))) != NULL)
  {
    strcpy(fkeys->name, name);
    if ((fkeys->fn_key_defs = DLLCreate()) == NULL)
    {
      free(fkeys);
      fkeys = NULL;
    }
  }
  return fkeys;
}

/******************************************************************************\

  Routine: DestroyFNKeyTable

 Function:

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

void DestroyFNKeyTable(s_fkey_table **fkeys)
{
  s_fkey_def *fkeydef;

  if (*fkeys != NULL)
  {
    fkeydef = DLLGetFirstItem((*fkeys)->fn_key_defs);
    while (fkeydef != NULL)
    {
      free(fkeydef->definition);
      free(fkeydef);
      fkeydef = DLLGetNextItem((*fkeys)->fn_key_defs);
    }
    DLLDestroy((*fkeys)->fn_key_defs);
    free(*fkeys);
    *fkeys = NULL;
  }
}

/******************************************************************************\

  Routine: LoadFNKeyTable

 Function:

     Pass:

   Return:

\******************************************************************************/

void LoadFNKeyTable(char *filename, s_fkey_table **p_fkeys_p)
{
  FILE *fnkey_file;
  s_fkey_def *fkeydef;
  s_fkey_table *fkeys;
  int length;
  char filespec[13] = "";
  BOOLN error = FALSE;

  fkeys = *p_fkeys_p;
  DestroyFNKeyTable(&fkeys);
  strcat(filespec, filename);
  strcat(filespec, ".fnk");
  if ((fkeys = CreateFNKeyTable(filename)) != NULL)
  {
    if ((fnkey_file = fopen(filespec, "rb")) != NULL)
    {
      SetCurrentFile(fnkey_file);
      while (!feof(fnkey_file))
      {
        length = ReadWordFromFile();
        if (!feof(fnkey_file))
        {
          if ((fkeydef = malloc(sizeof(s_fkey_def))) != NULL)
          {
            if ((fkeydef->definition = malloc((size_t)length)) != NULL)
            {
              fkeydef->key = ReadWordFromFile();
              ReadStringFromFile(fkeydef->definition);
              DLLAddItem(fkeys->fn_key_defs, fkeydef, fkeydef->key);
            }
            else
              error = TRUE;
          }
          else
            error = TRUE;
        }
      }
      fclose(fnkey_file);
    }
    else
      error = TRUE;
  }
  if (error == TRUE)
    MessageBoxPause("Error loading function keys");
  *p_fkeys_p = fkeys;
}

/******************************************************************************\

  Routine: SaveFNKeyTable

 Function:

     Pass:

   Return:

\******************************************************************************/

void SaveFNKeyTable(s_fkey_table *fkeys)
{
  FILE *fnkey_file;
  char filespec[13] = "";
  s_fkey_def *fkeydef;

  strcat(filespec, fkeys->name);
  strcat(filespec, ".fnk");
  GetInput(filespec, "Enter function key filename:", 13);
  if ((fnkey_file = fopen(filespec, "wb")) != NULL)
  {
    SetCurrentFile(fnkey_file);
    fkeydef = DLLGetFirstItem(fkeys->fn_key_defs);
    while (fkeydef != NULL)
    {
      WriteWordToFile(strlen(fkeydef->definition) + 1);
      WriteWordToFile(fkeydef->key);
      WriteStringToFile(fkeydef->definition);
      fkeydef = DLLGetNextItem(fkeys->fn_key_defs);
    }
    fclose(fnkey_file);
  }
  MessageBoxPause("Function Keys Saved");
}

/******************************************************************************\

  Routine: LoadTranslationTable

 Function:

     Pass:

   Return:

\******************************************************************************/

s_xlate_table *LoadTranslationTable(char *filename)
{
  FILE *xlate_file;
  s_xlate_table *xtable = NULL;
  int x;
  char filespec[13] = "";

  strcat(filespec, filename);
  strcat(filespec, ".tlt");
  if ((xtable = malloc(sizeof(s_xlate_table))) != NULL)
  {
    if ((xlate_file = fopen(filespec, "rb")) != NULL)
    {
      SetCurrentFile(xlate_file);
      strcpy(xtable->name, filename);
      for (x = 0; x < 256; x++)
        xtable->table[x] = ReadByteFromFile();
      fclose(xlate_file);
    }
    else
    {
      strcpy(xtable->name, "default");
      for (x = 0; x < 256; x++)
        xtable->table[x] = (BYTE)x;
      if (strlen(filename) > 0)
        MessageBoxPause("Error Loading Translation Table.");
    }
  }
  return xtable;
}

/******************************************************************************\

  Routine: SaveTranslationTable

 Function:

     Pass:

   Return:

\******************************************************************************/

BOOLN SaveTranslationTable(s_xlate_table *xtable)
{
  FILE *xlate_file;
  int x;
  char filespec[13] = "";

  strcat(filespec, xtable->name);
  strcat(filespec, ".tlt");
  if ((xlate_file = fopen(filespec, "wb")) != NULL)
  {
    SetCurrentFile(xlate_file);
    for (x = 0; x < 256; x++)
      WriteBytetoFile(xtable->table[x]);
    fclose(xlate_file);
    return TRUE;
  }
  else
    return FALSE;
}

/******************************************************************************\

  Routine: ShowTitleScreen

 Function: Display the title screen for 5 seconds or until the user presses a
	   key.

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

static void ShowTitleScreen(void)
{
  long timer;

  DrawBoxFilled(0, 0, BOTROW + 1, COLS, ' ');
  OutTextCentered(10, "Intralink v6.31");
  OutTextCentered(12, "Copyright(C) 1997, 1998, 1999, Intraspect");
  OutTextCentered(22, "<Press your favorite key to continue>");
  timer = StartTimer();
  while (!KeyInBuffer() && TimerValue(timer) < 500)
    ;
  if (KeyInBuffer())
    GetAKey();
}

/******************************************************************************\

  Routine: GetFKeyName

 Function:

     Pass:

   Return:

\******************************************************************************/

char *GetFKeyName(WORD key)
{
  int x;
  char *name = NULL;

  for (x = 0; x < 88 && name == NULL; x++)
    if (key == fkey_names[x].key)
      name = fkey_names[x].name;
  return name;
}

/******************************************************************************\

  Routine: DisplayFunctionKey

 Function:

     Pass:

   Return:

\******************************************************************************/

char *DisplayFunctionKey(void *fkeydef)
{
  char *name = GetFKeyName(((s_fkey_def *)fkeydef)->key);

  temp_string[0] = 0;
  if (((s_fkey_def *)fkeydef)->key != 0)
    strcat(temp_string, name);
  while (strlen(temp_string) < 10)
    strcat(temp_string, " ");
  strcat(temp_string, ((s_fkey_def *)fkeydef)->definition);
  temp_string[78] = 0;
  return temp_string;
}

/*Here*/

static void XferOptDB(void)
{
  s_dialog_box *db = CreateDialogBox("Xfer Options", 10, 25, 5, 30);

  ltoa(thread_timeout, temp_string, 10);
  AddDBItem(db, STRING_GADGET, 'T', 2, 2, "Thread Timeout Value", 2, 23, 4, temp_string, 4);
  DoDialogBox(db);
  thread_timeout = atol(temp_string);
  DestroyDialogBox(db);
}

/******************************************************************************\

  Routine: FunctionKeyDB

 Function:

     Pass:

   Return:

\******************************************************************************/

static void FunctionKeyDB(void)
{
  int choice = 0;

  while (ChoiceMenu("Function Keys", 6, 8, 26, 10, 27, &function_key_choices[0], &choice) != -1)
  {
    switch (choice)
    {
    case 0: /* Load */
      temp_string[0] = 0;
      GetInput(temp_string, "Enter function key filename:", 13);
      LoadFNKeyTable(temp_string, &fn_keys);
      break;

    case 1: /* Save */
      SaveFNKeyTable(fn_keys);
      break;

    case 2: /* Add/Edit */
    {
      WORD key = 0;
      WORD *box;
      s_fkey_def *fkeydef;
      s_fkey_def *add_fkeydef;
      BOOLN done = FALSE;
      BOOLN got_a_key;

      if (fn_keys == NULL)
        fn_keys = CreateFNKeyTable("default");
      while (done == FALSE)
      {
        if (DoublyLinkedListGetItemWKeyOf(fn_keys->fn_key_defs, 0) == NULL)
          if ((add_fkeydef = malloc(sizeof(s_fkey_def))) != NULL)
          {
            if ((add_fkeydef->definition = malloc((size_t)8)) != NULL)
            {
              add_fkeydef->key = 0;
              strcpy(add_fkeydef->definition, "Add New");
            }
            DLLAddItem(fn_keys->fn_key_defs, add_fkeydef, add_fkeydef->key);
          }
        fkeydef = ListBox("Add/Edit Function Keys", 0, 0, 25, 80,
                          fn_keys->fn_key_defs, DisplayFunctionKey);

        if (fkeydef != NULL)
        {
          box = SaveAndDrawBox(11, 0, 5, 80, ' ');
          if (fkeydef->key == 0)
          {
            OutTextAt(13, 2, "Press the key you wish to define.");
            got_a_key = FALSE;
            while (got_a_key == FALSE)
            {
              while ((key = GetAKey()) == 0)
                ;
              if ((GetFKeyName(key) != NULL && DoublyLinkedListGetItemWKeyOf(fn_keys->fn_key_defs, key) == NULL) || key == KEY_ESC)
                got_a_key = TRUE;
            }
            if (key != KEY_ESC)
            {
              fkeydef->key = key;
              fkeydef->definition[0] = 0;
            }
            HChar(13, 2, 33, ' ');
          }
          if (key != KEY_ESC)
          {
            temp_string[0] = 0;
            strcpy(temp_string, fkeydef->definition);
            free(fkeydef->definition);
            OutTextAt(13, 2, GetFKeyName(fkeydef->key));
            Input(13, 12, 66, 255, temp_string, temp_string, "", "");
            fkeydef->definition = malloc((size_t)strlen(temp_string));
            fkeydef->definition[0] = 0;
            strcpy(fkeydef->definition, temp_string);
            DoublyLinkedListResetKey(fn_keys->fn_key_defs, fkeydef->key);
          }
          RestoreRect(box);
        }
        else
          done = TRUE;
      }
      fkeydef = DoublyLinkedListGetItemWKeyOf(fn_keys->fn_key_defs, 0);
      free(fkeydef->definition);
      free(fkeydef);
      DoublyLinkedListDeleteElement(fn_keys->fn_key_defs);
    }
    break;

    case 3: /* Delete */
    {
      BOOLN done = FALSE;
      s_fkey_def *fkeydef;

      if (fn_keys != NULL)
      {
        while (done == FALSE)
        {
          fkeydef = ListBox("Delete Function Keys", 0, 0, 25, 80,
                            fn_keys->fn_key_defs, DisplayFunctionKey);
          if (fkeydef != NULL)
          {
            free(fkeydef->definition);
            free(fkeydef);
            DoublyLinkedListDeleteElement(fn_keys->fn_key_defs);
          }
          else
            done = TRUE;
        }
      }
    }
    break;

    case 4:
      DestroyFNKeyTable(&fn_keys);
      break;

    case 5: /* Copy */
    {
      WORD *box;
      BOOLN got_a_key;
      WORD key = 0;
      s_fkey_def *srcFKeyDef;
      s_fkey_def *destFKeyDef;

      box = SaveAndDrawBox(11, 0, 5, 80, ' ');
      OutTextAt(13, 2, "Press the key you wish to copy.");
      got_a_key = FALSE;
      while (got_a_key == FALSE)
      {
        while ((key = GetAKey()) == 0)
          ;
        if (DoublyLinkedListGetItemWKeyOf(fn_keys->fn_key_defs, key) != NULL || key == KEY_ESC)
          got_a_key = TRUE;
      }
      if (key != KEY_ESC)
      {
        srcFKeyDef = DoublyLinkedListGetItemWKeyOf(fn_keys->fn_key_defs, key);
        OutTextAt(13, 2, "Press the key you wish to copy it to.");
        got_a_key = FALSE;
        while (got_a_key == FALSE)
        {
          while ((key = GetAKey()) == 0)
            ;
          if ((GetFKeyName(key) != NULL && DoublyLinkedListGetItemWKeyOf(fn_keys->fn_key_defs, key) == NULL) || key == KEY_ESC)
            got_a_key = TRUE;
        }
        if (key != KEY_ESC)
        {
          if ((destFKeyDef = malloc(sizeof(s_fkey_def))) != NULL)
          {
            if ((destFKeyDef->definition = malloc((size_t)strlen(srcFKeyDef->definition))) != NULL)
            {
              destFKeyDef->key = key;
              strcpy(destFKeyDef->definition, srcFKeyDef->definition);
            }
            DLLAddItem(fn_keys->fn_key_defs, destFKeyDef, destFKeyDef->key);
          }
        }
      }
      RestoreRect(box);
    }
    break;
    }
  }
}

/******************************************************************************\

  Routine: GenOptsDB

 Function: Display a dialog box which allows configure general options

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

static void GenOptsDB(void)
{
  BOOLN old_slave_on = slave_port_in_use;
  s_dialog_box *db = CreateDialogBox("General Options", 5, 22, 11, 36);

  AddDBItem(db, ON_OFF_GADGET, 'S', 2, 2, "Sound", 2, 25, &sound_on);
  AddDBItem(db, ON_OFF_GADGET, 'L', 3, 2, "Local Echo", 3, 25, &local_echo_on);
  AddDBItem(db, ON_OFF_GADGET, 'A', 4, 2, "Add LF After CR", 4, 25, &add_lf_on);
  AddDBItem(db, ON_OFF_GADGET, 'H', 5, 2, "Strip High Bit", 5, 25, &strip_high_bit_on);
  AddDBItem(db, ON_OFF_GADGET, 'v', 6, 2, slave_port_str, 6, 25, &slave_port_in_use);
  AddDBItem(db, STRING_GADGET, 'R', 7, 2, "Remote Menu Activation", 7, 25, 9, remote_menu_string, 9);
  DoDialogBox(db);
  DestroyDialogBox(db);
  remote_menu_str_len = strlen(remote_menu_string) - 1;
  if (old_slave_on != slave_port_in_use)
    ToggleSlave();
}

/******************************************************************************\

  Routine: ScreenColorDB

 Function: Display a dialog box which allows the user to change the screen
	   colors.

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

static void ScreenColorDB(void)
{
  s_dialog_box *db = CreateDialogBox("Screen Colors", 5, 18, 14, 41);

  AddDBItem(db, CHOICE_GADGET, 'S', 2, 2, "Status Bar Foreground.....", 2, 29,
            &colors_str, 16, 2, 33, 20, 14, &colors[0], &status_fore);
  AddDBItem(db, CHOICE_GADGET, 't', 3, 2, "Status Bar Background.....", 3, 29,
            &colors_str, 8, 6, 33, 12, 14, &colors[0], &status_back);
  AddDBItem(db, CHOICE_GADGET, 'M', 4, 2, "Menu Frame Foreground.....", 4, 29,
            &colors_str, 16, 2, 33, 20, 14, &colors[0], &menu_frame_fore);
  AddDBItem(db, CHOICE_GADGET, 'e', 5, 2, "Menu Frame Background.....", 5, 29,
            &colors_str, 8, 6, 33, 12, 14, &colors[0], &menu_frame_back);
  AddDBItem(db, CHOICE_GADGET, 'F', 6, 2, "Menu Text Foreground......", 6, 29,
            &colors_str, 16, 2, 33, 20, 14, &colors[0], &menu_text_fore);
  AddDBItem(db, CHOICE_GADGET, 'x', 7, 2, "Menu Text Background......", 7, 29,
            &colors_str, 8, 6, 33, 12, 14, &colors[0], &menu_text_back);
  AddDBItem(db, CHOICE_GADGET, 'H', 8, 2, "Menu Highlight Foreground.", 8, 29,
            &colors_str, 16, 2, 33, 20, 14, &colors[0], &menu_highlight_fore);
  AddDBItem(db, CHOICE_GADGET, 'i', 9, 2, "Menu Highlight Background.", 9, 29,
            &colors_str, 8, 6, 33, 12, 14, &colors[0], &menu_highlight_back);

  AddDBItem(db, CHOICE_GADGET, 'o', 10, 2, "Menu Hotkey Foreground....", 10, 29,
            &colors_str, 16, 2, 33, 20, 14, &colors[0], &menu_hotkey_fore);
  AddDBItem(db, CHOICE_GADGET, 'k', 11, 2, "Menu Hotkey Background....", 11, 29,
            &colors_str, 8, 6, 33, 12, 14, &colors[0], &menu_hotkey_back);

  DoDialogBox(db);
  DestroyDialogBox(db);

  SetFrameAttrib(menu_frame_back << 4 | menu_frame_fore);
  SetTextAttrib(menu_text_back << 4 | menu_text_fore);
  SetFillAttrib(menu_text_back << 4 | menu_text_fore);
  SetMenuColors(menu_highlight_back << 4 | menu_highlight_fore, menu_hotkey_back << 4 | menu_hotkey_fore);
  SetFrameAttrib(menu_frame_back << 4 | menu_frame_fore);
  SetFillAttrib(menu_text_back << 4 | menu_text_fore);
  status_attrib = status_back << 4 | status_fore;

  /*
  status_attrib = status_back << 4 | status_fore;
  SetFrameAttrib(menu_frame_back << 4 | menu_frame_fore);
  SetFillAttrib(menu_text_back << 4 | menu_text_fore);
  SetMenuColors(menu_highlight_attrib, menu_hotkey_attrib);
  */
  RefreshStatusBar();
}

/******************************************************************************\

  Routine: ToggleSlave

 Function: Toggle the slave port on or off.

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

static void ToggleSlave(void)
{
  if (slave_port_in_use == TRUE)
    OpenSerialErrChk(1, com_add[slave_port], port_int[slave_port], atol(bauds[slave_baud].text), slave_data_length, slave_parity, slave_stop_bits);
  else
    CloseSerialErrChk(1);
  RefreshStatusBar();
}

/******************************************************************************\

  Routine: MasterCommParamDB

 Function: Display a dialog box allowing the user to change the master
	   communications port parameters.

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

void MasterCommParamDB(void)
{
  int old_master_port = master_port;
  s_dialog_box *db = CreateDialogBox("Master Port Configuration", 4, 3, 14, 74);

  AddDBItem(db, STRING_GADGET, 'I', 2, 2, &init_str[0], 2, 14, 58,
            master_init_string, 80);
  AddDBItem(db, CHOICE_GADGET, 'P', 3, 2, port_str, 3, 14,
            "COM Ports", 4, 6, 32, 8, 17, com_ports, &master_port);
  AddDBItem(db, CHOICE_GADGET, 'B', 4, 2, baud_str, 4, 14,
            "Baud Settings", 8, 7, 32, 12, 21, bauds, &master_baud);
  AddDBItem(db, CHOICE_GADGET, 'a', 5, 2, parity_str,
            5, 14, "Parity Settings", 5, 8, 32, 9, 23, parities, &master_parity);
  AddDBItem(db, CHOICE_GADGET, 'D', 6, 2, data_str, 6, 14,
            "Data Settings", 4, 9, 32, 8, 21, data_lengths, &master_data_length);
  AddDBItem(db, CHOICE_GADGET, 'S', 7, 2, stop_str, 7, 14,
            "Stop Settings", 2, 10, 31, 6, 21, stop_bits, &master_stop_bits);
  AddDBItem(db, ON_OFF_GADGET, 'X', 8, 2, xon_xoff_str, 8, 14,
            &master_xon_xoff);
  AddDBItem(db, ON_OFF_GADGET, 'R', 9, 2, rts_cts_str, 9, 14,
            &master_rts_cts);
  AddDBItem(db, ON_OFF_GADGET, 'l', 10, 2, always_init_str, 10, 14,
            &master_always_init);
  AddDBItem(db, ON_OFF_GADGET, 'o', 11, 2, drop_dtr_str, 11, 14,
            &master_drop_dtr);
  DoDialogBox(db);
  DestroyDialogBox(db);
  if (old_master_port != master_port)
  {
    CloseSerialErrChk(0);
    OpenSerialErrChk(0, com_add[master_port], port_int[master_port], atol(bauds[master_baud].text), master_data_length, master_parity, master_stop_bits);
  }
  if (master_initialized == TRUE)
  {
    SetBaud(ports[0], atol(bauds[master_baud].text));
    ConfigurePort(ports[0], data_length_vals[master_data_length], parity_vals[master_parity], stop_bit_vals[master_stop_bits]);
    SetXonXoff(ports[0], master_xon_xoff);
    SetRtsCts(ports[0], master_rts_cts);
  }
  RefreshStatusBar();
}

/******************************************************************************\

  Routine: SlaveCommParamDB

 Function: Display a dialog box allowing the user to change the slave
	   communications port parameters.

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

void SlaveCommParamDB(void)
{
  int old_slave_port = slave_port;
  s_dialog_box *db = CreateDialogBox("Slave Port Configuration", 4, 3, 14, 74);

  AddDBItem(db, STRING_GADGET, 'I', 2, 2, &init_str[0], 2, 14, 58,
            slave_init_string, 80);
  AddDBItem(db, CHOICE_GADGET, 'P', 3, 2, &port_str[0], 3, 14,
            "COM Ports", 4, 6, 32, 8, 17, &com_ports[0], &slave_port);
  AddDBItem(db, CHOICE_GADGET, 'B', 4, 2, &baud_str[0], 4, 14,
            "Baud Settings", 8, 7, 32, 12, 21, &bauds[0], &slave_baud);
  AddDBItem(db, CHOICE_GADGET, 'a', 5, 2, &parity_str[0],
            5, 14, "Parity Settings", 5, 8, 32, 9, 23, &parities[0], &slave_parity);
  AddDBItem(db, CHOICE_GADGET, 'D', 6, 2, &data_str[0], 6, 14,
            "Data Settings", 4, 9, 32, 8, 21, &data_lengths[0], &slave_data_length);
  AddDBItem(db, CHOICE_GADGET, 'S', 7, 2, &stop_str[0], 7, 14,
            "Stop Settings", 2, 10, 31, 6, 21, &stop_bits[0], &slave_stop_bits);
  AddDBItem(db, ON_OFF_GADGET, 'X', 8, 2, &xon_xoff_str[0], 8, 14,
            &slave_xon_xoff);
  AddDBItem(db, ON_OFF_GADGET, 'R', 9, 2, &rts_cts_str[0], 9, 14,
            &slave_rts_cts);
  AddDBItem(db, ON_OFF_GADGET, 'l', 10, 2, &always_init_str[0], 10, 14,
            &slave_always_init);
  AddDBItem(db, ON_OFF_GADGET, 'o', 11, 2, drop_dtr_str, 11, 14,
            &slave_drop_dtr);

  DoDialogBox(db);
  DestroyDialogBox(db);
  if (slave_port_in_use == TRUE)
  {
    if (old_slave_port != slave_port)
    {
      CloseSerialErrChk(1);
      OpenSerialErrChk(1, com_add[slave_port], port_int[slave_port], atol(bauds[slave_baud].text), slave_data_length, slave_parity, slave_stop_bits);
    }
    if (slave_initialized == TRUE)
    {
      SetBaud(ports[1], atol(bauds[slave_baud].text));
      ConfigurePort(ports[1], data_length_vals[slave_data_length], parity_vals[slave_parity], stop_bit_vals[slave_stop_bits]);
      SetXonXoff(ports[1], slave_xon_xoff);
      SetRtsCts(ports[1], slave_rts_cts);
    }
    RefreshStatusBar();
  }
}

/******************************************************************************\

  Routine: MasterOrSlave

 Function: Prompts the user to choose the master or slave port if the slave port
	   is on.

     Pass: Nothing

   Return: 0 for the master port, 1 for the slave port. If the slave port is not
	   on it always returns 0 without prompting the user.

\******************************************************************************/

static int MasterOrSlave(void)
{
  s_choice_items comm_choices[] = {{master_port_str, 'M'}, {slave_port_str, 'S'}};
  int choice = 0;

  if (slave_port_in_use == TRUE)
    return ChoiceMenu("Port Params", 2, 10, 30, 6, 19, &comm_choices[0], &choice);
  else
    return 0;
}

/******************************************************************************\

  Routine: COMPortDB

 Function: Display a dialog box allowing the user to edit the com port addresses
	   and IRQs.

     Pass: Nothing

   Return: Nothing

\******************************************************************************/

static void COMPortDB(void)
{
  WORD old_master_port_add = com_add[master_port];
  int old_master_port_int = port_int[master_port];
  WORD old_slave_port_add = com_add[slave_port];
  int old_slave_port_int = port_int[slave_port];

  s_dialog_box *db = CreateDialogBox(&com_port_setup_str[0], 6, 27, 12, 26);

  AddDBItem(db, WORD_GADGET, '1', 2, 2, "COM1 Address...", 2, 18, &com_add[0]);
  AddDBItem(db, CHOICE_GADGET, 'I', 3, 2, "COM1 Interrupt.", 3, 18, "Interrupts",
            8, 6, 52, 12, 18, &interrupts[0], &port_int[0]);
  AddDBItem(db, WORD_GADGET, '2', 4, 2, "COM2 Address...", 4, 18, &com_add[1]);
  AddDBItem(db, CHOICE_GADGET, 'n', 5, 2, "COM2 Interrupt.", 5, 18, "Interrupts",
            8, 6, 52, 12, 18, &interrupts[0], &port_int[1]);
  AddDBItem(db, WORD_GADGET, '3', 6, 2, "COM3 Address...", 6, 18, &com_add[2]);
  AddDBItem(db, CHOICE_GADGET, 't', 7, 2, "COM3 Interrupt.", 7, 18, "Interrupts",
            8, 6, 52, 12, 18, &interrupts[0], &port_int[2]);
  AddDBItem(db, WORD_GADGET, '4', 8, 2, "COM4 Address...", 8, 18, &com_add[3]);
  AddDBItem(db, CHOICE_GADGET, 'r', 9, 2, "COM4 Interrupt.", 9, 18, "Interrupts",
            8, 6, 52, 12, 18, &interrupts[0], &port_int[3]);
  DoDialogBox(db);
  DestroyDialogBox(db);
  if ((old_master_port_int != port_int[master_port]) ||
      (old_master_port_add != com_add[master_port]))
  {
    CloseSerialErrChk(0);
    OpenSerialErrChk(0, com_add[master_port], port_int[master_port],
                     atol(bauds[master_baud].text), master_data_length, master_parity,
                     master_stop_bits);
  }
  if (((old_slave_port_int != port_int[slave_port]) ||
       (old_slave_port_add != com_add[slave_port])) &&
      slave_port_in_use)
  {
    CloseSerialErrChk(1);
    OpenSerialErrChk(1, com_add[slave_port], port_int[slave_port], atol(bauds[slave_baud].text), slave_data_length, slave_parity, slave_stop_bits);
  }
}

/******************************************************************************\

  Routine: SaveSetup

 Function:

     Pass:

   Return:

\******************************************************************************/

static void SaveSetup(void)
{
  FILE *config_file;

  GetInput(config_filename, "Enter configuration filename:", 160);
  config_file = fopen(config_filename, "wb");
  SetCurrentFile(config_file);

  WriteStringToFile(config_header);
  WriteWordToFile(status_fore);
  WriteWordToFile(status_back);
  WriteWordToFile(menu_frame_fore);
  WriteWordToFile(menu_frame_back);
  WriteWordToFile(menu_text_fore);
  WriteWordToFile(menu_text_back);
  WriteWordToFile(menu_highlight_fore);
  WriteWordToFile(menu_highlight_back);
  WriteWordToFile(menu_hotkey_fore);
  WriteWordToFile(menu_hotkey_back);
  WriteWordToFile(sound_on);
  WriteWordToFile(local_echo_on);
  WriteWordToFile(add_lf_on);
  WriteWordToFile(strip_high_bit_on);
  WriteWordToFile(slave_port_in_use);
  WriteStringToFile(master_init_string);
  WriteWordToFile(master_port);
  WriteWordToFile(master_data_length);
  WriteWordToFile(master_parity);
  WriteWordToFile(master_stop_bits);
  WriteWordToFile(master_baud);
  WriteWordToFile(master_xon_xoff);
  WriteWordToFile(master_rts_cts);
  WriteWordToFile(master_always_init);
  WriteStringToFile(slave_init_string);
  WriteWordToFile(slave_port);
  WriteWordToFile(slave_data_length);
  WriteWordToFile(slave_parity);
  WriteWordToFile(slave_stop_bits);
  WriteWordToFile(slave_baud);
  WriteWordToFile(slave_xon_xoff);
  WriteWordToFile(slave_rts_cts);
  WriteWordToFile(slave_always_init);
  WriteWordToFile(com_add[0]);
  WriteWordToFile(com_add[1]);
  WriteWordToFile(com_add[2]);
  WriteWordToFile(com_add[3]);
  WriteWordToFile(port_int[0]);
  WriteWordToFile(port_int[1]);
  WriteWordToFile(port_int[2]);
  WriteWordToFile(port_int[3]);
  WriteWordToFile(curr_emul);
  WriteWordToFile(master_drop_dtr);
  WriteWordToFile(slave_drop_dtr);
  WriteStringToFile(remote_menu_string);
  WriteStringToFile(remote_callback_str);
  WriteLongToFile(thread_timeout);

  fclose(config_file);
  (*term->SaveTerminalSetup)(term);
  if (SaveTranslationTable(term->translation_table_p) == FALSE)
    MessageBoxPause("Translation table not saved");
  else
    MessageBoxPause("Translation table saved");
  MessageBoxPause("Setup Saved");
}

/******************************************************************************\

  Routine: LoadSetup

 Function:

     Pass:

   Return:

\******************************************************************************/

static void AskAndLoadSetup(void)
{
  GetInput(config_filename, "Enter configuration filename:", 160);
}

/******************************************************************************\

  Routine: LoadSetup

 Function:

     Pass:

   Return:

\******************************************************************************/

static void LoadSetup(void)
{
  FILE *config_file;

  if ((config_file = fopen(config_filename, "r")) != NULL)
  {
    char file_header[17];
    SetCurrentFile(config_file);

    ReadStringFromFile(&file_header[0]);
    if (!strcmp(&file_header[0], &config_header[0]))
    {
      status_fore = ReadWordFromFile();
      status_back = ReadWordFromFile();
      menu_frame_fore = ReadWordFromFile();
      menu_frame_back = ReadWordFromFile();
      menu_text_fore = ReadWordFromFile();
      menu_text_back = ReadWordFromFile();
      menu_highlight_fore = ReadWordFromFile();
      menu_highlight_back = ReadWordFromFile();
      menu_hotkey_fore = ReadWordFromFile();
      menu_hotkey_back = ReadWordFromFile();
      sound_on = ReadWordFromFile();
      local_echo_on = ReadWordFromFile();
      add_lf_on = ReadWordFromFile();
      strip_high_bit_on = ReadWordFromFile();
      slave_port_in_use = ReadWordFromFile();
      ReadStringFromFile(&master_init_string[0]);
      master_port = ReadWordFromFile();
      master_data_length = ReadWordFromFile();
      master_parity = ReadWordFromFile();
      master_stop_bits = ReadWordFromFile();
      master_baud = ReadWordFromFile();
      master_xon_xoff = ReadWordFromFile();
      master_rts_cts = ReadWordFromFile();
      master_always_init = ReadWordFromFile();
      ReadStringFromFile(&slave_init_string[0]);
      slave_port = ReadWordFromFile();
      slave_data_length = ReadWordFromFile();
      slave_parity = ReadWordFromFile();
      slave_stop_bits = ReadWordFromFile();
      slave_baud = ReadWordFromFile();
      slave_xon_xoff = ReadWordFromFile();
      slave_rts_cts = ReadWordFromFile();
      slave_always_init = ReadWordFromFile();
      com_add[0] = ReadWordFromFile();
      com_add[1] = ReadWordFromFile();
      com_add[2] = ReadWordFromFile();
      com_add[3] = ReadWordFromFile();
      port_int[0] = ReadWordFromFile();
      port_int[1] = ReadWordFromFile();
      port_int[2] = ReadWordFromFile();
      port_int[3] = ReadWordFromFile();
      curr_emul = ReadWordFromFile();
      master_drop_dtr = ReadWordFromFile();
      slave_drop_dtr = ReadWordFromFile();
      ReadStringFromFile(remote_menu_string);
      remote_menu_str_len = strlen(remote_menu_string) - 1;
      ReadStringFromFile(remote_callback_str);
      thread_timeout = ReadLongFromFile();
    }
  }
  SetFrameAttrib(menu_frame_back << 4 | menu_frame_fore);
  SetTextAttrib(menu_text_back << 4 | menu_text_fore);
  SetFillAttrib(menu_text_back << 4 | menu_text_fore);
  SetMenuColors(menu_highlight_back << 4 | menu_highlight_fore, menu_hotkey_back << 4 | menu_hotkey_fore);
  SetFrameAttrib(menu_frame_back << 4 | menu_frame_fore);
  SetFillAttrib(menu_text_back << 4 | menu_text_fore);
  status_attrib = status_back << 4 | status_fore;

  fclose(config_file);
}

/******************************************************************************\

  Routine: LogFile

 Function:

     Pass:

   Return:

\******************************************************************************/

static void LogFile(void)
{
  if (log_file_open == TRUE)
  {
    log_file_open = FALSE;
    fclose(log_file);
    MessageBoxPause("Log file closed.");
  }
  else
  {
    GetInput(log_file_string, "Enter log file name:", 160);
    if (strlen(log_file_string) > 0)
    {
      log_file = fopen(log_file_string, "w");
      if (log_file != NULL)
        log_file_open = TRUE;
      else
        MessageBoxPause("Unable to open log file.");
    }
  }
}

/******************************************************************************\

  Routine: GetInput

 Function:

     Pass:

   Return:

\******************************************************************************/

static void GetInput(char *string, char *prompt, int length)
{
  WORD *box;

  box = SaveAndDrawBox(9, 0, 6, COLS, ' ');
  OutTextAt(11, 2, prompt);
  Input(12, 2, (COLS - 4), length, string, string, null_str, null_str);
  RestoreRect(box);
}

/******************************************************************************\

  Routine: DialNumber

 Function:

     Pass:

   Return:

\******************************************************************************/

void DialNumber(void)
{
  GetInput(phone_number, "Enter number to dial:", 25);
  if (strlen(phone_number) > 0)
    DialModem(ports[0], phone_number, 0, 6000L, FALSE);
}

/******************************************************************************\

  Routine: Hangup

 Function:

     Pass:

   Return:

\******************************************************************************/

void Hangup(void)
{
  int port_to_hang_up = MasterOrSlave();

  if (port_to_hang_up != -1)
    HangUpModem(ports[port_to_hang_up], HANGUP_TIME);
}

/******************************************************************************\

  Routine: RunProg

 Function:

     Pass:

   Return:

\******************************************************************************/

void RunProg(char *command)
{
  s_point ncursorpos = GetPhysicalCursorPos();
  WORD *screen = SaveRect(0, 0, 25, 80);
  WORD old_cursor_shape;

  if (screen != NULL)
  {
    SetTextAttrib(0x07);
    HCharC(0, 0, 2000, ' ');
    OutTextAt(0, 0, "Type EXIT to return to Intralink.");
    SetPhysicalCursorPos(1, 0);
    DeInitKeyboard();
    old_cursor_shape = SetCursorShape(UNDERLINECURSOR);
    if (strcmp(command, "command") != 0)
      spawnlp(P_WAIT, "command", "", command, NULL);
    else
      spawnlp(P_WAIT, "command", NULL);
    InitKeyboard();
    SetVideoMode(0x03);
    LoadROMFont(ROM8BY16);
    SetPhysicalCursorPos(ncursorpos.row, ncursorpos.col);
    RestoreRect(screen);
    RefreshStatusBar();
    if (slave_initialized == TRUE)
    {
      if (!CarrierDetected(ports[1]) || slave_always_init == TRUE)
        SendStringToModem(ports[1], slave_init_string, TRUE);
      FlushPortInputBuffer(ports[1]);
    }
    SetCursorShape(old_cursor_shape);
  }
  else
    MessageBoxPause("Unable to shell to DOS, not enough memory.");
}

/******************************************************************************\

  Routine: DownloadMenu

 Function:

     Pass:

   Return:

\******************************************************************************/

static void DownloadMenu(void)
{
  int choice = 0;

  if (ChoiceMenu("Protocol", 5, 8, 32, 9, 16, &protocols[0], &choice) != -1)
    switch (choice)
    {
    case -1:
      break;

    case 0:
      GetInput(filename_string, "Enter download filename:", 160);
      ReceiveXModem(filename_string, 10, XMODEM, ports[0]);
      break;

    case 1:
      GetInput(filename_string, "Enter download filename:", 160);
      ReceiveXModem(filename_string, 10, XMODEMK, ports[0]);
      break;

    case 2:
      break;

    case 3:
      break;

    case 4:
      ReceiveKermit(12);
      break;
    }
}

/******************************************************************************\

  Routine: UploadMenu

 Function:

     Pass:

   Return:

\******************************************************************************/

static void UploadMenu(void)
{
  int choice = 0;

  if (ChoiceMenu("Protocol", 5, 8, 32, 9, 16, &protocols[0], &choice) != -1)
  {
    GetInput(filename_string, "Enter filename to upload:", 160);
    if (strlen(filename_string) > 0)
      switch (choice)
      {
      case 0:
        SendXModem(filename_string, 50, XMODEM, ports[0]);
        break;

      case 1:
        SendXModem(filename_string, 50, XMODEMK, ports[0]);
        break;

      case 2:
        break;

      case 3:
        break;

      case 4:
        SendKermit(filename_string, 12);
        break;
      }
  }
}

/******************************************************************************\

  Routine: DisableHostDisplay

 Function:

     Pass:

   Return:

\******************************************************************************/

static void DisableHostDisplay(void)
{
  disp_buff = SaveRect(0, 0, 25, 80);
  if (disp_buff != NULL)
  {
    SetTextAttrib(0x0f);
    HCharC(0, 0, 2000, ' ');
    OutTextAt(10, 29, "Host display disabled.");
    RedirectScreen((BYTE _far *)disp_buff + 4);
    (*term->SetScreenBase)(term);
  }
  SetCursorShape(HIDECURSOR);
}

/******************************************************************************\

  Routine: EnableHostDisplay

 Function:

     Pass:

   Return:

\******************************************************************************/

static void EnableHostDisplay(void)
{
  ResetScreenBase();
  (*term->SetScreenBase)(term);
  RestoreRect(disp_buff);
  SetCursorShape(BLOCKCURSOR);
}

/******************************************************************************\

  Routine: ShowSlaveMenu

 Function:

     Pass:

   Return:

\******************************************************************************/

static void ShowSlaveMenu(void)
{
  int ch = 0;

  if (host_display_enabled == TRUE)
    DisableHostDisplay();
  SerialStringWrite(ports[1], term->ClearScreenString);
  SerialStringWrite(ports[1], "ILINK Version 6.31 Remote Menu\n\r\n\r");
  SerialStringWrite(ports[1], "1. ");
  SerialStringWrite(ports[1], host_display_enabled == TRUE ? "Disable" : "Enable");
  SerialStringWrite(ports[1], " Host Display\n\r");
  SerialStringWrite(ports[1], "2. Drop to DOS\n\r");
  SerialStringWrite(ports[1], "3. Send Command to Remote Modem\n\r");
  SerialStringWrite(ports[1], "4. ");
  SerialStringWrite(ports[1], remote_callback_enabled == TRUE ? "Disable" : "Enable");
  SerialStringWrite(ports[1], " Remote Callback\n\r");
  SerialStringWrite(ports[1], "5. ");
  SerialStringWrite(ports[1], XonXoffEnabled == TRUE ? "Disable" : "Enable");
  SerialStringWrite(ports[1], " XON/XOff flow control on both ports\n\r");
  SerialStringWrite(ports[1], "6. Exit ILink\n\r");

  while (!ch)
  {
    while (!ReadySerial(ports[1]))
      ;
    ch = SerialRead(ports[1]);
    switch (ch)
    {
    case '1':
      host_display_enabled = (host_display_enabled == TRUE) ? FALSE : TRUE;
      break;

    case '2':
    {
      char *com_str = "com ";

      DeInitKeyboard();
      *(com_str + 3) = (BYTE)('1' + slave_port);
      spawnlp(P_WAIT, "command.com", "", com_str, NULL);
      InitKeyboard();
      CloseSerialErrChk(1);
      OpenSerialErrChk(1, com_add[slave_port], port_int[slave_port], atol(bauds[slave_baud].text), slave_data_length, slave_parity, slave_stop_bits);
    }
    break;

    case '3':
      break;

    case '4':
      remote_callback_enabled = ~remote_callback_enabled;
      if (remote_callback_enabled == TRUE)
      {
        SerialStringWrite(ports[1], term->ClearScreenString);
        SerialStringWrite(ports[1], "Enter number to call if carrier is lost:\n\r");
        InputSerial(ports[1], remote_callback_str, 25, 12000L, TRUE);
      }
      break;

    case '5':
      if (XonXoffEnabled == TRUE)
      {
        SetXonXoff(ports[0], FALSE);
        SetXonXoff(ports[1], FALSE);
        XonXoffEnabled = FALSE;
      }
      else
      {
        SetXonXoff(ports[0], master_xon_xoff);
        SetXonXoff(ports[1], slave_xon_xoff);
        XonXoffEnabled = FALSE;
      }
      break;

    case '6':
      EndProgram();
      break;

    case '\r':
    case 27: /* Escape */
      break;

    default:
      SerialWrite(ports[1], '\x07');
      ch = 0;
    }
  }
  if (host_display_enabled == TRUE)
    EnableHostDisplay();
  (*term->CopyScreenToSlave)(term);
}

/******************************************************************************\

  Routine: EndProgram

 Function:

     Pass:

   Return:

\******************************************************************************/

void EndProgram(void)
{
  int x;

  CloseSerialErrChk(0);
  if (slave_initialized == TRUE)
    CloseSerialErrChk(1);
  SetTextAttrib(0x07);
  HCharC(0, 0, 2000, ' ');
  if (host_display_enabled == FALSE)
    EnableHostDisplay();

  for (x = 0; x < 3; x++)
    if (pages[x] != NULL)
      (*pages[x]->EndTerm)(pages[x]);
#ifdef FROMFILE
  fclose(debug_input_file);
#endif
  DestroyFNKeyTable(&fn_keys);
  DeInitKeyboard();
  SetVideoMode(0x03);
  LoadROMFont(ROM8BY16);
  SetCursorShape(UNDERLINECURSOR);
#ifdef DEBUG
  mem_report();
#endif
  exit(1);
}

/******************************************************************************\

  Routine: SetEmulation

 Function:

     Pass:

   Return:

\******************************************************************************/

void SetEmulation(int emul_number)
{
  (*term->EndTerm)(term);
  emulations[curr_emul].enabled = FALSE;
  curr_emul = emul_number;
  term = emulations[emul_number].InitTerm();
  pages[page] = term;
  RefreshStatusBar();
}

/******************************************************************************\

  Routine: Setvt52Emulation

 Function:

     Pass:

   Return:

\******************************************************************************/

void Setvt52Emulation(void)
{
  SetEmulation(0);
}

/******************************************************************************\

  Routine: Setvt100Emulation

 Function:

     Pass:

   Return:

\******************************************************************************/

void Setvt100Emulation(void)
{
  SetEmulation(1);
}

/******************************************************************************\

  Routine: Setc332Emulation

 Function:

     Pass:

   Return:

\******************************************************************************/

void Setc332Emulation(void)
{
  SetEmulation(2);
}

/******************************************************************************\

  Routine: Setc332eEmulation

 Function:

     Pass:

   Return:

\******************************************************************************/

void Setc332eEmulation(void)
{
  SetEmulation(3);
}

/******************************************************************************\

  Routine: SetANSIEmulation

 Function:

     Pass:

   Return:

\******************************************************************************/

void SetANSIEmulation(void)
{
  SetEmulation(4);
}

/******************************************************************************\

  Routine: SetPCNANSIEmulation

 Function:

     Pass:

   Return:

\******************************************************************************/

void SetPCNANSIEmulation(void)
{
  SetEmulation(5);
}

/******************************************************************************\

  Routine: ExitILink

 Function:

     Pass:

   Return:

\******************************************************************************/

void ExitILink(void)
{
  int choice = 0;

  if (ChoiceMenu("Exit ILink?", 3, 10, 30, 7, 19, &exit_choices[0], &choice) != -1)
  {
    if (choice == 2)
    {
      HangUpModem(ports[0], HANGUP_TIME);
      HangUpModem(ports[1], HANGUP_TIME);
    }
    if (choice == 0 || choice == 2)
      EndProgram();
  }
}

/******************************************************************************\

  Routine: Setup

 Function:

     Pass:

   Return:

\******************************************************************************/

static void Setup(void)
{
  int x;

  s_menu *mainmenu;
  s_menu *ilinkmenu;
  s_menu *configmenu;
  s_menu *xfermenu;
  s_menu *commparammenu;
  s_menu *terminalsmenu;

  ilinkmenu = CreateMenu(null_str, 1, 0, 9, 21, IS_SUB_MENU | VERTICAL | BORDER);
  AddMenuItem(ilinkmenu, 1, 2, FUNCTION, "Load Setup", 'L', null_str, NULL, (void *)AskAndLoadSetup, NULL);
  AddMenuItem(ilinkmenu, 2, 2, FUNCTION, "Save Setup", 'S', null_str, NULL, (void *)SaveSetup, NULL);
  AddMenuItem(ilinkmenu, 3, 2, FUNCTION, "Log File", 'g', "ALT+G", NULL, (void *)LogFile, NULL);
  AddMenuItem(ilinkmenu, 4, 2, FUNCTION, "Dial", 'D', "ALT+C", NULL, (void *)DialNumber, NULL);
  AddMenuItem(ilinkmenu, 5, 2, FUNCTION, "Hangup", 'H', "ALT+H", NULL, (void *)Hangup, NULL);
  AddMenuItem(ilinkmenu, 6, 2, FUNCTION, "DOS Shell", 'O', "ALT+J", NULL, (void *)RunProg, "command");
  AddMenuItem(ilinkmenu, 7, 2, FUNCTION, "Exit", 'x', "Alt+X", NULL, (void *)ExitILink, NULL);

  commparammenu = CreateMenu(null_str, 4, 35, 4, 15, IS_SUB_MENU | VERTICAL | BORDER);
  AddMenuItem(commparammenu, 1, 2, FUNCTION, &master_port_str[0], 'M', null_str, NULL, (void *)MasterCommParamDB, NULL);
  AddMenuItem(commparammenu, 2, 2, FUNCTION, slave_port_str, 'S', null_str, NULL, (void *)SlaveCommParamDB, NULL);

  terminalsmenu = CreateMenu(null_str, 7, 35, NUM_TERMS + 2, 15, IS_SUB_MENU | VERTICAL | BORDER);
  for (x = 0; x < NUM_TERMS; x++)
    AddMenuItem(terminalsmenu, x + 1, 2, CHECKED, emulations[x].name, emulations[x].hotkey, null_str, &emulations[x].enabled, (void *)emulations[x].SetEmulation, NULL);

  configmenu = CreateMenu(null_str, 1, 7, 9 + term->num_menu_items,
                          31, IS_SUB_MENU | VERTICAL | BORDER);
  AddMenuItem(configmenu, 1, 2, FUNCTION, &colors_str[0], 'C', null_str, NULL, (void *)ScreenColorDB, NULL);
  AddMenuItem(configmenu, 2, 2, FUNCTION, "General Options", 'G', null_str, NULL, (void *)GenOptsDB, NULL);
  AddMenuItem(configmenu, 3, 2, SUB_MENU, "Communications Parameters", 'P', null_str, commparammenu, NULL, NULL);
  AddMenuItem(configmenu, 4, 2, FUNCTION, &com_port_setup_str[0], 'm', null_str, NULL, (void *)COMPortDB, NULL);
  AddMenuItem(configmenu, 5, 2, FUNCTION, "Function Keys", 'F', null_str, NULL, (void *)FunctionKeyDB, NULL);
  AddMenuItem(configmenu, 6, 2, SUB_MENU, "Terminal Emulation", 'T', null_str, terminalsmenu, NULL, NULL);
  AddMenuItem(configmenu, 7, 2, FUNCTION, "Xfer Options", 'X', null_str, NULL, (void *)XferOptDB, NULL);
  (*term->AddConfigMenuItems)(term, configmenu, 8);

  xfermenu = CreateMenu(null_str, 1, 22, 5, 24, IS_SUB_MENU | VERTICAL | BORDER);
  AddMenuItem(xfermenu, 1, 2, FUNCTION, "Upload File", 'U', "Alt-U", NULL, (void *)UploadMenu, NULL);
  AddMenuItem(xfermenu, 2, 2, FUNCTION, "Download File", 'D', "Alt-D", NULL, (void *)DownloadMenu, NULL);
  AddMenuItem(xfermenu, 3, 2, FUNCTION, "Protocol Options", 'P', null_str, NULL, (void *)NotImplemented, NULL);

  mainmenu = CreateMenu(null_str, 0, 0, 1, COLS, HORIZONTAL | NOBORDER);
  AddMenuItem(mainmenu, 0, 2, SUB_MENU, "ILink", 'I', null_str, ilinkmenu, NULL, NULL);
  AddMenuItem(mainmenu, 0, 9, SUB_MENU, "Configuration", 'C', null_str, configmenu, NULL, NULL);
  AddMenuItem(mainmenu, 0, 24, SUB_MENU, "Xfer", 'X', null_str, xfermenu, NULL, NULL);
  AddMenuItem(mainmenu, 0, COLS - 6, FUNCTION, "Help", 'e', null_str, NULL, (void *)ShowHelp, NULL);
  DoMenu(mainmenu);
}

/******************************************************************************\

  Routine: SwitchPage

 Function:

     Pass:

   Return:

\******************************************************************************/

void SwitchPage(WORD new_page)
{

  if (new_page < (WORD)4 && new_page > (WORD)0)
  {
    page = new_page - 1;
    (*term->SaveTerm)(term);
    if (pages[page] == NULL)
      pages[page] = emulations[curr_emul].InitTerm();
    else
      (*pages[page]->RestoreTerm)(pages[page]);
    term = pages[page];
    ShowChOnStatusBar(64, '1' + page);
    if ((*term->StatBar)(term))
    {
      ClearStatusBar();
      (*term->DisplayStatusMessage)(term);
    }
    else
      RefreshStatusBar();
    if (slave_port_online == TRUE && page_drawn[page] == FALSE)
    {
      (*term->CopyScreenToSlave)(term);
      page_drawn[page] = TRUE;
    }
  }
  else
    ErrorBox("Invalid page number received");
}

/******************************************************************************\

  Routine: ShowHelp

 Function:

     Pass:

   Return:

\******************************************************************************/

static void ShowHelp(void)
{
  WORD *sc_buff = SaveRect(0, 0, 25, 80);

  DrawRect(0, 0, 25, 80, ' ');
  WaitForKeyPress();
  RestoreRect(sc_buff);
}

/******************************************************************************\

  Routine: main

 Function:

     Pass:

   Return:

\******************************************************************************/

void main(int argc, char *argv[])
{
  BYTE ch;
  int x;
  WORD key;
  char slave_input[10] = "";
  s_fkey_def *fkeydef;
  BOOLN carrier_lost = TRUE;

  InitCharVideo();
  InitKeyboard();
  SetVideoMode(0x03);
  /*SetVideoMode (0x27);*/
  LoadROMFont(ROM8BY16);
  /*LoadROMFont (ROM8BY8);*/
  SetCursorShape(HIDECURSOR);
  Pause(25L);
  FlushKeyBuffer();
  if (argc > 1 && strlen(argv[1]) && strlen(argv[1]) < 66)
    strcpy(&config_filename[0], argv[1]);
  LoadSetup();
  ShowTitleScreen();

  /* Add New Terminal Info Here */
  emulations[0].enabled = FALSE;
  emulations[0].name = "vt52";
  emulations[0].hotkey = '5';
  emulations[0].InitTerm = Initvt52;
  emulations[0].SetEmulation = Setvt52Emulation;

  emulations[1].enabled = FALSE;
  emulations[1].name = "vt100";
  emulations[1].hotkey = 'v';
  emulations[1].InitTerm = Initvt100;
  emulations[1].SetEmulation = Setvt100Emulation;

  emulations[2].enabled = FALSE;
  emulations[2].name = "c332";
  emulations[2].hotkey = 'c';
  emulations[2].InitTerm = Initc332;
  emulations[2].SetEmulation = Setc332Emulation;

  emulations[3].enabled = FALSE;
  emulations[3].name = "c332e";
  emulations[3].hotkey = 'e';
  emulations[3].InitTerm = Initc332e;
  emulations[3].SetEmulation = Setc332eEmulation;

  emulations[4].enabled = FALSE;
  emulations[4].name = "ANSI";
  emulations[4].hotkey = 'A';
  emulations[4].InitTerm = InitANSI;
  emulations[4].SetEmulation = SetANSIEmulation;

  emulations[5].enabled = FALSE;
  emulations[5].name = "PCNANSI";
  emulations[5].hotkey = 'P';
  emulations[5].InitTerm = InitPCNANSI;
  emulations[5].SetEmulation = SetPCNANSIEmulation;

  term = emulations[curr_emul].InitTerm();
  pages[0] = term;
  emulations[curr_emul].enabled = TRUE;

  /* Load function keys */
  LoadFNKeyTable("default", &fn_keys);

  if (!(*term->StatBar)(term))
    ClearStatusBar();

  if (OpenSerialErrChk(0, com_add[master_port], port_int[master_port], atol(bauds[master_baud].text), master_data_length, master_parity, master_stop_bits) == 0)
  {
    SetXonXoff(ports[0], master_xon_xoff);
    SetRtsCts(ports[0], master_rts_cts);
    if (CarrierDetected(ports[0]))
      master_port_online = TRUE;
    else
      FlushPortInputBuffer(ports[0]);
    if (!CarrierDetected(ports[0]) || master_always_init == TRUE)
      SendStringToModem(ports[0], master_init_string, TRUE);
  }

  if (slave_port_in_use == TRUE)
  {
    if (OpenSerialErrChk(1, com_add[slave_port], port_int[slave_port], atol(bauds[slave_baud].text), slave_data_length, slave_parity, slave_stop_bits) == 0)
    {
      SetXonXoff(ports[1], slave_xon_xoff);
      SetRtsCts(ports[1], slave_rts_cts);
      if (CarrierDetected(ports[1]))
      {
        /*SerialStringWrite (ports[1], term->ClearScreenString);*/
        slave_port_online = TRUE;
      }
      else
      {
        FlushPortInputBuffer(ports[1]);
        slave_port_online = FALSE;
      }
      if (!CarrierDetected(ports[1]) || slave_always_init == TRUE)
        SendStringToModem(ports[1], slave_init_string, TRUE);
    }
  }
  online_time = StartTimer();
  RefreshStatusBar();
#ifdef FROMFILE
  debug_input_file = fopen("PAE.log", "rb");
#endif
  SetCursorShape(BLOCKCURSOR);

  while (1)
  {
    if (master_initialized)
    {
      /* Check to see if the master port has changed from online to offline or
         vice versa */

      if (SerialStatusChanged(ports[0]))
      {
        if (CarrierDetected(ports[0]))
        {
          if (master_port_online == FALSE)
          {
            master_port_online = TRUE;
            online_time = StartTimer();
          }
        }
        else if (master_port_online == TRUE)
        {
          master_port_online = FALSE;
          (*term->ReleaseStatusBar)(term);
          RefreshStatusBar();
          FlushPortOutputBuffer(ports[0]);
        }
        ShowStatusMaster();
      }

      /* Check to see if we have any hardware errors */
      if (HWError(ports[0]) == TRUE)
      {
        if (ParityErr(ports[0]) == TRUE)
          MessageBoxPause("Master port parity error");
        if (OverrunErr(ports[0]) == TRUE)
          MessageBoxPause("Master port overrun error");
        if (FramingErr(ports[0]) == TRUE)
          MessageBoxPause("Master port framing error");
        ClrHWError(ports[0]);
      }
    }
    /* Check to see if the slave port has changed from online to offline or
       vice versa */

    if (slave_initialized == TRUE)
    {
      if (SerialStatusChanged(ports[1]))
      {
        if (!CarrierDetected(ports[1]) && remote_callback_enabled == TRUE)
        {
          carrier_lost = TRUE;
          page_drawn[0] = page_drawn[1] = page_drawn[2] = FALSE;
          DialModem(ports[1], remote_callback_str, 4, 6000L, TRUE);
        }
        if (!CarrierDetected(ports[1]))
        {
          if (slave_port_online == TRUE)
          {
            if (host_display_enabled == FALSE)
            {
              EnableHostDisplay();
              host_display_enabled = TRUE;
            }
            slave_port_online = FALSE;
            MessageBoxPause(slave_port_offline_str);
            SendStringToModem(ports[1], slave_init_string, TRUE);
            FlushPortInputBuffer(ports[1]);
            ShowStatusSlave();
            page_drawn[0] = page_drawn[1] = page_drawn[2] = FALSE;
            SetXonXoff(ports[0], master_xon_xoff);
            SetXonXoff(ports[1], slave_xon_xoff);
            XonXoffEnabled = FALSE;
            carrier_lost = TRUE;
          }
        }
        else
        {
          if (slave_port_online == FALSE)
          {
            MessageBoxPause("The slave port is now online.");
            slave_port_online = TRUE;
          }
          if (carrier_lost == TRUE)
          {
            Pause(50l);
            SerialStringWrite(ports[1], term->ClearScreenString); /* Switch remote to correct page */
            SerialStringWrite(ports[1], "Connected to ILINK v6.31 by John Chrisman\n\rClearing garbage from slave port. Please wait.");
            for (x = 0; x < 10; x++)
              slave_input[x] = 0;
            Pause(200L);
            FlushPortInputBuffer(ports[1]);
            SerialStringWrite(ports[1], "\x1b["); /* Switch remote to correct page */
            SerialWrite(ports[1], '1' + page);
            SerialWrite(ports[1], 'S');
            (*term->CopyScreenToSlave)(term);
            page_drawn[page] = TRUE;
            carrier_lost = FALSE;
          }
        }
      }

      /* Check to see if we have any hardware errors */
      if (HWError(ports[1]))
      {
        if (ParityErr(ports[1]) == TRUE)
          MessageBoxPause("Slave port parity error");
        if (OverrunErr(ports[1]) == TRUE)
          MessageBoxPause("Slave port overrun error");
        if (FramingErr(ports[1]) == TRUE)
          MessageBoxPause("Slave port framing error");
        ClrHWError(ports[1]);
      }
    }
    /* Get character from local */

    if (nextFNKeyChar != NULL)
    {
      char dec_value[] = {0, 0, 0, 0};
      char ch;

      if (TimerValue(nextFKTimer) > FKTimeOut)
      {
        FKTimeOut = 10L;
        ch = *nextFNKeyChar++;
        switch (ch)
        {
        case '~':
          FKTimeOut = 100L;
          break;

        case '\\':
          if (*nextFNKeyChar == '\\')
          {
            SerialWrite(ports[0], '\\');
            nextFNKeyChar++;
          }
          else if (*nextFNKeyChar == '^')
          {
            SerialWrite(ports[0], '^');
            nextFNKeyChar++;
          }
          else if (*nextFNKeyChar == '~')
          {
            SerialWrite(ports[0], '~');
            nextFNKeyChar++;
          }
          else
          {
            dec_value[0] = *(nextFNKeyChar++);
            dec_value[1] = *(nextFNKeyChar++);
            dec_value[2] = *(nextFNKeyChar++);
            ch = (BYTE)atoi(&dec_value[0]);
            SerialWrite(ports[0], ch);
          }
          break;

        case '^':
          SerialWrite(ports[0], (char)(toupper(*nextFNKeyChar++) - 64));
          break;

        default:
          SerialWrite(ports[0], ch);
        }
        if (*(nextFNKeyChar + 1) == 0)
          nextFNKeyChar = NULL;
        else
          nextFKTimer = StartTimer();
      }
    }
    else
    {
      key = GetAKey();
      if (key)
      {
        if (IsASCIIKey(key))
        {
          ch = (BYTE)(key & 255);
          if (!(*term->KeyBoardLocked)(term))
          {
            if (master_initialized == TRUE)
              SerialWrite(ports[0], ch);
#ifdef DEBUG
            OutTextAt(24, 0, ultoa(ch, "        ", 16));
#endif
            if (local_echo_on == TRUE)
            {
              if (log_file_open == TRUE)
                fputc(ch, log_file);
              if (strip_high_bit_on == TRUE)
                ch = (char)(ch & 0x7f);
              if (slave_port_online == TRUE)
                SerialWrite(ports[1], ch);
              (*term->CharHandler)(term, term->translation_table_p->table[ch]);
              if (ch == 0x0d && add_lf_on)
                (*term->CharHandler)(term, term->translation_table_p->table[0x0a]);
            }
          }
        }
        else if ((*term->EvaluateKey)(term, key) == FALSE)
        {
          fkeydef = DoublyLinkedListGetItemWKeyOf(fn_keys->fn_key_defs, key);
          if (fkeydef != NULL)
          {
            nextFNKeyChar = fkeydef->definition;
            nextFKTimer = StartTimer();
            FKTimeOut = 10L;
            /*SendStringToModem (ports[0], fkeydef->definition, FALSE);*/
          }
          else
            switch (key)
            {
            case KEY_ALT_A:
              SendStringToModem(ports[1], "^MATH1^M~ATA^M~", TRUE);
              break;

            case KEY_ALT_C:
              DialNumber();
              break;

            case KEY_ALT_D:
              DownloadMenu();
              break;

            case KEY_ALT_E:
              local_echo_on = ~local_echo_on;
              MessageBoxPause(local_echo_on ? "Local Echo On" : "Local Echo Off");
              break;

            case KEY_ALT_G:
              LogFile();
              break;

            case KEY_ALT_H:
              Hangup();
              break;

            case KEY_ALT_J:
              RunProg("command");
              break;

            case KEY_ALT_L:
              add_lf_on = ~add_lf_on;
              MessageBoxPause(add_lf_on ? "Add LF After CR On" : "Add LF After CR Off");
              break;

            case KEY_ALT_M:
              Setup();
              break;

#ifdef DEBUG
            case KEY_ALT_S:
            {
              s_port_state port_state;
              winhdl *stat_win;

              SavePortState(ports[0], &port_state);

              stat_win = CreateWindow("Status", 2, 10, 21, 60, 0x0F, FRAME | SAVEUNDER | CLEAR);
              WindowPrintString(stat_win, itoa(port_state.IER, "      ", 10));
              WindowPrintString(stat_win, "\n\r");
              WindowPrintString(stat_win, itoa(port_state.LCR, "      ", 10));
              WindowPrintString(stat_win, "\n\r");
              WindowPrintString(stat_win, itoa(port_state.MCR, "      ", 10));
              WindowPrintString(stat_win, "\n\r");
              WindowPrintString(stat_win, itoa(port_state.DL, "      ", 10));
              WindowPrintString(stat_win, "\n\r");
              WindowPrintString(stat_win, ultoa((DWORD)(port_state.Old_ISR), "      ", 16));
              WindowPrintString(stat_win, "John");
              WindowPrintString(stat_win, "\n\r");
              WindowPrintString(stat_win, itoa(port_state.int_enabled, "      ", 10));
              while (GetAKey() == 0);
              DestroyWindow(stat_win);
              stat_win = CreateWindow("Status", 2, 10, 21, 60, 0x0F, FRAME | SAVEUNDER | CLEAR);
              WindowPrintString(stat_win, master_port_str);
              WindowPrintString(stat_win, port_stat_str);
              WindowSetLeftMargin(stat_win, 29);
              WindowCursorAt(stat_win, 1, 0);
              WindowPrintString(stat_win, slave_port_str);
              WindowPrintString(stat_win, port_stat_str);
              WindowSetLeftMargin(stat_win, 13);
              WindowCursorAt(stat_win, 0, 0);
              if (master_initialized)
              {
                if (ports[0]->UARTType == UART16550)
                  WindowPrintString(stat_win, "16550");
                else
                  WindowPrintString(stat_win, "8250");
                WindowPrintString(stat_win, "\n\r");
                WindowPrintString(stat_win, ultoa(ports[0]->rbuff->count, "      ", 10));
                WindowPrintString(stat_win, "\n\r");
                WindowPrintString(stat_win, ultoa(ports[0]->rbuff->maxcount, "      ", 10));
                WindowPrintString(stat_win, "\n\r");
                WindowPrintString(stat_win, ultoa(ports[0]->sbuff->count, "      ", 10));
                WindowPrintString(stat_win, "\n\r");
                WindowPrintString(stat_win, ultoa(ports[0]->sbuff->maxcount, "      ", 10));
                WindowPrintString(stat_win, "\n\r");
                if (GetDTR(ports[0]))
                  WindowPrintString(stat_win, "On");
                else
                  WindowPrintString(stat_win, "Off");
                WindowPrintString(stat_win, "\n\r");
                if (GetRTS(ports[0]))
                  WindowPrintString(stat_win, "On");
                else
                  WindowPrintString(stat_win, "Off");
                WindowPrintString(stat_win, "\n\r");
                if (ClearToSend(ports[0]))
                  WindowPrintString(stat_win, "On");
                else
                  WindowPrintString(stat_win, "Off");
                WindowPrintString(stat_win, "\n\r");
                if (DataSetReady(ports[0]))
                  WindowPrintString(stat_win, "On");
                else
                  WindowPrintString(stat_win, "Off");
                WindowPrintString(stat_win, "\n\r");
                if (CarrierDetected(ports[0]))
                  WindowPrintString(stat_win, "On");
                else
                  WindowPrintString(stat_win, "Off");
                WindowPrintString(stat_win, "\n\r");
                if (ports[0]->output_blocked_by_xoff == TRUE)
                  WindowPrintString(stat_win, "Yes");
                else
                  WindowPrintString(stat_win, "No");
                WindowPrintString(stat_win, "\n\r");
                if (ports[0]->output_blocked_by_cts == TRUE)
                  WindowPrintString(stat_win, "Yes");
                else
                  WindowPrintString(stat_win, "No");
                WindowPrintString(stat_win, "\n\r");
                if (ports[0]->input_blocked_by_flow_control == TRUE)
                  WindowPrintString(stat_win, "Yes");
                else
                  WindowPrintString(stat_win, "No");
                WindowPrintString(stat_win, "\n\r");
                WindowPrintString(stat_win, itoa(ports[0]->old_port_state.IER, "      ", 10));
                WindowPrintString(stat_win, "\n\r");
                WindowPrintString(stat_win, itoa(ports[0]->old_port_state.LCR, "      ", 10));
                WindowPrintString(stat_win, "\n\r");
                WindowPrintString(stat_win, itoa(ports[0]->old_port_state.MCR, "      ", 10));
                WindowPrintString(stat_win, "\n\r");
                WindowPrintString(stat_win, itoa(ports[0]->old_port_state.DL, "      ", 10));
                WindowPrintString(stat_win, "\n\r");
                WindowPrintString(stat_win, ultoa((DWORD)(ports[0]->old_port_state.Old_ISR), "      ", 16));
                WindowPrintString(stat_win, "John");
                WindowPrintString(stat_win, "\n\r");
                WindowPrintString(stat_win, itoa(ports[0]->old_port_state.int_enabled, "      ", 10));
              }
              if (slave_initialized)
              {
                WindowSetLeftMargin(stat_win, 42);
                WindowCursorAt(stat_win, 0, 0);
                if (ports[1]->UARTType == UART16550)
                  WindowPrintString(stat_win, "16550");
                else
                  WindowPrintString(stat_win, "8250");
                WindowPrintString(stat_win, "\n\r");
                WindowPrintString(stat_win, ultoa(ports[1]->rbuff->count, "      ", 10));
                WindowPrintString(stat_win, "\n\r");
                WindowPrintString(stat_win, ultoa(ports[1]->rbuff->maxcount, "      ", 10));
                WindowPrintString(stat_win, "\n\r");
                WindowPrintString(stat_win, ultoa(ports[1]->sbuff->count, "      ", 10));
                WindowPrintString(stat_win, "\n\r");
                WindowPrintString(stat_win, ultoa(ports[1]->sbuff->maxcount, "      ", 10));
                WindowPrintString(stat_win, "\n\r");
                if (GetDTR(ports[1]))
                  WindowPrintString(stat_win, "On");
                else
                  WindowPrintString(stat_win, "Off");
                WindowPrintString(stat_win, "\n\r");
                if (GetRTS(ports[1]))
                  WindowPrintString(stat_win, "On");
                else
                  WindowPrintString(stat_win, "Off");
                WindowPrintString(stat_win, "\n\r");
                if (ClearToSend(ports[1]))
                  WindowPrintString(stat_win, "On");
                else
                  WindowPrintString(stat_win, "Off");
                WindowPrintString(stat_win, "\n\r");
                if (DataSetReady(ports[1]))
                  WindowPrintString(stat_win, "On");
                else
                  WindowPrintString(stat_win, "Off");
                WindowPrintString(stat_win, "\n\r");
                if (CarrierDetected(ports[1]))
                  WindowPrintString(stat_win, "On");
                else
                  WindowPrintString(stat_win, "Off");
                WindowPrintString(stat_win, "\n\r");
                if (ports[1]->output_blocked_by_xoff == TRUE)
                  WindowPrintString(stat_win, "Yes");
                else
                  WindowPrintString(stat_win, "No");
                WindowPrintString(stat_win, "\n\r");
                if (ports[1]->output_blocked_by_cts == TRUE)
                  WindowPrintString(stat_win, "Yes");
                else
                  WindowPrintString(stat_win, "No");
                WindowPrintString(stat_win, "\n\r");
                if (ports[1]->input_blocked_by_flow_control == TRUE)
                  WindowPrintString(stat_win, "Yes");
                else
                  WindowPrintString(stat_win, "No");
              }
              while (GetAKey() == 0)
                ;
              DestroyWindow(stat_win);
            }
            break;
#endif

            case KEY_ALT_U:
              UploadMenu();
              break;

            case KEY_ALT_X:
              ExitILink();
              break;

            case KEY_ALT_Z:
              ShowHelp();
              break;

            case KEY_ALT_1:
              SerialWrite(ports[0], 0x1d);
              SerialWrite(ports[0], (BYTE)1);
              break;

            case KEY_ALT_2:
              SerialWrite(ports[0], 0x1d);
              SerialWrite(ports[0], (BYTE)2);
              break;

            case KEY_ALT_3:
              SerialWrite(ports[0], 0x1d);
              SerialWrite(ports[0], (BYTE)3);
              break;

            case KEY_ALT_PGUP:
              SerialWrite(ports[0], 0x1d);
              SerialWrite(ports[0], page < 2 ? page + 2 : 1);
              break;

            case KEY_ALT_PGDN:
              SerialWrite(ports[0], 0x1d);
              SerialWrite(ports[0], page > 0 ? page : 3);
              break;
            }
        }
      }
    }
    /* Get character from remote */

#ifdef FROMFILE
    if (!feof(debug_input_file))
#else
    if (master_initialized && ReadySerial(ports[0]))
#endif
    {
#ifdef FROMFILE
      ch = (BYTE)fgetc(debug_input_file);
#else
      ch = SerialRead(ports[0]);
#endif
      if (log_file_open == TRUE)
        fputc(ch, log_file);
      if (slave_port_online == TRUE)
        SerialWrite(ports[1], ch);
      if (strip_high_bit_on == TRUE)
        ch = (char)(ch & 0x7f);
      (*term->CharHandler)(term, term->translation_table_p->table[ch]);
      if (ch == 0x0d && add_lf_on == TRUE)
        (*term->CharHandler)(term, term->translation_table_p->table[0x0a]);
    }

    if (master_initialized)
    {
      if (GetInputFlowChanged(ports[0]) && !(*term->StatBar)(term))
        ShowChOnStatusBar(23, (char)(GetInputFlowStatus(ports[0]) ? 'I' : ' '));
    }

    /* Get character from slave port */

    if (slave_port_online == TRUE && slave_initialized && ReadySerial(ports[1]) != FALSE)
    {
      ch = SerialRead(ports[1]);
      for (x = 0; x < remote_menu_str_len; x++)
        slave_input[x] = slave_input[x + 1];
      slave_input[remote_menu_str_len] = (char)ch;
      if (strcmp(slave_input, remote_menu_string) == 0)
        ShowSlaveMenu();
      SerialWrite(ports[0], ch);
      if (local_echo_on == TRUE)
      {
        if (log_file_open == TRUE)
          fputc(ch, log_file);
        if (strip_high_bit_on == TRUE)
          ch = (char)(ch & 0x7f);
        (*term->CharHandler)(term, term->translation_table_p->table[ch]);
        if (ch == 0x0d && add_lf_on)
          (*term->CharHandler)(term, term->translation_table_p->table[0x0a]);
      }
      if (GetInputFlowChanged(ports[1]) && !(*term->StatBar)(term))
        ShowChOnStatusBar(48, (char)(GetInputFlowStatus(ports[1]) ? 'I' : ' '));
    }

    /* Check to see if the terminal emulation module has taken control
       of the status bar or released control of it */
    if ((*term->StatBarChanged)(term))
    {
      if ((*term->StatBar)(term))
      {
        ClearStatusBar();
        (*term->DisplayStatusMessage)(term);
      }
      else
        RefreshStatusBar();
    }
#ifndef DEBUG
    if (!(*term->StatBar)(term))
    {
      OutTextAt(BOTROW, 34, ConvertDOSTimeToString("        "));
      if (master_port_online == TRUE)
        OutTextAt(BOTROW, 43, ConvertTimeToString(TimerValue(online_time) / 100, "        "));
    }
#endif
  }
}

#ifndef DEBUG
void _nullcheck(void){};
#endif
