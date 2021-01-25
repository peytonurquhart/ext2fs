/************* link and unlink file **************/


// Link an existing file, create a new link file (hard)
int link(char *existing, char *new)
{
    int eino, npino;

    eino = getino(existing);

    // If the existing file does not exist (ino of 0)
    if(!eino)
    {
        printf("File: %s does not exist\n", existing);
        return 1;
    }

    // existing file MINODE
    MINODE *eip = iget(dev, eino);

    // If it is not a regular file or symlink file
    if(!min_isreg(eip) && !min_issymlink(eip))
    {
        printf("Error: you can only link to a regular or link file.\n");
        return 1;
    }

    // Dirname Basename the argument for new file path.
    strcpy(dbpath, new);
    dnamebname(dbpath);

    // Get the ino of the parent in the new filepath
    npino = getino(dname);

    if(!npino)
    {
        printf("Directory: %s does not exist\n", dname);
        return 1;
    }

    // Get new file parent directory inode
    MINODE *pnip = iget(dev, npino);

    // Check if new files parent exists and is a dir
    if (!min_isdir(pnip))
    {
        printf("Error: %s is not a directory, a link can't be created within it\n", dname);
        return 1;
    }

    // Check if the new filename already exists
    if (getino(new) != 0)
    {
        printf("Error: file %s already exists, you cannot link two existing files\n", new);
        return 1;
    }

    // End of error checking -----

    INODE *e_inode = &eip->INODE;

    // Put a new entry into the parents directory sharing existing inode and new name
    enter_name(pnip, eino, bname);

    // Increment links count
    e_inode->i_links_count++;

    // Write existing inode back to disk
    my_iput(eip);

    return 0;
}

// Unlink file at pathname, or if no links, remove the file
int unlink(char *pathname)
{
    int ino = getino(pathname);

    strcpy(dbpath, pathname);
    dnamebname(dbpath);

    if(!ino)
    {
        printf("Error: File %s does not exist\n", pathname);
        return 1;
    }

    MINODE *mip = iget(dev, ino);

    if (!min_isreg(mip) && !min_issymlink(mip))
    {
        printf("Error: you can only unlink a regular or symlink file\n");
        return 1;
    }

    INODE *m_inode = &mip->INODE;

    m_inode->i_links_count--;

    // If this is the last instance of the file
    if (m_inode->i_links_count == 0)
    {
        // dealloc all its data blocks for later use
        truncate_mip(mip);

        // dealloc its ino for later use
        idalloc(dev, ino);
    }

    // Get parent of the file we are trying to unlink
    int pino = getino(dname);
    MINODE *pmip = iget(dev, pino);

    // Remove the target from its parents directory
    rm_child(pmip, bname);

    return 0;
}

// Deallocate ALL data blocks of mip->INODE
void truncate_mip(MINODE* mip)
{
    INODE *inode = &mip->INODE;
    char buf[BLKSIZE];

    //int num_blocks = (int)ceil((double)(inode->i_size / (double)BLKSIZE));

    int num_blocks = (inode->i_size + 1023) / BLKSIZE;

    int blocks_left = num_blocks;

    int _dbc = 0;
    int _ibc = 0;
    int _idbc = 0;

    int *bptr;
    int bno;

    printf("\nTruncate() size=%d n_blocks=%d\n", inode->i_size, num_blocks);

    for(int i = 0; i < 12; i++)
    {
        if(inode->i_block[i] != 0)
        {
            bdalloc(dev, inode->i_block[i]);

            blocks_left--;

            _dbc++;//////
        }
    }

    // If there are indirect blocks
    if(inode->i_block[12] != 0)
    {

        // Get data block 12 into the buf
        get_block(mip->dev, inode->i_block[12], buf);

        bptr = (int*)buf;

        while(blocks_left > 0 && _ibc < 256)
        {
            bno = *bptr;

            bdalloc(dev, bno);

            bptr++;

            blocks_left--;

            _ibc++;//////
        }
    }

    // If there are doubly indirect blocks
    if(inode->i_block[13] != 0)
    {
        int *di_blocks;

        get_block(mip->dev, inode->i_block[13], buf);

        // points to start of block of pointers to block of pointers to blocks
        di_blocks = (int *)buf;

        while(blocks_left > 0)
        {
            get_block(mip->dev, *di_blocks, buf);

            bptr = (int*)buf;

            // will scan through 265 blocks then loop back to get the next di_block, scan again
            while(blocks_left > 0)
            {
                bno = *bptr;

                bdalloc(dev, bno);

                bptr++;

                blocks_left--;

                _idbc++;/////

                //if we have dealloced a factor of 256 blocks, get next di_block
                if(_idbc+1 % 257 == 0)
                    break;
            }

            di_blocks++;
        }
    }

    // Clear dealloced blocks from the i_block
    for(int i = 0; i < 15; i++)
    {
        inode->i_block[i] = 0;
    }


    int total = _dbc + _ibc + _idbc;
    printf("Truncate() dealloc direct=%d indirect=%d doubleindirect=%d\ntotal=%d\n\n", _dbc, _ibc, _idbc, total);

    inode->i_size = 0;

    inode->i_atime = time(0L);
    inode->i_mtime = time(0L);

    mip->dirty = 1;
}

