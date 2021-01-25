/************* mount and unmount EXT2fs virtual disk **************/

// mount disk diskname on mount point (a dir)mount_name
int mount_disk(char *diskname, char *mount_name)
{
    int newdev, ino;

    char buf[BLKSIZE];

    MINODE *mip;

    MTABLE *mn = 0;
    MTABLE *di;

    SUPER *sp;

    GD *gp;

    if(diskname[0] == 0)
    {
        printf("Show Mounted Devices:\n");
        printf("------------------------------\n");
        for(int i = 0; i < NMTABLE; i++)
        {
            mn = &mtable[i];
            
            if(mn->dev != 0)
            {
                printf("   dev: %d\n", mn->dev);
                printf("   device name: %s\n", mn->devName);
                printf("   mount name: %s\n", mn->mntName);
                mip = mn->mntDirPtr;
                printf("   dir pointer ino: %d\n", mip->ino);
                printf("   dir pointer device: %d\n", mip->dev);
                di = mip->mntPtr;
                printf("   mount->dir_pointer->mount device (=dev): %d\n", di->dev);
                printf("------------------------------\n");
            }
        }

        return 2;
    }

    mip = 0;
    mn = 0;

    newdev = open(diskname, O_RDWR);

    if(newdev < 0)
    {
        printf("ERROR: mount_disk: cannot open disk: %s\n", diskname);
        close(newdev);
        return 0;
    }
    else
    {
        printf("\nOpened disk %s for RW...\n", diskname);
    }

    ino = getino(mount_name);

    if(ino == 0)
    {
        printf("mount_disk: mount point not found\n");
        close(newdev);
        return 0;
    }

    mip = iget(root->dev, ino);

    if(!min_isdir(mip) || !isEmptyDir(mip))
    {
        printf("ERROR: mount_disk: %s must be an empty directory\n", mount_name);
        close(newdev);
        return 0;
    }

    printf("Mount point %s ready...\n", mount_name);

    //get super block of root dev
    get_block(newdev,1,buf);

    sp = (SUPER *)buf;

    if (sp->s_magic != 0xEF53)
    {
        printf("ERROR: mount_disk: %s is not an EXT2 file system. Cant continue\n", diskname);
        close(newdev);
        return 0;
    }
    else
    {
        printf("EXT2 file system found...\n");
    }

    dnamebname(diskname);

    mn = mtable_alloc(newdev, bname);

    if(!mn)
    {
        printf("ERROR: mount_disk: could not allocate MTABLE entry. May be full or device in use\n");
        close(newdev);
        return 0;
    }
    else
    {
        printf("Mount table entry: %s with dev:%d created.\n", mn->devName, mn->dev);
    }
 
    // Record mount name in the mtable entry
    strcpy(mn->mntName, mount_name);

    mn->ninodes = sp->s_inodes_count;
    mn->nblocks = sp->s_blocks_count;

    get_block(newdev,2,buf);

    gp = (GD *)buf;

    mn->bmap = gp->bg_block_bitmap;
    mn->imap = gp->bg_inode_bitmap;
    mn->iblock = gp->bg_inode_table;

    mip->mounted = 1;

    mn->dev = newdev;
    mip->mntPtr = mn;
    mn->mntDirPtr = mip;

    return 1;
}

// unmount if possible, wont work if mount being used or open files in mount
int umount_disk(char *diskname)
{
    MINODE *mip;

    int d;

    if(!is_mounted(diskname))
    {
        printf("%s is not mounted, cannot unmount\n", diskname);
        return 0;
    }

    d = get_mount_dev(diskname);

    // check all minodes working on device d of mount
    for(int i = 0; i < NMINODE; i++)
    {
        mip = &minode[i];
        
        if(mip->dev == d)
        {
            if(get_oft(mip))
            {
                printf("%s has open files, cannot umount\n", diskname);
                return 0;
            }
            if(running->cwd->ino == mip->ino && running->cwd->dev == mip->dev)
            {
                printf("%s is busy, cannot umount\n", diskname);
                return 0;           
            }
        }
    }

    MTABLE *mn = get_mtable(d);

    mip = mn->mntDirPtr;

    mip->mounted = 0;

    iput(mip);

    printf("Success\n");
    return 1;

}