/*********** util.c file ****************/

extern MINODE minode[NMINODE];
extern MINODE *root;
extern PROC   proc[NPROC], *running;
extern OFT    oft[NOFT];

extern char gpath[128],*name[32]; // assume at most 64 components
extern int  n;
extern int  fd, dev;
extern int  nblocks, ninodes, bmap, imap, inode_start;
extern char pathname[256], parameter[256];

// get block buf from disk with bno=blk
int get_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   read(dev, buf, BLKSIZE);
}   

// write block buf back to disk at bno=blk
int put_block(int dev, int blk, char *buf)
{
   lseek(dev, (long)blk*BLKSIZE, 0);
   write(dev, buf, BLKSIZE);
}   

// tokenize pathname into name[] global, set n = name.count
int tokenize(char *pathname)
{
  int i;
  char *s, t[256];
  strcpy(gpath, pathname);
  n = 0;

  s = strtok(gpath, "/");

  while(s)
  {
    name[n] = s;
    n++;
    s = strtok(0, "/");
  }
}

// return minode pointer to loaded INODE
MINODE *iget(int dev, int ino)
{
  int i;
  MINODE *mip;
  char buf[BLKSIZE];
  int blk, disp;
  INODE *ip;

  for (i = 0; i < NMINODE; i++)
  {
      mip = &minode[i];

      if (mip->dev == dev && mip->ino == ino)
      {
          mip->refCount++;
	        return mip;
      }
  }
    
  for (i=0; i<NMINODE; i++)
  {
      mip = &minode[i];
      if (mip->dev==0 && mip->ino==0)
      {
        mip->refCount = 1;
        mip->dev = dev;
        mip->ino = ino;
 
        blk  = (ino-1)/8 + inode_start;
        disp = (ino-1) % 8;

        get_block(dev, blk, buf);
        ip = (INODE *)buf + disp;

        // copy INODE to mp->INODE
        mip->INODE = *ip;

       return mip;
    }

  }   

  return 0;
}

// put minode mip back to disk
int my_iput(MINODE *mip)
{
 int i, block, offset;
 char buf[BLKSIZE];
 INODE *ip;

 if (mip==0) 
     return;

 mip->refCount--;
 
 if (mip->refCount > 0) return;
 if (!mip->dirty)       return;
 
 /* write back */
 //printf("iput: dev=%d ino=%d\n", mip->dev, mip->ino); 

 block =  ((mip->ino - 1) / 8) + inode_start;
 offset =  (mip->ino - 1) % 8;

 /* first get the block containing this inode */
 get_block(mip->dev, block, buf);

 ip = (INODE *)buf + offset;
 *ip = mip->INODE;

 put_block(mip->dev, block, buf);

} 

// put minode mip back to disk
int iput(MINODE *mip)
{
 int i, block, offset;
 char buf[BLKSIZE];
 INODE *ip;

 if (mip==0) 
     return;

 mip->refCount--;
 
 if (mip->refCount > 0) return;
 if (!mip->dirty)       return;
 
 /* write back */
 //printf("iput: dev=%d ino=%d\n", mip->dev, mip->ino); 

 block =  ((mip->ino - 1) / 8) + inode_start;
 offset =  (mip->ino - 1) % 8;

 /* first get the block containing this inode */
 get_block(mip->dev, block, buf);

 ip = (INODE *)buf + offset;
 *ip = mip->INODE;

 put_block(mip->dev, block, buf);

} 

// Search for name in dir minode
int search(MINODE *mip, char *name)
{
   int i; 
   char *cp, c, sbuf[BLKSIZE];
   DIR *dp;
   INODE *ip;

   if(!min_isdir(mip))
   {
      return 0;
   }

   ip = &(mip->INODE);

   /**********  search for a file name ***************/
   for (i=0; i<12; i++)
   {
        if (ip->i_block[i] == 0)
        {
           return 0;
        }

        get_block(dev, ip->i_block[i], sbuf);
        dp = (DIR *)sbuf;
        cp = sbuf;

        while (cp < sbuf + BLKSIZE)
        {
	          c = dp->name[dp->name_len];
            dp->name[dp->name_len] = 0;

            if (strcmp(dp->name, name)==0)
            {
                return dp->inode;
            }

            dp->name[dp->name_len] = c;
            cp += dp->rec_len;
            dp = (DIR *)cp;
        }
   }

   return 0;
}

