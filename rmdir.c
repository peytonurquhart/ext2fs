/************* rmdir **************/

//clear bit in char buf[1024]
void clr_bit(char *buf, int bit)
{
    buf[bit/8] &= ~(1 << (bit%8));
}

//increment free inodes count for filesystem
int incFreeInodes(int dev)
{
    char buf[BLKSIZE];

    get_block(dev,1,buf);
    //superblock
    SUPER* sp = (SUPER *)buf;
    sp->s_free_inodes_count++;
    put_block(dev,1,buf);

    get_block(dev,2,buf);
    //group descriptor
    GD* gp = (GD *)buf;
    gp->bg_free_inodes_count++;
    put_block(dev,2,buf);

    return 1;
}

// dealloc ino: ino : book implementation
int idalloc(int dev, int ino)
{
    int i;
    char buf[BLKSIZE];

    MTABLE *mp = get_mtable(dev);

    printf("idalloc() ino=%d\n", ino);

    if (ino > mp->ninodes)
    {
        printf("idalloc inode out of range: dev=%d dev->mp->ninodes=%d ino=%d\n", dev, mp->ninodes, ino);

        return 0;
    }

    get_block(dev, mp->imap, buf);
    clr_bit(buf, ino - 1);
    put_block(dev, mp->imap, buf);
    incFreeInodes(dev);

    return 1;
}

// my own implementation
int bdalloc(int dev, int bno)
{
    int i;
    char buf[BLKSIZE];

    MTABLE *mp = get_mtable(dev);

    if (bno > mp->nblocks)
    {
        printf("bdalloc bno out of range: dev=%d dev->mp->nblocks=%d bno=%d\n", dev, mp->nblocks, bno);

        return 0;
    }

    get_block(dev, mp->bmap, buf);
    clr_bit(buf, bno-1);
    put_block(dev, mp->bmap, buf);
    incFreeInodes(dev);

    return 1;
}

//remove directory at pathname
int rmdir(char *pathname)
{
    char buf[BLKSIZE];

    int ino = getino(pathname);
    MINODE *mip = iget(dev, ino);

    if(!min_isdir(mip))
    {
        printf("rmdir() rmdir can only be used to remove a directory file type\n");
        my_iput(mip);
        return 0;
    }

    if(!isEmptyDir(mip))
    {
        printf("rmdir() dir is not empty, can not remove\n");
        my_iput(mip);
        return 0;
    }

    // name of valid target dir is in buf

    // dealloc data blocks
    for(int i = 0; i < 12; i++)
    {
        if(mip->INODE.i_block[i]==0)
        {
            continue;
        }

        // dealloc all of the dirs block numbers for future use
        bdalloc(mip->dev, mip->INODE.i_block[i]);
    }

    //deallco its inode for future use
    idalloc(mip->dev, mip->ino);

    //mark it dirty and put it back to the disk
    mip->dirty = 1;
    my_iput(mip);


    //get targets parents ino
    int pino = get_parent_ino(mip);

    //get targets parents MINODE
    MINODE* pmip = iget(mip->dev, pino);

    //get targets name into the buffer
    get_name(pmip, ino, buf, BLKSIZE);

    //Pass targets parent and name to remove chile
    rm_child(pmip, buf);


    INODE * t = &pmip->INODE;
    t->i_links_count--;
    t->i_atime = time(0L);
    t->i_mtime = time(0L);

    pmip->dirty = 1;
    my_iput(pmip);


    return 1;
}

//remove child of pmip with name
int rm_child(MINODE *pmip, char *name)
{
    char buf[BLKSIZE];
    char dnamet[BLKSIZE];
    INODE *p_inode = &pmip->INODE;

    DIR *dir;
    DIR *prev = 0;
    char *t;

    // Note if a dir is the only entry in a block we dont need to worry about . and ..
    // We can NEVER remove . and .. because they are full or busy

    // Iterate through p_inode 12 i_block[]
    for(int i = 0; i < 12; i++)
    {
        // If the current block has a valid block number
        if(p_inode->i_block[i] != 0)
        {
            // Read the block into the buf by block number
            get_block(dev, p_inode->i_block[i], buf);
            // cast this buffer to a directory structure
            dir = (DIR *)buf;
            t = buf;

            prev = 0;

            //dir now points at first entry

            // while address of t is less than the addres of BUF + sizeof(block)
            // we can search for dir entry.
            // remove entry logic inside this loop
            while(t < buf + BLKSIZE)
            {
                //must do this for strcmp (dir->name is NOT null delimited)
                bzero(dnamet, BLKSIZE);
                strncat(dnamet, dir->name, dir->name_len);

                // If we found the matching entry
                if(strcmp(dnamet, name) == 0)
                {
                    // if the current spot in the buf + record length fills the buf
                    // it must be the FIRST or LAST entry in the buf
                    if(t + dir->rec_len == buf + BLKSIZE)
                    {
                        if(prev == 0)
                        {
                            //FIRST

                            //deallocate the whole block
                            bdalloc(dev, p_inode->i_block[i]);
                            //remove the block from the file size
                            p_inode->i_size -= BLKSIZE;

                            // Shift blocks left to fill the gaps
                            for(int j = i + 1; p_inode->i_block[j] != 0 && j < 12; j++)
                            {
                                p_inode->i_block[j - 1] = p_inode->i_block[j];
                            }
                            
                            return 1;
                        }
                        else 
                        {
                            //LAST

                            //add the space from last dir to previous dir
                            prev->rec_len += dir->rec_len;
                            //put back the updated block
                            put_block(dev, p_inode->i_block[i], buf);

                            return 1;
                        }
                    }
                    else
                    {
                        //MIDDLE

                        // make new pointer to move to last entry
                        DIR * dir2 = dir;
                        char *t2 = t;

                        // move new pointer to last entry
                        while(t2 + dir2->rec_len != buf + BLKSIZE)
                        {
                            t2 += dir2->rec_len;
                            dir2 = (DIR *)t2;
                        }

                        // add target dirs length to the last dir
                        dir2->rec_len += dir->rec_len;

                        //memmove (*mem1, *mem2, amt)
                        memmove(t, t + dir->rec_len, (buf + 1024)-(t + dir->rec_len));
                        
                        // put the updated block back to memory
                        put_block(dev, p_inode->i_block[i], buf);             

                        return 1;
                    }
                }

                // Advance dir entry
                prev = dir;
                t += dir->rec_len;
                dir = (DIR *)t;
            }

        }
        else
        {
            printf("rm_child() parent inode->i_block[%d] was 0. [%s] not found.\n", i, name);
            return 0;
        }
    }
    
}

