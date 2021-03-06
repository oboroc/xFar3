#ifndef tools_hpp
#define tools_hpp
#include "types.hpp"
#include "plugin.hpp"

bool  compareMemoryIgnoreCase(u8* p1, u8* p2, u16 size);
char* pointToName    (char *path);
char* pointToExt     (char *path);
void  addEndSlash    (char *path);
char* getMsg         (int msgId);
void  initDialogItems(InitDialogItem *init, FarDialogItem *item, int noItems);
int   messageBox     (u32 flags, char **items, int noItems, int noButtons);
bool  compareMemory  (void* p1, void* p2, u16 size);
char* cropLongName   (char* newName, const char* name, int noChars);
bool  isValidChar    (char ch);
char* makeFullName   (char* fullName, const char* path, const char* name);

void  makeCompatibleFileName  (char* newName, char* oldName);
void  makeCompatibleFolderName(char* newName, char* oldName);

#endif
