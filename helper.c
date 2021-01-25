/************* helper functions file **************/

//Given a/b/c, GLOBAL dname[] = a/b, GLOCAL bname = c
void dnamebname(char *pathname)
{
    int len = strlen(pathname);
    bzero(dname, BUF_SIZE);
    bzero(bname, BUF_SIZE);

    int r = 0;
    if (pathname[0] == '/')
        r = 1;

    for(int i = len-1; i >= 0; i--)
    {
        if(pathname[i] == '/')
        {
           pathname[i] = 0;
           strcpy(bname, &pathname[i+1]);
           break;
        }
    }

    // If the base name wasnt found it means it should be found in cwd, set dname to . and bname to pathname
    if(bname[0] == 0 && r != 1)
    {
      strcpy(bname, pathname);
      strcpy(dname, ".");
    }
    else if(len == (strlen(bname) + 1) && r == 1)
    {
        strcpy(dname, ".");
    }
    else
    {
      strcpy(dname, pathname);      
    }
}

// Returns 1 if reps dir
int min_isdir(MINODE *mip)
{
  INODE *in = &(mip->INODE);

  if((in->i_mode & S_IFMT) == S_IFDIR)
    return 1;

  return 0;
}

// Returns 1 if reps regular file
int min_isreg(MINODE *mip)
{
  INODE *in = &(mip->INODE);

  if((in->i_mode & S_IFMT) == S_IFREG)
    return 1;

  return 0;
}

// Returns 1 if reps symlink file
int min_issymlink(MINODE *mip)
{
  INODE *in = &(mip->INODE);

  if((in->i_mode & S_IFMT) == S_IFLNK)
    return 1;

  return 0;
}

// Source: stackoverflow.com question #31577866
void int_to_bits(unsigned int n, int count, int *arr)
{
    bzero(arr, count);

    unsigned int k = 1U << (count - 1);

    for(int i = 0; i < count; i++)
    {
        arr[i] = (n & k) ? 1 : 0;
        n <<= 1;
    }
}

// retrieve mount table information for a device
MTABLE * get_mtable(int dev)
{
    MTABLE *nm;

    for(int i = 0; i < NMTABLE; i++)
    {
        nm = &mtable[i];
        if (nm->dev == dev)
        {
            return nm;
        }
    }

    return 0;
}

// returns true if there is a mounted filesystem with the given name
int is_mounted(char *name)
{
    MTABLE *mn = &mtable[0];

    for(int i = 0; i < NMTABLE; i++)
    {
        if(strcmp(name, mn->devName) == 0)
        {
            return 1;
        }

        mn++;
    }

    return 0;
}

// gets the dev of a mount <name> or 0 for not mounted
int get_mount_dev(char *name)
{
     MTABLE *mn = &mtable[0];

    for(int i = 0; i < NMTABLE; i++)
    {
        if(strcmp(name, mn->devName) == 0)
        {
            return mn->dev;
        }

        mn++;
    }

    return 0;   
}

// allocates mtable entry with newdev#, return null if dev is in use, -1 if full
MTABLE * mtable_alloc(int newdev, char *name)
{
    // Check if device or name is in use
    if(get_mtable(newdev) || is_mounted(name))
    {
        return 0;
    }
    else
    {
        MTABLE *mn = &mtable[0];

        for(int i = 0; i < NMTABLE; i++)
        {
            if(mn->dev == 0)
            {
                mn->dev = newdev;
                strcpy(mn->devName, name);
                return mn;
            }
            mn++;
        }
    }

    printf("ERROR: mtable_alloc: mtable full, could not allocate\n");

    return 0;
}

