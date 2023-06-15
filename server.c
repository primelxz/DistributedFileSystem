#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include "udp.h"
#include "mfs.h"
#include "struct.h"

MFS_CR_t *cr = NULL;
int fd = -999;

int copy_inode(MFS_Inode_t *new, MFS_Inode_t *old){
	for(int i = 0; i < INODE_PTRS; i++){
		new->ptrs[i] = old->ptrs[i];
	}
	return 0;
}

int create_empty_inode(MFS_Inode_t *ind){
	for (int i = 0; i < INODE_PTRS; i++)
    ind->ptrs[i] = -1;
	return 0;
}

int copy_imap(MFS_Imap_t *new, MFS_Imap_t *old){
	for(int i = 0; i < IMAP_ENTRIES; i++){
		new->inode_addr[i] = old->inode_addr[i];
	}
	return 0;
}

int create_empty_imap(MFS_Imap_t *new){
	for(int i = 0; i < IMAP_ENTRIES; i++){
		new->inode_addr[i] = -1;
	}
	return 0;
}

int inum_invalid(int inum){
	if (inum < 0)
		return 1;
	if (inum >=  INODE_LIMIT)
		return 1;
	return 0;
}

int datablock_invalid(int db){
	if (db < 0)
		return 1;
	if (db >=  INODE_PTRS)
		return 1;
	return 0;
}



int lfs_lookup(int pinum, char*filename)
{
  if (inum_invalid(pinum))
  {
    // perror("lookup: Invalid parent inode number\n");
    return -1;
  }

  int imp_index = pinum / IMAP_ENTRIES; // inode map piece number
	int inode_index = pinum % IMAP_ENTRIES; // inode number (index) in imap piece
  if (cr->imap[imp_index] == -1)
  {
    // perror("lookup: Invalid imap piece\n");
  }
  int imp_offset = cr->imap[imp_index]; // imap offset for lseek()
  
  lseek(fd, imp_offset, SEEK_SET);
	MFS_Imap_t imp;
  read(fd, &imp, sizeof(MFS_Imap_t));

  int ind_offset = imp.inode_addr[inode_index]; // inode offset for lseek()
  if (ind_offset == -1)
  {
    // perror("LOOKUP: Invalid inode address\n");
    return -1;
  }
  MFS_Inode_t ind; // inode
  lseek(fd, ind_offset, SEEK_SET);
  read(fd, &ind, sizeof(MFS_Inode_t));
  if (ind.type != MFS_DIRECTORY)
  {
    // perror("lookup: Not a directory\n");
    return -1;
  }

  char db_buffer[MFS_BLOCK_SIZE]; // data block buffer
  for (int i = 0; i < INODE_PTRS; i++)
  {
    int db_offset = ind.ptrs[i];
    if (db_offset == -1)
      continue;
    
    lseek(fd, db_offset, SEEK_SET);
    read(fd, db_buffer, MFS_BLOCK_SIZE);

    MFS_Dir_t *dir_buffer = (MFS_Dir_t *)db_buffer;
    for (int j = 0; j < DIR_ENTRIES; j++)
    {
      MFS_DirEnt_t *entry = &dir_buffer->entries[j];
      if (strcmp(entry->name, filename) == 0)
        return entry->inum;
    }
  }
  return -1;
}

int lfs_stat(int inum, MFS_Stat_t *stat)
{
	if(cr == NULL){
		// perror("stat: CR not created yet\n");
		return -1;
	}
  if (inum_invalid(inum))
  {
    // perror("stat: Invalid inode number\n");
    return -1;
  }

  int imp_index =inum / IMAP_ENTRIES;
  if (cr->imap[imp_index] == -1)
  {
    // perror("stat: Invalid imap\n");
    return -1;
  }
  
  int imp_offset = cr->imap[imp_index];
  lseek(fd, imp_offset, SEEK_SET);
	MFS_Imap_t imp;
  read(fd, &imp, sizeof(MFS_Imap_t));

  int inode_num = inum % IMAP_ENTRIES; 
  int ind_offset = imp.inode_addr[inode_num]; 
  if (ind_offset == -1)
  {
    // perror("stat: Invalid inode address\n");
    return -1;
  }

  MFS_Inode_t ind; //inode
  lseek(fd, ind_offset, SEEK_SET);
  read(fd, &ind, sizeof(MFS_Inode_t));

	int type = ind.type;
  int size = ind.size;
	stat->type = type;
  stat->size = size;

  return 0;
}

