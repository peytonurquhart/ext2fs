/************* mkdir and creat file **************/

// test a bit in buffer, return 0 if free
int tst_bit(char *buf, int bit)
{
    return buf[bit/8] & (1 << (bit % 8));
}

// set a bit in buffer following convention
int set_bit(char *buf, int bit)
{
    buf[bit/8] |= (1 << (bit % 8));
}

// Decrement free inodes count in super block and group descriptor
int decFreeInodes(int dev)
{
    char buf[BLKSIZE];
    bzero(buf, BLKSIZE);

    get_block(dev, 1, buf);
    SUPER * sp = (SUPER *)buf;
    sp->s_free_inodes_count -= 1;
    put_block(dev, 1, buf);

    bzero(buf, BLKSIZE);

    get_block(dev, 2, buf);
    GD *gp = (GD *)buf;
    gp->bg_free_inodes_count -= 1;
    put_block(dev, 2, buf);
}

// allocate a new ino in bitmaps
int ialloc(int dev)
{
    int i;
    char buf[BLKSIZE];

    MTABLE *mp = get_mtable(dev);

    get_block(dev, mp->imap, buf);

    for(i = 0; i < mp->ninodes; i++)
    {
        if(tst_bit(buf, i)==0)
        {
            set_bit(buf, i);
            put_block(dev, mp->imap, buf);
            decFreeInodes(dev);
            return (i+1);
        }
    }

    printf("ialloc(): Error: out of free inodes\n");
}

// returns 1 if the block is alloced
int is_block(int dev, int bno)
{
    int i;
    char buf[BLKSIZE];

    MTABLE *mp = get_mtable(dev);

    get_block(dev, mp->bmap, buf);

    if(tst_bit(buf, bno) != 0)
    {
        return 1;
    }

    return 0;
}

// Allocates a free block
int balloc(int dev)
{
    int i;
    char buf[BLKSIZE];

    MTABLE *mp = get_mtable(dev);

    get_block(dev, mp->bmap, buf);

    for(int i = 0; i < mp->nblocks; i++)
    {
        if(tst_bit(buf, i) == 0)
        {
            set_bit(buf, i);
            decFreeInodes(dev);
            put_block(dev, bmap, buf);

            printf("success balloc(): returns %d\n", i+1);
            return i+1;
        }
    }

    printf("balloc(): Error: could not allocate free block\n");

    return 0;
}

// Create new file a pathname in dnamebname format
int creat_file(char *pathname)
{
    MINODE *start;
    
    // if the pathname is absolute
    if(pathname[0] == '/')
    {
        start = root;
        dev = root->dev;
    }
    else
    {
        start = running->cwd;
        dev = running->cwd->dev;
    }

    // Fill GLOBAL dname[] and bname[] 
    strcpy(gpath, pathname);
    dnamebname(gpath);

    int parent_ino = getino(dname);
    MINODE * pip = iget(dev, parent_ino);

    // Ensure the parent is a directory, or else you cant mkdir inside of it.
    if(!min_isdir(pip))
    {
       printf("You can only creat inside an existing directory\n");
       return 1;
    }

    // If parent isnt found
    if(!pip)
    {
        printf("Not a valid path\n");
    }

    // Check that a directory or file with the proposed name does not already exist
    if(getino(pathname) != 0)
    {
        printf("Cannot creat: name %s already exists\n", pathname);
        return 1;
    }

    mycreat(pip, bname);

    INODE * t = &pip->INODE;
    t->i_atime = time(0L);
    pip->dirty = 1;

    printf("creat_file() parent inode[%d] links_count: %d\n", pip->ino, t->i_links_count);

    my_iput(pip);
}

// Called by creat_file, pip is the parent dir, newname is the name of new file
int mycreat(MINODE *pip, char *newName)
{
    MINODE *mip;

    int ino = ialloc(dev);

    printf("mycreat() new ino: %d\n", ino);

    mip = iget(dev, ino);
    INODE *ip = &mip->INODE;

    printf("mycreat() got mip from new ino with ino: %d\n", mip->ino);

    //make it a reg file
    ip->i_mode = 0x81A4;

    // set ids of creating process
    ip->i_uid = running->uid;
    ip->i_gid = running->gid;

    // size is 0, it has no data block
    ip->i_size = 0;

    // links count = 1 for a file
    ip->i_links_count = 1;

    // set all time counts to current
    ip->i_atime = time(0L);
    ip->i_ctime = time(0L);
    ip->i_mtime = time(0L);

    // because they are in 512 byte chunks
    ip->i_blocks = 0;

    //mark as dirty
    mip->dirty = 1;
    mip->refCount = 1;

    //write mip to the disk
    my_iput(mip);

    enter_name(pip, ino, newName);
}

// Make a dir at pathname
int make_dir(char *pathname)
{
    // Fill GLOBAL dname[] and bname[] 
    strcpy(dbpath, pathname);
    dnamebname(dbpath);

    printf("path:%s dname:%s bname:%s\n", pathname, dname, bname);

    if(pathname[0] == '/' && dname[0] == '.')
    {
        dname[0] = '/';
        dname[1] = 0;
    }

    int parent_ino = getino(dname);
    MINODE * pip = iget(dev, parent_ino);

    // Ensure the parent is a directory, or else you cant mkdir inside of it.
    if(!min_isdir(pip))
    {
       printf("You can only mkdir inside an existing directory\n");
       return 1;
    }

    // If parent isnt found
    if(!pip)
    {
        printf("Not a valid path\n");
    }

    // Check that a directory or file with the proposed name does not already exist
    if(getino(pathname) != 0)
    {
        printf("Cannot mkdir: name %s already exists\n", pathname);
        return 1;
    }

    mymkdir(pip, bname);

    INODE * t = &pip->INODE;
    t->i_links_count++;
    t->i_atime = time(0L);
    pip->dirty = 1;

    my_iput(pip);
}

