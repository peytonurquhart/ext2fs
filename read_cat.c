/************* read and cat files **************/

int myread(int fd, char *buf, int nbytes)
{
    OFT *of = running->fd[fd];

    char readBuf[BLKSIZE];

    if(of == 0)
    {
        printf("Error: read: file descriptor not open\n");
        return 0;
    }

    int lbk, start_byte, bno, remain, *bptr, ibn, ibo;

    int count = 0;

    MINODE *mip = of->mptr;

    INODE *inode = &mip->INODE;

    int avil = inode->i_size - of->offset;

    char *cq = buf;

    char *cp;

    while(nbytes && avil)
    {
        lbk = of->offset / BLKSIZE;
        start_byte = of->offset % BLKSIZE;
	
        if(lbk < 12)
        {
            bno = inode->i_block[lbk];
        }
        else if(lbk >= 12 && lbk < 12 + 256)
        {
            get_block(mip->dev, inode->i_block[12], readBuf);

            bptr = (int *)readBuf;

            bno = bptr[lbk - 12];

            if(likelyError(bno))
            {
                printf("\n\nWARNING: read: found bno=%d. fd=%d, lbk=%d\nhaulted at %d bytes\n\n", bno, fd, lbk, count);
                return count;
            }
        }
        else
        {
            get_block(mip->dev, inode->i_block[13], readBuf);

            //Each indirect block contains 256 blocks->blocks, so first 256 or ibn 0, then 1...
            ibn = (lbk - 12 - 256) / 256;
            //Finds the number of the block in the set of indirect blocks, in form 0,1 0,2 0,3 0,4...
            ibo = (lbk - 12 - 256) % 256;

            bptr = (int *)readBuf;

            get_block(mip->dev, bptr[ibn], readBuf);

            //error checking only
            int e = bptr[ibn];

            bptr = (int *)readBuf;

            bno = bptr[ibo];

            if(likelyError(bno))
            {
                printf("\n\nWARNING: read: found bno=%d. fd=%d, lbk=%d: HAULTED at %d bytes\n", bno, fd, lbk, count);
                printf("inode->i_block[13] = %d indirect_bno (points to bno) = %d\n\n", inode->i_block[13], e);
                return count;
            }
        }
        	
        get_block(mip->dev, bno, readBuf);

        cp = readBuf + start_byte;

        remain = BLKSIZE - start_byte;

        //avil = total size - offset

        // number of bytes read bounded by remain, nbytes
        if(nbytes < remain)
            remain = nbytes;

        // number of bytes read also bounded by avil
        if(avil < remain)
            remain = avil;

            memcpy(cq, cp, remain);

            cq += remain;
            cp += remain;
            of->offset += remain;
            count += remain;
            avil -= remain;
            nbytes -= remain;
            remain = 0;

        //loop back for more blocks
    }

    return count; 
}

// Print all contents of file at pathname on the screen
int mycat(char *pathname)
{
    int fd = open_file(pathname, 0);

    char buf[BLKSIZE];
    bzero(buf, BLKSIZE);

    int n;
    int total = 0;
    int i = 1;

    int line = 1;
    char l[32];

    clear();
    printf("\n-=-=-=-=-=-=-=-=-=-=-=-=-CAT %s-=-=-=-=-=-=-=-=-=-=-=-=-\n", pathname);

    if(fd != -1)
    {
        bzero(l, 32);
        sprintf(l, "%d", line);
        printf("%s:   ", l);
        line++;

        while(n = myread(fd, buf, BLKSIZE-1))
        {
            for(int k = 0; k < n; k++)
            {
                if(buf[k] == '\n' && k < (n - 1)) 
                {
                    printf("\n");
                    bzero(l, 32);
                    sprintf(l, "%d", line);
                    printf("%s:   ", l);
                    line++;
                }
                else
                {
                    putchar(buf[k]);
                }
            }

            bzero(buf, BLKSIZE);

            total += n;
        }

        printf("-=-=-=-=-=-=-=-=-CAT FINISHED printed %d bytes-=-=-=-=-=-=-=-=-\n\n", total);

        close_file(fd);
    }
    else
    {
        printf("Error: cat: could not open file %s\n", pathname);
        return 0;
    }

    return 1;
}

int likelyError(int bno)
{
    if(bno <= 0)
        return 1;
    if(bno > 1440)
        return 1;
    
    return 0;
}
