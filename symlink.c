/************* symlink file **************/

// Creates a symbolic link file at new to old, whoms inode->iblock[] contain the name of old
int symlink(char *old, char *new)
{
    int oino = getino(old);
    int nino = getino(new);

    if(!oino)
    {
        printf("Error: %s does not exist\n", old);
        return 1;
    }

    if(nino)
    {
        printf("Error: %s already exists\n", new);
        return 1;
    }

    MINODE *oip = iget(dev, oino);

    if (!min_isdir(oip) && !min_isreg(oip))
    {
        printf("Error: you can only symlink to a directory or regular file\n");
        return 1;
    }

    strcpy(dbpath, old);

    int o_len = strlen(dbpath);

    creat_file(new);

    nino = getino(new);

    if (!nino)
    {
        printf("Error: symlink() lost inode of new file %s\n", new);
    }

    MINODE *nip = iget(dev, nino);

    INODE *n_inode = &nip->INODE;

    // Set mode to link type
    n_inode->i_mode = 0120000;

    // copy old name into new files i_block memory
    memcpy(&(n_inode->i_block[0]), &dbpath, o_len);

    // Size is that of old filename
    n_inode->i_size = o_len;

    nip->dirty = 1;

    my_iput(nip);
}

// Reads the symbolic link (filepath) of a symlink file. Returns an in-memory char*
char* readlink(char *pathname)
{
    int ino = getino(pathname);

    char buf[128];
    bzero(buf, 128);

    if (!ino)
    {
        printf("Error: file %s does not exist\n", pathname);
        return 0;
    }

    MINODE *mip = iget(dev, ino);

    if(!min_issymlink(mip))
    {
        printf("Error: you can only readlink a LNK file\n");
        return 0;
    }

    INODE *m_inode = &mip->INODE;

    return (char *)&(m_inode->i_block[0]);
}