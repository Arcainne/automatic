/*
 * Auto.c
 * Main executable for the automatic grading utility
 */

#include "Auto.h"

#undef autoError(args...)
// autoError() alternative that includes stack trace
#define autoError(args...) {\
   printf("\a\x1b[31m");\
   autoPrint("ERROR resulting in program crash");\
   autoPrint(args);\
   printf("\x1b[0m");\
   debugPrint("INFO stack trace");\
   debugPrint("CLASS <%s>", classId);\
   debugPrint("ASG <%s>", asgId);\
   debugPrint("PATH <%s>", currentPath());\
   if (asgList) { \
      studentWrite();\
      autoWrite();\
   } else {\
      autoWarn("Stack trace didn't happen because list was not ready");\
   }\
   exit(1);\
}

#define commanded(arg) listGetCur(cmdList) && streq(listGetCur(cmdList), arg)

// Global vars
char cwd[STRLEN]; // Current working directory
char cmd[STRLEN * 2]; // Last read command
List* cmdList; // Last read command stack

char studentId[STRLEN]; // Current student Id
Table* studentTable; // Current student config

// Memory storage
Table* graderTable; // User config

char exeId[5] = "auto";
char exeName[10] = "automatic";
Table* exeTable;
char exeDir[STRLEN]; // Executable directory, called automatic
Table* helpTable; // Executable help strings
Table* macroTable; // Expandable command macros -xxx

char binDir[STRLEN]; // Bin directory in class folder

char classId[STRLEN]; // cmps012b-pt.s15
char classDir[STRLEN]; // Class directory

char asgId[STRLEN]; // pa1
char asgDir[STRLEN]; // Assignment submission directory. called pa1
char asgBinDir[STRLEN]; // Assignment bin directory. called bin/pa1

char configId[STRLEN];
char configDir[STRLEN];

Table* asgTable; // Assignment config
List* asgList; // List of all students
List* asgDupList;

char* keyName = "user.name";
char* keyId = "user.id";

// Temp vars
int tempInt = 0;
char tempString[STRLEN];
List* tempList;

int main(int argc, char **argv) {

   // Initialize program
   requireChangeDir("automatic_config"); // this is where the config will now be, #define maybe?
   exeTable = tableRead("main_config");
   if (!tableGet(exeTable, "exe.name")) tablePut(exeTable, "exe.name", "automatic");
   if (!tableGet(exeTable, "exe.id")) tablePut(exeTable, "exe.id", "auto");
   if (!tableGet(exeTable, "class.id")) autoError("You are required to specify a class ID as 'class.id'");
   if (!tableGet(exeTable, "class.dir")) autoError("You are reqired to specify a class directory as 'class.dir'");
   tablePut(exeTable, "config.id", "automatic_config"); // with future changes, we may allow it to be elsewhere
   tablePut(exeTable, "config.dir", currentPath());
   sprintf(configId, "%s", tableGet(exeTable, "config.id"));
   sprintf(configDir, "%s", tableGet(exeTable, "config.dir"));
   sprintf(classId, "%s", tableGet(exeTable, "class.id"));
   sprintf(classDir, "%s", tableGet(exeTable, "class.dir"));
   asgList = NULL;

   // Get arguments
   // TODO: Actually support arguments
   if(argc == 1) autoError("USAGE %s [flags] [class] assignment", exeId);
   if(argc >= 3 && argv[argc - 2][1] != "-") strcpy(classId, argv[argc - 2]);
   strcpy(asgId, argv[argc - 1]);
   if(streq(asgId, "bin"))
      autoError("ASG <%s> invalid", asgId);
   if (!tableGet(exeTable, "exe.dir")) {
      changeDir("..");
      tablePut(exeTable, "exe.dir", currentPath());
      requireChangeDir(configDir);
   }
   //tablePrint(exeTable, "%s: %s\n");
   //debugPrint("tableGet(exe.name) = %s", tableGet(exeTable, "exe.name"));

   // Get executable info
   helpTable = tableRead("help");
   macroTable = tableRead("macro");

   debugPrint("EXE <%s> loaded", tableGet(exeTable, "exe.dir"));

   // Get class info
   if(streq(classId, "")) {
      changeDir(".");
      chdir("/");
      strtok(cwd, "/");
      char* temp = cwd; // afs
      for(tempInt = 0; tempInt < 3; tempInt++) {
         // 0: cats.ucsc.edu
         // 1: class
         // 2: cmps012b-pt.s15
         chdir(temp);
         temp = strtok(NULL, "/");
         if(tempInt == 1 && strcmp(temp, "class"))
            autoError("CLASS not provided, implicitly or by argument");
      }
      strcpy(classId, temp);
      requireChangeDir(classId);
      strcpy(classDir, currentPath());
   } else {
      //requireChangeDir(classDir); // called anyway
   }
   autoPrint("CLASS <%s> loaded", classId);
   debugPrint("CLASS dir <%s> loaded", classDir);

   // Get bin info
   requireChangeDir(classDir);
   requireChangeDir("bin");
   strcpy(binDir, currentPath());
   debugPrint("BIN <%s> loaded", binDir);

   // Get asg info
   changeDir(classDir);
   requireChangeDir(asgId);
   autoPrint("ASG <%s> loaded", asgId);
   strcpy(asgDir, currentPath());

   // Get asgbin info
   changeDir(binDir);
   assertChangeDir(asgId);
   strcpy(asgBinDir, currentPath());
   changeDir(asgBinDir);
   asgTable = tableRead(asgId);
   debugPrint("ASG bin <%s> loaded", asgBinDir);

   // Get asglist
   changeDir(asgDir);
   asgList = dirList(asgId);
   if(listGetSize(asgList) <= 0)
      autoError("ASG <%s> has no submissions", asgId);

   // Get grader config
   changeDir(binDir);
   assertChangeDir(configDir);
   assertChangeDir("grader");
   sprintf(tempString, "grader_%s", getlogin());
   graderTable = tableRead(tempString);
   tablePut(graderTable, "user.id", getlogin());
   realName(tempString, getlogin());
   tablePut(graderTable, "user.name", tempString);
   autoPrint("GRADER <%s> (%s) loaded", tableGet(graderTable, "user.id"),
         tableGet(graderTable, "user.name"));
   tablePrint(graderTable, "%s: %s\n");

   // Run shell
   autoShell();

   // Free data
   autoWrite();
   return 0;
}