int lfs_write(int inum, char*buffer, int db)
{
  if (inum_invalid(inum) || datablock_invalid(db))
  {
    // perror("write: Invalid inode number / data block\n");
    return -1;
  }
  
  
  
  int buf_index = 0;
  char* temp = NULL;
  char write_buffer[MFS_BLOCK_SIZE];
  for (buf_index = 0, temp = buffer ; buf_index < MFS_BLOCK_SIZE; buf_index++)
  {
    if (temp == NULL)
    {
      write_buffer[buf_index] = '\0';
    }
    else
    {
      write_buffer[buf_index] = *temp;
      temp++;
    }
  }

  int imp_index = inum / IMAP_ENTRIES;
  int imp_offset = cr->imap[imp_index];
  int map_existed = 0;
	int inode_num = 0;
	MFS_Imap_t imp;
	int ind_offset = -1;
  if (imp_offset != -1)
  {
    map_existed = 1;

    inode_num = inum % IMAP_ENTRIES; 
    lseek(fd, imp_offset, SEEK_SET);
    read(fd, &imp, sizeof(MFS_Imap_t));

    ind_offset = imp.inode_addr[inode_num];
  }
  
	int node_existed = 0;
	MFS_Inode_t ind;
	int db_offset = -1;
  if (ind_offset != -1 && map_existed)
  {
    node_existed = 1;
    lseek(fd, ind_offset, SEEK_SET);
    read(fd, &ind, sizeof(MFS_Inode_t));
    if (ind.type != MFS_REGULAR_FILE)
    {
      // perror("write: Not a regular file\n");
      return -1;
    }
    db_offset = ind.ptrs[db];
  }

	int offset = cr->end;
  if (db_offset != -1 && map_existed && node_existed)
  {
    offset = db_offset;
  }

  cr->end += MFS_BLOCK_SIZE;
  lseek(fd, offset, SEEK_SET);
  write(fd, write_buffer, MFS_BLOCK_SIZE);

  MFS_Inode_t new_node;
  MFS_Inode_t *new_node_ptr = &new_node;
  MFS_Inode_t *old_node_ptr = &ind;
  if (node_existed)
  {
    new_node.size = (db + 1) * MFS_BLOCK_SIZE;
    new_node.type = ind.type;

		copy_inode(new_node_ptr, old_node_ptr);
    new_node.ptrs[db] = offset;   
  }
  else
  {
    new_node.size = 0;
    new_node.type = MFS_REGULAR_FILE;
		create_empty_inode(new_node_ptr);
    new_node.ptrs[db] = offset; 
  }

  offset = cr->end;     
  cr->end += sizeof(MFS_Inode_t);
  lseek(fd, offset, SEEK_SET);
  write(fd, &new_node, sizeof(MFS_Inode_t));

  MFS_Imap_t new_map;
  MFS_Imap_t *new_map_ptr = &new_map;
  MFS_Imap_t *old_map_ptr = &imp;
  if (map_existed)
  {
		copy_imap(new_map_ptr, old_map_ptr);
  }
  else
  {
		create_empty_imap(new_map_ptr);
  }
	new_map.inode_addr[inode_num] = offset; 

  offset = cr->end;
  cr->end += sizeof(MFS_Imap_t);
  lseek(fd, offset, SEEK_SET);
  write(fd, &new_map, sizeof(MFS_Imap_t));

  cr->imap[imp_index] = offset; 
  lseek(fd, 0, SEEK_SET);
  write(fd, cr, sizeof(MFS_CR_t));

  fsync(fd);
  return 0;
}

