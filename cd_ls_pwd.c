/************* cd_ls_pwd.c file **************/


// Changes PROC (running->cwd) if given valid dir to point at new minode
int chdir(char *pathname)   
{
  int ino = getino(pathname);

  // Get the minode giving the device and the inode number (unique)
  MINODE *mip = iget(dev, ino);

  // Get the actual INODE from the minode we found
  INODE *in = &(mip->INODE);

  // Check if the mode is a directory before changing cwd
  if(min_isdir(mip))
  {
    // Release the old cwd
    iput(running->cwd);

    // The new cwd is the directory inode we found
    running->cwd = mip;
  }
  else
  {
      return 1;
  }

  return 0;
}

// Given a single file name and its MINODE provides single ls-l line with newl
int ls_file(MINODE *mip, char *name)
{
    INODE *inode = &(mip->INODE);

    int bits[16];

    char time[1024];

    char *t1 = "rwxrwxrwx       ";

    // Start the string off with file type
    if(min_isdir(mip))
      printf("d");

    if(min_isreg(mip))
      printf("-");

    if(min_issymlink(mip))
      printf("l");

      // Create a bit array of INODE->i_mode to read file permissions
      int_to_bits(inode->i_mode, 16, bits);

      // We must start to read the array from the 7th bit, this is where permission data is
      for(int i = 7; i < 16; i++)
      {
          if(bits[i] == 1)
            printf("%c", t1[i-7]);
          else
            printf("-");
      }

      printf(" %d", inode->i_links_count);
      printf(" %d", inode->i_gid);
      printf(" %d", inode->i_uid);
      printf(" %d", inode->i_size);

      // Print the time
      time_t seconds = (time_t)inode->i_ctime;
      char * t = ctime(&seconds);
      bzero(time, 1024);
      strcpy(time, t);
      time[strlen(time) - 1] = 0;

      printf(" %s", time);

      printf(" %s", name);

      if(min_issymlink(mip))
      {
        if(inode->i_links_count > 0)
        {
          printf(" => %s", (char *)&(inode->i_block[0]));
        }
      }

      printf("  [dev %d]", mip->dev);
      printf("[ino %d]", mip->ino);

      printf("\n");
}

// DIR ONLY ls -l all entries in a directory pointed to by mip
int ls_dir(MINODE *mip)
{
  //WE KNOW *mip is the target dir, verified in ls

  char buf[BLKSIZE], temp[256];
  DIR *dp;
  char *cp;

  MINODE *mt;
  
  // Assume DIR has only one data block i_block[0]
  get_block(dev, mip->INODE.i_block[0], buf); 
  dp = (DIR *)buf;
  cp = buf;
  
  while (cp < buf + BLKSIZE)
  {
     strncpy(temp, dp->name, dp->name_len);
     temp[dp->name_len] = 0;
	
     // Get the minode of the current found ino
     mt = iget(dev, dp->inode);

     // Ls at that location
     ls_file(mt, temp);

     cp += dp->rec_len;
     dp = (DIR *)cp;
  }
}

// ls -l for a file or directory at pathname
int ls(char *pathname)  
{
    MINODE *t_cwd = running->cwd;
    int t_dev = dev;

    // If there is no argument ls the running procs cwd
    if(strlen(pathname) == 0)
    {
       ls_dir(running->cwd);
       return 0;
    }

    int ino = getino(pathname);

    if(ino == 0)
    {
      printf("That file/dir does not exist\n");
      return 1;
    }

    MINODE * mip = iget(dev, ino);

    // If the minode we found is a directory call ls_dir()
    if(min_isdir(mip))
    {
        ls_dir(mip);
    }
    else
    {
        int i = 0;

        while(name[i+1] != 0)
        {
          i++;
        }

        // Call lsfile giving the minode and name
        ls_file(mip, name[i]);
    }

    running->cwd = t_cwd;
    dev = t_dev;

    return 0;
}

// DIR ONLY Gets a MINODE parent inode directly from i_block[0]
int get_parent_ino(MINODE *wd)
{
    char buf[1024];
    bzero(buf, 1024);

    // Get the first block from the given minode, give to buf
    get_block(dev, wd->INODE.i_block[0], buf);

    // Assess buf as a DIR structure
    DIR *direc = (DIR *)buf;

    // Let bp point at buf
    char *bp = buf;

    // Advance bp by the previous direc len in byte
    bp += direc->rec_len;

    // Cast bp to direc again (will point at next entry)
    direc = (DIR *)bp;

    return direc->inode;  
}

// DIR ONLY Gets the name of a parents child given the childs ino
int get_name(MINODE *parent, int target_ino, char *fill_buf, int len)
{
    char buf[1024];
    bzero(buf, 1024);
    bzero(fill_buf, len);
   // Get the first block from the given minode, give to buf
    get_block(dev, parent->INODE.i_block[0], buf);

    // Assess buf as a DIR structure
    DIR *direc = (DIR *)buf;

    // Let bp point at buf
    char *bp = buf;

    while(bp < (buf + 1024))
    {
       direc = (DIR *)bp;
       
       if(direc->inode == target_ino)
       {
          strncpy(fill_buf, direc->name, direc->name_len);

          return 0;
       }

       bp += direc->rec_len;
    }
 

    return 1;
}

// DIR ONLY Gets the inode number of a MINODE directly from i_block[0]
int get_ino(MINODE *wd)
{
    char buf[1024];
    bzero(buf, 1024);

    // Get the first block from the given minode, give to buf
    get_block(dev, wd->INODE.i_block[0], buf);

    // Assess buf as a DIR structure
    DIR *direc = (DIR *)buf;

    return direc->inode;
}

// DIR ONLY Just calls rpwd()
int pwd(MINODE *wd)
{
  rpwd(wd);
}

// DIR ONLY Pwd
void rpwd(MINODE *wd)
{
    // If the calling minode is the root
    if(wd->ino == 2)
    {
      printf("/");
      return;
    }

    // Get wd's parents num
    int parent_ino = get_parent_ino(wd);

    // Get wd's num
    int my_ino = get_ino(wd);

    // Get wd's parents MINODE
    MINODE *pip = iget(dev, parent_ino);

    char myname[1024];
    bzero(myname, 1024);

    // Get my name given parents MINODE
    get_name(pip, my_ino, myname, 1024);

    rpwd(pip);

    printf("%s/", myname);
}