// Called by make_dir
int mymkdir(MINODE *pip, char *newName)
{
    MINODE *mip;

    int ino = ialloc(dev);
    int bno = balloc(dev);

    char buf[BLKSIZE];

    mip = iget(dev, ino);
    INODE *ip = &mip->INODE;

    //make it a dir
    ip->i_mode = 0x41ED;

    // set ids of creating process
    ip->i_uid = running->uid;
    ip->i_gid = running->gid;

    // pre-defined blocksize 1024
    ip->i_size = BLKSIZE;

    // links count 2 because of . and ..
    ip->i_links_count = 2;

    // set all time counts to current
    ip->i_atime = time(0L);
    ip->i_ctime = time(0L);
    ip->i_mtime = time(0L);

    // because they are in 512 byte chunks
    ip->i_blocks = 2;

    // set i_block[0] to block number
    ip->i_block[0] = bno;

    //init the rest of the block to 0
    for(int i = 1; i<=14; i++)
    {
        ip->i_block[i] = 0;
    }

    //mark as dirty
    mip->refCount = 0;
    mip->dirty = 1;

    //write mip to the disk
    my_iput(mip);



    // create a data block for the new dir (using block number we found)
    // should contain . and .. entries
    get_block(dev, bno, buf);

    // cast the block to a DIR struct to start adding entries
    DIR* dir = (DIR *)buf;

    // CREATE ENTRY = .
    // the inode that owns this dir is the new inode we created
    dir->inode = ino;
    // len(.) = 1
    dir->name_len = 1;
    // the record length is 4 * ((8 + name_len + 3) / 4) as specified to be multiple of 4
    dir->rec_len = 4*((8+dir->name_len+3)/4);
    int rTemp = dir->rec_len;
    // name the dir .
    dir->name[0] = '.';
    // make file type dir
    dir->file_type = 2;

    char *t = buf;
    // Advance the dir structure to where the next record will be 
    t += dir->rec_len;
    dir = (DIR *)t;

    // Create entry for ..

    // Parent entries inode will be pip-> ino (function was passed in the parent)
    dir->inode = pip->ino;
    dir->name_len = 2;
    // Record length is blocksize - rec length of all of its predecessors (in this case only 1 predecessor)
    dir->rec_len = BLKSIZE - rTemp;
    // Set name to ..
    dir->name[0] = '.';
    dir->name[1] = '.';
    dir->file_type = 2;

    // Write the updated block back for new inode to the disk
    put_block(dev, bno, buf);

    enter_name(pip, ino, newName);
}

// Enter a new name into the parents directory for new inode target ino, with newName
int enter_name(MINODE *pip, int targetino, char *newName)
{
    INODE *pino = &pip->INODE;

    int nameLen = strlen(newName);

    char buf[BLKSIZE];
    DIR * dir;
    char *t;

    int i;
    // Assume 12 direct data blocks in parent DIR. Iterate through them
    for(i = 0; i < 12; i++)
    {
        if(pino->i_block[i] == 0)
        {
            break;
        }

        // The block number can be recieved from INODe->i_block[]
        int bno = pino->i_block[i];
    
        bzero(buf, BLKSIZE);

        // Use the current retrieved block number to read block into buf
        get_block(dev, bno, buf);
        dir = (DIR*)buf;

        // Let needLen be the amount of space we need for the new entry
        int needLen = 4*((8+nameLen+3)/4);

        // Let t point at buf
        t = buf;
        // We must iterate to the last entry in the dir
        while((t + dir->rec_len) < (buf + BLKSIZE))
        {
            t += dir->rec_len;
            dir = (DIR *)t;
        }

        // Let l point back at buf of last dir entry
        char *l = (char *)dir;


        // Get what the last entry of the dirs length should be
        int idealLen = 4*((8 + dir->name_len + 3)/4);

        // The amount remaining is the actual record length - what it should be
        int remainingLength = dir->rec_len - idealLen;

        // If there is enough space for the new entry
        if(remainingLength > needLen)
        {
            // reduce last entries record length to its ideal length
            dir->rec_len = idealLen;
        
            // last entry pointer += the new length to point at spot for new entry
            l += idealLen;
            dir = (DIR*)l;

            // set new i number to the target passed to the function
            dir->inode = targetino;
            // set the new rec length
            dir->rec_len = BLKSIZE - ((unsigned int)l - (unsigned int)buf);

            dir->name_len = nameLen;

            dir->file_type = 2;

            // Copy in the new name
            strcpy(dir->name, newName);

            // Put back the updated block
            put_block(dev, bno, buf);
        }
        else
        {
            extend_block(pip, targetino, newName);
            printf("EXTEND BLOCK\n");
        }
    
        return 0;
    }
}

// Extends the data block for a dir and inputs new dir with targetino and newName
void extend_block(MINODE* pip, int targetino, char *newName)
{
    INODE *pino = &pip->INODE;
    char buf[BLKSIZE];
    bzero(buf, BLKSIZE);
    int nameLen = strlen(newName);

    // Allocate a new block
    int bno = balloc(dev);

    // New block
    pino->i_block[12] = bno;
    // Update the size to use a whole new block (no .. or .)
    pino->i_size += BLKSIZE;
    pip->dirty = 1;

    // Get the new block into buf, cast into dir
    get_block(dev, bno, buf);

    DIR *dir = (DIR *)buf;
    
    dir->inode = targetino;
    dir->rec_len = BLKSIZE;
    dir->name_len = nameLen;
    dir->file_type = 2;
    strcpy(dir->name, newName);

    put_block(dev, bno, buf);

    return;
}