// Auto shell loop
// Assume start at root directory
void autoShell() {
   char *startDir = NULL; // will tell which directory we started on in this run of Auto
   listMoveFront(asgList);
   startDir = listGetCur(asgList);
   if (graderTable) { // attempt to seek to the last place the grader was at before they exited
      char temps[51];
      sprintf(temps, "LastDir_%s_%s", classId, asgId);
      if (startDir = tableGet(graderTable, temps)) if (!listSeek(asgList, startDir)) autoWarn("The directory you were last at no longer exists");
   }
   studentRead();
   while(true) {
      autoPrompt();
      if (cmd[0] == '\0') {
         // do nothing
      } else if(commanded("exit")) {
         autoPrint("INFO would you like to save your changes to <%s>", studentId);
         if(autoAsk("y")) studentWrite();
         break;
      } else if (commanded("first")) {
         studentWrite();
         if (!listMoveFront(asgList)) {
            autoError("Couldn't move to first student <%s>",
                  currentDir());
         }
         studentRead();
      } else if(commanded("next")) {
         studentWrite();
         if(! listMoveNext(asgList)) {
            listMoveFront(asgList);
            autoWarn("INFO end of list, moving to first student <%s>",
                  currentDir());
         }
         studentRead();
      } else if(commanded("prev")) {
         studentWrite();
         if(! listMovePrev(asgList)) {
            listMoveBack(asgList);
            autoWarn("INFO beginning of list, moving to last student <%s>",
                  currentDir());
         }
         studentRead();
      } else if (commanded("last")) {
         studentWrite();
         if (!listMoveBack(asgList)) {
            autoError("Couldn't move to last student <%s>",
                  currentDir());
         }
         studentRead();
      } else if(0 && commanded("skip")) { // This seems to be a copy of previous
         if(! listMovePrev(asgList)) {
            listMoveBack(asgList);
            autoWarn("INFO beginning of list, moving to last student <%s>",
                  currentDir());
         }
         studentRead();
      } else if(commanded("user")) {
         if(listMoveNext(cmdList)) {
            if(commanded("print")) {
               tablePrint(studentTable, "%s: %s\n");
            } else if(commanded("reset")) {
               studentRead();
            } else if(commanded("write")) {
               studentWrite();
               studentRead();
            }
         } else {
            debugPrint("help user");
         }
      } else if (commanded("mail")) {
         sendMail();
         break;
      } else if (commanded("grade")) {
         printf("This method is not yet ready\n");
         continue;
         int autocont = 0;
         if (listMoveNext(cmdList)) {
            if (commanded("all")) {
               autocont = 1;
            }
         }
         int count = -1;
         do {
            if (count < 0) {
               count = 0;
            } else {
               studentWrite();
               if(! listMoveNext(asgList)) {
                  listMoveFront(asgList);
                  autoWarn("INFO end of list, moving to first student <%s>",
                        currentDir());
               }
               //studentRead();
            }
            //printf("This function is not yet available\n");
            requireChangeDir(asgBinDir);
            //requireChangeDir(asgId);
            FILE *exe_test = fopen(asgId, "r");
            if (exe_test) {
               fclose(exe_test);
            } else {
               autoWarn("The assignment grading script, %s, doesn't yet exist in %s/\n",
                     asgId, asgBinDir);
               studentRead();
               break;
            }
            studentRead();
            char stemp[101];
            sprintf(stemp, "cp %s/%s .", asgBinDir, asgId);
            system(stemp);
            sprintf(stemp, "./%s", asgId);
            system(stemp);
            sprintf(stemp, "rm -f %s", asgId);
            system(stemp);
            stemp[0] = '\0';
            while(!streq(stemp, "-e")) {
               printf("Please enter a command (\"-h\" is help)");
               fgets(stemp, 100, stdin);
               if (streq(stemp, "-h")) {
                  printf("");
                  printf("");
               }
            }
            printf("Finished grading %s\n", listGetCur(asgList));
            if (++count == 5) {
               autoPrint("Would you like to quit autograde? [y/<anything>]");
               char stopcont[1024] = {};
               fgets(stopcont, 1023, stdin);
               stopcont[strlen(stopcont) - 1] = '\0';
               if (streq(stopcont, "y")) {
                  break;
               }
               count = 0;
            }
         } while(autocont);

         continue;

         // the following doesn't get executed
         if (strcmp(asgId, "lab6")) {
            printf("Grade automation only ready for lab6\n");
            continue;
         }
         int check = 0;
         do {
            system("cp /afs/cats.ucsc.edu/class/cmps012a-pt.w16/bin/lab6/ErrorDeduct .");
            system("./ErrorDeduct");
            system("rm ErrorDeduct");
            studentWrite();
            if(! listMoveNext(asgList)) {
               listMoveFront(asgList);
               autoWarn("INFO end of list, moving to first student <%s>",
                     currentDir());
            }
            studentRead();
            printf("Take a break and hit enter to continue or type 'y' to stop\n");
            char tempcheck[1024];
            fgets(tempcheck, 1023, stdin);
            if (tempcheck[0] == 'y' && tempcheck[1] == '\n') check = 1;
            //printf("lol:%c", c);
         } while(listGetPos(asgList) > 1 && !check);
         //continue;
         /*
            system("cp /afs/cats.ucsc.edu/users/f/ptantalo/public/LetterHome.class Temp.class");
            List *tempList = dirList("");
            listMoveFront(tempList);
            do {
            if (strstr(".dat", listGetCur(tempList))) {
            printf("Test for LetterHome with %s\n================\n", listGetCur(tempList));
            char lfilen [strlen(listGetCur(tempList)) + 1 + strlen("java Temp ")];
            sprintf(lfilen, "java Temp %s", listGetCur(tempList));
            system(lfilen);
            }
            } while(listMoveNext(tempList));
            listDestroy(tempList);
            system("rm Temp.class");
            printf("\n================\n\nHere are the errors\n================\n");
            system("cat errors");
            printf("\n");
            */
         //insert grade report automation here
      } else {
         /* listString doesn't work yet TODO
            listString(tempString, cmdList);
            system(tempString);
            */
         system(cmd);
      }
   }
}

