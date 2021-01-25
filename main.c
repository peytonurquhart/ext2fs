/****************************************************************************
      *                       Main function                           *
*****************************************************************************/
#define BUF_SIZE 1024

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ext2fs/ext2_fs.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>

#include "type.h"

// Disk image file default to "diskimage" unless specified in argv
//char *disk = "diskimage";
char *disk = "d2";

MTABLE mtable[NMTABLE];

MINODE minode[NMINODE];
MINODE *root;

PROC proc[NPROC];
PROC *running;
OFT OpenFiles[NOFT];

char gpath[128]; // for use with tokenize() --> name[]
char *name[32];  // assume at most 32 components in pathname
int n;           // number of component strings


char dbpath[BUF_SIZE];  //for use with dname and bname tokenization
char dname[BUF_SIZE];
char bname[BUF_SIZE];

int fd, dev;
int nblocks;
int ninodes;
int bmap;
int imap;
int inode_start;

MINODE *iget();

char *commands[] = {"ls", "cd", "pwd", "mkdir", "creat", "quit", 
                    "rmdir", "clear", "link", "unlink", "symlink", 
                    "pfd", "close", "open", "readlink", "isempty", 
                    "cat", "writeline", "cp", "mount", "umount", "menu", 0};

// Include here, these files use globals defined above
#include "helper.c"
#include "mount_root.c"
#include "cd_ls_pwd.c"
#include "mkdir_creat.c"
#include "rmdir.c"
#include "u.c"   
#include "link_unlink.c"
#include "symlink.c"
#include "open_close.c"
#include "read_cat.c"
#include "write_cp.c"
#include "mount_umount.c"


int main(int argc, char *argv[ ])
{
  int ino;
  char buf[BLKSIZE];
  char line[128], cmd[32], pathname[128], arg3[128];

  // use different disk if specified in argv
  if (argc > 1)
  {
    disk = argv[1];
  }

  // init procs and minodes
  init();  
  //mount root
  mount_root(disk);

  // print stats
  printf("nblocks = %d\nninodes = %d\n", nblocks, ninodes);
  // print root ref count
  printf("root refcount = %d\n", root->refCount);

  printf("uid:%d gid:%d\n", running->uid, running->gid);

  showbmap();
  
  // infinite command loop
  while(1)
  {
    // print input line
    printf("<"); pwd(running->cwd); printf(">: ");

    fgets(line, 128, stdin);
    line[strlen(line)-1] = 0;

    if (line[0]==0)
       continue;
    pathname[0] = 0;

    bzero(cmd, 32); bzero(pathname, 128); bzero(arg3, 128);
    sscanf(line, "%s %s %s", cmd, pathname, arg3);

    //Temps
    int f, r;
    char *t;
    MINODE *mx;

    switch(getCommand(cmd))
    {
       case 0:          //ls
       ls(pathname);
       break;

       case 1:          //cd
       chdir(pathname);
       break;

       case 2:          //pwd
       pwd(running->cwd);
       printf("\n");
       break;

       case 3:          //mkdir
       make_dir(pathname);
       break;

       case 4:          //creat
       creat_file(pathname);
       break;

       case 5:          //quit
       quit();
       break;

       case 6:          //rmdir
       rmdir(pathname);
       break;

       case 7:          //clear
       clear();
       break;

       case 8:          //link
       link(pathname, arg3);
       break;

       case 9:          //unlink
       unlink(pathname);
       break;

       case 10:         //symlink
       symlink(pathname, arg3);
       break;

       case 11:         //pfd
       pfd();
       break;

       case 12:         //close
       sscanf(pathname, "%d", &f);
       close_file(f);
       break;

       case 13:         //open
       sscanf(arg3, "%d", &f);
       open_file(pathname, f);
       break;

       case 14:         //readlink        
       t = readlink(pathname);
       printf("readlink: %s\n", t);
       break;

       case 15:         //isempty (dir)
       f = getino(pathname);
       mx = iget(dev, f);
       if (isEmptyDir(mx) == 1)
       {
         printf("true\n");
       }
       else
       {
         printf("false\n");
       }
       break;

       case 16:         //cat
       mycat(pathname);
       break;

       case 17:         //writeline (fd, line)
       sscanf(pathname, "%d", &f);
       arg3[strlen(arg3)] = '\n';
       arg3[strlen(arg3) + 1] = 0;
       r = write_file(f, arg3, 127);
       printf("\n\nWrote %d bytes to %d\n\n", r, f);
       close_file(f);
       break;

       case 18:         //cp
       cp(pathname, arg3);
       break;

       case 19:         //mount
       if(mount_disk(pathname, arg3) == 1)
          printf("Mount successful!\n");
       break;

       case 20:         //umount
       umount_disk(pathname);
       break;

       case 21:
       clear();
       print_menu();
       printf("\n\n");
       break;

       default:
       printf("Command: %s not recognized\n", cmd);
       break;
    } 
  }
}

/*
      NOTES:

      lvl1 and lvl2 done and optimized

*/