int lfs_read(int inum, char* buffer, int db)
{
  if (inum_invalid(inum) || datablock_invalid(db))
  {
    // perror("read: Invalid inode number / data block\n");
    return -1;
  }

  int imp_index = inum / IMAP_ENTRIES;
  if (cr->imap[imp_index] == -1)
  {
    // perror("read: Invalid imap\n");
    return -1;
  }

  int imp_offset = cr->imap[imp_index];
  
  lseek(fd, imp_offset, SEEK_SET);
  MFS_Imap_t imp;
  read(fd, &imp, sizeof(MFS_Imap_t));
  
  int inode_num = inum % IMAP_ENTRIES; 
  int ind_offset = imp.inode_addr[inode_num];
  if (ind_offset == -1)
  {
    // perror("read: Invalid inode addr\n");
    return -1;
  }

  lseek(fd, ind_offset, SEEK_SET);
  MFS_Inode_t ind;
  read(fd, &ind, sizeof(MFS_Inode_t));
  
  if (!(ind.type == MFS_DIRECTORY || ind.type == MFS_REGULAR_FILE))
  {
    // perror("read: Invalid inode type");
    return -1;
  }

  int db_offset = ind.ptrs[db]; 
  lseek(fd, db_offset, SEEK_SET);
  read(fd, buffer, MFS_BLOCK_SIZE); 

  return 0;
}