// Getino for pathname, supporting cross mount points
int getino(char *pathname)
{
  int i, ino, blk, disp;
  char buf[BLKSIZE];
  INODE *ip;
  MINODE *mip;
  MTABLE *mn;

  int x = 0;

  if (strcmp(pathname, "/") == 0)
  {
      return 2;
  }

  if (pathname[0]=='/')
  {
    mip = iget(dev, 2);
  }
  else
  {
    mip = iget(running->cwd->dev, running->cwd->ino);
  }

  strcpy(buf, pathname);
  tokenize(buf);

  for (i = 0; i < n; i++)
  {
      ino = search(mip, name[i]);

      // Does not exist
      if (ino==0)
      {
         iput(mip);
         return 0;
      }
      else
      {
        iput(mip);

        mip = iget(dev, ino);

        // get off the mount
        if(ino == 2 && dev != root->dev)
        {
            // We must go back TWICE to switch mount points ../../ otherwise we loop to mounted root
            if(x > 0)
            {
              printf("Cross mount points: dev=%d, newdev=%d\n", dev, root->dev);

              mn = get_mtable(dev);

              mip = mn->mntDirPtr;

              dev = mip->dev;
            }

            x++;

        }
        else if(mip->mounted) //get on the mount
        {
            mn = mip->mntPtr;

            if(mn != 0)
            {
                if(dev != mn->dev)
                {  
                    printf("Cross mount points: dev=%d, newdev=%d\n", dev, mn->dev);

                    dev = mn->dev;

                    mip = iget(dev, 2);

                    ino = 2;
                }
            }
            else
            {
                printf("ERROR: getino: MINODE:%d marked mounted will NULL mount table\n", ino);
                return 0;
            }
        }
      }
   }

   iput(mip);

   return ino;
}

// Getino for pathname, not supporting cross mount points
int getino_OLD(char *pathname)
{
  int i, ino, blk, disp;
  char buf[BLKSIZE];
  INODE *ip;
  MINODE *mip;

  printf("getino: pathname=%s\n", pathname);

  if (strcmp(pathname, "/") == 0)
  {
      

      return 2;
  }

  if (pathname[0]=='/')
  {
    mip = iget(dev, 2);
  }
  else
  {
    mip = iget(running->cwd->dev, running->cwd->ino);
  }

  strcpy(buf, pathname);
  tokenize(buf);

  for (i = 0; i < n; i++)
  {
      printf("===========================================\n");
      printf("getino: i=%d name[%d]=%s\n", i, i, name[i]);
 
      ino = search(mip, name[i]);

      if (ino==0)
      {
         iput(mip);
         printf("name %s does not exist\n", name[i]);
         return 0;
      }

      iput(mip);
      mip = iget(dev, ino);
   }

   iput(mip);

   return ino;
}

// not in use, getname in implementation in helper.c
int findmyname(MINODE *parent, u32 myino, char *myname) 
{
 int i;
 char buf[BLKSIZE], temp[256], *cp;  
 DIR    *dp;
 MINODE *mip = parent;

 /**********  search for a file name ***************/
 for (i=0; i<12; i++){ /* search direct blocks only */
     if (mip->INODE.i_block[i] == 0) 
           return -1;

     get_block(mip->dev, mip->INODE.i_block[i], buf);
     dp = (DIR *)buf;
     cp = buf;

     while (cp < buf + BLKSIZE){
       strncpy(temp, dp->name, dp->name_len);
       temp[dp->name_len] = 0;
       //printf("%s  ", temp);

       if (dp->inode == myino){
           strncpy(myname, dp->name, dp->name_len);
           myname[dp->name_len] = 0;
           return 0;
       }
 
       cp += dp->rec_len;
       dp = (DIR *)cp;
     }
 }
 return(-1);
}

// not in use, implementation in helper.c
int findino(MINODE *mip, u32 *myino) 
{
  char buf[BLKSIZE], *cp;   
  DIR *dp;

  get_block(mip->dev, mip->INODE.i_block[0], buf);
  cp = buf; 
  dp = (DIR *)buf;
  *myino = dp->inode;
  cp += dp->rec_len;
  dp = (DIR *)cp;
  return dp->inode;
}
