/************* lseek, open and close files **************/

/* 
0 = R
1 = W
2 = RW
3 = AP (append)
*/

char* modes[] = {"R", "W", "RW", "AP", 0};


// Open a file at pathname for mode, create fd[OFT *] entry in running PROC
int open_file(char *pathname, int mode)
{
    int ino, i_offset;
    MINODE * mip;
    INODE *m_inode;
    OFT *of;

    // For file permissions
    char pbuf[16];

    // Invalid mode
    if (mode < 0 || mode > 3)
    {
        printf("Error: invalid mode: %d\n", mode);
        return -1;
    }

    // Get correct device
    if (pathname[0] == '/')
    {
        dev = root->dev;
    }
    else
    {
        dev = running->cwd->dev;
    }

    ino = getino(pathname);

    // File does not exist
    if (!ino)
    {
        printf("Error: file %s does not exist\n", pathname);
        return -1;
    }

    mip = iget(dev, ino);
    m_inode = &mip->INODE;

    // File is not a regular file
    if(!min_isreg(mip))
    {
        printf("Error: file must be a regular file to open for %s\n", modes[mode]);
        return -1;
    }

    // RWX RWX RWX - USER GROUP OTHER
    // Get file permissions string into a buf
    get_permissions(mip, pbuf);

    printf("Open: file uid:%d file gid:%d\n", m_inode->i_uid, m_inode->i_gid);

    if(running->uid == m_inode->i_uid)
    {
        i_offset = 0;
    }
    else if(running->gid == m_inode->i_gid)
    {
        i_offset = 3;
    }
    else
    {
        i_offset = 6;
    }

    // Check if the file is already open for W RW or APPEND
    if(f_isopen(mip, 1) || f_isopen(mip, 2) || f_isopen(mip,3))
    {
        printf("Error: file is already open with incomptible mode\n");
        return -1;
    }
    else if(mode != 0 && f_isopen(mip, 0))
    {
        printf("Error: you cannot write to file which is open for read\n");
        return -1;
    }

    // If not superuser check permissions
    if(running->uid != 0)
    {
        switch(mode)
        {
            case 0:
            // READ
            if (pbuf[i_offset] == '-')
            {
               printf("Open: Permission denied for %s. uid:%d gid%d\n", modes[mode], running->uid, running->gid);
               return -1;
            }
            break;
            //WRITE
            case 1:
            if (pbuf[i_offset + 1] == '-')
            {
               printf("Open: Permission denied for %s. uid:%d gid%d\n", modes[mode], running->uid, running->gid);
               return -1;
            }
            break;
            // READ WRITE
            case 2:
            if (pbuf[i_offset] == '-' || pbuf[i_offset + 1] == '-')
            {
               printf("Open: Permission denied for %s. uid:%d gid%d\n", modes[mode], running->uid, running->gid);
               return -1;
            }
            break;
            // APPEND
            case 3:
            if (pbuf[i_offset + 1] == '-')
            {
               printf("Open: Permission denied for %s. uid:%d gid%d\n", modes[mode], running->uid, running->gid);
               return -1;
            }
            break;
        }
    }

    // If the file is open for read, and we want to read
    if(f_isopen(mip, 0) && mode==0)
    {
        of = get_oft(mip);
        of->refCount++;
    }
    else
    {

        // Allocate free open file table
        of = oft_alloc();
        of->mode = mode;
        of->refCount = 1;
        of->mptr = mip;
    }

    switch(mode)
    {
        case 0:
        of->offset = 0;
        break;
        case 1:
        truncate_mip(mip);
        of->offset = 0;
        break;
        case 2:
        of->offset = 0;
        break;
        case 3:
        of->offset = m_inode->i_size;
        break;
    }

    // Let running processes file descriptor at some index point at the new OFT entry
    for(int i = 0; i < NFD; i++)
    {
        if(running->fd[i] == 0)
        {
            running->fd[i] = of;

            m_inode->i_atime = time(0L);

            if(mode != 0)
                m_inode->i_mtime = time(0L);

            mip->dirty = 1;

            printf("Open: Success: fd=%d\n", i);

            return i;
        }
    }

    printf("Fatal Error: No open file descriptors for this process\n");
    return -1;
}

// Close file with certain file descriptor
int close_file(int fd)
{
    OFT *of;

    if(fd < 0 || fd >= NFD)
    {
        printf("Error: close_file() invalid fd\n");
        return 0;
    }

    // File was not open, we dont need to close it
    if(running->fd[fd] == 0)
    {
        printf("Warning: close_file() fd:%d was not open\n", fd);
        return 0;
    }

    of = running->fd[fd];

    running->fd[fd] = 0;

    of->refCount--;

    // If this file is stil being used by another proc
    if(of->refCount > 0)
    {
        printf("Warning: close_file() fd:%d is still open elsewhere\n", fd);
        return 0;
    }

    MINODE *mip = of->mptr;

    // Remove open file table
    oft_dalloc(mip);

    my_iput(mip);

    printf("Success: fd:%d disposed\n", fd);

    return 1;
}

// Change position for fd[OFT*] to position
int my_lseek(int fd, int position)
{
    OFT *of;
    MINODE *mip;
    INODE *m_inode;

    if(fd < 0 || fd >= NFD)
    {
       printf("Error: lseek() invalid fd\n");
       return 0;
    }

    if(running->fd[fd]==0)
    {
        printf("Error: lseek file fd:%d not open\n", fd);
        return 0;
    }

    of = running->fd[fd];
    mip = of->mptr;
    m_inode = &mip->INODE;

    if(position > m_inode->i_size)
    {
        printf("Error: lseek position out of range. %d > %d\n", position, m_inode->i_size);
        return 0;
    }

    of->offset = position;

    return 1;
}

// Print file descriptors
void pfd()
{
    OFT *of;
    MINODE *mip;

    printf("fd   mode   offset   [dev,ino]\n");
    printf("------------------------------\n");

    for(int i = 0; i < NFD; i++)
    {
        if(running->fd[i] != 0)
        {
            of = running->fd[i];
            mip = of->mptr;

            printf("%d    ",i);
            printf("%s      ", modes[of->mode]);
            printf("%d         ", of->offset);
            printf("[%d, %d]\n", mip->dev, mip->ino);
        }
    }

    printf("------------------------------\n");
}

// Dup fd into first free open file descriptor
int dup(int fd)
{
    OFT *of;

    if(fd < 0 || fd >= NFD)
    {
       printf("Error: dup: invalid fd\n");
       return 0;
    }

    if(running->fd[fd] != 0)
    {
        of = running->fd[fd];

        for(int i = 0; i < NFD; i++)
        {
            if(running->fd[i] == 0)
            {
                running->fd[i] = of;
                of->refCount++;
                return 1;
            }
        }
        printf("Error: dup: no free file descriptors. dup failed\n");
        return 0;
    }
    else
    {
        printf("Error: dup: fd:%d is not an open descriptor\n", fd);
        return 0;
    }
}

int dup2(int fd, int gd)
{
    if(fd < 0 || fd >= NFD)
    {
       printf("Error: dup2: invalid fd\n");
       return 0;
    }

    if(running->fd[fd] == 0)
    {
        printf("Error: dup2: fd:%d not open\n", fd);
        return 0;
    }

    if(running->fd[gd] != 0)
    {
        close_file(gd);
    }

    running->fd[gd] = running->fd[fd];

    return 1;
}