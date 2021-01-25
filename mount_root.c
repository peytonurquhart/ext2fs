/************* mount root **************/

// init root file structures
int init()
{
  int i, j;
  MINODE *mip;
  PROC   *p;
  MTABLE *nm;
  OFT *of;

  printf("init()\n");

  for(i=0; i<NOFT; i++)
  {
      of = &OpenFiles[i];
      of->mode = 0;
      of->refCount = 0;
      of->mptr = 0;
      of->offset = 0;
  }

  for(i=0; i<NMTABLE; i++)
  {
      nm = &mtable[i];
      nm->dev = 0;
      nm->ninodes = 0;
      nm->nblocks = 0;
      nm->free_blocks = 0;
      nm->free_inodes = 0;
      nm->bmap = 0;
      nm->imap = 0;
      nm->iblock = 0;
      nm->mntDirPtr = 0;
      bzero(nm->devName, 64);
      bzero(nm->mntName, 64);
  }
  for (i=0; i<NMINODE; i++)
  {
    mip = &minode[i];
    mip->dev = mip->ino = 0;
    mip->refCount = 0;
    mip->mounted = 0;
    mip->mntPtr = 0;
  }
  for (i=0; i<NPROC; i++)
  {
    p = &proc[i];
    p->pid = i;
    p->uid = p->gid = 0;
    p->cwd = 0;
    p->status = FREE;
    for (j=0; j<NFD; j++)
      p->fd[j] = 0;
  }

  proc[1].next = &proc[0];
  proc[0].next = &proc[1];

  running = &proc[0];
  running->status = READY;
}

// load root INODE and set root pointer to it
int mount_root(char *rootdev)
{  
  int i;
  MTABLE *mp;
  SUPER *sp;
  GD *gp;
  char buf[BLKSIZE];

  dev = open(rootdev, O_RDWR);
  
  if(dev < 0)
  {
    printf("Mount Root: Cant open root device\n");
    exit(1);
  }

  //get super block of root dev
  get_block(dev,1,buf);
  sp = (SUPER *)buf;

  if (sp->s_magic != 0xEF53)
  {
    printf("Not an EXT2 file system! Cant continue\n");
    exit(0);
  }

  //fill mount table m_table[0]

  // get pointer to 0 entry
  mp = &mtable[0];

  mp->dev = dev;

  ninodes = mp->ninodes = sp->s_inodes_count;
  nblocks = mp->nblocks = sp->s_blocks_count;

  strcpy(mp->devName, rootdev);
  strcpy(mp->mntName, "/");




  get_block(dev,2,buf);
  gp = (GD *)buf;
  bmap = mp->bmap = gp->bg_block_bitmap;
  imap = mp->imap = gp->bg_inode_bitmap;
  inode_start = mp->iblock = gp->bg_inode_table;

  printf("mount_root():\n  bmap=%d\n  imap=%d\n  iblock=%d  nblocks=%d\n", bmap, imap, inode_start, nblocks);

  // call to iget increments minodes refCount
  root = iget(dev, 2);

  // mp contains root as the dir pointer
  mp->mntDirPtr = root;
  root->mntPtr = mp;

  //set proc CWD's
  for (i = 0; i < NPROC; i++)
  {
    proc[i].cwd = iget(dev, 2);
  }

  return 0;
}

// put all dirty minode and exit
int quit()
{
  int i;
  MINODE *mip;
  for (i=0; i<NMINODE; i++)
  {
    mip = &minode[i];

    if (mip->refCount && mip->dirty)
    {
      mip->refCount = 1;
      my_iput(mip);
      printf("quit() put ino=%d\n", mip->ino);
    }
  }
  exit(0);
}