// returns 1 if dir is empty, 0 otherwise
int isEmptyDir(MINODE *mip)
{
    if(!min_isdir(mip))
    {
        printf("isEmpty_Dir(): mip was not a dir\n");
        return -1;
    }

    char buf[1024];
    bzero(buf, 1024);

    int c = 0;

   // Get the first block from the given minode, give to buf
    get_block(dev, mip->INODE.i_block[0], buf);

    // Assess buf as a DIR structure
    DIR *direc = (DIR *)buf;

    // Let bp point at buf
    char *bp = buf;

    while(bp < (buf + 1024))
    {
       direc = (DIR *)bp;
       
       if(direc->inode != 0)
       {
          //increment c for each existing entry
          c++;
       }

       bp += direc->rec_len;
    }

    if(c <= 2)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

// Clears the screen
int clear()
{
  printf("\033[H\033[J");

  return 0;
}

// Fills buf with permissions string from a file pointed to my mip
// NO ERROR CHECKING
void get_permissions(MINODE *mip, char buf16[16])
{
    char *perm = "rwxrwxrwx       ";
    int bits[16];
    bzero(bits, 16);
    INODE *m_inode = &mip->INODE;
    int_to_bits(m_inode->i_mode, 16, bits);

    bzero(buf16, 16);

    int i, j;

    for(i = 7,j = 0; i < 16; i++, j++)
    {
          if(bits[i] == 1)
            buf16[j] = perm[i-7];
          else
            buf16[j] = '-';
    }
}

// Gets oft entry for an mip or 0
OFT *get_oft(MINODE *mip)
{
    OFT *of;
    MINODE *t;

    for(int i = 0; i < NOFT; i++)
    {
        of = &OpenFiles[i];
        t = of->mptr;

        if(t != 0)
        {
            if(t->ino == mip->ino)
            {
               return of;
            }
        }
    }
    return 0;
}

// Returns free open file descriptor
OFT *oft_alloc()
{
    OFT *of;

    for (int i = 0; i < NOFT; i++)
    {
        of = &OpenFiles[i];

        if (of->mptr == 0)
        {
            of->mode = 0;
            of->refCount = 0;
            of->offset = 0;

            return of;
        }
    }

    printf("oft_alloc() could not allocate oft index\n");

    return 0;
}

// Removes open file descriptor with mip from the list
int oft_dalloc(MINODE *mip)
{
    OFT *of;
    MINODE *t;

    for (int i = 0; i < NOFT; i++)
    {
        of = &OpenFiles[i];

        t = of->mptr;

        if(t != 0)
        {
            if(t->ino == mip->ino)
            {
                of->mode = 0;
                of->refCount = 0;
                of->mptr = 0;
                of->offset = 0;

                return 1;
            }
        }
    }

    return 0;
}

// return 1 if a file is open for a specific mode, else returns 0
int f_isopen(MINODE *mip, int mode)
{
    OFT *of;
    MINODE *t;

    for(int i = 0; i < NOFT; i++)
    {
        of = &OpenFiles[i];
        t = of->mptr;

        if(t != 0)
        {
            if(t->ino == mip->ino)
            {
                if(of->mode == mode)
                {
                    return 1;
                }
            }
        }
    }

    return 0;
}

// gets command index from list of commands
int getCommand(char *command)
{
    for(int i = 0; commands[i] != 0; i++)
    {
      if(strcmp(commands[i], command)==0)
      {
        return i;
      }
    }

    return -1;
}

// show block map in binary format
int showbmap()
{
  int i, k;
  char buf[BLKSIZE];

  printf("dev=%d bmap=%d nblocks=%d\n", dev, bmap, nblocks);

  get_block(dev, bmap, buf);

  k = 0;
  for (i=0; i<64; i++)
  {
    if (tst_bit(buf, i))
    {
	    printf("1");
    }
    else
    {
        printf("0");
    }

    k++;

    if ((k % 8)==0)
    {
      printf(" ");
    }
  }

  printf("\n");

  return 0;
}

void print_menu()
{
    printf("           MENU\n");
    printf("------------------------------\n");

    for(int i = 0; commands[i] != 0; i++)
    {
        printf("<%s> ", commands[i]);

        if((i + 1) % 4 == 0)
        {
            printf("\n");
            printf("------------------------------\n");
        }
    }
}