int lfs_creat(int pinum, int type, char*name)
{
  int offset = 0;
  int imp_index = 0;
  int map_existed = 0;
  int inode_num = 0;

  int imp_offset = -1, ind_offset = -1, db_offset = -1;

  MFS_Imap_t imp;

  if (inum_invalid(pinum))
  {
    // perror("creat: Invalid inode number\n");
    return -1;
  }

  int name_length = 0;
  for (int i = 0; name[i] != '\0'; i++){
		name_length++;
	}

  if (name_length > 28)
  {
    // perror("creat: filename is too long\n");
    return -1;
  }

  if (lfs_lookup(pinum, name) != -1)
  {
    return 0;
  }

  imp_index = pinum / IMAP_ENTRIES; 
  
  inode_num = pinum % IMAP_ENTRIES; 
  MFS_Imap_t imp_parent; // parent imap piece
	imp_offset = cr->imap[imp_index];
  if (imp_offset == -1)
  {
    // perror("creat: Invalid imap piece\n");
    return -1;
  }
  lseek(fd, imp_offset, SEEK_SET);
  read(fd, &imp_parent, sizeof(MFS_Imap_t));
  ind_offset = imp_parent.inode_addr[inode_num]; 
  if (ind_offset == -1)
  {
    // perror("creat: Invalid inode address\n");
    return -1;
  }

  MFS_Inode_t nd_par;
  lseek(fd, ind_offset, SEEK_SET);
  read(fd, &nd_par, sizeof(MFS_Inode_t));

  if (nd_par.type != MFS_DIRECTORY)
  {
    // perror("creat: Not a directory\n");
    return -1;
  }

  int free_inum = -1;
  int if_free_inum_found = 0;
  for (int i = 0; i < INODE_LIMIT / IMAP_ENTRIES; i++)
  {

    imp_offset = cr->imap[i];

    if (imp_offset != -1)
    {
      MFS_Imap_t imp_parent; 
      lseek(fd, imp_offset, SEEK_SET);
      read(fd, &imp_parent, sizeof(MFS_Imap_t));
      for (int j = 0; j < IMAP_ENTRIES; j++)
      {
        ind_offset = imp_parent.inode_addr[j]; 

        if (ind_offset == -1)
        {
					if_free_inum_found = 1;
					int temp = i * IMAP_ENTRIES;
          free_inum = temp + j; 
          break;
        }
      }
    }
    else
    {

      MFS_Imap_t new_map;
      for (int j = 0; j < IMAP_ENTRIES; j++)
        new_map.inode_addr[j] = -1; 

      offset = cr->end;
      cr->end += sizeof(MFS_Imap_t);
      lseek(fd, offset, SEEK_SET);
      write(fd, &new_map, sizeof(MFS_Imap_t));

      cr->imap[i] = offset;
      lseek(fd, 0, SEEK_SET);
      write(fd, cr, sizeof(MFS_CR_t));

      fsync(fd);

      for (int j = 0; j < IMAP_ENTRIES; j++)
      {
        ind_offset = new_map.inode_addr[j];
        
        if (ind_offset == -1)
        {
					if_free_inum_found = 1;
					int temp = i * IMAP_ENTRIES;
          free_inum = temp + j;
          break;
        }
      }
    }

    if (if_free_inum_found)
      break;
  }

  if (free_inum == -1 || free_inum >= INODE_LIMIT)
  {
    // perror("creat: cannot find free inode");
    return -1;
  }

  char data_buf[MFS_BLOCK_SIZE];
  MFS_Dir_t *dir_buf = NULL;
  int flag_found_entry = 0;
  int block_par = 0;
  MFS_Inode_t p_nd = nd_par;

  for (int i = 0; i < INODE_PTRS; i++)
  {
		block_par = i;
    db_offset = p_nd.ptrs[i]; 

    if (db_offset == -1)
    {
      MFS_Dir_t *p_dir = (MFS_Dir_t *)data_buf;
      for (i = 0; i < DIR_ENTRIES; i++)
      {
        strcpy(p_dir->entries[i].name, "\0");
      }
      for (i = 0; i < DIR_ENTRIES; i++)
      {
        p_dir->entries[i].inum = -1;
      }
      
      offset = cr->end;
      lseek(fd, offset, SEEK_SET);
      write(fd, p_dir, sizeof(MFS_Dir_t));
			cr->end += MFS_BLOCK_SIZE;

      db_offset = offset;

      MFS_Inode_t nd_dir_new;
      nd_dir_new.size = nd_par.size;
      nd_dir_new.type = MFS_DIRECTORY;

			MFS_Inode_t *nd_dir_new_ptr = &nd_dir_new;
			MFS_Inode_t *nd_dir_old_ptr = &nd_par;
			copy_inode(nd_dir_new_ptr, nd_dir_old_ptr);

      nd_dir_new.ptrs[block_par] = offset;
      p_nd = nd_dir_new;

      offset = cr->end;
      cr->end += sizeof(MFS_Inode_t);
      lseek(fd, offset, SEEK_SET);
      write(fd, &nd_dir_new, sizeof(MFS_Inode_t));

      MFS_Imap_t mp_dir_new;
      MFS_Imap_t *mp_dir_new_ptr = &mp_dir_new;
      MFS_Imap_t *mp_dir_old_ptr = &imp_parent;
			copy_imap(mp_dir_new_ptr, mp_dir_old_ptr);
      mp_dir_new.inode_addr[inode_num] = offset;

      offset = cr->end;
      
      lseek(fd, offset, SEEK_SET);
      write(fd, &mp_dir_new, sizeof(MFS_Imap_t));
			cr->end += sizeof(MFS_Imap_t);
			fsync(fd);

      cr->imap[imp_index] = offset;
      lseek(fd, 0, SEEK_SET);
      write(fd, cr, sizeof(MFS_CR_t));
      fsync(fd);
    }
		int ofst_size = db_offset;
    lseek(fd, ofst_size, SEEK_SET);
    read(fd, data_buf, MFS_BLOCK_SIZE);

    dir_buf = (MFS_Dir_t *)data_buf;
    for (int j = 0; j < DIR_ENTRIES; j++)
    {
      MFS_DirEnt_t *p_de = &dir_buf->entries[j];
      if (p_de->inum == -1)
      {	
        flag_found_entry = 1;
        int medium = free_inum;
        strcpy(p_de->name, name);
				p_de->inum = medium;
        break;
      }
    }

    if (flag_found_entry)
      break;
  }

  offset = cr->end;
  cr->end += MFS_BLOCK_SIZE;
  lseek(fd, offset, SEEK_SET);
  write(fd, dir_buf, sizeof(MFS_Dir_t));

  MFS_Inode_t nd_par_new;
  nd_par_new.size = p_nd.size;
  nd_par_new.type = MFS_DIRECTORY;
  for (int i = 0; i < INODE_PTRS; i++)
    nd_par_new.ptrs[i] = p_nd.ptrs[i];
  nd_par_new.ptrs[block_par] = offset;

  offset = cr->end;
  cr->end += sizeof(MFS_Inode_t);
  lseek(fd, offset, SEEK_SET);
  write(fd, &nd_par_new, sizeof(MFS_Inode_t));

  MFS_Imap_t new_par_map;
  MFS_Imap_t *new_pmap_ptr = &new_par_map;
  MFS_Imap_t *old_pmap_ptr = &imp_parent;
	copy_imap(new_pmap_ptr, old_pmap_ptr);
  new_par_map.inode_addr[inode_num] = offset;

  offset = cr->end;
  cr->end += sizeof(MFS_Imap_t);
  lseek(fd, offset, SEEK_SET);
  write(fd, &new_par_map, sizeof(MFS_Imap_t));

  cr->imap[imp_index] = offset;
  lseek(fd, 0, SEEK_SET);
  write(fd, cr, sizeof(MFS_CR_t));
  fsync(fd);

  char wr_buffer[MFS_BLOCK_SIZE];
  for (int i = 0; i < MFS_BLOCK_SIZE; i++)
  {
    wr_buffer[i] = '\0';
  }

  int inum = free_inum;
  map_existed = 0;

  if (type == MFS_DIRECTORY)
  {
    MFS_Dir_t *p_dir = (MFS_Dir_t *)wr_buffer;
    for (int i = 2; i < DIR_ENTRIES; i++)
    {
      strcpy(p_dir->entries[i].name, "\0");
    }
    strcpy(p_dir->entries[0].name, ".\0");
		strcpy(p_dir->entries[1].name, "..\0");
    p_dir->entries[0].inum = inum;
    p_dir->entries[1].inum = pinum; 
		for (int i = 2; i < DIR_ENTRIES; i++)
    {
      p_dir->entries[i].inum = -1;
    }

    offset = cr->end; 
    lseek(fd, offset, SEEK_SET);
    write(fd, wr_buffer, MFS_BLOCK_SIZE);
  }

  imp_index = inum / IMAP_ENTRIES; 
  imp_offset = cr->imap[imp_index];
  if (imp_offset != -1)
  {
		lseek(fd, imp_offset, SEEK_SET);
    read(fd, &imp, sizeof(MFS_Imap_t));
    map_existed = 1;

    inode_num = inum % IMAP_ENTRIES;
    ind_offset = imp.inode_addr[inode_num]; 
  }

  MFS_Inode_t new_node;
  MFS_Inode_t *nnode_ptr = &new_node;
  new_node.size = 0;
  new_node.type = type;
	create_empty_inode(nnode_ptr);

  if (type == MFS_DIRECTORY)
    new_node.ptrs[0] = offset;

  offset += MFS_BLOCK_SIZE;
  cr->end += MFS_BLOCK_SIZE;
  cr->end += sizeof(MFS_Inode_t);
  lseek(fd, offset, SEEK_SET);
  write(fd, &new_node, sizeof(MFS_Inode_t));

  MFS_Imap_t new_map;
  MFS_Imap_t *new_map_ptr = &new_map;
  MFS_Imap_t *old_map_ptr = &imp;
  if (map_existed)
  {
		copy_imap(new_map_ptr, old_map_ptr);
  }
  else
  {
		create_empty_imap(new_map_ptr);
  }
	new_map.inode_addr[inode_num] = offset; 

  offset = cr->end;
  cr->end += sizeof(MFS_Imap_t);
  lseek(fd, offset, SEEK_SET);
  write(fd, &new_map, sizeof(MFS_Imap_t));

  cr->imap[imp_index] = offset; 
  lseek(fd, 0, SEEK_SET);
  write(fd, cr, sizeof(MFS_CR_t));
  fsync(fd);
  return 0;
}

