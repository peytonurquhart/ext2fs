/************* write and copy files **************/

int write_file(int fd, char *text, int nbytes)
{
    char buf[BLKSIZE];
    char idbuf[BLKSIZE];
    char write_buf[BLKSIZE];
    
    int blk, b13, dblk;
    int buf13[256], dbuf[256];
    
    OFT *of = running->fd[fd];

    // Ensure file is open
    if(of == 0)
    {
        printf("Error: read: file descriptor not open\n");
        return 0;
    }

    // Ensure file is opened to be written to
    if(of->mode < 1 || of->mode > 3)
    {
        printf("Error: write: this file must be open for R, RW, or APPEND\n");
        return 0;
    }

    MINODE *mip = of->mptr;

    INODE *inode = &mip->INODE;

    int bno, lbk, start_byte, remain, k, *bptr, ib_num, ib_off;

    int total_bytes = nbytes;

    char *cp;

    while(nbytes > 0)
    {

        // Get the start block and start byte to write to
        lbk = of->offset / BLKSIZE;
        start_byte = of->offset % BLKSIZE;

	    printf("WRITE: lbk=%d ", lbk);
	
        if(lbk < 12)
        {
            // DIRECT

            // If unalloced, we must alloc it.
            if(inode->i_block[lbk] == 0)
            {
                bno = inode->i_block[lbk] = balloc(mip->dev);
		        
                printf("alloc bno=%d ", bno);
            }

            bno = inode->i_block[lbk];
        }
        else if (lbk >= 12 && lbk < 256 + 12)
        {
            // INDIRECT

            if(inode->i_block[12] == 0)
            {
                inode->i_block[12] = balloc(mip->dev);
                //zero the new block
                //zero_block(buf, BLKSIZE);
                zero_block(buf);
                put_block(mip->dev, inode->i_block[12], buf);

                printf("alloc b12=%d ", inode->i_block[12]);
            }

            get_block(mip->dev, inode->i_block[12], buf);

            bptr = (int *)buf;

            if(bptr[lbk - 12] == 0)
            {
                bptr[lbk - 12] = balloc(mip->dev);

                printf("alloc DB=%d ", bptr[lbk - 12]);
            }

            bno = bptr[lbk - 12];

            put_block(mip->dev, inode->i_block[12], buf);
        }
        else 
        {   
            // DOUBLE INDIRECT

            // relative lbk ignores direct (12) and indirect (256)
            lbk -= (12 + 256);

            // if i_block[13] is not alloced we must alloc it
            if (mip->INODE.i_block[13] == 0)
            {
	            b13 = mip->INODE.i_block[13] = balloc(mip->dev); // new block[13]

                // zero out contents of block 13 and write it back for future use
                zero_block(buf13);
                put_block(mip->dev, mip->INODE.i_block[13], (char *)buf13);
            } 
            
            // get block 13 (zeroed or not)
            get_block(mip->dev, mip->INODE.i_block[13], (char *)buf13);

            // get the double indirect block number (will be 0 always for these files)
            dblk = buf13[lbk/256];       

            // we must now alloc a new double indirect block if not alloced
            if (dblk == 0)
            {
                dblk = buf13[lbk/256] = balloc(mip->dev);

                // zero its contests and put it back
	            zero_block(dbuf);

                put_block(mip->dev, dblk, (char *)dbuf);   

                put_block(mip->dev, mip->INODE.i_block[13], (char *)buf13); 
            }

            get_block(mip->dev, dblk, (char *)dbuf);

            // get the final block number we use to write from indirect bno
            if (dbuf[lbk % 256] == 0)
            {
                blk = dbuf[lbk % 256] = balloc(mip->dev);
                printf("blk=%d ", blk);
                put_block(mip->dev, dblk, (char *)dbuf); // update the d_block
            }

	        bno = blk;
        }

	
        // bno should be set, time to write

        printf("write bno= %d\n", bno);

        get_block(mip->dev, bno, write_buf);

        remain = BLKSIZE - start_byte;

        if(nbytes < remain)
            remain = nbytes;

        memcpy((char *)&write_buf[start_byte], (char *)&text[total_bytes - nbytes], remain);

        nbytes -= remain;
        of->offset += remain;

        if(of->offset > inode->i_size)
            inode->i_size = of->offset;


        put_block(mip->dev, bno, write_buf);
    }

    mip->dirty = 1;

    return total_bytes;
}

int cp(char *source, char *dest)
{
    int fds, fdd;

    char buf[BLKSIZE + 1];
    bzero(buf, BLKSIZE + 1);

    int n = 0;
    int w = 0;
    int totalr = 0;
    int totalw = 0;

    fds = open_file(source, 0);

    if(fds == -1)
    {
        printf("Error: cp: file %s does not exist\n", source);
        return 0;
    }

    fdd = open_file(dest, 1);

    if(fdd == -1)
    {
        creat_file(dest);
    }
    else
    {
        close_file(fdd);
    }

    fdd = open_file(dest, 2);

    if(fdd == -1)
    {
        printf("Error: cp: file was not found, and its creation has failed\n");
        return 0;
    }

    int i = 1;

    pfd();

    while(n = myread(fds, buf, BLKSIZE))
    {
      //printf("\n CP Call #%d\n", i);
        write_file(fdd, buf, n);
        i++;
    }

    close_file(fds);
    close_file(fdd);

    return 1;
}

void zero_block(char buf[BLKSIZE])
{
    for(int i = 0; i < BLKSIZE; i++)
    {
        buf[i] = 0;
    }
}