void studentRead() {
   requireChangeDir(asgDir);
   requireChangeDir(listGetCur(asgList));
   strcpy(studentId, currentDir());
   requireChangeDir(asgBinDir);
   assertChangeDir("student");
   sprintf(tempString, "student_%s", studentId);
   studentTable = tableRead(tempString);
   tablePut(studentTable, keyId, studentId);
   realName(tempString, studentId);
   tablePut(studentTable, keyName, tempString);
   requireChangeDir(asgDir);
   requireChangeDir(listGetCur(asgList));
   autoPrint("STUDENT <%s> (%s) loaded", studentId,
         tableGet(studentTable, keyName));
}

void studentWrite() {
   requireChangeDir(asgBinDir);
   assertChangeDir("student");
   //if(!studentTable) debugPrint("STUDENT <%s> did not have a table.", studentId);
   if(studentTable) tableWrite(studentTable);
   requireChangeDir(asgDir);
   debugPrint("STUDENT <%s> closed", studentId);
   strcpy(studentId, "null");
}

// @param dir: directory to move to
// @return success
// Version of chdir() that also updates cwd string
bool changeDir(char* dir) {
   int ret = chdir(dir);
   getcwd(cwd, sizeof(cwd));
   return ret == 0;
}

// @param dir: directory to move to
// Version of changeDir() that creates dir if nonexistent
void assertChangeDir(char* dir) {
   if(! changeDir(dir)) {
      char temp[1024];
      sprintf(temp, "mkdir %s", dir);
      system(temp);
      changeDir(dir);
      autoWarn("DIR <%s> created", dir);
   }
}

// @ param dir: directory to move to
// Version of changeDir() that exits program if nonexistent
void requireChangeDir(char* dir) {
   if(! changeDir(dir)) {
      autoWarn("INFO could not find directory <%s>", dir);
      autoWarn("INFO listing directories in <%s>", currentDir());
      system("ls -d */");
      autoError("DIR <%s> not accessible", dir);
   }
}

// @return current directory path
char* currentPath() {
   getcwd(cwd, sizeof(cwd));
   return cwd;
}

// @return current directory name
// Warning: Uses strtok()
char* currentDir() {
   getcwd(cwd, sizeof(cwd));
   char* dir = strtok(cwd, "/");
   while(true) {
      char* temp = strtok(NULL, "/");
      if(! temp) return dir;
      dir = temp;
   }
}

void autoPrompt() {
   char temp[STRLEN * 2];
   if(cmdList) listDestroy(cmdList);
   cmdList = NULL;
   while(! cmdList) {
      autoInput(temp, "$");
      strcpy(cmd, temp);
      cmdList = listCreateFromToken(temp, " ");
   }
   listPrint(cmdList);
   listMoveFront(cmdList);
   if(listGetCur(cmdList) && listGetCur(cmdList)[0] == '-') {
      List *expandList = tableGetList(macroTable, &listGetCur(cmdList)[1], " ");
      if(expandList) {
         listRemove(cmdList, listGetCur(cmdList));
         listPrint(expandList);
         listPrint(cmdList);
         listConcat(expandList, cmdList);
         cmdList = expandList;
      } else {
         autoWarn("INFO could not expand macro <%s>", listGetCur(cmdList));
         autoPrompt();
      }
   }
   listMoveFront(cmdList);
}

// @param result: string to hold result of prompt
// Get input from user
void autoInput(char* result, char* prompt) {
   printf("[%s@%s %s]%s ", tableGet(graderTable, "user.id"), exeId, currentDir(),
         prompt);
   result[0] = '\0';
   fgets(result, 1023, stdin);
   if (strlen(result) < 1) {
      debugPrint("autoInput() detected newline");
      return;
   }

   result[strlen(result) - 1] = '\0';
}

// @return result
// Get boolean input from user
bool autoAsk(char *std) {
   char result[STRLEN * 2];
   sprintf(result, "(y/n)[%s]", std);
   autoInput(result, result);
   if(streq(result, "")) strcpy(result, std);

   return streq(result, "y")
      || streq(result, "Y")
      || streq(result, "yes")
      || streq(result, "Yes")
      || streq(result, "YES");
}

// Auto writeback on exit
void autoWrite() {
   requireChangeDir(binDir);
   requireChangeDir(configDir);
   if(exeTable) tableWrite(exeTable);
   if (graderTable) {
      requireChangeDir("grader");
      char temps[51];
      sprintf(temps, "LastDir_%s_%s", classId, asgId);
      tablePut(graderTable, temps, listGetCur(asgList));
      tableWrite(graderTable);
   }
   //requireChangeDir(asgBinDir);
   if(asgList) listDestroy(asgList);
   if(cmdList) listDestroy(cmdList);
   if(tempList) listDestroy(tempList);
   if(helpTable) tableDestroy(helpTable);
   if(macroTable) tableDestroy(macroTable);
   if (asgTable) tableDestroy(asgTable);
   debugPrint("EXE safely exited", exeId);
}