int lfs_unlink(int pinum, char*name)
{
  if (inum_invalid(pinum))
  {
    // perror("unlink: Invalid inode number\n");
    return -1;
  }

  int inum = lfs_lookup(pinum, name);
  if (inum == -1)
  {
    return 0;
  }

  int imp_index = inum / IMAP_ENTRIES;
	int imp_offset = cr->imap[imp_index];
  if (imp_offset == -1)
  {
    // perror("unlink: Invalid imap\n");
    return -1;
  }

	MFS_Imap_t imp;
  int inode_num = inum % IMAP_ENTRIES; 
  lseek(fd, imp_offset, SEEK_SET);
  read(fd, &imp, sizeof(MFS_Imap_t));
  int ind_offset = imp.inode_addr[inode_num];
  if (ind_offset == -1)
  {
    // perror("unlink: Invalid inode address\n");
    return -1;
  }

	MFS_Inode_t ind;
  lseek(fd, ind_offset, SEEK_SET);
  read(fd, &ind, sizeof(MFS_Inode_t));

  if (ind.type == MFS_DIRECTORY)
  {
    char data_buffer[MFS_BLOCK_SIZE];
    for (int i = 0; i < INODE_PTRS; i++)
    {
      int db_offset = ind.ptrs[i];
      if (db_offset == -1)
        continue;

      lseek(fd, db_offset, SEEK_SET);
      read(fd, data_buffer, MFS_BLOCK_SIZE);

      MFS_Dir_t *dir_buffer = (MFS_Dir_t *)data_buffer;
      for (int j = 0; j < DIR_ENTRIES; j++)
      {
        MFS_DirEnt_t *entry = &dir_buffer->entries[j];
        if (entry->inum != pinum && entry->inum != inum && entry->inum != -1)
        {
          // perror("unlink: Directory is not empty\n");
          return -1;
        }
      }
    }
  }

  MFS_Imap_t new_imp;
  MFS_Imap_t *new_map_ptr = &new_imp;
  MFS_Imap_t *old_map_ptr = &imp;
		copy_imap(new_map_ptr, old_map_ptr);
	new_imp.inode_addr[inode_num] = -1; 

  int if_new_imp_empty = 1;
  for (int i = 0; i < IMAP_ENTRIES; i++)
  {
		int ad = new_imp.inode_addr[i];
    if (ad != -1)
    {
      if_new_imp_empty = 0;
      break;
    }
  }

	int offset = -1;
  if (!if_new_imp_empty)
  {
		offset = cr->end;
    cr->end += sizeof(MFS_Imap_t);
    lseek(fd, offset, SEEK_SET);
    write(fd, &new_imp, sizeof(MFS_Imap_t));

    cr->imap[imp_index] = offset;
  }
  if(if_new_imp_empty)
  {
    cr->imap[imp_index] = -1;
  }
	lseek(fd, 0, SEEK_SET);
  write(fd, cr, sizeof(MFS_CR_t));
  fsync(fd);

  imp_index = pinum / IMAP_ENTRIES;
  int imp_p_offset = cr->imap[imp_index];
  if (imp_p_offset == -1)
  {
    // perror("unlink: Invalid parent imap\n");
    return -1;
  }

  inode_num = pinum % IMAP_ENTRIES;
  MFS_Imap_t imp_parent;
  lseek(fd, imp_p_offset, SEEK_SET);
  read(fd, &imp_parent, sizeof(MFS_Imap_t));
  int ind_p_offset = imp_parent.inode_addr[inode_num];
  if (ind_p_offset == -1)
  {
    // perror("unlink: Invalid parent inode address\n");
    return -1;
  }

  MFS_Inode_t ind_parent;
  lseek(fd, ind_p_offset, SEEK_SET);
  read(fd, &ind_parent, sizeof(MFS_Inode_t));

  if (ind_parent.type != MFS_DIRECTORY)
  {
    // perror("unlink: Not a directory\n");
    return -1;
  }

  char data_buffer[MFS_BLOCK_SIZE];
  MFS_Dir_t *dir_buffer = NULL;
  int entry_found = 0;
  int db_parent = 0;
  for (int i = 0; i < INODE_PTRS; i++)
  {

    int db_p_offset = ind_parent.ptrs[i];
    if (db_p_offset == -1)
      continue;
    db_parent = i;
    lseek(fd, db_p_offset, SEEK_SET);
    read(fd, data_buffer, MFS_BLOCK_SIZE);

    dir_buffer = (MFS_Dir_t *)data_buffer;
    for (int j = 0; j < DIR_ENTRIES; j++)
    {
      MFS_DirEnt_t *entry = &dir_buffer->entries[j];
      if (entry->inum == inum)
      {
				entry_found = 1;
        
        strcpy(entry->name, "\0");
        entry->inum = -1;
        break;
      }
    }

    if (entry_found)
      break;
  }

  if (!entry_found)
  {
    return 0;
  }

  offset = cr->end;
  cr->end += MFS_BLOCK_SIZE;
  lseek(fd, offset, SEEK_SET);
  write(fd, dir_buffer, sizeof(MFS_Dir_t));

  MFS_Inode_t new_ind_parent;
  MFS_Inode_t *new_ind_parent_ptr = &new_ind_parent;
  MFS_Inode_t *old_ind_parent_ptr = &ind_parent;
	int determinant = ind_parent.size - MFS_BLOCK_SIZE;
	if (determinant > 0){
		new_ind_parent.size = ind_parent.size - MFS_BLOCK_SIZE;
	} else {
		new_ind_parent.size = 0;
	}

  new_ind_parent.type = MFS_DIRECTORY;
	copy_inode(new_ind_parent_ptr, old_ind_parent_ptr);
  new_ind_parent.ptrs[db_parent] = offset;

  offset = cr->end;
  cr->end += sizeof(MFS_Inode_t);
  lseek(fd, offset, SEEK_SET);
  write(fd, &new_ind_parent, sizeof(MFS_Inode_t));

  MFS_Imap_t new_imp_parent;
  MFS_Imap_t *new_pimp_ptr = &new_imp_parent;
  MFS_Imap_t *old_pimp_ptr = &imp_parent;
	copy_imap(new_pimp_ptr, old_pimp_ptr);
  new_imp_parent.inode_addr[inode_num] = offset;

  offset = cr->end;
  cr->end += sizeof(MFS_Imap_t);
  lseek(fd, offset, SEEK_SET);
  write(fd, &new_imp_parent, sizeof(MFS_Imap_t));

  cr->imap[imp_index] = offset;
  lseek(fd, 0, SEEK_SET);
  write(fd, cr, sizeof(MFS_CR_t));
  fsync(fd);
  return 0;
}

