#define StatBarLEFT 52

#define s_term_emul struct terminal_emulations
struct terminal_emulations
{
  BOOLN enabled;
  char *name;
  char hotkey;
  term_obj *(*InitTerm) (void);
  void (*SetEmulation) (void);
};

#define s_fkey_table struct fkey_table
struct fkey_table
{
  char name[9];
  dlinkedlist *fn_key_defs;
};

#define s_fkey_def struct fkey_def
struct fkey_def
{
  WORD key;
  char *definition;
};

#define s_fkey_names struct fkey_names
struct fkey_names
{
  char *name;
  WORD key;
};

static void AskAndLoadSetup    (void);
static void NotImplemented     (void);
       void ShowChOnStatusBar  (int, int);
static void ShowSerialStatus   (s_com_port *, int);
       void ShowStrOnStatusBar (int, const char *);
static void ShowStatusMaster   (void);
static void ShowStatusSlave    (void);
static void ClearStatusBar     (void);
       void RefreshStatusBar   (void);
static void ShowTitleScreen    (void);
static void FunctionKeyDB      (void);
static void ScreenColorDB      (void);
static void ToggleSlave        (void);
static void MasterCommParamDB  (void);
static void SlaveCommParamDB   (void);
static  int MasterOrSlave      (void);
static void COMPortDB          (void);
static void SaveSetup          (void);
static void LoadSetup          (void);
s_xlate_table *LoadTranslationTable (char *);
static void LogFile            (void);
static void GetInput           (char *, char *, int);
static void DownloadMenu       (void);
static void UploadMenu         (void);
static void DisableHostDisplay (void);
static void EnableHostDisplay  (void);
static void ShowSlaveMenu      (void);
       void EndProgram         (void);
static void Setup              (void);
static void ShowHelp           (void);
       void SwitchPage         (WORD);
       void LoadFNKeyTable     (char *, s_fkey_table **);
       void DestroyFNKeyTable  (s_fkey_table **);
       void RunProg            (char *);