/////////////////////////////////////////////////////////////////////

void testGrade(char *dir) {
   if (strncmp(dir, "pa2", 4)) {
      printf("This method is not yet ready\n");
      return;
   }
   char path[500];
   FILE *fp;
   FILE *fp2;
   struct dirent **fileList;
   sprintf(path, "/afs/cats.ucsc.edu/class/cmps012b-pt.s15/%s", dir);
   int filecount;
   if (chdir(path) != 0) {
      printf("Trouble switching to the specified directory\n");
      return;
   }
   filecount = scandir(".", &fileList, NULL, alphasort);
   char *fl[filecount];
   for (int l = 0; l < filecount; l++) {
      fl[l] = fileList[l]->d_name;
   }
   char temps[501];
   char temps1[501];
   char mode = '2';
   List *l = listCreate(fileList, 2);
   List *ltemp = NULL;
   if (!l) {
      printf("Error compiling list");
      return;
   }
   sprintf(temps, "/afs/cats.ucsc.edu/class/cmps012b-pt.s15/%s/%s", dir, l->first->sdir);
   chdir(temps);
   printf("Type '-h' for help\n");
   Node *temp = l->first;
   while (1) {
      printf("auto> ");
      gets(temps1);
      if (strncmp(temps1, "-h", 2) == 0) {
         printf("List of commands:\n-f - go to first directory\n-l - go to last directory\n-n - go to next directory\n-p - go to previous directory\n-ce - check Extrema.java\n-ct - check tests.txt\n-cd - check diff*\n-co - check out*\n-cg - check peformance.txt and design.txt\n-cm - check Makefile\n-c - check major files\n-m - change modes\n-ftr - filter the list to only have directories that contain (or don't by adding '!') a file\n-ft - fast filter that accepts direct argument and otherwise works like -ftr\n-vdt - open design.txt in vim\n-vpt - open performance.txt in vim\n-vbt - open bugs.txt\n-vgt - open grade.txt in vim\n-e - exit the program securely\n-h - bring up help\ntype anything else and it will be run as a unix command\n");
      } else if (strncmp(temps1, "-f", 3) == 0) {
         temp = l->first;
         chdir("..");
         chdir(temp->sdir);
      } else if (strncmp(temps1, "-n", 3) == 0) {
         if (temp->next) {
            temp = temp->next;
            chdir("..");
            chdir(temp->sdir);
         } else printf("No next directory\n");
      } else if (strncmp(temps1, "-p", 3) == 0) {
         if (temp->prev) {
            temp = temp->prev;
            chdir("..");
            chdir(temp->sdir);
         } else printf("No previous directory\n");
      } else if (strncmp(temps1, "-l", 3) == 0) {
         temp = l->last;
         chdir("..");
         chdir(temp->sdir);
      } else if (strncmp(temps1, "-m", 3) == 0) {
         do {
            printf("Enter a mode: 0 for ungraded, 1 for graded, 2 for mixed: ");
            mode = getchar();
         } while(mode < 48 || mode > 50);
         ltemp = listCreate(fileList, (int) mode - 48);
         if (!ltemp) {
            printf("%s mode has has no directories, try %s mode or mixed mode\n", mode == 48 ? "ungraded" : "graded", mode == 49 ? "ungraded" : "graded");
         } else {
            listDestroy(l);
            l = ltemp;
            ltemp = NULL;
            temp = l->first;
            chdir("..");
            chdir(temp->sdir);
            printf("Changing to mode %d\n", (int) mode - 48);
         }
      } else if (strncmp(temps1, "-e", 3) == 0) {
         getGrades(dir);
         printf("Exiting program\n");
         break;
      } else if (strncmp(temps1, "-vpt", 5) == 0) {
         fp = fopen("performance.txt", "r");
         if (!fp) {
            sprintf(temps1, "cp /afs/cats.ucsc.edu/class/cmps012b-pt.s15/bin/%s/performance.txt .", dir);
            system(temps1);
         } else fclose(fp);
         system("vi performance.txt");
      } else if (strncmp(temps1, "-vdt", 5) == 0) {
         fp = fopen("design.txt", "r");
         if (!fp) {
            sprintf(temps1, "cp /afs/cats.ucsc.edu/class/cmps012b-pt.s15/bin/%s/design.txt .", dir);
            system(temps1);
            fp = fopen("design.txt", "a");
            fprintf(fp, "\n\n");
            fp2 = fopen("design.temp", "r");
            if (fp2) while (fgets(temps1, 500, fp2)) {
               fprintf(fp, "%s", temps1);
            }
            if (fp2) fclose(fp2);
         }
         fclose(fp);
         system("vi design.txt");
      } else if (strncmp(temps1, "-vgt", 5) == 0) {
         system("vi grade.txt");
         //} else if (strncmp(temps1, "-pvgt", 6) == 0) {
         //  fp = fopen("grade.txt", "w");
         //  fprintf(fp, "%s points\n", (strncmp(dir, "pa3", 4) == 0 || strncmp(dir, "pa2", 4) == 0 || strncmp(dir, "pa1", 4)) ? "80/80" : "100/100");
         //  fclose(fp);i
   } else if (strncmp(temps1, "-vbt", 5) == 0) {
      system("vi bugs.txt");
   } else if (strncmp(temps1, "-c", 3) == 0) {
      system("more tests.txt");
      system("more *");
      fp = fopen("bugs.txt", "r");
      printf("%s", fp ? "::::::::::::::\nBUGS.TXT EXISTS!\n" : "");
      if (fp) fclose(fp);
   } else if (strncmp(temps1, "-cc", 4) == 0) {
      system("more tests.txt bugs.txt performance.txt design.txt design.temp diff1 diff21 diff31 diff41 Search.java Makefile README tests.txt");
   } else if (strncmp(temps1, "-ct", 4) == 0) {
      system("more tests.txt");
   } else if (strncmp(temps1, "-cm", 4) == 0) {
      system("more Makefile");
   } else if (strncmp(temps1, "-cd", 4) == 0) {
      system("more diff*");
   } else if (strncmp(temps1, "-cg", 4) == 0) {
      system("more performance.txt design.txt");
   } else if (strncmp(temps1, "-ce", 4) == 0) {
      system("more Search.java");
   } else if (strncmp(temps1, "-co", 4) == 0) {
      system("more out*");
   } else if (strncmp(temps1, "-pos", 5) == 0) {
      printf("Your current position is %d out of %d\n", listGetPos(l), listGetSize(l));
   } else if (strncmp(temps1, "-ftr", 5) == 0) {
      printf("Please enter the name of the text file you would like to filter for (add '!' to front of name to make it filter to directories without the file): ");
      fgets(temps1, 500, stdin);
      //printf("The thing being filtered is: %s which is size %d\n", temps1, strlen(temps1));
      int tempint = listGetSize(l);
      listFilter(l, dir, temps1);
      if (tempint != listGetSize(l)) {
         temp = l->first;
         chdir("..");
         chdir(temp->sdir);
      }
   } else if (strncmp(temps1, "-ft ", 4) == 0) {
      //printf("Please enter the name of the text file you would like to filter for (add '!' to front of name to make it filter to directories without the file): ");
      strncpy(temps, temps1 + 4, 500);
      temps[strlen(temps)] = '\n';
      temps[strlen(temps) + 1] = '\0';
      //printf("The thing being filtered is: %s which is size %d\n", temps, strlen(temps));
      int tempint = listGetSize(l);
      listFilter(l, dir, temps);
      if (tempint != listGetSize(l)) {
         temp = l->first;
         chdir("..");
         chdir(temp->sdir);
      }
   } else {
      //printList(l);
      //printf("The size of the list is %d\n", listGetSize(l));
      system(temps1);
   }
   }
   for (int i = 0; i < filecount; i++)
      free(fileList[i]);
   free(fileList);
   listDestroy(l);
}