int lfs_shutdown()
{
  fsync(fd);
  exit(0);
}

int request_type (int sd, struct sockaddr_in sock, MFS_MSG_t msg_sd, MFS_MSG_t msg_rc) 
{
  if (msg_sd.req == LOOKUP)
    {
      msg_rc.inum = lfs_lookup(msg_sd.inum, msg_sd.name);
    }
    else if (msg_sd.req == STAT)
    {
      msg_rc.inum = lfs_stat(msg_sd.inum, &(msg_rc.stat));
    }
    else if (msg_sd.req == WRITE)
    {
      msg_rc.inum = lfs_write(msg_sd.inum, msg_sd.buffer, msg_sd.block);
    }
    else if (msg_sd.req == READ)
    {
      msg_rc.inum = lfs_read(msg_sd.inum, msg_rc.buffer, msg_sd.block);
    }
    else if (msg_sd.req == CREAT)
    {
      msg_rc.inum = lfs_creat(msg_sd.inum, msg_sd.stat.type, msg_sd.name);
    }
    else if (msg_sd.req == UNLINK)
    {
      msg_rc.inum = lfs_unlink(msg_sd.inum, msg_sd.name);
    }
    else if (msg_sd.req == SHUTDOWN)
    {
      msg_rc.req = RESPONSE;
      UDP_Write(sd, &sock, (char*)&msg_rc, sizeof(MFS_MSG_t));
      lfs_shutdown();
      return 0;
    }
    else
    {
      // perror("Illegal Request Received");
      return -1;
    }

    msg_rc.req = RESPONSE;
    UDP_Write(sd, &sock, (char*)&msg_rc, sizeof(MFS_MSG_t));
    return 0;
}

