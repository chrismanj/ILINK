#define s_fkey_table struct fkey_table

#define s_xlate_table struct xlate_table
struct xlate_table
{
  char name[9];
  char table[256];
};

#define term_obj struct s_term_obj
struct s_term_obj
{
  void  (*CharHandler)           (term_obj *, BYTE);
  void  (*ShowStatus)            (term_obj *);
  BOOLN  (*KeyBoardLocked)        (term_obj *);
  BOOLN  (*EvaluateKey)           (term_obj *, WORD);
  BOOLN  (*StatBarChanged)        (term_obj *);
  BOOLN  (*StatBar)               (term_obj *);
  void  (*DisplayStatusMessage)  (term_obj *);
  void  (*SaveTerminalSetup)     (term_obj *);
  void  (*ReleaseStatusBar)      (term_obj *);
  void  (*AddConfigMenuItems)    (term_obj *, s_menu *, int);
  void  (*CopyScreenToSlave)     (term_obj *);
  void  (*SetScreenBase)         (term_obj *);
  void  (*SaveTerm)              (term_obj *);
  void  (*RestoreTerm)           (term_obj *);
  void  (*EndTerm)		 (term_obj *);
  s_xlate_table *translation_table_p;
  s_fkey_table  *fn_keys;
  char          *name;
  char          *ClearScreenString;
  int            num_menu_items;
  winhdl        *window;
  WORD          *save_win;
  void          *emulator;
};

term_obj *NewTerm            (BOOLN, char *, char *, char *);
void DefCharHandler          (term_obj *, BYTE);
void DefShowStatus           (term_obj *);
BOOLN DefKeyboardLocked       (term_obj *);
BOOLN DefEvaluateKey          (term_obj *, WORD);
BOOLN DefStatBarChanged       (term_obj *);
BOOLN DefStatBar              (term_obj *);
void DefDisplayStatusMessage (term_obj *);
void DefSaveTerminalSetup    (term_obj *);
void DefReleaseStatusBar     (term_obj *);
void DefAddConfigMenuItems   (term_obj *, s_menu *, int);
void DefCopyScreenToSlave    (term_obj *);
void DefSetScreenBase        (term_obj *);
void DefSaveTerm             (term_obj *);
void DefRestoreTerm          (term_obj *);
void DefDestroyTerm          (term_obj *);