void autoGrade(char *dir) {
   //printf("This method is not yet ready\n");
   //return;
   if (1 || strncmp(dir, "pa2", 4)) {
      printf("Auto grade function is not yet available for this directory\n");
      return;
   }
   char execfile[10];
   //sprintf(dir, "pa4");
   //printf("%s\n", dir);
   sprintf(execfile, "m%s", dir);
   //printf("%s\n", dir);
   char path[500];
   FILE *fp;
   FILE *fp2;
   FILE *fp3;
   FILE *fp4 = NULL;
   struct dirent **fileList;
   sprintf(path, "/afs/cats.ucsc.edu/class/cmps012b-pt.s15/%s", dir);
   //printf("%s\n", path);
   int filecount;
   chdir("..");
   if (chdir(dir) != 0) {
      printf("Trouble switching to %s\n", dir);
      return;
   }
   filecount = scandir(".", &fileList, NULL, alphasort);
   char temp[501] = "";
   char temp1[501] = "";
   int diff = 0;
   for (int i = 2; i < filecount; i++) { //start at i = 2 because we are ignoring the "." and ".." directories
      //printf("test\n");
      diff = 0;
      if (chdir(fileList[i]->d_name) != 0) {
         printf("Error changing directories\n");
         return;
      }
      sprintf(temp, "cp /afs/cats.ucsc.edu/class/cmps012b-pt.s15/bin/%s/%s /afs/cats.ucsc.edu/class/cmps012b-pt.s15/%s/%s", dir, execfile, dir, fileList[i]->d_name);
      system(temp);
      fp = fopen("tests.txt", "w");
      fp4 = fopen("design.temp", "w");
      system("make");
      fp3 = fopen("Search", "r");
      fprintf(fp, "Makefile Test %s\n", fp3 ? "Passed" : "Failed");
      if (fp3) {
         fclose(fp3);
      } else {
         fprintf(fp4, "-5 points - Makefile doesn't make.");
         sprintf(temp, "cp /afs/cats.ucsc.edu/users/d/icherdak/cs12b_pt.s15/%s/mfile .", dir);
         system(temp);
         system("mfile");
         system("rm mfile");
      }
      sprintf(temp, "timeout 25 %s", execfile);
      system(temp);
      fp2 = fopen("diff1", "r");
      fprintf(fp, "User Input Tests %s\n", (!fp2 || fp2 && fgetc(fp2) != EOF) ? "Failed" : "Passed");
      if (fp2) fclose(fp2);
      int scount = 0; // this is how we know that both untit tests passed special tests
      fp2 = fopen("diff21", "r");
      fp3 = fopen("diff22", "r");
      fprintf(fp, "Unit Tests 1 %s\n", (fp2 && fgetc(fp2) == EOF || fp3 && fgetc(fp3) == EOF) ? (fp2 && fgetc(fp2) == EOF ? "Normal Tests Passed" : "Special Tests Passed") : "Failed Tests");
      if (fp3 && feof(fp3)) scount = 1;
      if (fp2) fclose(fp2);
      if (fp3) fclose(fp3);
      fp2 = fopen("diff31", "r");
      fp3 = fopen("diff32", "r");
      fprintf(fp, "Unit Tests 2 %s\n", (fp2 && fgetc(fp2) == EOF || fp3 && fgetc(fp3) == EOF) ? (fp2 && fgetc(fp2) == EOF ? "Normal Tests Passed" : "Special Tests Passed") : "Failed Tests");
      if (fp3 && feof(fp3)) scount = 1;
      if (fp2) fclose(fp2);
      if (fp3) fclose(fp3);
      fp2 = fopen("diff41", "r");
      fp3 = fopen("diff42", "r");
      fprintf(fp, "Multi Target %s\n", (fp2 && fgetc(fp2) == EOF || fp3 && fgetc(fp3) == EOF) ? (fp2 && fgetc(fp2) == EOF ? "Normal Tests Passed" : "Special Tests Passed") : "Tests Failed");
      if (fp3 && feof(fp3)) scount = 1;
      if (scount) {
         //  if (!fp4) fp4 = fopen("design.temp", "w");
         fprintf(fp4, "-10 points - Program doesn't support line numbers\n\n");
      }
      if (!(fp2 && feof(fp2) || fp3 && feof(fp3))) {
         //  if (!fp4) fp4 = fopen("design.temp", "w");
         //fprintf(fp4, "-5 points - Program doesn't support variable arguments\n\n");
      }
      if (fp2) fclose(fp2);
      if (fp3) fclose(fp3);
      if (fp) fclose(fp);
      fp = fopen("README", "r");
      fp2 = fopen("Makefile", "r");
      fp3 = fopen("Search.java", "r");
      if (!fp || !fp2 || !fp3) {
         //  if (!fp4) fp4 = fopen("design.temp", "w");
         if (!fp) fprintf(fp4, "-3 points - Missing/incorrectly named file: README\n\n");
         if (!fp2) fprintf(fp4, "-3 points - Missing/incorrectly named file: Makefile\n\n");
         if (!fp3) fprintf(fp4, "-3 points - Missing/incorrectly named file: Search.java\n\n");
      }
      if (fp) fclose(fp);
      if (fp2) fclose(fp2);
      if (fp3) fclose(fp3);
      if (fp4) fclose(fp4);
      /*fp2 = fopen("diff3", "r");
        fprintf(fp, "Advanced Unit Tests %s\n", (!fp2 || fp2 && fgetc(fp2) != EOF) ? "Failed" : "Passed");
        if (fp2 && fgetc(fp2) == EOF) diff++;
        if (fp2) fclose(fp2);
        fp2 = fopen("diff4", "r");
        fprintf(fp, "Credibility Test %s\n", (!fp2 || fp2 && fgetc(fp2) != EOF) ? "Failed" : "Passed");
        if (fp2 && fgetc(fp2) == EOF) diff++;
        if (fp2) fclose(fp2);*/

      /*
         fp2 = fopen("diff5", "r");
         fprintf(fp, "Test 5 %s\n", (!fp2 || fp2 && fgetc(fp2) != EOF) ? "Failed" : "Passed");
         if (fp2 && fgetc(fp2) == EOF) diff++;
         if (fp2) fclose(fp2);
         fp2 = fopen("diff6", "r");
         fprintf(fp, "\"make\" Test %s\n", (!fp2 || fp2 && fgetc(fp2) != EOF) ? "Failed" : "Passed");
         if (fp2 && fgetc(fp2) == EOF) diff++;
         if (fp2) fclose(fp2);
         fp2 = fopen("diff7", "r");
         fprintf(fp, "\"make submit\" Test %s\n", (!fp2 || fp2 && fgetc(fp2) != EOF) ? "Failed" : "Passed");
         if (fp2 && fgetc(fp2) == EOF) diff++;
         if (fp2) fclose(fp2);
         fp2 = fopen("diff8", "r");
         fprintf(fp, "\"make clean\" Test %s\n", (!fp2 || fp2 && fgetc(fp2) != EOF) ? "Failed" : "Passed");
         if (fp2 && fgetc(fp2) == EOF) diff++;
         if (fp2) fclose(fp2);
         */

      /*if (0 && diff == 8) {
        fp = fopen("grade.txt", "r");
        if (!fp) {
        fp = fopen("grade.txt", "w");
        fprintf(fp, "\n\nAll Test Passed");
        }
        fclose(fp);
        }*/
      sprintf(temp, "rm %s", execfile);
      system(temp);
      system("rm -f *.class Search");
      printf("Finished grading %s for %s\n", fileList[i]->d_name, dir);
      chdir("..");
   }
   for (int i = 0; i < filecount; i++)
      free(fileList[i]);
   free(fileList);
   printf("Grading routine finished\n");
}