int lfs_init(int port, char* image_path)
{
  fd = open(image_path, O_RDWR | O_CREAT, S_IRWXU);
  if (fd < 0)
  {
    // perror("init: Cannot open file");
  }
  struct stat f_stat;
  if (fstat(fd, &f_stat) < 0)
  {
    // perror("init: Cannot open file");
  }

  cr = (MFS_CR_t*)malloc(sizeof(MFS_CR_t));
  

  if (f_stat.st_size < sizeof(MFS_CR_t))
  {
    fd = open(image_path, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU);
    if (fd < 0)
      return -1;

    for (int i = 0; i < INODE_LIMIT / IMAP_ENTRIES; i++)
      cr->imap[i] = -1;
		cr->end = sizeof(MFS_CR_t);

    lseek(fd, 0, SEEK_SET);
    write(fd, cr, sizeof(MFS_CR_t));

    MFS_Dir_t db;
		for (int i = 2; i < DIR_ENTRIES; i++)
    {
      strcpy(db.entries[i].name, "\0");
    }
		strcpy(db.entries[0].name, ".\0");
		strcpy(db.entries[1].name, "..\0");
    db.entries[0].inum = 0;
    db.entries[1].inum = 0;
    for (int i = 2; i < DIR_ENTRIES; i++)
    {
      db.entries[i].inum = -1;
    }
    

		int cr_offset = cr->end;
    cr->end += MFS_BLOCK_SIZE;
    lseek(fd, cr_offset, SEEK_SET);
    write(fd, &db, sizeof(MFS_Dir_t));


    MFS_Inode_t ind;
    MFS_Inode_t *ind_ptr = &ind;
    ind.size = 0;
    ind.type = MFS_DIRECTORY;
		create_empty_inode(ind_ptr);
    ind.ptrs[0] = cr_offset;

		int nd_offset = 0;
    nd_offset = cr->end;
    cr->end += sizeof(MFS_Inode_t);
    lseek(fd, nd_offset, SEEK_SET);
    write(fd, &ind, sizeof(MFS_Inode_t));


    MFS_Imap_t imp;
    MFS_Imap_t* imp_ptr = &imp;
		create_empty_imap(imp_ptr);
    imp.inode_addr[0] = nd_offset;

		int mp_offset = 0;
    mp_offset = cr->end;
    cr->end += sizeof(MFS_Imap_t);
    lseek(fd, mp_offset, SEEK_SET);
    write(fd, &imp, sizeof(MFS_Imap_t));

    cr->imap[0] = mp_offset; 
    lseek(fd, 0, SEEK_SET);
    write(fd, cr, sizeof(MFS_CR_t));

    fsync(fd);
  }
  else
  {
    lseek(fd, 0, SEEK_SET);
    read(fd, cr, sizeof(MFS_CR_t));
  }


  int sd = UDP_Open(port);
  if (sd < 0)
  {
    // perror("Port occupied");
    return -1;
  }

  struct sockaddr_in sock;
  MFS_MSG_t msg_sd;
  MFS_MSG_t msg_rc;

  while (1)
  {
    if (UDP_Read(sd, &sock, (char*)&msg_sd, sizeof(MFS_MSG_t)) < 1)
      continue;
    request_type(sd, sock, msg_sd, msg_rc);
  }
  return 0;
}

int main(int argc, char*argv[]) {
  if (argc != 3)
  {
    // perror("Usage: server <portnum> <image>\n");
    exit(1);
  }
  lfs_init(atoi(argv[1]), argv[2]);
}