void sendMail() {

   changeDir(asgBinDir);
   FILE *mscript = fopen("mailscript", "w");
   fprintf(mscript, "#!/bin/bash\ncd %s\n", asgDir);
   int count = 0; // how many students will be emailed
   listMoveFront(asgList);
   while(1) {
      changeDir(asgDir);
      requireChangeDir(listGetCur(asgList));
      FILE *fp = fopen("grade.txt", "r");
      if (fp) {
         fclose(fp);
         fprintf(mscript, "cd %s\necho \"Mailing %s@ucsc.edu\"\nmailx -s \"grade for %s\" %s@ucsc.edu < grade.txt\ncd ..\nsleep 2\n",
               listGetCur(asgList), listGetCur(asgList), asgId, listGetCur(asgList));
         count++;
      } else {
         autoWarn("%s: No grade.txt", listGetCur(asgList));
      }
      if (!listMoveNext(asgList)) break;
   }
   fprintf(mscript, "echo \"Finished mailing all students\"");
   fclose(mscript);
   changeDir(asgBinDir);
   while(1) {
      autoPrint("Approximate runtime: %d minute%s %d second%s, Would you like to email all students that have been graded? <y/n> [y]",
            (count * 2) / 60, ((count * 2) / 60) == 1 ? "" : "s", (count * 2) % 60, ((count * 2) % 60) == 1 ? "" : "s");
      char ask = getchar();
      //debugPrint("ask is %c with %d", ask, ask);
      if (ask == 'y') {
         system("chmod 700 mailscript");
         system("./mailscript");
         system("rm mailscript");
         debugPrint("Mail Routine Complete");
         break;
      } else if (ask =='n') {
         system("rm mailscript");
         debugPrint("Exiting Program");
         break;
      } else if (ask == '\n') {
         system("chmod 700 mailscript");
         system("./mailscript");
         system("rm mailscript");
         debugPrint("Mail Routine Complete");
         break;
      } else {
         printf("Invalid Command!\nPlease Enter An Appropriate Character <y/n> [y]\n");
      }
   }


   return; // the rest is old stuff

   /*

   //  printf("Mail method not yet available\n");
   //  return;
   char path[500];
   //  sprintf(path, "/afs/cats.ucsc.edu/class/cmps011-pt.w15/bin/%s/mail_all", dir);
   //  FILE *fp = fopen(path, "w");
   FILE *fp;
   struct dirent **fileList;
   sprintf(path, "/afs/cats.ucsc.edu/class/cmps012b-pt.s15/%s", dir);
   int filecount;
   chdir("..");
   if (chdir(dir) != 0) {
   printf("Trouble switching to the specified directory\n");
   return;
   }
   filecount = scandir(".", &fileList, NULL, alphasort);
   FILE *fp2 = fopen("/afs/cats.ucsc.edu/class/cmps012b-pt.s15/bin/mailscript", "w");
   fprintf(fp2, "#!/bin/csh\ncd /afs/cats.ucsc.edu/class/cmps012b-pt.s15/%s\n", dir);
   int mailcount = 0;
   //  char temp[501];
   //  char commands[filecount - 2][501];
   for (int i = 2; i < filecount; i++) { //start at i = 2 because we are ignoring the "." and ".." directories
   chdir(fileList[i]->d_name);
   fp = fopen("grade.txt", "r");
   if (!fp) printf("grade.txt does not exist for %s\n", fileList[i]->d_name);
   else {
   fprintf (fp2, "cd %s\necho \"Mailing %s@ucsc.edu\"\nmailx -s \"grade for %s\" %s@ucsc.edu < grade.txt\ncd ..\nsleep 3\n", fileList[i]->d_name, fileList[i]->d_name, dir, fileList[i]->d_name);
   //      strncpy(commands[i - 2], temp, 500);
   //      printf("%s\n", commands[i - 2]);
   fclose(fp);
   mailcount++;
   }
   chdir("..");
   }
   //  fclose(fp);
   for (int i = 0; i < filecount; i++)
   free(fileList[i]);
   free(fileList);
   fclose(fp2);
   chdir("/afs/cats.ucsc.edu/class/cmps012b-pt.s15/bin");
   while(1) {
   printf("Approximate runtime: %d minute%s %d second%s, Would you like to email all students that have been graded? [y/n]\n", (mailcount * 3) / 60, ((mailcount * 3) / 60) == 1 ? "" : "s", (mailcount * 3) % 60, ((mailcount * 3) % 60) == 1 ? "" : "s");
   char ask = getchar();
   if (ask == 'y') {
   system("chmod 700 mailscript");
   system("mailscript");
   system("rm mailscript");
   printf("Mail Routine Complete\n");
   return;
   } else if (ask =='n') {
   system("rm mailscript");
   printf("Exiting Program\n");
   return;
   } else {
   printf("Invalid Command!\nPlease Enter An Appropriate Character [y/n]\n");
   }
   }

*/
}

void getGrades(char *dir) {
   if (strncmp(dir, "pa2", 4)) {
      printf("This method is currently unavailable\n");
      return;
   }
   char path[500];
   sprintf(path, "/afs/cats.ucsc.edu/class/cmps012b-pt.s15/bin/%s/gradelist.txt", dir);
   FILE *fp = fopen(path, "w");
   FILE *fp2;
   FILE *fp10;
   FILE *fp20;
   FILE *fp30;
   struct dirent **fileList;
   sprintf(path, "/afs/cats.ucsc.edu/class/cmps012b-pt.s15/%s", dir);
   int filecount;
   if (chdir(path) != 0) {
      printf("Trouble switching to the specified directory\n");
      return;
   }
   filecount = scandir(".", &fileList, NULL, alphasort);
   char temp[501];
   char temp1[501];
   int grade1, grade2, grade3, grade4;
   for (int i = 2; i < filecount; i++) { //start at i = 2 because we are ignoring the "." and ".." directories
      chdir(fileList[i]->d_name);
      fp2 = fopen("grade.txt", "r");
      //system("rm -f grade.txt");
      if (1 || !fp2) {
         if (fp2) fclose(fp2);
         fp10 = fopen("performance.txt", "r");
         fp20 = fopen("design.txt", "r");
         fp30 = fopen("bugs.txt", "r");
         if (fp10) {
            sprintf(temp1, "cp performance.txt /afs/cats.ucsc.edu/class/cmps012b-pt.s15/bin/%s/performance/%s_performance.txt", dir, fileList[i]->d_name);
            system(temp1);
         }
         if (fp20) {
            sprintf(temp1, "cp design.txt /afs/cats.ucsc.edu/class/cmps012b-pt.s15/bin/%s/design/%s_design.txt", dir, fileList[i]->d_name);
            system(temp1);
         }
         if (fp30) {
            sprintf(temp1, "cp bugs.txt /afs/cats.ucsc.edu/class/cmps012b-pt.s15/bin/%s/bugs/%s_bugs.txt", dir, fileList[i]->d_name);
            system(temp1);
            fclose(fp30);
         }
         if (fp10 && fp20) {
            fp2 = fopen("grade.txt", "w");
            fscanf(fp10, "%d", &grade3);
            fscanf(fp20, "%d", &grade4);
            grade1 = grade3;
            grade2 = grade4;
            while(fgets(temp1, 500, fp10)) {
               if (temp1[0] == '-') {
                  int index = 1;
                  while (isdigit(temp1[index])) {
                     temp[index - 1] = temp1[index];
                     index++;
                  }
                  temp[index - 1] = '\0';
                  index = strlen(temp) - 1;
                  while (index >= 0) {
                     //printf("Subtracting %d from %d in %s as a result of array: {%s}\n", pow(10, ((int) strlen(temp) - index - 1)) * ((int) temp[index] - 48), grade1, fileList[i]->d_name, temp);
                     grade1 -= pow (10, strlen(temp) - index - 1) * ((int) temp[index] - 48);
                     index--;
                  }
               }
            }
            while(fgets(temp1, 500, fp20)) {
               if (temp1[0] == '-') {
                  int index = 1;
                  while (isdigit(temp1[index])) {
                     temp[index - 1] = temp1[index];
                     index++;
                  }
                  temp[index - 1] = '\0';
                  index = strlen(temp) - 1;
                  while (index >= 0) {
                     //printf("Subtracting %d from %d in %s as a result of array: {%s}\n", pow(10, ((int) strlen(temp) - index - 1)) * ((int) temp[index] - 48), grade2, fileList[i]->d_name, temp);
                     grade2 -= pow(10, strlen(temp) - index - 1) * ((int) temp[index] - 48);
                     index--;
                  }
               }
            }
            fclose(fp10);
            fp10 = fopen("performance.txt", "r");
            fgets(temp1, 500, fp10);
            fclose(fp20);
            fp20 = fopen("design.txt", "r");
            fgets(temp1, 500, fp20);
            //fscanf(fp10, "%d", &grade3);
            //fscanf(fp20, "%d", &grade4);
            fprintf(fp2, "%d/%d points\n", grade1 + grade2, grade3 + grade4);
            //printf("test1");
            fprintf(fp2, "====================\nPerformance\n====================\n\n%d/%d points\n", grade1, grade3);
            while(fgets(temp, 500, fp10)) {
               fprintf(fp2, "%s", temp);
               //printf("test2");
            }
            fprintf(fp2, "\n====================\nDesign\n====================\n\n%d/%d points\n", grade2, grade4);
            while(fgets(temp, 500, fp20)) {
               fprintf(fp2, "%s", temp);
               //printf("test3");
            }
            fprintf(fp2, "\n====================");
            if (fp2) fclose(fp2);
            fp2 = fopen("grade.txt", "r");
         } else {
            if (fp10) fclose(fp10);
            if (fp20) fclose(fp20);
         }
      }
      if (!fp2) printf("grade.txt does not exist for %s\n", fileList[i]->d_name);
      else {
         printf("Finished Compiling %s\n", fileList[i]->d_name);
         if (fgets(temp, 500, fp2)) {
            strncpy (temp1, temp, 501);
            temp1[strlen(temp1) - 1] = '\0';
            sprintf (temp, "%s: %s (Performance: %d points, Design: %d points)\n", fileList[i]->d_name, temp1, grade1, grade2);
            fputs(temp, fp);
         }
         fclose(fp2);
         fclose(fp10);
         fclose(fp20);
      }
      chdir("..");
   }
   fclose(fp);
   for (int i = 0; i < filecount; i++)
      free(fileList[i]);
   free(fileList);
}

void restoreGrades(char *dir) { //buggy
   if (strncmp(dir, "pa", 2)) {
      printf("This method is unavailable for this directory");
      return;
   }
   char path[501];
   //  sprintf(path, "/afs/cats.ucsc.edu/class/cmps011-pt.w15/bin/%s/mail_all", dir);
   //  FILE *fp = fopen(path, "w");
   //FILE *fp;
   struct dirent **fileList;
   sprintf(path, "/afs/cats.ucsc.edu/class/cmps012b-pt.s15/bin/%s/performance", dir);
   int filecount;
   if (chdir(path) != 0) {
      printf("Trouble switching to performance backup directory\n");
      return;
   }
   filecount = scandir(".", &fileList, NULL, alphasort);
   char temp[501];
   //char temp1[501];
   int j;
   for (int i = 2; i < filecount; i++) { //start at i = 2 because we are ignoring the "." and ".." directories
      for (j = 0; fileList[i]->d_name[j] != '_'; j++) {
         path[j] = fileList[i]->d_name[j];
      }
      path[j] = '\0';
      sprintf(temp, "cp %s_performance.txt /afs/cats.ucsc.edu/class/cmps012b-pt.s15/%s/%s/performance.txt", path, dir, path);
      system(temp);
      printf("Restoring performance.txt for %s\n", path);
   }
   for (int i = 0; i < filecount; i++)
      free(fileList[i]);
   free(fileList);
   printf("Restored %d performance.txt files\n", filecount - 2);
   sprintf(path, "/afs/cats.ucsc.edu/class/cmps012b-pt.s15/bin/%s/design", dir);
   if (chdir(path) != 0) {
      printf("Trouble switching to design backup directory\n");
      return;
   }
   filecount = scandir(".", &fileList, NULL, alphasort);
   for (int i = 2; i < filecount; i++) { //start at i = 2 because we are ignoring the "." and ".." directories
      for (j = 0; fileList[i]->d_name[j] != '_'; j++) {
         path[j] = fileList[i]->d_name[j];
      }
      path[j] = '\0';
      sprintf(temp, "cp %s_design.txt /afs/cats.ucsc.edu/class/cmps012b-pt.s15/%s/%s/design.txt", path, dir, path);
      system(temp);
      printf("Restoring design.txt for %s\n", path);
   }
   for (int i = 0; i < filecount; i++)
      free(fileList[i]);
   free(fileList);
   printf("Restored %d design.txt files\n", filecount - 2);
   getGrades(dir);
}
